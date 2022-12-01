// Fill out your copyright notice in the Description page of Project Settings.


#include "ClothMeshComponent.h"

#include "DynamicMeshBuilder.h"
#include "MeshMaterialShader.h"

#pragma region Forward Decl
class FClothMeshVertexFactoryShaderParameters;
class FClothMeshSceneProxy;
#pragma endregion  Forward Decl

#pragma region Vertex Factory

struct FClothMeshVertexFactory : FLocalVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FClothMeshVertexFactory);

public:
	explicit FClothMeshVertexFactory(ERHIFeatureLevel::Type InFeatureLevel)
		: FLocalVertexFactory(InFeatureLevel, "FClothMeshVertexFactory")
		, SceneProxy(nullptr)
	{
	}

	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& InParameters)
	{
		return InParameters.MaterialParameters.MaterialDomain != MD_UI;
	}

	// Passing preprocess directives.
	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& InParameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		if (const bool ContainsManualVertexFetch = OutEnvironment.GetDefinitions().Contains("MANUAL_VERTEX_FETCH"); !ContainsManualVertexFetch)
		{
			OutEnvironment.SetDefine(TEXT("MANUAL_VERTEX_FETCH"), TEXT("0"));
		}

		OutEnvironment.SetDefine(TEXT("CLOTH_MESH"), TEXT("1"));
	}

	void InitVertexResource_RenderThread(FRHICommandListImmediate& RHICmdList)
	{
		FRHIResourceCreateInfo CreateInfo(TEXT("ClothMesh:InitVertexResource_RenderThread"));
		CreateInfo.ResourceArray = nullptr;

		PositionBuffer.InitRHI();
		NormalBuffer.InitRHI();
	}

	virtual void InitRHI() override
	{
		FLocalVertexFactory::InitRHI();

		PositionBuffer.Init(0);
		NormalBuffer.Init(0);
		ENQUEUE_RENDER_COMMAND(FClothMeshVertexFactoryInitRHI) (
			[this](FRHICommandListImmediate& RHICmdList)
			{
				InitVertexResource_RenderThread(RHICmdList);
			}
		);
		//TODO
	}

	virtual void ReleaseRHI() override
	{
		// We're not creating any resources.
		// Nothing to do :).
		FLocalVertexFactory::ReleaseRHI();
	}

	FORCEINLINE void SetSceneProxy(FClothMeshSceneProxy* Proxy) { SceneProxy = Proxy; }

private:

	FClothMeshSceneProxy* SceneProxy;

	FPositionVertexBuffer PositionBuffer;
	FPositionVertexBuffer NormalBuffer;

	friend class FClothMeshVertexFactoryShaderParameters;
};

class FClothMeshVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
	DECLARE_TYPE_LAYOUT(FClothMeshVertexFactoryShaderParameters, NonVirtual);

public:
	void Bind(const FShaderParameterMap& ParameterMap)
	{
		PositionBufferParamter.Bind(ParameterMap, TEXT("PositionBuffer"));
		NormalBufferParamter.Bind(ParameterMap, TEXT("NormalBuffer"));
	}
	
	void GetElementShaderBindings(
		const FSceneInterface* Scene,
		const FSceneView* View,
		const FMeshMaterialShader* Shader,
		const EVertexInputStreamType InputStreamType,
		ERHIFeatureLevel::Type FeatureLevel,
		const FVertexFactory* VertexFactory,
		const FMeshBatchElement& BatchElement,
		FMeshDrawSingleShaderBindings& ShaderBindings,
		FVertexInputStreamArray& VertexStreams) const
	{
		FClothMeshVertexFactory* ClothMeshVertexFactory = const_cast<FClothMeshVertexFactory*>(static_cast<const FClothMeshVertexFactory*>(VertexFactory));
	
		ShaderBindings.Add(PositionBufferParamter, ClothMeshVertexFactory->PositionBuffer.GetSRV());
		ShaderBindings.Add(NormalBufferParamter, ClothMeshVertexFactory->NormalBuffer.GetSRV());
	}

private:
	LAYOUT_FIELD(FShaderResourceParameter, PositionBufferParamter);
	LAYOUT_FIELD(FShaderResourceParameter, NormalBufferParamter);
};

IMPLEMENT_TYPE_LAYOUT(FClothMeshVertexFactoryShaderParameters);

IMPLEMENT_VERTEX_FACTORY_PARAMETER_TYPE(FClothMeshVertexFactory, SF_Vertex, FVertexFactoryShaderParameters);

IMPLEMENT_VERTEX_FACTORY_TYPE(FClothMeshVertexFactory, "/Plugin/CustomClothShader/Private/LocalVertexFactory.ush", EVertexFactoryFlags::SupportsPositionOnly);

#pragma endregion Vertex Factory

#pragma region Proxies

static void ConvertClothMeshToDynMeshVertex(FDynamicMeshVertex& OutVert, const FClothMeshVertex& InVert)
{
	OutVert.Position = static_cast<FVector3f>(InVert.Position);
	OutVert.Color = InVert.Color;
}

class FClothMeshProxyData
{
public:
	/** Vertex buffer for this section */
	FStaticMeshVertexBuffers VertexBuffers;
	/** Index buffer for this section */
	FDynamicMeshIndexBuffer32 IndexBuffer;

#if RHI_RAYTRACING
	FRayTracingGeometry RayTracingGeometry;
#endif
};

class FClothMeshSceneProxy final : public FPrimitiveSceneProxy
{
public:
	//~Start FPrimitiveSceneProxy Interface
	virtual SIZE_T GetTypeHash() const override
	{
		static size_t UniquePointer;
		return reinterpret_cast<size_t>(&UniquePointer);
	}

	virtual ~FClothMeshSceneProxy() override
	{
		ProxyData.VertexBuffers.PositionVertexBuffer.ReleaseResource();
		ProxyData.VertexBuffers.StaticMeshVertexBuffer.ReleaseResource();
		ProxyData.VertexBuffers.ColorVertexBuffer.ReleaseResource();
		ProxyData.IndexBuffer.ReleaseResource();
		VertexFactory.ReleaseResource();
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View);
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bDynamicRelevance = true;
		Result.bRenderInMainPass = ShouldRenderInMainPass();
		Result.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
		Result.bRenderCustomDepth = ShouldRenderCustomDepth();
		Result.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;
		MaterialRelevance.SetPrimitiveViewRelevance(Result);
		Result.bVelocityRelevance = DrawsVelocity() && Result.bOpaque && Result.bRenderInMainPass;
		return Result;
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily,
	                                    uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{

		// Set up wireframe material (if needed)
		const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;

		FColoredMaterialRenderProxy* WireframeMaterialInstance = nullptr;
		if (bWireframe)
		{
			WireframeMaterialInstance = new FColoredMaterialRenderProxy(
				GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy() : nullptr,
				FLinearColor(0, 0.5f, 1.f)
				);

			Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);
		}

		const FMaterialRenderProxy* MaterialProxy = bWireframe ? WireframeMaterialInstance : UMaterial::GetDefaultMaterial(MD_Surface)->GetRenderProxy();
		
		for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
		{
			if (VisibilityMap & (1 << ViewIndex))
			{
				const FSceneView* View = Views[ViewIndex];
				// Draw the mesh.
				FMeshBatch& Mesh = Collector.AllocateMesh();
				FMeshBatchElement& BatchElement = Mesh.Elements[0];
				BatchElement.IndexBuffer = &ProxyData.IndexBuffer;
				Mesh.bWireframe = bWireframe;
				Mesh.VertexFactory = &VertexFactory;
				Mesh.MaterialRenderProxy = MaterialProxy;

				bool bHasPrecomputedVolumetricLightmap;
				FMatrix PreviousLocalToWorld;
				int32 SingleCaptureIndex;
				bool bOutputVelocity;
				GetScene().GetPrimitiveUniformShaderParameters_RenderThread(GetPrimitiveSceneInfo(), bHasPrecomputedVolumetricLightmap, PreviousLocalToWorld, SingleCaptureIndex, bOutputVelocity);
				bOutputVelocity |= AlwaysHasVelocity();

				FDynamicPrimitiveUniformBuffer& DynamicPrimitiveUniformBuffer = Collector.AllocateOneFrameResource<FDynamicPrimitiveUniformBuffer>();
				DynamicPrimitiveUniformBuffer.Set(GetLocalToWorld(), PreviousLocalToWorld, GetBounds(), GetLocalBounds(), GetLocalBounds(), true, bHasPrecomputedVolumetricLightmap, bOutputVelocity, GetCustomPrimitiveData());
				BatchElement.PrimitiveUniformBufferResource = &DynamicPrimitiveUniformBuffer.UniformBuffer;

				BatchElement.FirstIndex = 0;
				BatchElement.NumPrimitives = ProxyData.IndexBuffer.Indices.Num() / 3;
				BatchElement.MinVertexIndex = 0;
				BatchElement.MaxVertexIndex = ProxyData.VertexBuffers.PositionVertexBuffer.GetNumVertices() - 1;
				Mesh.ReverseCulling = IsLocalToWorldDeterminantNegative();
				Mesh.Type = PT_TriangleList;
				Mesh.DepthPriorityGroup = SDPG_World;
				Mesh.bCanApplyViewModeOverrides = false;
				Collector.AddMesh(ViewIndex, Mesh);
			}
		}
	}

	virtual bool CanBeOccluded() const override
	{
		return !MaterialRelevance.bDisableDepthTest;
	}

	virtual uint32 GetMemoryFootprint() const override
	{
		return sizeof(*this) + GetAllocatedSize();
	}

	uint32 GetAllocatedSize(void) const
	{
		return FPrimitiveSceneProxy::GetAllocatedSize();
	}
	
	//~End FPrimitiveSceneProxy Interface

	explicit FClothMeshSceneProxy(UClothMeshComponent* InComponent)
		: FPrimitiveSceneProxy(InComponent)
		, VertexFactory(InComponent->GetScene()->GetFeatureLevel(), "FClothMeshSceneProxy")
	{
		ClothColor = InComponent->ClothColor;
		
		const auto& [VertexBuffer, IndexBuffer] = InComponent->ClothMesh;
		
		const int32 NumVerts = VertexBuffer.Num();

		TArray<FDynamicMeshVertex> Vertices;
		Vertices.SetNumUninitialized(NumVerts);
		// Copy verts
		for (int VertIdx = 0; VertIdx < NumVerts; VertIdx++)
		{
			const FClothMeshVertex& ClothMeshVertex = VertexBuffer[VertIdx];
			FDynamicMeshVertex& Vert = Vertices[VertIdx];
			ConvertClothMeshToDynMeshVertex(Vert, ClothMeshVertex);
		}

		// Copy indices
		ProxyData.IndexBuffer.Indices = IndexBuffer;
		ProxyData.VertexBuffers.InitFromDynamicVertex(&VertexFactory, Vertices, 4);

		BeginInitResource(&ProxyData.VertexBuffers.PositionVertexBuffer);
		BeginInitResource(&ProxyData.VertexBuffers.StaticMeshVertexBuffer);
		BeginInitResource(&ProxyData.VertexBuffers.ColorVertexBuffer);
		BeginInitResource(&ProxyData.IndexBuffer);
		BeginInitResource(&VertexFactory);

		if (nullptr == MaterialInterface)
		{
			MaterialInterface = UMaterial::GetDefaultMaterial(MD_Surface);
		}
	}

	void SetMeshData_RenderThread(FClothMeshData* ClothMeshData)
	{
		check(IsInRenderingThread());

		const int32 NumVerts = ClothMeshData->VertexBuffer.Num();
		for (int32 Idx = 0; Idx < NumVerts; ++Idx)
		{
			const FClothMeshVertex& NewVert = ClothMeshData->VertexBuffer[Idx];
			FDynamicMeshVertex Vertex;
			ConvertClothMeshToDynMeshVertex(Vertex, NewVert);

			ProxyData.VertexBuffers.PositionVertexBuffer.VertexPosition(Idx) = Vertex.Position;
			ProxyData.VertexBuffers.ColorVertexBuffer.VertexColor(Idx) = Vertex.Color;

			{
				auto& VertexBuffer = ProxyData.VertexBuffers.PositionVertexBuffer;
				void* VertexBufferData = RHILockBuffer(VertexBuffer.VertexBufferRHI, 0, VertexBuffer.GetNumVertices() * VertexBuffer.GetStride(), RLM_WriteOnly);
				FMemory::Memcpy(VertexBufferData, VertexBuffer.GetVertexData(), VertexBuffer.GetNumVertices() * VertexBuffer.GetStride());
				RHIUnlockBuffer(VertexBuffer.VertexBufferRHI);
			}
			
			{
				auto& VertexBuffer = ProxyData.VertexBuffers.ColorVertexBuffer;
				void* VertexBufferData = RHILockBuffer(VertexBuffer.VertexBufferRHI, 0, VertexBuffer.GetNumVertices() * VertexBuffer.GetStride(), RLM_WriteOnly);
				FMemory::Memcpy(VertexBufferData, VertexBuffer.GetVertexData(), VertexBuffer.GetNumVertices() * VertexBuffer.GetStride());
				RHIUnlockBuffer(VertexBuffer.VertexBufferRHI);
			}
		}
	}

private:
	FMaterialRelevance MaterialRelevance;
	UMaterialInterface* MaterialInterface;

	FColor ClothColor;

	FClothMeshProxyData ProxyData;

	FLocalVertexFactory VertexFactory;
};
#pragma endregion Proxies

#pragma region Component
void UClothMeshComponent::RecreateMesh()
{
	RecreateMeshData();
}

void UClothMeshComponent::SendMeshDataToRenderThread() const
{
	// Do a copy
	FClothMeshData* NewClothMesh = new FClothMeshData;
	*NewClothMesh = ClothMesh;

	// enqueue command
	if (FClothMeshSceneProxy* ClothMeshSceneProxy = static_cast<FClothMeshSceneProxy*>(SceneProxy))
	{
		ENQUEUE_RENDER_COMMAND(FClothMeshData)(
			[ClothMeshSceneProxy, NewClothMesh] (FRHICommandListImmediate& RHICmdList)
			{
				ClothMeshSceneProxy->SetMeshData_RenderThread(NewClothMesh);
			}
		);
	}
}

void UClothMeshComponent::RecreateMeshData()
{
	ClothMesh.Reset();

	const FVector LocalCenter = FVector::ZeroVector;
	const FVector LocalXAxis { 1.0f, .0f, .0f };
	const FVector LocalYAxis { 1.0f, .0f, .0f };
	const float Width = ClothSize.X;
	const float Height = ClothSize.Y;
	
	FVector XOffset = LocalXAxis * Width * .5f;
	FVector YOffset = LocalYAxis * Height * .5f;

	ClothMesh.VertexBuffer.Add(FClothMeshVertex{ { .0f, .0f, .0f} });
	ClothMesh.VertexBuffer.Add(FClothMeshVertex{ { Width, .0f, .0f} });
	ClothMesh.VertexBuffer.Add(FClothMeshVertex{ { .0f, Height, .0f} });
	ClothMesh.VertexBuffer.Add(FClothMeshVertex{ { Width, Height, .0f} });

	ClothMesh.IndexBuffer.Append({
		0, 2, 1,
		1, 2, 3,
	});

	UpdateLocalBounds();
}

void UClothMeshComponent::UpdateLocalBounds()
{
	FBox LocalBox(ForceInit);

	for (const auto& MeshVertex : ClothMesh.VertexBuffer)
	{
		const FVector Pos { MeshVertex.Position.X, MeshVertex.Position.Y, MeshVertex.Position.Z };
		LocalBox += Pos;
	}

	LocalBounds = LocalBox.IsValid ? FBoxSphereBounds(LocalBox) : FBoxSphereBounds{ {}, {},  0};

	UpdateBounds();
	MarkRenderTransformDirty();
}

FPrimitiveSceneProxy* UClothMeshComponent::CreateSceneProxy()
{
	return new FClothMeshSceneProxy(this);
}

void UClothMeshComponent::OnRegister()
{
	RecreateMesh();
	Super::OnRegister();
}

int32 UClothMeshComponent::GetNumMaterials() const
{
	return Super::GetNumMaterials();
}

FBoxSphereBounds UClothMeshComponent::CalcBounds(const FTransform& LocalToWorld) const
{
	FBoxSphereBounds BoxBounds(LocalBounds.TransformBy(LocalToWorld));

	BoxBounds.BoxExtent *= BoundsScale;
	BoxBounds.SphereRadius *= BoundsScale;

	return BoxBounds;
}
#pragma endregion Component
