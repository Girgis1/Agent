// Copyright Epic Games, Inc. All Rights Reserved.

#include "Machine/RecipeAsset.h"
#include "Factory/ResourceDefinitionAsset.h"
#include "Factory/ResourceTypes.h"

bool FRecipeResourceEntry::IsDefined() const
{
	return Resource != nullptr
		&& !GetResolvedResourceId().IsNone()
		&& QuantityUnits > 0;
}

FName FRecipeResourceEntry::GetResolvedResourceId() const
{
	return Resource ? Resource->GetResolvedResourceId() : NAME_None;
}

int32 FRecipeResourceEntry::GetQuantityScaled() const
{
	return AgentResource::WholeUnitsToScaled(QuantityUnits);
}

bool FRecipeGroupInputEntry::IsDefined() const
{
	return !GroupId.IsNone() && RequiredEquivalentUnits > 0;
}

int32 FRecipeGroupInputEntry::GetRequiredEquivalentScaled() const
{
	return AgentResource::WholeUnitsToScaled(RequiredEquivalentUnits);
}

bool URecipeAsset::IsRecipeConfigured() const
{
	if (CraftTimeSeconds <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	bool bHasInput = false;
	for (const FRecipeResourceEntry& Entry : Inputs)
	{
		if (Entry.IsDefined())
		{
			bHasInput = true;
			break;
		}
	}

	if (!bHasInput)
	{
		for (const FRecipeGroupInputEntry& GroupEntry : InputGroups)
		{
			if (GroupEntry.IsDefined())
			{
				bHasInput = true;
				break;
			}
		}
	}

	if (!bHasInput)
	{
		return false;
	}

	for (const FRecipeResourceEntry& Entry : Outputs)
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

	for (const FRecipeResourceEntry& Entry : Inputs)
	{
		if (!Entry.IsDefined())
		{
			continue;
		}

		OutRequirementsScaled.FindOrAdd(Entry.GetResolvedResourceId()) += Entry.GetQuantityScaled();
	}
}

void URecipeAsset::BuildInputGroupRequirementsScaled(TMap<FName, int32>& OutGroupRequirementsScaled) const
{
	OutGroupRequirementsScaled.Reset();

	for (const FRecipeGroupInputEntry& Entry : InputGroups)
	{
		if (!Entry.IsDefined())
		{
			continue;
		}

		OutGroupRequirementsScaled.FindOrAdd(Entry.GroupId) += Entry.GetRequiredEquivalentScaled();
	}
}

void URecipeAsset::BuildOutputAmountsScaled(TMap<FName, int32>& OutOutputScaled) const
{
	OutOutputScaled.Reset();

	for (const FRecipeResourceEntry& Entry : Outputs)
	{
		if (!Entry.IsDefined())
		{
			continue;
		}

		OutOutputScaled.FindOrAdd(Entry.GetResolvedResourceId()) += Entry.GetQuantityScaled();
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
