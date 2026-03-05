// Copyright Epic Games, Inc. All Rights Reserved.

#include "Machine/MachineComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Factory/ResourceDefinitionAsset.h"
#include "Factory/ResourceTypes.h"
#include "Machine/InputVolume.h"
#include "Machine/OutputVolume.h"
#include "Machine/RecipeAsset.h"
#include "UObject/UObjectIterator.h"
#include <limits>

namespace
{
FString FormatUnitsText(int32 QuantityScaled)
{
	const float Units = AgentResource::ScaledToUnits(FMath::Max(0, QuantityScaled));
	if (FMath::IsNearlyEqual(Units, FMath::RoundToFloat(Units), 0.001f))
	{
		return FString::FromInt(FMath::RoundToInt(Units));
	}

	return FString::Printf(TEXT("%.2f"), Units);
}

bool IsGenericMachineTag(const FName& MachineTag)
{
	return MachineTag.IsNone() || MachineTag == TEXT("Machine");
}

bool IsRecipeAllowedForMachineTag(const FName& AllowedMachineTag, const FName& MachineTag)
{
	if (AllowedMachineTag.IsNone())
	{
		return true;
	}

	if (IsGenericMachineTag(MachineTag))
	{
		return true;
	}

	return AllowedMachineTag == MachineTag;
}
}

UMachineComponent::UMachineComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	RuntimeStatus = FText::FromString(TEXT("Idle"));
}

void UMachineComponent::BeginPlay()
{
	Super::BeginPlay();

	ResolveMachineVolumes();
	RebuildResourceMassLookup();

	if (AActor* OwnerActor = GetOwner())
	{
		CachedOwnerPrimitive = Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent());
		if (CachedOwnerPrimitive)
		{
			CachedOwnerBaseMassKg = FMath::Max(0.0f, CachedOwnerPrimitive->GetMass());
		}
	}

	ApplyStoredMassToOwner();
}

void UMachineComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ResolveMachineVolumes();
	FlushPendingOutput();

	if (!bEnabled)
	{
		SetRuntimeState(EMachineRuntimeState::Paused, TEXT("Paused"));
		return;
	}

	if (PendingOutputScaled.Num() > 0)
	{
		SetRuntimeState(EMachineRuntimeState::WaitingForOutput, TEXT("Waiting for output"));
		return;
	}

	if (CurrentCraftRecipe.bIsValid && CurrentCraftDurationSeconds > KINDA_SMALL_NUMBER)
	{
		SetRuntimeState(EMachineRuntimeState::Crafting, TEXT("Crafting"));
		AdvanceCraft(DeltaTime);
		return;
	}

	FRecipeView NextRecipe;
	if (SelectCraftableRecipe(NextRecipe) && TryStartCraft(NextRecipe))
	{
		SetRuntimeState(EMachineRuntimeState::Crafting, TEXT("Crafting"));
		return;
	}

	FString WaitingReason = TEXT("Waiting for input");
	if (Recipes.Num() == 0)
	{
		WaitingReason = TEXT("No recipe assigned");
	}
	else
	{
		FRecipeView PreviewRecipe;
		bool bHasValidRecipe = false;
		if (!bRecipeAny)
		{
			const int32 SafeIndex = FMath::Clamp(ActiveRecipeIndex, 0, Recipes.Num() - 1);
			BuildRecipeView(Recipes.IsValidIndex(SafeIndex) ? Recipes[SafeIndex].Get() : nullptr, PreviewRecipe);
			bHasValidRecipe = PreviewRecipe.bIsValid;
		}
		else
		{
			for (const TObjectPtr<URecipeAsset>& RecipeAssetPtr : Recipes)
			{
				if (!BuildRecipeView(RecipeAssetPtr.Get(), PreviewRecipe))
				{
					continue;
				}

				bHasValidRecipe = true;
				break;
			}
		}

		if (!bHasValidRecipe)
		{
			WaitingReason = TEXT("Recipe is invalid");
		}
		else if (!IsRecipeAllowedForMachineTag(PreviewRecipe.AllowedMachineTag, MachineTag))
		{
			WaitingReason = FString::Printf(TEXT("Tag mismatch (%s != %s)"), *PreviewRecipe.AllowedMachineTag.ToString(), *MachineTag.ToString());
		}
		else
		{
			bool bMissingExplicitInput = false;
			for (const TPair<FName, int32>& RequiredPair : PreviewRecipe.InputScaled)
			{
				const int32 RequiredScaled = FMath::Max(0, RequiredPair.Value);
				const int32 HaveScaled = FMath::Max(0, StoredResourcesScaled.FindRef(RequiredPair.Key));
				if (HaveScaled >= RequiredScaled)
				{
					continue;
				}

				const int32 MissingScaled = RequiredScaled - HaveScaled;
				WaitingReason = FString::Printf(TEXT("Need %s %s"), *FormatUnitsText(MissingScaled), *RequiredPair.Key.ToString());
				bMissingExplicitInput = true;
				break;
			}

			if (!bMissingExplicitInput)
			{
				for (const TPair<FName, int32>& GroupPair : PreviewRecipe.InputGroupsScaled)
				{
					const FName GroupId = GroupPair.Key;
					const int32 RequiredScaled = FMath::Max(0, GroupPair.Value);
					if (GroupId.IsNone() || RequiredScaled <= 0)
					{
						continue;
					}

					const double RequiredEquivalentUnits = static_cast<double>(AgentResource::ScaledToUnits(RequiredScaled));
					const double AvailableEquivalentUnits = GetAvailableEquivalentUnitsForGroup(GroupId);
					if (AvailableEquivalentUnits + KINDA_SMALL_NUMBER >= RequiredEquivalentUnits)
					{
						continue;
					}

					const double MissingEquivalentUnits = FMath::Max(0.0, RequiredEquivalentUnits - AvailableEquivalentUnits);
					WaitingReason = FString::Printf(TEXT("Need %.2f %s equivalent"), MissingEquivalentUnits, *GroupId.ToString());
					break;
				}
			}
		}
	}

	SetRuntimeState(EMachineRuntimeState::WaitingForInput, *WaitingReason);
}

void UMachineComponent::SetInputVolume(UInputVolume* InInputVolume)
{
	InputVolume = InInputVolume;
}

void UMachineComponent::SetOutputVolume(UOutputVolume* InOutputVolume)
{
	OutputVolume = InOutputVolume;
}

int32 UMachineComponent::AddInputResourceScaled(FName ResourceId, int32 QuantityScaled)
{
	if (ResourceId.IsNone())
	{
		return 0;
	}

	const int32 SafeQuantityScaled = FMath::Max(0, QuantityScaled);
	if (SafeQuantityScaled <= 0 || !IsResourceAllowed(ResourceId))
	{
		return 0;
	}

	const int32 RemainingCapacityScaled = GetRemainingCapacityScaled();
	const int32 AcceptedScaled = FMath::Clamp(SafeQuantityScaled, 0, RemainingCapacityScaled);
	if (AcceptedScaled <= 0)
	{
		return 0;
	}

	int32& StoredQuantityScaled = StoredResourcesScaled.FindOrAdd(ResourceId);
	const int64 NewStoredScaled = static_cast<int64>(StoredQuantityScaled) + static_cast<int64>(AcceptedScaled);
	const int64 NewTotalScaled = static_cast<int64>(TotalStoredScaled) + static_cast<int64>(AcceptedScaled);
	if (NewStoredScaled > TNumericLimits<int32>::Max() || NewTotalScaled > TNumericLimits<int32>::Max())
	{
		return 0;
	}

	StoredQuantityScaled = static_cast<int32>(NewStoredScaled);
	TotalStoredScaled = static_cast<int32>(NewTotalScaled);
	ApplyStoredMassToOwner();
	return AcceptedScaled;
}

bool UMachineComponent::AddInputResourcesScaledAtomic(const TMap<FName, int32>& ResourceQuantitiesScaled)
{
	if (ResourceQuantitiesScaled.Num() == 0)
	{
		return false;
	}

	int64 RequiredTotalScaled = 0;
	for (const TPair<FName, int32>& ResourcePair : ResourceQuantitiesScaled)
	{
		const FName ResourceId = ResourcePair.Key;
		const int32 QuantityScaled = FMath::Max(0, ResourcePair.Value);
		if (ResourceId.IsNone() || QuantityScaled <= 0 || !IsResourceAllowed(ResourceId))
		{
			return false;
		}

		const int32* ExistingQuantityScaled = StoredResourcesScaled.Find(ResourceId);
		const int64 ExistingScaled = ExistingQuantityScaled ? static_cast<int64>(*ExistingQuantityScaled) : 0LL;
		if (ExistingScaled + static_cast<int64>(QuantityScaled) > TNumericLimits<int32>::Max())
		{
			return false;
		}

		RequiredTotalScaled += static_cast<int64>(QuantityScaled);
		if (RequiredTotalScaled > TNumericLimits<int32>::Max())
		{
			return false;
		}
	}

	const int64 RemainingCapacityScaled = static_cast<int64>(GetRemainingCapacityScaled());
	if (RemainingCapacityScaled < RequiredTotalScaled)
	{
		return false;
	}

	const int64 NewTotalScaled = static_cast<int64>(TotalStoredScaled) + RequiredTotalScaled;
	if (NewTotalScaled > TNumericLimits<int32>::Max())
	{
		return false;
	}

	for (const TPair<FName, int32>& ResourcePair : ResourceQuantitiesScaled)
	{
		const FName ResourceId = ResourcePair.Key;
		const int32 QuantityScaled = FMath::Max(0, ResourcePair.Value);
		if (QuantityScaled <= 0)
		{
			continue;
		}

		int32& StoredQuantityScaled = StoredResourcesScaled.FindOrAdd(ResourceId);
		StoredQuantityScaled += QuantityScaled;
	}

	TotalStoredScaled = static_cast<int32>(NewTotalScaled);
	ApplyStoredMassToOwner();
	return true;
}

int32 UMachineComponent::AddInputResourceUnits(FName ResourceId, int32 QuantityUnits)
{
	return AddInputResourceScaled(ResourceId, AgentResource::WholeUnitsToScaled(QuantityUnits));
}

float UMachineComponent::GetStoredUnits(FName ResourceId) const
{
	const int32* FoundQuantityScaled = StoredResourcesScaled.Find(ResourceId);
	return FoundQuantityScaled ? AgentResource::ScaledToUnits(*FoundQuantityScaled) : 0.0f;
}

float UMachineComponent::GetTotalStoredUnits() const
{
	return AgentResource::ScaledToUnits(TotalStoredScaled);
}

float UMachineComponent::GetStoredMassKg() const
{
	return ComputeStoredMassKg();
}

float UMachineComponent::GetCraftProgress01() const
{
	if (!CurrentCraftRecipe.bIsValid || CurrentCraftDurationSeconds <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	return FMath::Clamp(CurrentCraftElapsedSeconds / CurrentCraftDurationSeconds, 0.0f, 1.0f);
}

float UMachineComponent::GetCraftRemainingSeconds() const
{
	if (!CurrentCraftRecipe.bIsValid || CurrentCraftDurationSeconds <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	return FMath::Max(0.0f, CurrentCraftDurationSeconds - CurrentCraftElapsedSeconds);
}

void UMachineComponent::ClearStoredResources()
{
	StoredResourcesScaled.Reset();
	TotalStoredScaled = 0;
	CurrentCraftByproductsScaled.Reset();
	ApplyStoredMassToOwner();
}

void UMachineComponent::SetSpeed(float NewSpeed)
{
	Speed = FMath::Max(0.01f, NewSpeed);
}

void UMachineComponent::SetActiveRecipeIndex(int32 NewIndex)
{
	ActiveRecipeIndex = FMath::Max(0, NewIndex);
}

bool UMachineComponent::ResolveMachineVolumes()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return false;
	}

	if (!InputVolume)
	{
		InputVolume = OwnerActor->FindComponentByClass<UInputVolume>();
	}

	if (!OutputVolume)
	{
		OutputVolume = OwnerActor->FindComponentByClass<UOutputVolume>();
	}

	return true;
}

bool UMachineComponent::IsResourceAllowed(FName ResourceId) const
{
	if (ResourceId.IsNone())
	{
		return false;
	}

	if (bUseWhitelist)
	{
		bool bFoundInWhitelist = false;
		for (const UResourceDefinitionAsset* Definition : WhitelistResources)
		{
			if (Definition && Definition->GetResolvedResourceId() == ResourceId)
			{
				bFoundInWhitelist = true;
				break;
			}
		}

		if (!bFoundInWhitelist)
		{
			return false;
		}
	}

	if (bUseBlacklist)
	{
		for (const UResourceDefinitionAsset* Definition : BlacklistResources)
		{
			if (Definition && Definition->GetResolvedResourceId() == ResourceId)
			{
				return false;
			}
		}
	}

	return true;
}

int32 UMachineComponent::GetRemainingCapacityScaled() const
{
	const int32 MaxCapacityScaled = AgentResource::WholeUnitsToScaled(CapacityUnits);
	if (MaxCapacityScaled <= 0)
	{
		return TNumericLimits<int32>::Max();
	}

	return FMath::Max(0, MaxCapacityScaled - TotalStoredScaled);
}

bool UMachineComponent::ConsumeResourceScaled(FName ResourceId, int32 QuantityScaled)
{
	const int32 SafeQuantityScaled = FMath::Max(0, QuantityScaled);
	if (SafeQuantityScaled <= 0)
	{
		return true;
	}

	int32* FoundQuantityScaled = StoredResourcesScaled.Find(ResourceId);
	if (!FoundQuantityScaled || *FoundQuantityScaled < SafeQuantityScaled)
	{
		return false;
	}

	*FoundQuantityScaled -= SafeQuantityScaled;
	TotalStoredScaled = FMath::Max(0, TotalStoredScaled - SafeQuantityScaled);
	if (*FoundQuantityScaled <= 0)
	{
		StoredResourcesScaled.Remove(ResourceId);
	}

	ApplyStoredMassToOwner();
	return true;
}

bool UMachineComponent::BuildRecipeView(const URecipeAsset* RecipeAsset, FRecipeView& OutRecipeView) const
{
	OutRecipeView = {};
	OutRecipeView.Asset = RecipeAsset;

	if (const URecipeAsset* Recipe = RecipeAsset)
	{
		if (!Recipe->IsRecipeConfigured())
		{
			return false;
		}

		OutRecipeView.AllowedMachineTag = Recipe->AllowedMachineTag;
		OutRecipeView.CraftTimeSeconds = Recipe->GetResolvedCraftTimeSeconds();
		Recipe->BuildInputRequirementsScaled(OutRecipeView.InputScaled);
		Recipe->BuildInputGroupRequirementsScaled(OutRecipeView.InputGroupsScaled);
		Recipe->BuildOutputAmountsScaled(OutRecipeView.OutputScaled);
		OutRecipeView.bConvertOtherInputsToScrap = Recipe->bConvertOtherInputsToScrap;
		OutRecipeView.ScrapRecoveryScalar = FMath::Max(0.0f, Recipe->ScrapRecoveryScalar);
		const bool bHasRecipeInput = OutRecipeView.InputScaled.Num() > 0 || OutRecipeView.InputGroupsScaled.Num() > 0;
		OutRecipeView.bIsValid = bHasRecipeInput && OutRecipeView.OutputScaled.Num() > 0;
		return OutRecipeView.bIsValid;
	}

	return false;
}

const UResourceDefinitionAsset* UMachineComponent::ResolveResourceDefinition(FName ResourceId) const
{
	if (ResourceId.IsNone())
	{
		return nullptr;
	}

	if (const TObjectPtr<UResourceDefinitionAsset>* FoundDefinition = ResourceDefinitionById.Find(ResourceId))
	{
		return FoundDefinition->Get();
	}

	for (TObjectIterator<UResourceDefinitionAsset> It; It; ++It)
	{
		UResourceDefinitionAsset* Definition = *It;
		if (!Definition || Definition->GetResolvedResourceId() != ResourceId)
		{
			continue;
		}

		ResourceDefinitionById.Add(ResourceId, Definition);
		ResourceMassPerUnitKgById.FindOrAdd(ResourceId) = Definition->GetMassPerUnitKg();
		return Definition;
	}

	return nullptr;
}

float UMachineComponent::GetResourceEquivalentPerUnit(FName ResourceId) const
{
	if (const UResourceDefinitionAsset* Definition = ResolveResourceDefinition(ResourceId))
	{
		return Definition->GetRefiningEquivalentPerUnit();
	}

	return 0.0f;
}

FName UMachineComponent::GetResourceRefiningGroup(FName ResourceId) const
{
	if (const UResourceDefinitionAsset* Definition = ResolveResourceDefinition(ResourceId))
	{
		return Definition->GetRefiningGroup();
	}

	return NAME_None;
}

double UMachineComponent::GetAvailableEquivalentUnitsForGroup(FName GroupId) const
{
	if (GroupId.IsNone())
	{
		return 0.0;
	}

	double TotalEquivalentUnits = 0.0;
	for (const TPair<FName, int32>& StoredPair : StoredResourcesScaled)
	{
		const FName ResourceId = StoredPair.Key;
		const int32 QuantityScaled = FMath::Max(0, StoredPair.Value);
		if (QuantityScaled <= 0)
		{
			continue;
		}

		if (GetResourceRefiningGroup(ResourceId) != GroupId)
		{
			continue;
		}

		const float EquivalentPerUnit = GetResourceEquivalentPerUnit(ResourceId);
		if (EquivalentPerUnit <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		TotalEquivalentUnits += static_cast<double>(AgentResource::ScaledToUnits(QuantityScaled)) * static_cast<double>(EquivalentPerUnit);
	}

	return TotalEquivalentUnits;
}

bool UMachineComponent::CanSatisfyGroupRequirements(const TMap<FName, int32>& GroupRequirementsScaled) const
{
	for (const TPair<FName, int32>& GroupRequirement : GroupRequirementsScaled)
	{
		const FName GroupId = GroupRequirement.Key;
		const int32 RequiredScaled = FMath::Max(0, GroupRequirement.Value);
		if (GroupId.IsNone() || RequiredScaled <= 0)
		{
			continue;
		}

		const double RequiredEquivalentUnits = static_cast<double>(AgentResource::ScaledToUnits(RequiredScaled));
		const double AvailableEquivalentUnits = GetAvailableEquivalentUnitsForGroup(GroupId);
		if (AvailableEquivalentUnits + KINDA_SMALL_NUMBER < RequiredEquivalentUnits)
		{
			return false;
		}
	}

	return true;
}

bool UMachineComponent::ConsumeGroupRequirements(const TMap<FName, int32>& GroupRequirementsScaled)
{
	TArray<FName> GroupIds;
	GroupRequirementsScaled.GetKeys(GroupIds);
	GroupIds.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});

	for (const FName& GroupId : GroupIds)
	{
		const int32 RequiredScaled = FMath::Max(0, GroupRequirementsScaled.FindRef(GroupId));
		if (GroupId.IsNone() || RequiredScaled <= 0)
		{
			continue;
		}

		double RemainingEquivalentUnits = static_cast<double>(AgentResource::ScaledToUnits(RequiredScaled));

		struct FGroupCandidate
		{
			FName ResourceId = NAME_None;
			float EquivalentPerUnit = 0.0f;
		};

		TArray<FGroupCandidate> Candidates;
		for (const TPair<FName, int32>& StoredPair : StoredResourcesScaled)
		{
			const FName ResourceId = StoredPair.Key;
			const int32 QuantityScaled = FMath::Max(0, StoredPair.Value);
			if (QuantityScaled <= 0 || GetResourceRefiningGroup(ResourceId) != GroupId)
			{
				continue;
			}

			const float EquivalentPerUnit = GetResourceEquivalentPerUnit(ResourceId);
			if (EquivalentPerUnit <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			FGroupCandidate Candidate;
			Candidate.ResourceId = ResourceId;
			Candidate.EquivalentPerUnit = EquivalentPerUnit;
			Candidates.Add(Candidate);
		}

		Candidates.Sort([](const FGroupCandidate& Left, const FGroupCandidate& Right)
		{
			if (!FMath::IsNearlyEqual(Left.EquivalentPerUnit, Right.EquivalentPerUnit))
			{
				return Left.EquivalentPerUnit > Right.EquivalentPerUnit;
			}

			return Left.ResourceId.LexicalLess(Right.ResourceId);
		});

		for (const FGroupCandidate& Candidate : Candidates)
		{
			const int32 AvailableScaled = FMath::Max(0, StoredResourcesScaled.FindRef(Candidate.ResourceId));
			if (AvailableScaled <= 0 || RemainingEquivalentUnits <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			const double AvailableUnits = static_cast<double>(AgentResource::ScaledToUnits(AvailableScaled));
			const double UnitsNeeded = RemainingEquivalentUnits / static_cast<double>(Candidate.EquivalentPerUnit);
			const double UnitsToConsume = FMath::Min(AvailableUnits, UnitsNeeded);

			int32 ConsumeScaled = FMath::Min(AvailableScaled, AgentResource::UnitsToScaled(static_cast<float>(UnitsToConsume)));
			if (ConsumeScaled <= 0 && AvailableScaled > 0)
			{
				ConsumeScaled = 1;
			}

			if (ConsumeScaled <= 0)
			{
				continue;
			}

			if (!ConsumeResourceScaled(Candidate.ResourceId, ConsumeScaled))
			{
				return false;
			}

			const double ConsumedUnits = static_cast<double>(AgentResource::ScaledToUnits(ConsumeScaled));
			RemainingEquivalentUnits -= ConsumedUnits * static_cast<double>(Candidate.EquivalentPerUnit);
		}

		if (RemainingEquivalentUnits > 0.001)
		{
			return false;
		}
	}

	return true;
}

void UMachineComponent::ExtractOtherResourcesAsScrap(const FRecipeView& RecipeView, TMap<FName, int32>& OutScrapByproductScaled)
{
	OutScrapByproductScaled.Reset();

	TSet<FName> RecipeInputResources;
	for (const TPair<FName, int32>& InputPair : RecipeView.InputScaled)
	{
		if (!InputPair.Key.IsNone())
		{
			RecipeInputResources.Add(InputPair.Key);
		}
	}

	TSet<FName> RecipeInputGroups;
	for (const TPair<FName, int32>& GroupPair : RecipeView.InputGroupsScaled)
	{
		if (!GroupPair.Key.IsNone())
		{
			RecipeInputGroups.Add(GroupPair.Key);
		}
	}

	TArray<FName> StoredResourceIds;
	StoredResourcesScaled.GetKeys(StoredResourceIds);
	StoredResourceIds.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});

	const float ScrapScalar = FMath::Max(0.0f, RecipeView.ScrapRecoveryScalar);
	for (const FName& StoredResourceId : StoredResourceIds)
	{
		const int32 QuantityScaled = FMath::Max(0, StoredResourcesScaled.FindRef(StoredResourceId));
		if (StoredResourceId.IsNone() || QuantityScaled <= 0)
		{
			continue;
		}

		if (RecipeInputResources.Contains(StoredResourceId))
		{
			continue;
		}

		const FName ResourceGroup = GetResourceRefiningGroup(StoredResourceId);
		if (!ResourceGroup.IsNone() && RecipeInputGroups.Contains(ResourceGroup))
		{
			continue;
		}

		if (!ConsumeResourceScaled(StoredResourceId, QuantityScaled))
		{
			continue;
		}

		FName ScrapResourceId = StoredResourceId;
		float ScrapUnitsPerUnit = 1.0f;
		if (const UResourceDefinitionAsset* Definition = ResolveResourceDefinition(StoredResourceId))
		{
			ScrapResourceId = Definition->GetResolvedScrapResourceId();
			ScrapUnitsPerUnit = Definition->GetScrapUnitsPerUnit();
		}

		if (ScrapResourceId.IsNone())
		{
			continue;
		}

		const float SourceUnits = AgentResource::ScaledToUnits(QuantityScaled);
		const float ScrapUnits = SourceUnits * FMath::Max(0.0f, ScrapUnitsPerUnit) * ScrapScalar;
		const int32 ScrapScaled = AgentResource::UnitsToScaled(ScrapUnits);
		if (ScrapScaled <= 0)
		{
			continue;
		}

		OutScrapByproductScaled.FindOrAdd(ScrapResourceId) += ScrapScaled;
	}
}

bool UMachineComponent::CanCraftRecipe(const FRecipeView& RecipeView) const
{
	if (!RecipeView.bIsValid)
	{
		return false;
	}

	if (!IsRecipeAllowedForMachineTag(RecipeView.AllowedMachineTag, MachineTag))
	{
		return false;
	}

	for (const TPair<FName, int32>& RequiredPair : RecipeView.InputScaled)
	{
		const int32* FoundQuantityScaled = StoredResourcesScaled.Find(RequiredPair.Key);
		if (!FoundQuantityScaled || *FoundQuantityScaled < FMath::Max(0, RequiredPair.Value))
		{
			return false;
		}
	}

	if (!CanSatisfyGroupRequirements(RecipeView.InputGroupsScaled))
	{
		return false;
	}

	return true;
}

bool UMachineComponent::SelectCraftableRecipe(FRecipeView& OutRecipeView) const
{
	OutRecipeView = {};

	if (Recipes.Num() == 0)
	{
		return false;
	}

	if (!bRecipeAny)
	{
		const int32 SafeIndex = FMath::Clamp(ActiveRecipeIndex, 0, Recipes.Num() - 1);
		const URecipeAsset* ActiveRecipe = Recipes.IsValidIndex(SafeIndex) ? Recipes[SafeIndex].Get() : nullptr;
		FRecipeView CandidateRecipe;
		if (BuildRecipeView(ActiveRecipe, CandidateRecipe) && CanCraftRecipe(CandidateRecipe))
		{
			OutRecipeView = CandidateRecipe;
			return true;
		}

		return false;
	}

	for (const TObjectPtr<URecipeAsset>& RecipeAssetPtr : Recipes)
	{
		const URecipeAsset* RecipeAsset = RecipeAssetPtr.Get();
		FRecipeView CandidateRecipe;
		if (!BuildRecipeView(RecipeAsset, CandidateRecipe))
		{
			continue;
		}

		if (!CanCraftRecipe(CandidateRecipe))
		{
			continue;
		}

		OutRecipeView = CandidateRecipe;
		return true;
	}

	return false;
}

bool UMachineComponent::TryStartCraft(const FRecipeView& RecipeView)
{
	if (!CanCraftRecipe(RecipeView))
	{
		return false;
	}

	for (const TPair<FName, int32>& RequiredPair : RecipeView.InputScaled)
	{
		if (!ConsumeResourceScaled(RequiredPair.Key, RequiredPair.Value))
		{
			SetRuntimeState(EMachineRuntimeState::Error, TEXT("Failed consuming recipe input"));
			return false;
		}
	}

	if (!ConsumeGroupRequirements(RecipeView.InputGroupsScaled))
	{
		SetRuntimeState(EMachineRuntimeState::Error, TEXT("Failed consuming grouped recipe input"));
		return false;
	}

	CurrentCraftByproductsScaled.Reset();
	if (RecipeView.bConvertOtherInputsToScrap)
	{
		ExtractOtherResourcesAsScrap(RecipeView, CurrentCraftByproductsScaled);
	}

	CurrentCraftRecipe = RecipeView;
	CurrentCraftElapsedSeconds = 0.0f;
	CurrentCraftDurationSeconds = FMath::Max(KINDA_SMALL_NUMBER, RecipeView.CraftTimeSeconds) / FMath::Max(0.01f, Speed);
	return true;
}

void UMachineComponent::AdvanceCraft(float DeltaTime)
{
	if (!CurrentCraftRecipe.bIsValid || CurrentCraftDurationSeconds <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	CurrentCraftElapsedSeconds += FMath::Max(0.0f, DeltaTime);
	if (CurrentCraftElapsedSeconds < CurrentCraftDurationSeconds)
	{
		return;
	}

	CompleteCraft();
}

void UMachineComponent::CompleteCraft()
{
	for (const TPair<FName, int32>& OutputPair : CurrentCraftRecipe.OutputScaled)
	{
		const FName ResourceId = OutputPair.Key;
		const int32 QuantityScaled = FMath::Max(0, OutputPair.Value);
		if (ResourceId.IsNone() || QuantityScaled <= 0)
		{
			continue;
		}

		PendingOutputScaled.FindOrAdd(ResourceId) += QuantityScaled;
	}

	for (const TPair<FName, int32>& ByproductPair : CurrentCraftByproductsScaled)
	{
		const FName ResourceId = ByproductPair.Key;
		const int32 QuantityScaled = FMath::Max(0, ByproductPair.Value);
		if (ResourceId.IsNone() || QuantityScaled <= 0)
		{
			continue;
		}

		PendingOutputScaled.FindOrAdd(ResourceId) += QuantityScaled;
	}

	CurrentCraftRecipe = {};
	CurrentCraftByproductsScaled.Reset();
	CurrentCraftElapsedSeconds = 0.0f;
	CurrentCraftDurationSeconds = 0.0f;

	FlushPendingOutput();
	if (PendingOutputScaled.Num() > 0)
	{
		SetRuntimeState(EMachineRuntimeState::WaitingForOutput, TEXT("Waiting for output"));
	}
	else
	{
		SetRuntimeState(EMachineRuntimeState::Idle, TEXT("Craft complete"));
	}
}

void UMachineComponent::FlushPendingOutput()
{
	if (!OutputVolume || PendingOutputScaled.Num() == 0)
	{
		return;
	}

	TArray<FName> ResourceIds;
	PendingOutputScaled.GetKeys(ResourceIds);
	ResourceIds.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});

	for (const FName& ResourceId : ResourceIds)
	{
		int32* FoundQuantityScaled = PendingOutputScaled.Find(ResourceId);
		if (!FoundQuantityScaled)
		{
			continue;
		}

		const int32 QuantityScaled = FMath::Max(0, *FoundQuantityScaled);
		if (QuantityScaled <= 0)
		{
			PendingOutputScaled.Remove(ResourceId);
			continue;
		}

		const int32 AcceptedScaled = OutputVolume->QueueResourceScaled(ResourceId, QuantityScaled);
		*FoundQuantityScaled = FMath::Max(0, QuantityScaled - AcceptedScaled);
		if (*FoundQuantityScaled == 0)
		{
			PendingOutputScaled.Remove(ResourceId);
		}
	}
}

void UMachineComponent::SetRuntimeState(EMachineRuntimeState NewState, const TCHAR* Message)
{
	RuntimeState = NewState;
	RuntimeStatus = FText::FromString(Message ? Message : TEXT(""));
}

void UMachineComponent::RebuildResourceMassLookup()
{
	ResourceMassPerUnitKgById.Reset();
	ResourceDefinitionById.Reset();

	for (const UResourceDefinitionAsset* Definition : WhitelistResources)
	{
		RegisterResourceMass(Definition);
	}

	for (const UResourceDefinitionAsset* Definition : BlacklistResources)
	{
		RegisterResourceMass(Definition);
	}

	for (const TObjectPtr<URecipeAsset>& RecipeAssetPtr : Recipes)
	{
		const URecipeAsset* Recipe = RecipeAssetPtr.Get();
		if (Recipe)
		{
			for (const FRecipeResourceEntry& Entry : Recipe->Inputs)
			{
				RegisterResourceMass(Entry.Resource);
			}

			for (const FRecipeResourceEntry& Entry : Recipe->Outputs)
			{
				RegisterResourceMass(Entry.Resource);
			}

			continue;
		}
	}

	for (TObjectIterator<UResourceDefinitionAsset> It; It; ++It)
	{
		RegisterResourceMass(*It);
	}
}

void UMachineComponent::RegisterResourceMass(const UResourceDefinitionAsset* ResourceDefinition)
{
	if (!ResourceDefinition)
	{
		return;
	}

	const FName ResourceId = ResourceDefinition->GetResolvedResourceId();
	if (ResourceId.IsNone())
	{
		return;
	}

	ResourceDefinitionById.Add(ResourceId, const_cast<UResourceDefinitionAsset*>(ResourceDefinition));
	ResourceMassPerUnitKgById.Add(ResourceId, ResourceDefinition->GetMassPerUnitKg());
}

float UMachineComponent::ResolveResourceMassPerUnitKg(FName ResourceId) const
{
	if (ResourceId.IsNone())
	{
		return 0.0f;
	}

	if (const float* FoundMass = ResourceMassPerUnitKgById.Find(ResourceId))
	{
		return FMath::Max(0.0f, *FoundMass);
	}

	if (const UResourceDefinitionAsset* Definition = ResolveResourceDefinition(ResourceId))
	{
		const float MassPerUnitKg = Definition->GetMassPerUnitKg();
		ResourceMassPerUnitKgById.Add(ResourceId, MassPerUnitKg);
		return FMath::Max(0.0f, MassPerUnitKg);
	}

	return 0.0f;
}

void UMachineComponent::ApplyStoredMassToOwner()
{
	if (!bApplyStoredMassToOwner)
	{
		return;
	}

	if (!CachedOwnerPrimitive)
	{
		if (const AActor* OwnerActor = GetOwner())
		{
			CachedOwnerPrimitive = Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent());
		}
	}

	if (!CachedOwnerPrimitive)
	{
		return;
	}

	if (CachedOwnerBaseMassKg <= KINDA_SMALL_NUMBER)
	{
		CachedOwnerBaseMassKg = FMath::Max(0.0f, CachedOwnerPrimitive->GetMass());
	}

	const float FinalMassKg = FMath::Max(0.01f, CachedOwnerBaseMassKg + ComputeStoredMassKg() * FMath::Max(0.0f, StoredMassMultiplier));
	CachedOwnerPrimitive->SetMassOverrideInKg(NAME_None, FinalMassKg, true);
}

float UMachineComponent::ComputeStoredMassKg() const
{
	float TotalMassKg = 0.0f;

	for (const TPair<FName, int32>& StoredPair : StoredResourcesScaled)
	{
		const FName ResourceId = StoredPair.Key;
		const int32 QuantityScaled = FMath::Max(0, StoredPair.Value);
		if (ResourceId.IsNone() || QuantityScaled <= 0)
		{
			continue;
		}

		const float Units = AgentResource::ScaledToUnits(QuantityScaled);
		const float MassPerUnitKg = ResolveResourceMassPerUnitKg(ResourceId);
		TotalMassKg += Units * MassPerUnitKg;
	}

	return TotalMassKg;
}
