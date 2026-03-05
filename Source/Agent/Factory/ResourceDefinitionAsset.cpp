// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/ResourceDefinitionAsset.h"
#include "GameFramework/Actor.h"

FName UResourceDefinitionAsset::GetResolvedResourceId() const
{
	return ResourceId.IsNone() ? GetFName() : ResourceId;
}

TSubclassOf<AActor> UResourceDefinitionAsset::ResolveOutputActorClass() const
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

FName UResourceDefinitionAsset::GetResolvedScrapResourceId() const
{
	if (ScrapResource)
	{
		return ScrapResource->GetResolvedResourceId();
	}

	return GetResolvedResourceId();
}

FPrimaryAssetId UResourceDefinitionAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("Resource"), GetResolvedResourceId());
}
