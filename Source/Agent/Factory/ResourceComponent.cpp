// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/ResourceComponent.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Factory/FactoryWorldConfig.h"
#include "Factory/ResourceDefinitionAsset.h"
#include "Factory/ResourceTypes.h"

bool FResourceMaterialEntry::IsUsable() const
{
	return Resource != nullptr
		&& (!bUseRange ? Units > KINDA_SMALL_NUMBER : GetMaxPossibleUnits() > KINDA_SMALL_NUMBER);
}

float FResourceMaterialEntry::ResolveUnits(bool bAllowRangeRoll) const
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

float FResourceMaterialEntry::GetMaxPossibleUnits() const
{
	if (!bUseRange)
	{
		return FMath::Max(0.0f, Units);
	}

	return FMath::Max(FMath::Max(0.0f, MinUnits), FMath::Max(0.0f, MaxUnits));
}

UResourceComponent::UResourceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UResourceComponent::HasDefinedResources() const
{
	return HasMaterialEntries();
}

bool UResourceComponent::HasMaterialEntries() const
{
	for (const FResourceMaterialEntry& Entry : Materials)
	{
		if (Entry.IsUsable())
		{
			return true;
		}
	}

	return false;
}

float UResourceComponent::ResolveEffectiveMassKg(const UPrimitiveComponent* SourcePrimitive) const
{
	return SourcePrimitive ? FMath::Max(0.0f, SourcePrimitive->GetMass()) : 0.0f;
}

void UResourceComponent::InitializeRuntimeResourceState(UPrimitiveComponent* SourcePrimitive)
{
	ResolveGeneratedContents(SourcePrimitive);
	CalculateAndApplyFinalMassKg(SourcePrimitive);
}

bool UResourceComponent::BuildResolvedResourceQuantitiesScaled(UPrimitiveComponent* SourcePrimitive, TMap<FName, int32>& OutQuantitiesScaled)
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

float UResourceComponent::CalculateAndApplyFinalMassKg(UPrimitiveComponent* SourcePrimitive)
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

void UResourceComponent::ResetGeneratedContents()
{
	bGeneratedContentsResolved = false;
	GeneratedResourceAmounts.Reset();
	CachedTotalMaterialWeightKg = 0.0f;
	bBaseMassResolved = false;
	CachedBaseMassKg = 0.0f;
}

void UResourceComponent::ResolveGeneratedContents(UPrimitiveComponent* SourcePrimitive)
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

void UResourceComponent::BuildFixedContents(UPrimitiveComponent* SourcePrimitive)
{
	const float ScaleFactor = ResolveLinearScaleFactor(SourcePrimitive);
	for (const FResourceMaterialEntry& Entry : Materials)
	{
		if (!Entry.IsUsable() || !Entry.Resource)
		{
			continue;
		}

		AppendResolvedResource(Entry.Resource, Entry.ResolveUnits(true) * ScaleFactor);
	}
}

void UResourceComponent::BuildRandomizedContents(UPrimitiveComponent* SourcePrimitive)
{
	struct FLocalResolvedEntry
	{
		const UResourceDefinitionAsset* Resource = nullptr;
		float Units = 0.0f;
	};

	TArray<int32> CandidateIndices;
	for (int32 Index = 0; Index < Materials.Num(); ++Index)
	{
		const FResourceMaterialEntry& Entry = Materials[Index];
		if (!Entry.IsUsable() || !Entry.Resource)
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

		const FResourceMaterialEntry& Entry = Materials[EntryIndex];
		const float Units = Entry.ResolveUnits(true);
		if (Units <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		FLocalResolvedEntry ResolvedEntry;
		ResolvedEntry.Resource = Entry.Resource;
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
		if (!Entry.Resource)
		{
			continue;
		}

		AppendResolvedResource(Entry.Resource, Entry.Units * ScaleFactor);
	}
}

void UResourceComponent::AppendResolvedResource(const UResourceDefinitionAsset* ResourceAsset, float QuantityUnits)
{
	if (!ResourceAsset || QuantityUnits <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FName ResourceId = ResourceAsset->GetResolvedResourceId();
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

float UResourceComponent::ResolveLinearScaleFactor(const UPrimitiveComponent* SourcePrimitive) const
{
	if (!SourcePrimitive)
	{
		return 1.0f;
	}

	const FVector Scale3D = SourcePrimitive->GetComponentScale().GetAbs();
	const float UniformScale = (Scale3D.X + Scale3D.Y + Scale3D.Z) / 3.0f;
	return FMath::Max(KINDA_SMALL_NUMBER, UniformScale);
}

float UResourceComponent::ResolveBaseMassKgForFormula(UPrimitiveComponent* SourcePrimitive)
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

float UResourceComponent::ResolveGlobalMassMultiplier(const UWorld* World) const
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
