// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/ResourceComponent.h"
#include "Components/PrimitiveComponent.h"
#include "EngineUtils.h"
#include "Engine/World.h"
#include "Factory/FactoryWorldConfig.h"
#include "Factory/ResourceCompositionAsset.h"
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

FName FResourceMaterialEntry::GetResourceId() const
{
	return Resource ? Resource->GetResolvedResourceId() : NAME_None;
}

float FResourceMaterialEntry::GetPickWeight() const
{
	return FMath::Max(0.0f, PickWeight);
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

void UResourceComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(GetOwner() ? GetOwner()->GetRootComponent() : nullptr))
	{
		InitializeRuntimeResourceState(RootPrimitive);
	}
}

bool UResourceComponent::HasDefinedResources() const
{
	return HasMaterialEntries()
		|| (Composition && Composition->HasDefinedResources())
		|| HasSimpleResourceDefinition();
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
	float EffectiveMassKg = bUsePhysicsMass && SourcePrimitive
		? SourcePrimitive->GetMass()
		: FMath::Max(0.0f, MassOverrideKg);

	EffectiveMassKg *= FMath::Max(0.0f, MassMultiplier);
	return FMath::Max(0.0f, EffectiveMassKg);
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

	if (OutQuantitiesScaled.Num() > 0)
	{
		return true;
	}

	// Legacy composition fallback.
	if (Composition && Composition->HasDefinedResources())
	{
		const float EffectiveMassKg = ResolveEffectiveMassKg(SourcePrimitive);
		const float TotalMassFraction = Composition->GetTotalMassFraction();
		const float RecoveryScalar = ResolveRecoveryScalar();
		if (EffectiveMassKg <= 0.0f || TotalMassFraction <= KINDA_SMALL_NUMBER || RecoveryScalar <= 0.0f)
		{
			return false;
		}

		for (const FResourceCompositionEntry& Entry : Composition->Entries)
		{
			if (!Entry.IsDefined())
			{
				continue;
			}

			const FName ResourceId = Entry.Resource->GetResolvedResourceId();
			if (ResourceId.IsNone())
			{
				continue;
			}

			const float NormalizedFraction = FMath::Max(0.0f, Entry.MassFraction) / TotalMassFraction;
			const float QuantityUnits = EffectiveMassKg
				* NormalizedFraction
				* FMath::Max(0.0f, Entry.YieldScalar)
				* RecoveryScalar;
			const int32 QuantityScaled = AgentResource::UnitsToScaled(QuantityUnits);
			if (QuantityScaled > 0)
			{
				OutQuantitiesScaled.FindOrAdd(ResourceId) += QuantityScaled;
			}
		}

		return OutQuantitiesScaled.Num() > 0;
	}

	// Legacy simple fallback.
	if (HasSimpleResourceDefinition())
	{
		const int32 QuantityScaled = ResolveSimpleResourceQuantityScaled(SourcePrimitive);
		if (QuantityScaled > 0)
		{
			OutQuantitiesScaled.FindOrAdd(GetPrimaryResourceId()) += QuantityScaled;
			return true;
		}
	}

	return false;
}

float UResourceComponent::CalculateAndApplyFinalMassKg(UPrimitiveComponent* SourcePrimitive)
{
	if (!SourcePrimitive)
	{
		return 0.0f;
	}

	ResolveGeneratedContents(SourcePrimitive);

	// Ensure base mass comes from the current physical body, not a prior override.
	if (bUsePhysicsMass)
	{
		SourcePrimitive->SetMassOverrideInKg(NAME_None, 0.0f, false);
	}

	const float BaseMassKg = ResolveBaseMassKgForFormula(SourcePrimitive);
	const float GlobalMassMultiplier = ResolveGlobalMassMultiplier(GetWorld());
	float FinalMassKg = CachedTotalMaterialWeightKg + (BaseMassKg * GlobalMassMultiplier);
	FinalMassKg *= FMath::Max(0.0f, MassMultiplier);
	FinalMassKg = FMath::Max(0.1f, FinalMassKg);

	SourcePrimitive->SetMassOverrideInKg(NAME_None, FinalMassKg, true);
	return FinalMassKg;
}

void UResourceComponent::ResetGeneratedContents()
{
	bGeneratedContentsResolved = false;
	CachedTotalMaterialWeightKg = 0.0f;
	GeneratedResourceAmounts.Reset();
}

bool UResourceComponent::HasSimpleResourceDefinition() const
{
	return PrimaryResource != nullptr
		&& !GetPrimaryResourceId().IsNone()
		&& PrimaryResourceUnitsPerKg > 0.0f;
}

FName UResourceComponent::GetPrimaryResourceId() const
{
	return PrimaryResource ? PrimaryResource->GetResolvedResourceId() : NAME_None;
}

int32 UResourceComponent::ResolveSimpleResourceQuantityScaled(const UPrimitiveComponent* SourcePrimitive) const
{
	if (!HasSimpleResourceDefinition())
	{
		return 0;
	}

	const float EffectiveMassKg = ResolveEffectiveMassKg(SourcePrimitive);
	if (EffectiveMassKg <= 0.0f)
	{
		return 0;
	}

	const float QuantityUnits = EffectiveMassKg * FMath::Max(0.0f, PrimaryResourceUnitsPerKg) * ResolveRecoveryScalar();
	return AgentResource::UnitsToScaled(QuantityUnits);
}

float UResourceComponent::ResolveRecoveryScalar() const
{
	return FMath::Max(0.0f, RecoveryMultiplier);
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
	const float ScaleFactor = ResolveScaleUnitsFactor(SourcePrimitive);
	for (const FResourceMaterialEntry& Entry : Materials)
	{
		if (!Entry.IsUsable() || !Entry.Resource)
		{
			continue;
		}

		const float Units = Entry.ResolveUnits(true) * ScaleFactor;
		AppendResolvedResource(Entry.Resource, Units);
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
		if (!Entry.IsUsable() || !Entry.Resource || Entry.GetPickWeight() <= KINDA_SMALL_NUMBER)
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
		const int32 PickedPosition = PickWeightedEntryIndex(RemainingIndices);
		if (!RemainingIndices.IsValidIndex(PickedPosition))
		{
			break;
		}

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

	const float ScaleFactor = ResolveScaleUnitsFactor(SourcePrimitive);
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

float UResourceComponent::ResolveScaleUnitsFactor(const UPrimitiveComponent* SourcePrimitive) const
{
	if (!SourcePrimitive || ScaleUnitsMode == EResourceScaleUnitsMode::None)
	{
		return 1.0f;
	}

	const FVector Scale3D = SourcePrimitive->GetComponentScale().GetAbs();
	const float UniformScale = FMath::Max(KINDA_SMALL_NUMBER, (Scale3D.X + Scale3D.Y + Scale3D.Z) / 3.0f);

	switch (ScaleUnitsMode)
	{
	case EResourceScaleUnitsMode::Linear:
		return UniformScale;

	case EResourceScaleUnitsMode::Volume:
		return FMath::Pow(UniformScale, 3.0f);

	case EResourceScaleUnitsMode::None:
	default:
		return 1.0f;
	}
}

float UResourceComponent::ResolveBaseMassKgForFormula(const UPrimitiveComponent* SourcePrimitive) const
{
	if (bUsePhysicsMass && SourcePrimitive)
	{
		return FMath::Max(0.0f, SourcePrimitive->GetMass());
	}

	return FMath::Max(0.0f, MassOverrideKg);
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

int32 UResourceComponent::PickWeightedEntryIndex(const TArray<int32>& CandidateIndices) const
{
	if (CandidateIndices.Num() == 0)
	{
		return INDEX_NONE;
	}

	float TotalWeight = 0.0f;
	for (const int32 CandidateIndex : CandidateIndices)
	{
		if (Materials.IsValidIndex(CandidateIndex))
		{
			TotalWeight += Materials[CandidateIndex].GetPickWeight();
		}
	}

	if (TotalWeight <= KINDA_SMALL_NUMBER)
	{
		return FMath::RandRange(0, CandidateIndices.Num() - 1);
	}

	float RemainingWeight = FMath::FRandRange(0.0f, TotalWeight);
	for (int32 ArrayIndex = 0; ArrayIndex < CandidateIndices.Num(); ++ArrayIndex)
	{
		const int32 CandidateIndex = CandidateIndices[ArrayIndex];
		if (!Materials.IsValidIndex(CandidateIndex))
		{
			continue;
		}

		RemainingWeight -= Materials[CandidateIndex].GetPickWeight();
		if (RemainingWeight <= 0.0f)
		{
			return ArrayIndex;
		}
	}

	return CandidateIndices.Num() - 1;
}
