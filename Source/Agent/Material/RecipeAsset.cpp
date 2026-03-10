// Copyright Epic Games, Inc. All Rights Reserved.

#include "Material/RecipeAsset.h"
#include "GameFramework/Actor.h"
#include "Material/MaterialDefinitionAsset.h"
#include "Material/AgentResourceTypes.h"

bool FRecipeMaterialInputEntry::IsDefined() const
{
	if (QuantityUnits <= 0)
	{
		return false;
	}

	if (IsSpecificMaterialInput())
	{
		return Material != nullptr && !GetResolvedMaterialId().IsNone();
	}

	if (IsAnyMaterialInput())
	{
		return true;
	}

	if (IsItemBlueprintInput())
	{
		return ResolveItemActorClass() != nullptr;
	}

	return false;
}

bool FRecipeMaterialInputEntry::IsSpecificMaterialInput() const
{
	return InputType == ERecipeInputType::SpecificMaterial;
}

bool FRecipeMaterialInputEntry::IsAnyMaterialInput() const
{
	return InputType == ERecipeInputType::AnyMaterial;
}

bool FRecipeMaterialInputEntry::IsItemBlueprintInput() const
{
	return InputType == ERecipeInputType::ItemBlueprint;
}

FName FRecipeMaterialInputEntry::GetResolvedMaterialId() const
{
	return Material ? Material->GetResolvedMaterialId() : NAME_None;
}

TSubclassOf<AActor> FRecipeMaterialInputEntry::ResolveItemActorClass() const
{
	if (ItemActorClass.IsNull())
	{
		return nullptr;
	}

	UClass* LoadedOutputClass = ItemActorClass.LoadSynchronous();
	if (!LoadedOutputClass || !LoadedOutputClass->IsChildOf(AActor::StaticClass()))
	{
		return nullptr;
	}

	return LoadedOutputClass;
}

int32 FRecipeMaterialInputEntry::GetQuantityScaled() const
{
	return AgentResource::WholeUnitsToScaled(QuantityUnits);
}

int32 FRecipeMaterialInputEntry::GetRequiredItemCount() const
{
	return FMath::Max(1, QuantityUnits);
}

void FRecipeMaterialInputEntry::BuildResolvedWhitelist(TSet<FName>& OutWhitelistMaterialIds) const
{
	OutWhitelistMaterialIds.Reset();

	if (!bUseWhitelist || !IsAnyMaterialInput())
	{
		return;
	}

	for (const UMaterialDefinitionAsset* MaterialDefinition : WhitelistMaterials)
	{
		if (!MaterialDefinition)
		{
			continue;
		}

		const FName ResourceId = MaterialDefinition->GetResolvedMaterialId();
		if (!ResourceId.IsNone())
		{
			OutWhitelistMaterialIds.Add(ResourceId);
		}
	}
}

void FRecipeMaterialInputEntry::BuildResolvedBlacklist(TSet<FName>& OutBlacklistMaterialIds) const
{
	OutBlacklistMaterialIds.Reset();

	if (!bUseBlacklist || !IsAnyMaterialInput())
	{
		return;
	}

	for (const UMaterialDefinitionAsset* MaterialDefinition : BlacklistMaterials)
	{
		if (!MaterialDefinition)
		{
			continue;
		}

		const FName ResourceId = MaterialDefinition->GetResolvedMaterialId();
		if (!ResourceId.IsNone())
		{
			OutBlacklistMaterialIds.Add(ResourceId);
		}
	}
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
			if (Entry.IsAnyMaterialInput() && Entry.bUseWhitelist)
			{
				TSet<FName> ResolvedWhitelistIds;
				Entry.BuildResolvedWhitelist(ResolvedWhitelistIds);
				if (ResolvedWhitelistIds.Num() == 0)
				{
					continue;
				}
			}

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
		if (!Entry.IsDefined() || !Entry.IsSpecificMaterialInput())
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
