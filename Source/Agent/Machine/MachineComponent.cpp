// Copyright Epic Games, Inc. All Rights Reserved.

#include "Machine/MachineComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Factory/FactoryWorldConfig.h"
#include "Material/MaterialDefinitionAsset.h"
#include "Material/AgentResourceTypes.h"
#include "Machine/InputVolume.h"
#include "Machine/OutputVolume.h"
#include "Material/RecipeAsset.h"
#include "UObject/UObjectIterator.h"
#include "EngineUtils.h"
#include "PhysicsEngine/BodyInstance.h"
#include <limits>

namespace
{
const TCHAR* MachineRecipeClassOutputPrefix = TEXT("__RecipeClassOutput__:");
const TCHAR* MachineItemClassInputPrefix = TEXT("__ItemClassInput__:");

FString FormatUnitsText(int32 QuantityScaled)
{
	const float Units = AgentResource::ScaledToUnits(FMath::Max(0, QuantityScaled));
	if (FMath::IsNearlyEqual(Units, FMath::RoundToFloat(Units), 0.001f))
	{
		return FString::FromInt(FMath::RoundToInt(Units));
	}

	return FString::Printf(TEXT("%.2f"), Units);
}

FName BuildRecipeClassOutputKey(const UClass* OutputClass)
{
	if (!OutputClass)
	{
		return NAME_None;
	}

	return FName(*FString::Printf(TEXT("%s%s"), MachineRecipeClassOutputPrefix, *OutputClass->GetPathName()));
}

FName BuildItemClassInputKey(const UClass* ItemClass)
{
	if (!ItemClass)
	{
		return NAME_None;
	}

	return FName(*FString::Printf(TEXT("%s%s"), MachineItemClassInputPrefix, *ItemClass->GetPathName()));
}

FString ResolveItemClassDisplayName(const TSubclassOf<AActor>& ItemClass)
{
	if (ItemClass.Get())
	{
		return ItemClass->GetName();
	}

	return TEXT("Item");
}

float ResolvePrimitiveMassKgWithoutPhysicsWarningForMachine(const UPrimitiveComponent* PrimitiveComponent)
{
	if (!PrimitiveComponent)
	{
		return 0.0f;
	}

	if (const FBodyInstance* BodyInstance = PrimitiveComponent->GetBodyInstance())
	{
		const float BodyMassKg = BodyInstance->GetBodyMass();
		if (BodyMassKg > KINDA_SMALL_NUMBER)
		{
			return FMath::Max(0.0f, BodyMassKg);
		}
	}

	// GetMass() logs warnings when simulation is disabled.
	if (PrimitiveComponent->IsSimulatingPhysics())
	{
		return FMath::Max(0.0f, PrimitiveComponent->GetMass());
	}

	return 0.0f;
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
			CachedOwnerBaseMassKg = ResolvePrimitiveMassKgWithoutPhysicsWarningForMachine(CachedOwnerPrimitive);
		}
	}

	ApplyStoredMassToOwner();

	if (OutputVolume)
	{
		OutputVolume->SetOutputPureMaterials(bOutputPureMaterials);
	}
}

void UMachineComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	ResolveMachineVolumes();
	if (OutputVolume)
	{
		OutputVolume->SetOutputPureMaterials(bOutputPureMaterials);
	}
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

	if (ResolveEffectiveCraftSpeed() <= KINDA_SMALL_NUMBER)
	{
		SetRuntimeState(EMachineRuntimeState::Paused, TEXT("Paused (global factory speed)"));
		return;
	}

	if (CurrentCraftRecipe.bIsValid && CurrentCraftDurationSeconds > KINDA_SMALL_NUMBER)
	{
		const float EffectiveCraftSpeed = ResolveEffectiveCraftSpeed();
		if (EffectiveCraftSpeed <= KINDA_SMALL_NUMBER)
		{
			SetRuntimeState(EMachineRuntimeState::Paused, TEXT("Paused (global factory speed)"));
			return;
		}

		SetRuntimeState(EMachineRuntimeState::Crafting, TEXT("Crafting"));
		AdvanceCraft(DeltaTime, EffectiveCraftSpeed);
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
		else
		{
			bool bHasMissingRequirement = false;

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
				bHasMissingRequirement = true;
				break;
			}

			if (!bHasMissingRequirement)
			{
				for (const FAnyMaterialRequirement& AnyRequirement : PreviewRecipe.AnyMaterialInputs)
				{
					const int32 RequiredScaled = FMath::Max(0, AnyRequirement.QuantityScaled);
					const int32 HaveScaled = ResolveAnyMaterialAvailableScaled(AnyRequirement);
					if (HaveScaled >= RequiredScaled)
					{
						continue;
					}

					const int32 MissingScaled = RequiredScaled - HaveScaled;
					WaitingReason = FString::Printf(TEXT("Need %s any material"), *FormatUnitsText(MissingScaled));
					bHasMissingRequirement = true;
					break;
				}
			}

			if (!bHasMissingRequirement)
			{
				TArray<FName> ItemRequirementKeys;
				PreviewRecipe.InputItemCountsByClassKey.GetKeys(ItemRequirementKeys);
				ItemRequirementKeys.Sort([](const FName& Left, const FName& Right)
				{
					return Left.LexicalLess(Right);
				});

				for (const FName& ItemClassKey : ItemRequirementKeys)
				{
					const int32 RequiredCount = FMath::Max(0, PreviewRecipe.InputItemCountsByClassKey.FindRef(ItemClassKey));
					const int32 HaveCount = FMath::Max(0, StoredItemCountsByClassKey.FindRef(ItemClassKey));
					if (HaveCount >= RequiredCount)
					{
						continue;
					}

					const int32 MissingCount = RequiredCount - HaveCount;
					const FString ItemLabel = ResolveItemClassDisplayName(PreviewRecipe.InputItemClassByKey.FindRef(ItemClassKey));
					WaitingReason = FString::Printf(TEXT("Need %d x %s"), MissingCount, *ItemLabel);
					bHasMissingRequirement = true;
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
	return AddInputContentsScaledAtomic(ResourceQuantitiesScaled, {}, {});
}

bool UMachineComponent::AddInputActorContentsScaledAtomic(AActor* SourceActor, const TMap<FName, int32>& ResourceQuantitiesScaled)
{
	if (!SourceActor)
	{
		return AddInputContentsScaledAtomic(ResourceQuantitiesScaled, {}, {});
	}

	TMap<FName, int32> ItemCountsByClassKey;
	TMap<FName, TSubclassOf<AActor>> ItemClassByKey;

	if (UClass* SourceClass = SourceActor->GetClass())
	{
		const FName ItemClassKey = BuildItemClassInputKey(SourceClass);
		if (!ItemClassKey.IsNone())
		{
			ItemCountsByClassKey.Add(ItemClassKey, 1);
			ItemClassByKey.Add(ItemClassKey, SourceClass);
		}
	}

	return AddInputContentsScaledAtomic(ResourceQuantitiesScaled, ItemCountsByClassKey, ItemClassByKey);
}

bool UMachineComponent::IsItemClassReferencedByAnyRecipe(const UClass* ItemActorClass) const
{
	const FName ItemClassKey = BuildItemClassInputKey(ItemActorClass);
	if (ItemClassKey.IsNone())
	{
		return false;
	}

	for (const TObjectPtr<URecipeAsset>& RecipeAssetPtr : Recipes)
	{
		FRecipeView RecipeView;
		if (!BuildRecipeView(RecipeAssetPtr.Get(), RecipeView))
		{
			continue;
		}

		if (RecipeView.InputItemCountsByClassKey.Contains(ItemClassKey))
		{
			return true;
		}
	}

	return false;
}

bool UMachineComponent::ConsumeStoredResourcesScaledAtomic(const TMap<FName, int32>& ResourceQuantitiesScaled)
{
	if (ResourceQuantitiesScaled.Num() == 0)
	{
		return false;
	}

	TMap<FName, int32> WorkingResources = StoredResourcesScaled;
	int32 WorkingTotalScaled = TotalStoredScaled;

	for (const TPair<FName, int32>& Pair : ResourceQuantitiesScaled)
	{
		const FName ResourceId = Pair.Key;
		const int32 QuantityScaled = FMath::Max(0, Pair.Value);
		if (ResourceId.IsNone() || QuantityScaled <= 0)
		{
			return false;
		}

		int32* FoundQuantityScaled = WorkingResources.Find(ResourceId);
		if (!FoundQuantityScaled || *FoundQuantityScaled < QuantityScaled)
		{
			return false;
		}

		*FoundQuantityScaled = FMath::Max(0, *FoundQuantityScaled - QuantityScaled);
		WorkingTotalScaled = FMath::Max(0, WorkingTotalScaled - QuantityScaled);
		if (*FoundQuantityScaled == 0)
		{
			WorkingResources.Remove(ResourceId);
		}
	}

	StoredResourcesScaled = MoveTemp(WorkingResources);
	TotalStoredScaled = WorkingTotalScaled;
	ApplyStoredMassToOwner();
	return true;
}

bool UMachineComponent::AddInputContentsScaledAtomic(
	const TMap<FName, int32>& ResourceQuantitiesScaled,
	const TMap<FName, int32>& ItemCountsByClassKey,
	const TMap<FName, TSubclassOf<AActor>>& ItemClassByKey)
{
	if (ResourceQuantitiesScaled.Num() == 0 && ItemCountsByClassKey.Num() == 0)
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

	int64 IncomingItemCount = 0;
	for (const TPair<FName, int32>& ItemPair : ItemCountsByClassKey)
	{
		const FName ItemClassKey = ItemPair.Key;
		const int32 ItemCount = FMath::Max(0, ItemPair.Value);
		if (ItemClassKey.IsNone() || ItemCount <= 0)
		{
			return false;
		}

		const int64 ExistingCount = static_cast<int64>(StoredItemCountsByClassKey.FindRef(ItemClassKey));
		if (ExistingCount + static_cast<int64>(ItemCount) > TNumericLimits<int32>::Max())
		{
			return false;
		}

		IncomingItemCount += static_cast<int64>(ItemCount);
		if (IncomingItemCount > TNumericLimits<int32>::Max())
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

	const int64 NewTotalItemCount = static_cast<int64>(TotalStoredItemCount) + IncomingItemCount;
	if (NewTotalItemCount > TNumericLimits<int32>::Max())
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

	for (const TPair<FName, int32>& ItemPair : ItemCountsByClassKey)
	{
		const FName ItemClassKey = ItemPair.Key;
		const int32 ItemCount = FMath::Max(0, ItemPair.Value);
		if (ItemCount <= 0)
		{
			continue;
		}

		int32& StoredCount = StoredItemCountsByClassKey.FindOrAdd(ItemClassKey);
		StoredCount += ItemCount;
	}

	for (const TPair<FName, TSubclassOf<AActor>>& ItemClassPair : ItemClassByKey)
	{
		if (ItemClassPair.Key.IsNone() || !ItemClassPair.Value.Get())
		{
			continue;
		}

		StoredItemClassByKey.FindOrAdd(ItemClassPair.Key) = ItemClassPair.Value;
	}

	TotalStoredScaled = static_cast<int32>(NewTotalScaled);
	TotalStoredItemCount = static_cast<int32>(NewTotalItemCount);
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

void UMachineComponent::GetStoredResourceSnapshot(TArray<FResourceStorageEntry>& OutEntries) const
{
	OutEntries.Reset();

	TArray<FName> ResourceIds;
	StoredResourcesScaled.GetKeys(ResourceIds);
	ResourceIds.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});

	for (const FName& ResourceId : ResourceIds)
	{
		const int32 QuantityScaled = FMath::Max(0, StoredResourcesScaled.FindRef(ResourceId));
		if (ResourceId.IsNone() || QuantityScaled <= 0)
		{
			continue;
		}

		FResourceStorageEntry Entry;
		Entry.ResourceId = ResourceId;
		Entry.QuantityScaled = QuantityScaled;
		Entry.Units = AgentResource::ScaledToUnits(QuantityScaled);
		OutEntries.Add(Entry);
	}
}

int32 UMachineComponent::GetStoredItemCount(TSubclassOf<AActor> ItemActorClass) const
{
	const FName ItemClassKey = BuildItemClassInputKey(ItemActorClass.Get());
	if (ItemClassKey.IsNone())
	{
		return 0;
	}

	return FMath::Max(0, StoredItemCountsByClassKey.FindRef(ItemClassKey));
}

int32 UMachineComponent::GetTotalStoredItemCount() const
{
	return FMath::Max(0, TotalStoredItemCount);
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

	const float RemainingWorkSeconds = FMath::Max(0.0f, CurrentCraftDurationSeconds - CurrentCraftElapsedSeconds);
	const float EffectiveCraftSpeed = ResolveEffectiveCraftSpeed();
	if (EffectiveCraftSpeed <= KINDA_SMALL_NUMBER)
	{
		return TNumericLimits<float>::Max();
	}

	return RemainingWorkSeconds / EffectiveCraftSpeed;
}

void UMachineComponent::ClearStoredResources()
{
	StoredResourcesScaled.Reset();
	TotalStoredScaled = 0;
	StoredItemCountsByClassKey.Reset();
	StoredItemClassByKey.Reset();
	TotalStoredItemCount = 0;
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
		for (const UMaterialDefinitionAsset* Definition : WhitelistResources)
		{
			if (Definition && Definition->GetResolvedMaterialId() == ResourceId)
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
		for (const UMaterialDefinitionAsset* Definition : BlacklistResources)
		{
			if (Definition && Definition->GetResolvedMaterialId() == ResourceId)
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

		OutRecipeView.CraftTimeSeconds = Recipe->GetResolvedCraftTimeSeconds();
		OutRecipeView.InputScaled.Reset();
		OutRecipeView.InputItemCountsByClassKey.Reset();
		OutRecipeView.InputItemClassByKey.Reset();
		OutRecipeView.AnyMaterialInputs.Reset();

		for (const FRecipeMaterialInputEntry& InputEntry : Recipe->Inputs)
		{
			if (!InputEntry.IsDefined())
			{
				continue;
			}

			if (InputEntry.IsSpecificMaterialInput())
			{
				const FName ResourceId = InputEntry.GetResolvedMaterialId();
				if (ResourceId.IsNone())
				{
					continue;
				}

				OutRecipeView.InputScaled.FindOrAdd(ResourceId) += InputEntry.GetQuantityScaled();
				continue;
			}

			if (InputEntry.IsAnyMaterialInput())
			{
				FAnyMaterialRequirement AnyRequirement;
				AnyRequirement.QuantityScaled = InputEntry.GetQuantityScaled();
				AnyRequirement.bUseWhitelist = InputEntry.bUseWhitelist;
				AnyRequirement.bUseBlacklist = InputEntry.bUseBlacklist;
				InputEntry.BuildResolvedWhitelist(AnyRequirement.WhitelistMaterialIds);
				InputEntry.BuildResolvedBlacklist(AnyRequirement.BlacklistMaterialIds);

				if (AnyRequirement.QuantityScaled <= 0)
				{
					continue;
				}

				if (AnyRequirement.bUseWhitelist && AnyRequirement.WhitelistMaterialIds.Num() == 0)
				{
					continue;
				}

				OutRecipeView.AnyMaterialInputs.Add(AnyRequirement);
				continue;
			}

			if (InputEntry.IsItemBlueprintInput())
			{
				const TSubclassOf<AActor> ItemClass = InputEntry.ResolveItemActorClass();
				const FName ItemClassKey = BuildItemClassInputKey(ItemClass.Get());
				if (ItemClassKey.IsNone())
				{
					continue;
				}

				OutRecipeView.InputItemCountsByClassKey.FindOrAdd(ItemClassKey) += InputEntry.GetRequiredItemCount();
				OutRecipeView.InputItemClassByKey.FindOrAdd(ItemClassKey) = ItemClass;
			}
		}

		OutRecipeView.OutputScaled.Reset();
		OutRecipeView.OutputActorClassByResourceId.Reset();
		TArray<TSubclassOf<AActor>> ResolvedOutputActorClasses;
		Recipe->BuildResolvedOutputs(ResolvedOutputActorClasses);

		for (const TSubclassOf<AActor>& OutputActorClass : ResolvedOutputActorClasses)
		{
			if (!OutputActorClass.Get())
			{
				continue;
			}

			const FName OutputResourceId = BuildRecipeClassOutputKey(OutputActorClass.Get());
			if (OutputResourceId.IsNone())
			{
				continue;
			}

			OutRecipeView.OutputScaled.FindOrAdd(OutputResourceId) += AgentResource::WholeUnitsToScaled(1);
			OutRecipeView.OutputActorClassByResourceId.FindOrAdd(OutputResourceId) = OutputActorClass;
		}

		const bool bHasRecipeInput = OutRecipeView.InputScaled.Num() > 0
			|| OutRecipeView.AnyMaterialInputs.Num() > 0
			|| OutRecipeView.InputItemCountsByClassKey.Num() > 0;
		OutRecipeView.bIsValid = bHasRecipeInput && OutRecipeView.OutputScaled.Num() > 0;
		return OutRecipeView.bIsValid;
	}

	return false;
}

const UMaterialDefinitionAsset* UMachineComponent::ResolveResourceDefinition(FName ResourceId) const
{
	if (ResourceId.IsNone())
	{
		return nullptr;
	}

	if (const TObjectPtr<UMaterialDefinitionAsset>* FoundDefinition = ResourceDefinitionById.Find(ResourceId))
	{
		return FoundDefinition->Get();
	}

	for (TObjectIterator<UMaterialDefinitionAsset> It; It; ++It)
	{
		UMaterialDefinitionAsset* Definition = *It;
		if (!Definition || Definition->GetResolvedMaterialId() != ResourceId)
		{
			continue;
		}

		ResourceDefinitionById.Add(ResourceId, Definition);
		ResourceMassPerUnitKgById.FindOrAdd(ResourceId) = Definition->GetMassPerUnitKg();
		return Definition;
	}

	return nullptr;
}

bool UMachineComponent::CanCraftRecipe(const FRecipeView& RecipeView) const
{
	if (!RecipeView.bIsValid)
	{
		return false;
	}

	TMap<FName, int32> WorkingResources = StoredResourcesScaled;
	TMap<FName, int32> WorkingItemCounts = StoredItemCountsByClassKey;
	int32 WorkingTotalScaled = TotalStoredScaled;

	for (const TPair<FName, int32>& RequiredPair : RecipeView.InputScaled)
	{
		const FName ResourceId = RequiredPair.Key;
		const int32 RequiredScaled = FMath::Max(0, RequiredPair.Value);
		if (ResourceId.IsNone() || RequiredScaled <= 0)
		{
			continue;
		}

		int32* FoundQuantityScaled = WorkingResources.Find(ResourceId);
		if (!FoundQuantityScaled || *FoundQuantityScaled < RequiredScaled)
		{
			return false;
		}

		*FoundQuantityScaled = FMath::Max(0, *FoundQuantityScaled - RequiredScaled);
		WorkingTotalScaled = FMath::Max(0, WorkingTotalScaled - RequiredScaled);
		if (*FoundQuantityScaled == 0)
		{
			WorkingResources.Remove(ResourceId);
		}
	}

	for (const TPair<FName, int32>& ItemRequirement : RecipeView.InputItemCountsByClassKey)
	{
		const FName ItemClassKey = ItemRequirement.Key;
		const int32 RequiredCount = FMath::Max(0, ItemRequirement.Value);
		if (ItemClassKey.IsNone() || RequiredCount <= 0)
		{
			continue;
		}

		int32* FoundStoredCount = WorkingItemCounts.Find(ItemClassKey);
		if (!FoundStoredCount || *FoundStoredCount < RequiredCount)
		{
			return false;
		}

		*FoundStoredCount = FMath::Max(0, *FoundStoredCount - RequiredCount);
		if (*FoundStoredCount == 0)
		{
			WorkingItemCounts.Remove(ItemClassKey);
		}
	}

	for (const FAnyMaterialRequirement& AnyRequirement : RecipeView.AnyMaterialInputs)
	{
		if (!TryConsumeAnyMaterialRequirementFromMap(AnyRequirement, WorkingResources, WorkingTotalScaled))
		{
			return false;
		}
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

	TMap<FName, int32> WorkingResources = StoredResourcesScaled;
	TMap<FName, int32> WorkingItemCounts = StoredItemCountsByClassKey;
	int32 WorkingTotalScaled = TotalStoredScaled;
	int32 WorkingTotalItemCount = TotalStoredItemCount;

	for (const TPair<FName, int32>& RequiredPair : RecipeView.InputScaled)
	{
		const FName ResourceId = RequiredPair.Key;
		const int32 SafeRequiredScaled = FMath::Max(0, RequiredPair.Value);
		if (ResourceId.IsNone() || SafeRequiredScaled <= 0)
		{
			continue;
		}

		int32* FoundQuantityScaled = WorkingResources.Find(ResourceId);
		if (!FoundQuantityScaled || *FoundQuantityScaled < SafeRequiredScaled)
		{
			SetRuntimeState(EMachineRuntimeState::Error, TEXT("Failed consuming recipe input"));
			return false;
		}

		*FoundQuantityScaled = FMath::Max(0, *FoundQuantityScaled - SafeRequiredScaled);
		WorkingTotalScaled = FMath::Max(0, WorkingTotalScaled - SafeRequiredScaled);
		if (*FoundQuantityScaled == 0)
		{
			WorkingResources.Remove(ResourceId);
		}
	}

	for (const TPair<FName, int32>& ItemRequirement : RecipeView.InputItemCountsByClassKey)
	{
		const FName ItemClassKey = ItemRequirement.Key;
		const int32 RequiredCount = FMath::Max(0, ItemRequirement.Value);
		if (ItemClassKey.IsNone() || RequiredCount <= 0)
		{
			continue;
		}

		int32* FoundStoredCount = WorkingItemCounts.Find(ItemClassKey);
		if (!FoundStoredCount || *FoundStoredCount < RequiredCount)
		{
			SetRuntimeState(EMachineRuntimeState::Error, TEXT("Failed consuming item input"));
			return false;
		}

		*FoundStoredCount = FMath::Max(0, *FoundStoredCount - RequiredCount);
		WorkingTotalItemCount = FMath::Max(0, WorkingTotalItemCount - RequiredCount);
		if (*FoundStoredCount == 0)
		{
			WorkingItemCounts.Remove(ItemClassKey);
		}
	}

	for (const FAnyMaterialRequirement& AnyRequirement : RecipeView.AnyMaterialInputs)
	{
		if (!TryConsumeAnyMaterialRequirementFromMap(AnyRequirement, WorkingResources, WorkingTotalScaled))
		{
			SetRuntimeState(EMachineRuntimeState::Error, TEXT("Failed consuming any-material input"));
			return false;
		}
	}

	StoredResourcesScaled = MoveTemp(WorkingResources);
	TotalStoredScaled = WorkingTotalScaled;
	StoredItemCountsByClassKey = MoveTemp(WorkingItemCounts);
	TotalStoredItemCount = WorkingTotalItemCount;
	ApplyStoredMassToOwner();

	CurrentCraftRecipe = RecipeView;
	CurrentCraftElapsedSeconds = 0.0f;
	CurrentCraftDurationSeconds = FMath::Max(KINDA_SMALL_NUMBER, RecipeView.CraftTimeSeconds);
	return true;
}

void UMachineComponent::AdvanceCraft(float DeltaTime, float EffectiveCraftSpeed)
{
	if (!CurrentCraftRecipe.bIsValid || CurrentCraftDurationSeconds <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	CurrentCraftElapsedSeconds += FMath::Max(0.0f, DeltaTime) * FMath::Max(0.0f, EffectiveCraftSpeed);
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
		if (const TSubclassOf<AActor>* OutputActorClass = CurrentCraftRecipe.OutputActorClassByResourceId.Find(ResourceId))
		{
			if (OutputActorClass->Get())
			{
				PendingOutputActorClassByResourceId.FindOrAdd(ResourceId) = *OutputActorClass;
			}
		}
	}

	CurrentCraftRecipe = {};
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
			PendingOutputActorClassByResourceId.Remove(ResourceId);
			continue;
		}

		const TSubclassOf<AActor> OutputActorClassOverride = PendingOutputActorClassByResourceId.FindRef(ResourceId);
		const int32 AcceptedScaled = OutputVolume->QueueResourceScaled(ResourceId, QuantityScaled, OutputActorClassOverride);
		*FoundQuantityScaled = FMath::Max(0, QuantityScaled - AcceptedScaled);
		if (*FoundQuantityScaled == 0)
		{
			PendingOutputScaled.Remove(ResourceId);
			PendingOutputActorClassByResourceId.Remove(ResourceId);
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

	for (const UMaterialDefinitionAsset* Definition : WhitelistResources)
	{
		RegisterResourceMass(Definition);
	}

	for (const UMaterialDefinitionAsset* Definition : BlacklistResources)
	{
		RegisterResourceMass(Definition);
	}

	for (const TObjectPtr<URecipeAsset>& RecipeAssetPtr : Recipes)
	{
		const URecipeAsset* Recipe = RecipeAssetPtr.Get();
		if (Recipe)
		{
			for (const FRecipeMaterialInputEntry& Entry : Recipe->Inputs)
			{
				RegisterResourceMass(Entry.Material);

				for (const UMaterialDefinitionAsset* WhitelistDefinition : Entry.WhitelistMaterials)
				{
					RegisterResourceMass(WhitelistDefinition);
				}

				for (const UMaterialDefinitionAsset* BlacklistDefinition : Entry.BlacklistMaterials)
				{
					RegisterResourceMass(BlacklistDefinition);
				}
			}

			continue;
		}
	}

	for (TObjectIterator<UMaterialDefinitionAsset> It; It; ++It)
	{
		RegisterResourceMass(*It);
	}
}

void UMachineComponent::RegisterResourceMass(const UMaterialDefinitionAsset* ResourceDefinition)
{
	if (!ResourceDefinition)
	{
		return;
	}

	const FName ResourceId = ResourceDefinition->GetResolvedMaterialId();
	if (ResourceId.IsNone())
	{
		return;
	}

	ResourceDefinitionById.Add(ResourceId, const_cast<UMaterialDefinitionAsset*>(ResourceDefinition));
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

	if (const UMaterialDefinitionAsset* Definition = ResolveResourceDefinition(ResourceId))
	{
		const float MassPerUnitKg = Definition->GetMassPerUnitKg();
		ResourceMassPerUnitKgById.Add(ResourceId, MassPerUnitKg);
		return FMath::Max(0.0f, MassPerUnitKg);
	}

	return 0.0f;
}

float UMachineComponent::ResolveGlobalFactorySpeed() const
{
	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<AFactoryWorldConfig> It(World); It; ++It)
		{
			if (const AFactoryWorldConfig* WorldConfig = *It)
			{
				return FMath::Max(0.0f, WorldConfig->FactoryCraftSpeedMultiplier);
			}
		}
	}

	if (const AFactoryWorldConfig* DefaultConfig = GetDefault<AFactoryWorldConfig>())
	{
		return FMath::Max(0.0f, DefaultConfig->FactoryCraftSpeedMultiplier);
	}

	return 1.0f;
}

float UMachineComponent::ResolveEffectiveCraftSpeed() const
{
	return FMath::Max(0.0f, Speed) * ResolveGlobalFactorySpeed();
}

bool UMachineComponent::IsMaterialAllowedForAnyRequirement(const FAnyMaterialRequirement& Requirement, FName ResourceId) const
{
	if (ResourceId.IsNone())
	{
		return false;
	}

	if (Requirement.bUseWhitelist && Requirement.WhitelistMaterialIds.Num() > 0 && !Requirement.WhitelistMaterialIds.Contains(ResourceId))
	{
		return false;
	}

	if (Requirement.bUseBlacklist && Requirement.BlacklistMaterialIds.Contains(ResourceId))
	{
		return false;
	}

	return true;
}

int32 UMachineComponent::ResolveAnyMaterialAvailableScaled(const FAnyMaterialRequirement& Requirement) const
{
	int64 AvailableScaled = 0;
	for (const TPair<FName, int32>& StoredPair : StoredResourcesScaled)
	{
		const FName ResourceId = StoredPair.Key;
		const int32 QuantityScaled = FMath::Max(0, StoredPair.Value);
		if (QuantityScaled <= 0 || !IsMaterialAllowedForAnyRequirement(Requirement, ResourceId))
		{
			continue;
		}

		AvailableScaled += static_cast<int64>(QuantityScaled);
		if (AvailableScaled > TNumericLimits<int32>::Max())
		{
			return TNumericLimits<int32>::Max();
		}
	}

	return static_cast<int32>(AvailableScaled);
}

bool UMachineComponent::TryConsumeAnyMaterialRequirementFromMap(
	const FAnyMaterialRequirement& Requirement,
	TMap<FName, int32>& InOutResourcesScaled,
	int32& InOutTotalScaled) const
{
	int32 RemainingScaledToConsume = FMath::Max(0, Requirement.QuantityScaled);
	if (RemainingScaledToConsume <= 0)
	{
		return true;
	}

	if (Requirement.bUseWhitelist && Requirement.WhitelistMaterialIds.Num() == 0)
	{
		return false;
	}

	TArray<FName> ResourceIds;
	InOutResourcesScaled.GetKeys(ResourceIds);
	ResourceIds.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});

	for (const FName& ResourceId : ResourceIds)
	{
		if (!IsMaterialAllowedForAnyRequirement(Requirement, ResourceId))
		{
			continue;
		}

		int32* FoundQuantityScaled = InOutResourcesScaled.Find(ResourceId);
		if (!FoundQuantityScaled)
		{
			continue;
		}

		const int32 AvailableScaled = FMath::Max(0, *FoundQuantityScaled);
		if (AvailableScaled <= 0)
		{
			continue;
		}

		const int32 ConsumedScaled = FMath::Min(AvailableScaled, RemainingScaledToConsume);
		*FoundQuantityScaled = FMath::Max(0, AvailableScaled - ConsumedScaled);
		InOutTotalScaled = FMath::Max(0, InOutTotalScaled - ConsumedScaled);
		RemainingScaledToConsume -= ConsumedScaled;
		if (*FoundQuantityScaled == 0)
		{
			InOutResourcesScaled.Remove(ResourceId);
		}

		if (RemainingScaledToConsume <= 0)
		{
			break;
		}
	}

	if (RemainingScaledToConsume > 0)
	{
		return false;
	}

	return true;
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
		CachedOwnerBaseMassKg = ResolvePrimitiveMassKgWithoutPhysicsWarningForMachine(CachedOwnerPrimitive);
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

