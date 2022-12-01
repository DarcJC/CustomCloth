﻿// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "ClothMeshComponent.generated.h"

#pragma region Forward Decl
class FPrimitiveSceneProxy;
class FClothMeshSceneProxy;
#pragma endregion Forward Decl


USTRUCT(BlueprintType)
struct FClothMeshVertex
{
	GENERATED_BODY()
public:

	/** Vertex position */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vertex)
	FVector Position;

	/** Vertex normal */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vertex)
	FVector Normal;

	/** Vertex color */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vertex)
	FColor Color;

	FClothMeshVertex()
		: Position(0.f, 0.f, 0.f)
		, Normal(0.f, 0.f, 1.f)
		, Color(255, 255, 255)
	{}

	explicit FClothMeshVertex(const FVector& InPosition, const FColor& InColor = {255, 255, 255}, const FVector& InNormal = {0.f, 0.f, 1.f})
		: Position(InPosition)
		, Normal(InNormal)
		, Color(InColor)
	{}
};

USTRUCT(BlueprintType)
struct FClothMeshData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FClothMeshVertex> VertexBuffer;
	UPROPERTY()
	TArray<uint32> IndexBuffer;

	FORCEINLINE void Reset()
	{
		VertexBuffer.Empty();
		IndexBuffer.Empty();
	}
};

UCLASS(meta = (BlueprintSpawnableComponent), ClassGroup = Rendering)
class CUSTOMCLOTH_API UClothMeshComponent : public UMeshComponent
{
	GENERATED_BODY()

public:
	void RecreateMesh();
	void SendMeshDataToRenderThread() const;

private:
	void RecreateMeshData();
	void UpdateLocalBounds();

public:
	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	//~ End UPrimitiveComponent Interface.
	
	//~ Begin UActorComponent Interface.
	virtual void OnRegister() override;
	//~ End UActorComponent Interface.

	//~ Begin UMeshComponent Interface.
	virtual int32 GetNumMaterials() const override;
	//~ End UMeshComponent Interface.

private:
	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End USceneComponent Interface.

	friend class FClothMeshSceneProxy;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ClothMeshComponent")
	UMaterialInterface* ClothMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ClothMeshComponent")
	FClothMeshData ClothMesh;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ClothMeshComponent")
	FVector2D ClothSize { 10.0f, 10.0f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ClothMeshComponent")
	FColor ClothColor;
	
private:
	UPROPERTY()
	FBoxSphereBounds LocalBounds;
};
