// Copyright Epic Games, Inc. All Rights Reserved.

#include "Material/MaterialDefinitionAsset.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"

namespace
{
TSubclassOf<AActor> ResolveSoftOutputActorClass(const TSoftClassPtr<AActor>& SoftClass)
{
	if (SoftClass.IsNull())
	{
		return nullptr;
	}

	UClass* LoadedClass = SoftClass.LoadSynchronous();
	if (!LoadedClass || !LoadedClass->IsChildOf(AActor::StaticClass()))
	{
		return nullptr;
	}

	return LoadedClass;
}
}

FName UMaterialDefinitionAsset::GetResolvedMaterialId() const
{
	if (!MaterialId.IsNone())
	{
		return MaterialId;
	}

	return GetFName();
}

TSubclassOf<AActor> UMaterialDefinitionAsset::ResolveOutputActorClass(EMaterialOutputForm OutputForm) const
{
	if (OutputForm == EMaterialOutputForm::Pure)
	{
		if (const TSubclassOf<AActor> PureClass = ResolveSoftOutputActorClass(PureOutputActorClass))
		{
			return PureClass;
		}

		if (const TSubclassOf<AActor> RawClass = ResolveSoftOutputActorClass(RawOutputActorClass))
		{
			return RawClass;
		}
	}
	else
	{
		if (const TSubclassOf<AActor> RawClass = ResolveSoftOutputActorClass(RawOutputActorClass))
		{
			return RawClass;
		}
	}

	if (OutputForm == EMaterialOutputForm::Raw)
	{
		if (const TSubclassOf<AActor> PureFallbackClass = ResolveSoftOutputActorClass(PureOutputActorClass))
		{
			return PureFallbackClass;
		}
	}

	return nullptr;
}

FName UMaterialDefinitionAsset::GetResolvedColorId() const
{
	if (!ColorId.IsNone())
	{
		return ColorId;
	}

	return GetResolvedMaterialId();
}

FLinearColor UMaterialDefinitionAsset::GetResolvedVisualColor() const
{
	if (!VisualColor.Equals(FLinearColor::White, KINDA_SMALL_NUMBER) || DebugColor.Equals(FLinearColor::White, KINDA_SMALL_NUMBER))
	{
		return VisualColor;
	}

	return DebugColor;
}

UMaterialInterface* UMaterialDefinitionAsset::ResolveVisualMaterial() const
{
	if (VisualMaterial.IsNull())
	{
		return nullptr;
	}

	return VisualMaterial.LoadSynchronous();
}

FPrimaryAssetId UMaterialDefinitionAsset::GetPrimaryAssetId() const
{
	return FPrimaryAssetId(TEXT("Material"), GetResolvedMaterialId());
}

