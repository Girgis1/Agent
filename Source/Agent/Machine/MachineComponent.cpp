// Copyright Epic Games, Inc. All Rights Reserved.

#include "Machine/MachineComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Material/MaterialDefinitionAsset.h"
#include "Material/MaterialTypes.h"
#include "Machine/InputVolume.h"
#include "Machine/OutputVolume.h"
#include "Material/RecipeAsset.h"
#include "UObject/UObjectIterator.h"
#include <limits>

namespace
{
const TCHAR* MachineRecipeClassOutputPrefix = TEXT("__RecipeClassOutput__:");

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
		else
		{
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
				break;
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
		for (const UMaterialDefinitionAsset* Definition : BlacklistResources)
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

		OutRecipeView.CraftTimeSeconds = Recipe->GetResolvedCraftTimeSeconds();
		Recipe->BuildInputRequirementsScaled(OutRecipeView.InputScaled);
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

		const bool bHasRecipeInput = OutRecipeView.InputScaled.Num() > 0;
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

bool UMachineComponent::CanCraftRecipe(const FRecipeView& RecipeView) const
{
	if (!RecipeView.bIsValid)
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

	const FName ResourceId = ResourceDefinition->GetResolvedResourceId();
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

