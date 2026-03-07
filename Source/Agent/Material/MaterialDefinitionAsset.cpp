// Copyright Epic Games, Inc. All Rights Reserved.

#include "Material/MaterialDefinitionAsset.h"
#include "GameFramework/Actor.h"

FName UMaterialDefinitionAsset::GetResolvedResourceId() const
{
	return ResourceId.IsNone() ? GetFName() : ResourceId;
}

TSubclassOf<AActor> UMaterialDefinitionAsset::ResolveOutputActorClass() const
{
	if (OutputActorClass.IsNull())
	{
		return nullptr;
	}

	UClass* LoadedClass = OutputActorClass.LoadSynchronous();
	if (!LoadedClass || !LoadedClass->IsChildOf(AActor::StaticClass()))
	{
		return nullptr;
	}

	return LoadedClass;
}

FPrimaryAssetId UMaterialDefinitionAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("Material"), GetResolvedResourceId());
}

