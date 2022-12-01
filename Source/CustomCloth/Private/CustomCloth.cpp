// Copyright Epic Games, Inc. All Rights Reserved.

#include "CustomCloth.h"

#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FCustomClothModule"

void FCustomClothModule::StartupModule()
{
	FString RealShaderDir = FPaths::Combine(
		IPluginManager::Get().FindPlugin(TEXT("CustomCloth"))->GetBaseDir(),
		TEXT("Shaders")
	);
	AddShaderSourceDirectoryMapping("/Plugin/CustomClothShader", RealShaderDir);
	
}

void FCustomClothModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FCustomClothModule, CustomCloth)