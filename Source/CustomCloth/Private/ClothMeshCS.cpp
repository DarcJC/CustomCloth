// Fill out your copyright notice in the Description page of Project Settings.


#include "ClothMeshCS.h"

void FClothMeshCSDispatcher::BeginRendering()
{
	if (OnPostResolvedSceneColorHandle.IsValid())
	{
		return;
	}

	const FName RendererModuleName("Renderer");
	if (IRendererModule* RendererModule = FModuleManager::GetModulePtr<IRendererModule>(RendererModuleName); nullptr != RendererModule)
	{
		OnPostResolvedSceneColorHandle = RendererModule->GetResolvedSceneColorCallbacks().AddRaw(this, &FClothMeshCSDispatcher::Execute_RenderThread);
	}
}

void FClothMeshCSDispatcher::InitRHI(const FClothMeshCSDispatcherParameters& InParameters)
{
	CachedParameters.Positions.Empty();
	CachedParameters.Velocities.Empty();

	CachedParameters.Positions.Append(InParameters.Positions);
	CachedParameters.Velocities.Append(InParameters.Velocities);

	// Create Buffer
	FRHIResourceCreateInfo PosCreateInfo(TEXT("FClothMeshCSDispatcher::InitRHI_PositionArray"), &CachedParameters.Positions);
	FRHIResourceCreateInfo VelCreateInfo(TEXT("FClothMeshCSDispatcher::InitRHI_VelocityArray"), &CachedParameters.Velocities);
	
	PosBuffer = RHICreateStructuredBuffer(sizeof(FVector4f), sizeof(FVector4f) * CachedParameters.Positions.Num(), BUF_StructuredBuffer | BUF_UnorderedAccess, PosCreateInfo);
	VelBuffer = RHICreateStructuredBuffer(sizeof(FVector3f), sizeof(FVector3f) * CachedParameters.Velocities.Num(), BUF_StructuredBuffer | BUF_UnorderedAccess, VelCreateInfo);
}

void FClothMeshCSDispatcher::Execute_RenderThread(FRDGBuilder& Builder, const FSceneTextures& SceneTextures)
{
	FRHICommandListImmediate& RHICmdList = Builder.RHICmdList;

	check(IsInRenderingThread());
}
