// Copyright Epic Games, Inc. All Rights Reserved.

#include "Material/RecipeAsset.h"
#include "GameFramework/Actor.h"
#include "Material/MaterialDefinitionAsset.h"
#include "Material/AgentResourceTypes.h"

bool FRecipeMaterialInputEntry::IsDefined() const
{
	return Material != nullptr
		&& !GetResolvedMaterialId().IsNone()
		&& QuantityUnits > 0;
}

FName FRecipeMaterialInputEntry::GetResolvedMaterialId() const
{
	return Material ? Material->GetResolvedResourceId() : NAME_None;
}

int32 FRecipeMaterialInputEntry::GetQuantityScaled() const
{
	return AgentResource::WholeUnitsToScaled(QuantityUnits);
}

bool FRecipeBlueprintOutputEntry::IsDefined() const
{
	return ResolveOutputActorClass() != nullptr;
}

TSubclassOf<AActor> FRecipeBlueprintOutputEntry::ResolveOutputActorClass() const
{
	if (OutputActorClass.IsNull())
	{
		return nullptr;
	}

	UClass* LoadedOutputClass = OutputActorClass.LoadSynchronous();
	if (!LoadedOutputClass || !LoadedOutputClass->IsChildOf(AActor::StaticClass()))
	{
		return nullptr;
	}

	return LoadedOutputClass;
}

bool URecipeAsset::IsRecipeConfigured() const
{
	if (CraftTimeSeconds <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	bool bHasInput = false;
	for (const FRecipeMaterialInputEntry& Entry : Inputs)
	{
		if (Entry.IsDefined())
		{
			bHasInput = true;
			break;
		}
	}

	if (!bHasInput)
	{
		return false;
	}

	for (const FRecipeBlueprintOutputEntry& Entry : Outputs)
	{
		if (Entry.IsDefined())
		{
			return true;
		}
	}

	return false;
}

void URecipeAsset::BuildInputRequirementsScaled(TMap<FName, int32>& OutRequirementsScaled) const
{
	OutRequirementsScaled.Reset();

	for (const FRecipeMaterialInputEntry& Entry : Inputs)
	{
		if (!Entry.IsDefined())
		{
			continue;
		}

		OutRequirementsScaled.FindOrAdd(Entry.GetResolvedMaterialId()) += Entry.GetQuantityScaled();
	}
}

void URecipeAsset::BuildResolvedOutputs(TArray<TSubclassOf<AActor>>& OutOutputActorClasses) const
{
	OutOutputActorClasses.Reset();

	for (const FRecipeBlueprintOutputEntry& Entry : Outputs)
	{
		if (TSubclassOf<AActor> OutputClass = Entry.ResolveOutputActorClass())
		{
			OutOutputActorClasses.Add(OutputClass);
		}
	}
}

float URecipeAsset::GetResolvedCraftTimeSeconds() const
{
	return FMath::Max(KINDA_SMALL_NUMBER, CraftTimeSeconds);
}

FPrimaryAssetId URecipeAsset::GetPrimaryAssetId() const
{
	const FName ResolvedRecipeId = RecipeId.IsNone() ? GetFName() : RecipeId;
	return FPrimaryAssetId(TEXT("Recipe"), ResolvedRecipeId);
}
