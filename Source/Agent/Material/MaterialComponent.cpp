// Copyright Epic Games, Inc. All Rights Reserved.

#include "Material/MaterialComponent.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Factory/FactoryWorldConfig.h"
#include "Material/MaterialDefinitionAsset.h"
#include "Material/AgentResourceTypes.h"
#include "UObject/UObjectIterator.h"

namespace
{
UPrimitiveComponent* ResolveOwnerSourcePrimitive(AActor* OwnerActor)
{
	if (!OwnerActor)
	{
		return nullptr;
	}

	if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent()))
	{
		return RootPrimitive;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	OwnerActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	UPrimitiveComponent* FirstUsablePrimitive = nullptr;
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent || PrimitiveComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
		{
			continue;
		}

		if (!FirstUsablePrimitive)
		{
			FirstUsablePrimitive = PrimitiveComponent;
		}

		if (PrimitiveComponent->IsSimulatingPhysics())
		{
			return PrimitiveComponent;
		}
	}

	return FirstUsablePrimitive;
}

UMaterialDefinitionAsset* FindMaterialDefinitionById(FName ResourceId)
{
	if (ResourceId.IsNone())
	{
		return nullptr;
	}

	for (TObjectIterator<UMaterialDefinitionAsset> It; It; ++It)
	{
		UMaterialDefinitionAsset* Candidate = *It;
		if (Candidate && Candidate->GetResolvedMaterialId() == ResourceId)
		{
			return Candidate;
		}
	}

	return nullptr;
}
}

bool FMaterialEntry::IsUsable() const
{
	return Material != nullptr
		&& (!bUseRange ? Units > KINDA_SMALL_NUMBER : GetMaxPossibleUnits() > KINDA_SMALL_NUMBER);
}

float FMaterialEntry::ResolveUnits(bool bAllowRangeRoll) const
{
	if (!IsUsable())
	{
		return 0.0f;
	}

	if (!bUseRange || !bAllowRangeRoll)
	{
		return FMath::Max(0.0f, Units);
	}

	const float MinValue = FMath::Max(0.0f, MinUnits);
	const float MaxValue = FMath::Max(MinValue, MaxUnits);
	return FMath::FRandRange(MinValue, MaxValue);
}

float FMaterialEntry::GetMaxPossibleUnits() const
{
	if (!bUseRange)
	{
		return FMath::Max(0.0f, Units);
	}

	return FMath::Max(FMath::Max(0.0f, MinUnits), FMath::Max(0.0f, MaxUnits));
}

UMaterialComponent::UMaterialComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UMaterialComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(GetOwner() ? GetOwner()->GetRootComponent() : nullptr))
	{
		InitializeRuntimeResourceState(RootPrimitive);
	}
}

bool UMaterialComponent::HasDefinedResources() const
{
	return HasMaterialEntries();
}

bool UMaterialComponent::HasMaterialEntries() const
{
	for (const FMaterialEntry& Entry : Materials)
	{
		if (Entry.IsUsable())
		{
			return true;
		}
	}

	return false;
}

float UMaterialComponent::ResolveEffectiveMassKg(const UPrimitiveComponent* SourcePrimitive) const
{
	return SourcePrimitive ? FMath::Max(0.0f, SourcePrimitive->GetMass()) : 0.0f;
}

void UMaterialComponent::InitializeRuntimeResourceState(UPrimitiveComponent* SourcePrimitive)
{
	ResolveGeneratedContents(SourcePrimitive);
	CalculateAndApplyFinalMassKg(SourcePrimitive);
}

bool UMaterialComponent::BuildResolvedResourceQuantitiesScaled(UPrimitiveComponent* SourcePrimitive, TMap<FName, int32>& OutQuantitiesScaled)
{
	OutQuantitiesScaled.Reset();
	ResolveGeneratedContents(SourcePrimitive);

	for (const FResourceAmount& Amount : GeneratedResourceAmounts)
	{
		if (!Amount.HasQuantity())
		{
			continue;
		}

		OutQuantitiesScaled.FindOrAdd(Amount.ResourceId) += Amount.GetScaledQuantity();
	}

	return OutQuantitiesScaled.Num() > 0;
}

bool UMaterialComponent::ConfigureSingleResourceById(FName ResourceId, int32 QuantityScaled)
{
	const FName SafeResourceId = ResourceId;
	const int32 SafeQuantityScaled = FMath::Max(0, QuantityScaled);
	if (SafeResourceId.IsNone() || SafeQuantityScaled <= 0)
	{
		return false;
	}

	UMaterialDefinitionAsset* MatchedResource = FindMaterialDefinitionById(SafeResourceId);
	if (!MatchedResource)
	{
		return false;
	}

	const float QuantityUnits = AgentResource::ScaledToUnits(SafeQuantityScaled);
	if (QuantityUnits <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	FMaterialEntry ResourceEntry;
	ResourceEntry.Material = MatchedResource;
	ResourceEntry.bUseRange = false;
	ResourceEntry.Units = QuantityUnits;
	ResourceEntry.MinUnits = QuantityUnits;
	ResourceEntry.MaxUnits = QuantityUnits;

	Materials.Reset();
	Materials.Add(ResourceEntry);
	bUseRandomizedContents = false;
	ChosenMaterialCountMin = 1;
	ChosenMaterialCountMax = 1;
	TotalUnitsMin = 0.0f;
	TotalUnitsMax = 0.0f;

	ResetGeneratedContents();
	if (UPrimitiveComponent* SourcePrimitive = ResolveOwnerSourcePrimitive(GetOwner()))
	{
		InitializeRuntimeResourceState(SourcePrimitive);
	}
	else
	{
		ResolveGeneratedContents(nullptr);
	}

	return true;
}

bool UMaterialComponent::ConfigureResourcesById(const TMap<FName, int32>& ResourceQuantitiesScaled)
{
	if (ResourceQuantitiesScaled.Num() == 0)
	{
		return false;
	}

	TArray<FMaterialEntry> ResolvedEntries;
	ResolvedEntries.Reserve(ResourceQuantitiesScaled.Num());

	TArray<FName> ResourceIds;
	ResourceQuantitiesScaled.GetKeys(ResourceIds);
	ResourceIds.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});

	for (const FName& ResourceId : ResourceIds)
	{
		const int32 QuantityScaled = FMath::Max(0, ResourceQuantitiesScaled.FindRef(ResourceId));
		if (ResourceId.IsNone() || QuantityScaled <= 0)
		{
			continue;
		}

		UMaterialDefinitionAsset* MatchedResource = FindMaterialDefinitionById(ResourceId);
		if (!MatchedResource)
		{
			continue;
		}

		const float QuantityUnits = AgentResource::ScaledToUnits(QuantityScaled);
		if (QuantityUnits <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		FMaterialEntry ResourceEntry;
		ResourceEntry.Material = MatchedResource;
		ResourceEntry.bUseRange = false;
		ResourceEntry.Units = QuantityUnits;
		ResourceEntry.MinUnits = QuantityUnits;
		ResourceEntry.MaxUnits = QuantityUnits;
		ResolvedEntries.Add(ResourceEntry);
	}

	if (ResolvedEntries.Num() == 0)
	{
		return false;
	}

	Materials = MoveTemp(ResolvedEntries);
	bUseRandomizedContents = false;
	ChosenMaterialCountMin = 1;
	ChosenMaterialCountMax = 1;
	TotalUnitsMin = 0.0f;
	TotalUnitsMax = 0.0f;

	ResetGeneratedContents();
	if (UPrimitiveComponent* SourcePrimitive = ResolveOwnerSourcePrimitive(GetOwner()))
	{
		InitializeRuntimeResourceState(SourcePrimitive);
	}
	else
	{
		ResolveGeneratedContents(nullptr);
	}

	return true;
}

float UMaterialComponent::CalculateAndApplyFinalMassKg(UPrimitiveComponent* SourcePrimitive)
{
	if (!SourcePrimitive)
	{
		return 0.0f;
	}

	ResolveGeneratedContents(SourcePrimitive);

	const float BaseMassKg = ResolveBaseMassKgForFormula(SourcePrimitive);
	const float GlobalMassMultiplier = ResolveGlobalMassMultiplier(GetWorld());
	const float FinalMassKg = FMath::Max(0.1f, CachedTotalMaterialWeightKg + (BaseMassKg * GlobalMassMultiplier));

	SourcePrimitive->SetMassOverrideInKg(NAME_None, FinalMassKg, true);
	return FinalMassKg;
}

void UMaterialComponent::ResetGeneratedContents()
{
	bGeneratedContentsResolved = false;
	GeneratedResourceAmounts.Reset();
	CachedTotalMaterialWeightKg = 0.0f;
	bBaseMassResolved = false;
	CachedBaseMassKg = 0.0f;
}

void UMaterialComponent::ResolveGeneratedContents(UPrimitiveComponent* SourcePrimitive)
{
	if (bGeneratedContentsResolved)
	{
		return;
	}

	GeneratedResourceAmounts.Reset();
	CachedTotalMaterialWeightKg = 0.0f;

	if (HasMaterialEntries())
	{
		if (bUseRandomizedContents)
		{
			BuildRandomizedContents(SourcePrimitive);
		}
		else
		{
			BuildFixedContents(SourcePrimitive);
		}
	}

	bGeneratedContentsResolved = true;
}

void UMaterialComponent::BuildFixedContents(UPrimitiveComponent* SourcePrimitive)
{
	const float ScaleFactor = ResolveLinearScaleFactor(SourcePrimitive);
	for (const FMaterialEntry& Entry : Materials)
	{
		if (!Entry.IsUsable() || !Entry.Material)
		{
			continue;
		}

		AppendResolvedResource(Entry.Material, Entry.ResolveUnits(true) * ScaleFactor);
	}
}

void UMaterialComponent::BuildRandomizedContents(UPrimitiveComponent* SourcePrimitive)
{
	struct FLocalResolvedEntry
	{
		const UMaterialDefinitionAsset* Material = nullptr;
		float Units = 0.0f;
	};

	TArray<int32> CandidateIndices;
	for (int32 Index = 0; Index < Materials.Num(); ++Index)
	{
		const FMaterialEntry& Entry = Materials[Index];
		if (!Entry.IsUsable() || !Entry.Material)
		{
			continue;
		}

		CandidateIndices.Add(Index);
	}

	if (CandidateIndices.Num() == 0)
	{
		return;
	}

	const int32 MinSelectionCount = FMath::Max(1, ChosenMaterialCountMin);
	const int32 MaxSelectionCount = FMath::Max(MinSelectionCount, ChosenMaterialCountMax);
	const int32 SelectionCount = FMath::Clamp(FMath::RandRange(MinSelectionCount, MaxSelectionCount), 1, CandidateIndices.Num());

	TArray<int32> RemainingIndices = CandidateIndices;
	TArray<int32> SelectedEntryIndices;
	SelectedEntryIndices.Reserve(SelectionCount);

	for (int32 SelectionIndex = 0; SelectionIndex < SelectionCount; ++SelectionIndex)
	{
		if (RemainingIndices.Num() == 0)
		{
			break;
		}

		const int32 PickedPosition = FMath::RandRange(0, RemainingIndices.Num() - 1);
		SelectedEntryIndices.Add(RemainingIndices[PickedPosition]);
		RemainingIndices.RemoveAtSwap(PickedPosition);
	}

	TArray<FLocalResolvedEntry> ResolvedEntries;
	ResolvedEntries.Reserve(SelectedEntryIndices.Num());
	for (const int32 EntryIndex : SelectedEntryIndices)
	{
		if (!Materials.IsValidIndex(EntryIndex))
		{
			continue;
		}

		const FMaterialEntry& Entry = Materials[EntryIndex];
		const float Units = Entry.ResolveUnits(true);
		if (Units <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		FLocalResolvedEntry ResolvedEntry;
		ResolvedEntry.Material = Entry.Material;
		ResolvedEntry.Units = Units;
		ResolvedEntries.Add(ResolvedEntry);
	}

	if (ResolvedEntries.Num() == 0)
	{
		return;
	}

	const float MinTotalUnits = FMath::Max(0.0f, TotalUnitsMin);
	const float MaxTotalUnits = FMath::Max(MinTotalUnits, TotalUnitsMax);
	if (MaxTotalUnits > KINDA_SMALL_NUMBER)
	{
		const float TargetUnits = FMath::FRandRange(MinTotalUnits, MaxTotalUnits);
		float CurrentUnits = 0.0f;
		for (const FLocalResolvedEntry& Entry : ResolvedEntries)
		{
			CurrentUnits += FMath::Max(0.0f, Entry.Units);
		}

		if (CurrentUnits > KINDA_SMALL_NUMBER)
		{
			const float NormalizationScale = TargetUnits / CurrentUnits;
			for (FLocalResolvedEntry& Entry : ResolvedEntries)
			{
				Entry.Units = FMath::Max(0.0f, Entry.Units * NormalizationScale);
			}
		}
	}

	const float ScaleFactor = ResolveLinearScaleFactor(SourcePrimitive);
	for (const FLocalResolvedEntry& Entry : ResolvedEntries)
	{
		if (!Entry.Material)
		{
			continue;
		}

		AppendResolvedResource(Entry.Material, Entry.Units * ScaleFactor);
	}
}

void UMaterialComponent::AppendResolvedResource(const UMaterialDefinitionAsset* ResourceAsset, float QuantityUnits)
{
	if (!ResourceAsset || QuantityUnits <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FName ResourceId = ResourceAsset->GetResolvedMaterialId();
	if (ResourceId.IsNone())
	{
		return;
	}

	const int32 QuantityScaled = AgentResource::UnitsToScaled(QuantityUnits);
	if (QuantityScaled <= 0)
	{
		return;
	}

	for (FResourceAmount& ExistingAmount : GeneratedResourceAmounts)
	{
		if (ExistingAmount.ResourceId == ResourceId)
		{
			ExistingAmount.SetScaledQuantity(ExistingAmount.GetScaledQuantity() + QuantityScaled);
			CachedTotalMaterialWeightKg += QuantityUnits * ResourceAsset->GetMassPerUnitKg();
			return;
		}
	}

	FResourceAmount NewAmount;
	NewAmount.ResourceId = ResourceId;
	NewAmount.SetScaledQuantity(QuantityScaled);
	GeneratedResourceAmounts.Add(NewAmount);
	CachedTotalMaterialWeightKg += QuantityUnits * ResourceAsset->GetMassPerUnitKg();
}

float UMaterialComponent::ResolveLinearScaleFactor(const UPrimitiveComponent* SourcePrimitive) const
{
	if (!SourcePrimitive)
	{
		return 1.0f;
	}

	const FVector Scale3D = SourcePrimitive->GetComponentScale().GetAbs();
	const float UniformScale = (Scale3D.X + Scale3D.Y + Scale3D.Z) / 3.0f;
	return FMath::Max(KINDA_SMALL_NUMBER, UniformScale);
}

float UMaterialComponent::ResolveBaseMassKgForFormula(UPrimitiveComponent* SourcePrimitive)
{
	if (!SourcePrimitive)
	{
		return 0.0f;
	}

	if (!bBaseMassResolved)
	{
		CachedBaseMassKg = FMath::Max(0.0f, SourcePrimitive->GetMass());
		bBaseMassResolved = true;
	}

	return CachedBaseMassKg;
}

float UMaterialComponent::ResolveGlobalMassMultiplier(const UWorld* World) const
{
	if (World)
	{
		for (TActorIterator<AFactoryWorldConfig> It(World); It; ++It)
		{
			if (const AFactoryWorldConfig* WorldConfig = *It)
			{
				return FMath::Max(0.0f, WorldConfig->ResourceBaseMassMultiplier);
			}
		}
	}

	if (const AFactoryWorldConfig* DefaultConfig = GetDefault<AFactoryWorldConfig>())
	{
		return FMath::Max(0.0f, DefaultConfig->ResourceBaseMassMultiplier);
	}

	return 1.0f;
}

