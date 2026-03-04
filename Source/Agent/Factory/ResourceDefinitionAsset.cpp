// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/ResourceDefinitionAsset.h"

FName UResourceDefinitionAsset::GetResolvedResourceId() const
{
	return ResourceId.IsNone() ? GetFName() : ResourceId;
}

FPrimaryAssetId UResourceDefinitionAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("Resource"), GetResolvedResourceId());
}
