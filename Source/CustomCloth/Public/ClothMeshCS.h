// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "Renderer/Private/SceneTextures.h"


BEGIN_SHADER_PARAMETER_STRUCT(FClothMeshCSParameters, )
	SHADER_PARAMETER_RDG_BUFFER_SRV(RWStructuredBuffer<float4>, Positions)
	SHADER_PARAMETER_RDG_BUFFER_SRV(RWStructuredBuffer<float3>, Velocities)
END_SHADER_PARAMETER_STRUCT()

inline FClothMeshCSParameters GetClothMeshCSParameters(FRDGBufferSRV* const PositionBuffer, FRDGBufferSRV* const VelocityBuffer)
{
	FClothMeshCSParameters Parameters;
	Parameters.Positions = PositionBuffer;
	Parameters.Velocities = VelocityBuffer;
	return Parameters;
}

class FClothMeshCS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FClothMeshCS);
	SHADER_USE_PARAMETER_STRUCT_WITH_LEGACY_BASE(FClothMeshCS, FGlobalShader);

	using FParameters = FClothMeshCSParameters;
	
public:

protected:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5) || Parameters.Platform == SP_PCD3D_ES3_1;
	}
	
	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
	
};

IMPLEMENT_GLOBAL_SHADER(FClothMeshCS, "/Plugin/CustomClothShader/Private/CustomClothCS.usf", "MainCS", SF_Compute);

struct FClothMeshCSDispatcherParameters
{
	TResourceArray<FVector4f> Positions;
	TResourceArray<FVector3f> Velocities;
};

class CUSTOMCLOTH_API FClothMeshCSDispatcher
{
public:
	FClothMeshCSDispatcher();

	void BeginRendering();
	void EndRendering();
	void InitRHI(const FClothMeshCSDispatcherParameters& InParameters);
private:
	void Execute_RenderThread(FRDGBuilder& Builder, const FSceneTextures& SceneTextures);

	FDelegateHandle OnPostResolvedSceneColorHandle;

	FClothMeshCSDispatcherParameters CachedParameters;

	FBufferRHIRef PosBuffer;
	FBufferRHIRef VelBuffer;
};
