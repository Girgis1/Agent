// Copyright Epic Games, Inc. All Rights Reserved.

#include "Objects/Components/ObjectFractureComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Material/MaterialComponent.h"
#include "Objects/Actors/ObjectFragmentActor.h"
#include "Objects/Actors/ObjectGeometryCollectionActor.h"
#include "Objects/Assets/ObjectFractureDefinitionAsset.h"
#include "Objects/Components/ObjectHealthComponent.h"
#include "Objects/Types/ObjectHealthTypes.h"
#include "PhysicsEngine/BodyInstance.h"

namespace
{
struct FResolvedObjectFractureSourceState
{
	AActor* OwnerActor = nullptr;
	UPrimitiveComponent* SourcePrimitive = nullptr;
	UMaterialComponent* SourceMaterialComponent = nullptr;
	UObjectHealthComponent* SourceHealthComponent = nullptr;
	TMap<FName, int32> SourceResourceQuantitiesScaled;
	float SourceMaterialWeightKg = 0.0f;
	float SourceGlobalMassMultiplier = 1.0f;
	float SourceCurrentMassKg = 0.0f;
	float SourceBaseContributionKg = 0.0f;
	float SourceBaseMassBeforeMultiplierKg = 0.0f;
	FVector SourceLinearVelocity = FVector::ZeroVector;
	FVector SourceAngularVelocityDeg = FVector::ZeroVector;
	FVector SourceLocation = FVector::ZeroVector;
	FTransform SourceTransform = FTransform::Identity;
	float InheritedDamagedPenaltyPercent = 0.0f;
};

UPrimitiveComponent* ResolveBestPrimitiveOnActor(AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
	{
		return RootPrimitive;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

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

float ResolvePrimitiveMassKgWithoutWarningForFracture(const UPrimitiveComponent* PrimitiveComponent)
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

	if (PrimitiveComponent->IsSimulatingPhysics())
	{
		return FMath::Max(0.0f, PrimitiveComponent->GetMass());
	}

	return 0.0f;
}

double EstimateStaticMeshRelativeMassWeight(const UStaticMesh* StaticMesh)
{
	if (!StaticMesh)
	{
		return 0.0;
	}

	const FBoxSphereBounds MeshBounds = StaticMesh->GetBounds();
	const FVector BoxSize = MeshBounds.BoxExtent * 2.0f;
	const double EstimatedVolume = static_cast<double>(BoxSize.X) * static_cast<double>(BoxSize.Y) * static_cast<double>(BoxSize.Z);
	return EstimatedVolume > KINDA_SMALL_NUMBER ? EstimatedVolume : 1.0;
}

void BuildNormalizedFragmentWeights(
	const UObjectFractureDefinitionAsset* Definition,
	TArray<int32>& OutUsableFragmentIndices,
	TArray<double>& OutNormalizedWeights)
{
	OutUsableFragmentIndices.Reset();
	OutNormalizedWeights.Reset();

	if (!Definition)
	{
		return;
	}

	double TotalWeight = 0.0;
	for (int32 Index = 0; Index < Definition->Fragments.Num(); ++Index)
	{
		const FObjectFractureFragmentEntry& Entry = Definition->Fragments[Index];
		const TSubclassOf<AActor> ResolvedFragmentClass = Entry.FragmentActorClass.Get()
			? Entry.FragmentActorClass
			: Definition->DefaultFragmentActorClass;
		if (!ResolvedFragmentClass.Get())
		{
			continue;
		}

		OutUsableFragmentIndices.Add(Index);
		const double SafeMassRatio = FMath::Max(0.0, static_cast<double>(Entry.MassRatio));
		OutNormalizedWeights.Add(SafeMassRatio);
		TotalWeight += SafeMassRatio;
	}

	if (OutUsableFragmentIndices.Num() == 0)
	{
		return;
	}

	if (TotalWeight <= KINDA_SMALL_NUMBER)
	{
		const double EqualWeight = 1.0 / static_cast<double>(OutUsableFragmentIndices.Num());
		for (double& Weight : OutNormalizedWeights)
		{
			Weight = EqualWeight;
		}
		return;
	}

	for (double& Weight : OutNormalizedWeights)
	{
		Weight /= TotalWeight;
	}
}

void SplitResourceQuantitiesExact(
	const TMap<FName, int32>& SourceQuantitiesScaled,
	const TArray<double>& NormalizedWeights,
	TArray<TMap<FName, int32>>& OutSplitQuantitiesScaled)
{
	OutSplitQuantitiesScaled.Reset();
	OutSplitQuantitiesScaled.SetNum(NormalizedWeights.Num());

	if (NormalizedWeights.Num() == 0 || SourceQuantitiesScaled.Num() == 0)
	{
		return;
	}

	for (const TPair<FName, int32>& Pair : SourceQuantitiesScaled)
	{
		const FName ResourceId = Pair.Key;
		const int32 SourceQuantityScaled = FMath::Max(0, Pair.Value);
		if (ResourceId.IsNone() || SourceQuantityScaled <= 0)
		{
			continue;
		}

		TArray<int32> WholeShares;
		TArray<double> FractionalRemainders;
		WholeShares.SetNumZeroed(NormalizedWeights.Num());
		FractionalRemainders.SetNumZeroed(NormalizedWeights.Num());

		int32 AllocatedScaled = 0;
		for (int32 Index = 0; Index < NormalizedWeights.Num(); ++Index)
		{
			const double RawShare = static_cast<double>(SourceQuantityScaled) * NormalizedWeights[Index];
			const int32 WholeShare = FMath::Max(0, static_cast<int32>(FMath::FloorToDouble(RawShare)));
			WholeShares[Index] = WholeShare;
			FractionalRemainders[Index] = RawShare - static_cast<double>(WholeShare);
			AllocatedScaled += WholeShare;
		}

		int32 RemainingScaled = FMath::Max(0, SourceQuantityScaled - AllocatedScaled);
		while (RemainingScaled > 0)
		{
			int32 BestIndex = 0;
			double BestRemainder = -1.0;
			for (int32 Index = 0; Index < FractionalRemainders.Num(); ++Index)
			{
				if (FractionalRemainders[Index] > BestRemainder)
				{
					BestRemainder = FractionalRemainders[Index];
					BestIndex = Index;
				}
			}

			++WholeShares[BestIndex];
			FractionalRemainders[BestIndex] = -1.0;
			--RemainingScaled;
		}

		for (int32 Index = 0; Index < WholeShares.Num(); ++Index)
		{
			if (WholeShares[Index] > 0)
			{
				OutSplitQuantitiesScaled[Index].FindOrAdd(ResourceId) += WholeShares[Index];
			}
		}
	}
}

void BuildSourceState(
	AActor* OwnerActor,
	UPrimitiveComponent* SourcePrimitive,
	UMaterialComponent* SourceMaterialComponent,
	UObjectHealthComponent* SourceHealthComponent,
	FResolvedObjectFractureSourceState& OutState)
{
	OutState = FResolvedObjectFractureSourceState();
	OutState.OwnerActor = OwnerActor;
	OutState.SourcePrimitive = SourcePrimitive;
	OutState.SourceMaterialComponent = SourceMaterialComponent;
	OutState.SourceHealthComponent = SourceHealthComponent;
	OutState.SourceLocation = OwnerActor ? OwnerActor->GetActorLocation() : FVector::ZeroVector;
	OutState.SourceTransform = OwnerActor ? OwnerActor->GetActorTransform() : FTransform::Identity;
	OutState.SourceLinearVelocity = SourcePrimitive ? SourcePrimitive->GetPhysicsLinearVelocity() : (OwnerActor ? OwnerActor->GetVelocity() : FVector::ZeroVector);
	OutState.SourceAngularVelocityDeg = SourcePrimitive ? SourcePrimitive->GetPhysicsAngularVelocityInDegrees() : FVector::ZeroVector;
	OutState.InheritedDamagedPenaltyPercent = SourceHealthComponent
		? SourceHealthComponent->GetTotalDamagedPenaltyPercent()
		: 0.0f;

	if (SourceMaterialComponent && SourcePrimitive)
	{
		SourceMaterialComponent->BuildResolvedResourceQuantitiesScaled(SourcePrimitive, OutState.SourceResourceQuantitiesScaled);
		OutState.SourceMaterialWeightKg = SourceMaterialComponent->GetResolvedMaterialWeightKg(SourcePrimitive);
		OutState.SourceGlobalMassMultiplier = FMath::Max(KINDA_SMALL_NUMBER, SourceMaterialComponent->GetResolvedGlobalMassMultiplier());
	}

	OutState.SourceCurrentMassKg = ResolvePrimitiveMassKgWithoutWarningForFracture(SourcePrimitive);
	OutState.SourceBaseContributionKg = FMath::Max(0.0f, OutState.SourceCurrentMassKg - OutState.SourceMaterialWeightKg);
	OutState.SourceBaseMassBeforeMultiplierKg = SourceMaterialComponent
		? (OutState.SourceGlobalMassMultiplier > KINDA_SMALL_NUMBER ? (OutState.SourceBaseContributionKg / OutState.SourceGlobalMassMultiplier) : 0.0f)
		: 0.0f;
}
}

bool FObjectFractureOption::IsUsable() const
{
	switch (OptionType)
	{
	case EObjectFractureOptionType::FragmentDefinition:
		return FractureDefinition != nullptr && FractureDefinition->HasUsableFragments();

	case EObjectFractureOptionType::InlineFragments:
		for (const FObjectFractureFragmentEntry& FragmentEntry : InlineFragments)
		{
			const bool bHasResolvedFragmentActor = FragmentEntry.FragmentActorClass.Get() != nullptr
				|| InlineDefaultFragmentActorClass.Get() != nullptr;
			if (FragmentEntry.StaticMeshOverride != nullptr || bHasResolvedFragmentActor)
			{
				return true;
			}
		}
		return false;

	case EObjectFractureOptionType::SpawnActor:
		return ReplacementActorClass.Get() != nullptr;

	case EObjectFractureOptionType::GeometryCollection:
		return GeometryCollectionAsset != nullptr;

	default:
		return false;
	}
}

UObjectFractureComponent::UObjectFractureComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	StaticMeshFragmentActorClass = AObjectFragmentActor::StaticClass();
}

void UObjectFractureComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoFractureOnDepleted)
	{
		if (UObjectHealthComponent* HealthComponent = ResolveHealthComponent())
		{
			HealthComponent->OnDepleted.AddUniqueDynamic(this, &UObjectFractureComponent::HandleOwnerHealthDepleted);
		}
	}

	if (bFractureEnabled && (HasSimpleStaticMeshFragments() || GeometryCollection != nullptr || HasFractureOptions() || HasFractureDefinitions()))
	{
		if (UObjectHealthComponent* HealthComponent = ResolveHealthComponent())
		{
			HealthComponent->bDestroyOwnerWhenDepleted = false;
		}
	}
}

bool UObjectFractureComponent::CanFracture() const
{
	if (!bFractureEnabled)
	{
		return false;
	}

	if (bOnlyFractureOnce && bHasFractured)
	{
		return false;
	}

	if (HasSimpleStaticMeshFragments() || GeometryCollection != nullptr)
	{
		return true;
	}

	return HasFractureOptions() || HasFractureDefinitions();
}

bool UObjectFractureComponent::TriggerFracture()
{
	if (!CanFracture())
	{
		return false;
	}

	if (UObjectFractureDefinitionAsset* SimpleStaticMeshDefinition = BuildSimpleStaticMeshDefinition())
	{
		return TriggerFractureDefinition(SimpleStaticMeshDefinition);
	}

	if (GeometryCollection != nullptr)
	{
		return TriggerSimpleGeometryCollection();
	}

	if (const FObjectFractureOption* SelectedOption = ResolveFractureOptionForThisBreak())
	{
		if (SelectedOption->OptionType == EObjectFractureOptionType::FragmentDefinition)
		{
			return TriggerFractureDefinition(SelectedOption->FractureDefinition);
		}

		if (SelectedOption->OptionType == EObjectFractureOptionType::InlineFragments)
		{
			if (UObjectFractureDefinitionAsset* InlineDefinition = BuildInlineFragmentDefinition(*SelectedOption))
			{
				return TriggerFractureDefinition(InlineDefinition);
			}

			return false;
		}

		return TriggerSpawnedReplacement(*SelectedOption);
	}

	if (UObjectFractureDefinitionAsset* SelectedDefinition = ResolveFractureDefinitionForThisBreak())
	{
		return TriggerFractureDefinition(SelectedDefinition);
	}

	return false;
}

bool UObjectFractureComponent::HasSimpleStaticMeshFragments() const
{
	for (UStaticMesh* StaticMesh : StaticMeshFragments)
	{
		if (StaticMesh != nullptr)
		{
			return true;
		}
	}

	return false;
}

bool UObjectFractureComponent::HasFractureOptions() const
{
	for (const FObjectFractureOption& Option : FractureOptions)
	{
		if (Option.IsUsable())
		{
			return true;
		}
	}

	return false;
}

bool UObjectFractureComponent::HasFractureDefinitions() const
{
	if (FractureDefinition && FractureDefinition->HasUsableFragments())
	{
		return true;
	}

	for (const TObjectPtr<UObjectFractureDefinitionAsset>& Definition : FractureDefinitions)
	{
		if (Definition && Definition->HasUsableFragments())
		{
			return true;
		}
	}

	return false;
}

const FObjectFractureOption* UObjectFractureComponent::ResolveFractureOptionForThisBreak() const
{
	TArray<const FObjectFractureOption*> ValidOptions;
	ValidOptions.Reserve(FractureOptions.Num());

	double TotalWeight = 0.0;
	for (const FObjectFractureOption& Option : FractureOptions)
	{
		if (!Option.IsUsable())
		{
			continue;
		}

		ValidOptions.Add(&Option);
		TotalWeight += FMath::Max(0.0, static_cast<double>(Option.SelectionWeight));
	}

	if (ValidOptions.Num() == 0)
	{
		return nullptr;
	}

	if (!bSelectRandomFractureOption || ValidOptions.Num() == 1)
	{
		return ValidOptions[0];
	}

	if (TotalWeight <= KINDA_SMALL_NUMBER)
	{
		const int32 RandomIndex = FMath::RandRange(0, ValidOptions.Num() - 1);
		return ValidOptions[RandomIndex];
	}

	const double RandomWeight = FMath::FRandRange(0.0, TotalWeight);
	double RunningWeight = 0.0;
	for (const FObjectFractureOption* Option : ValidOptions)
	{
		RunningWeight += FMath::Max(0.0, static_cast<double>(Option->SelectionWeight));
		if (RandomWeight <= RunningWeight)
		{
			return Option;
		}
	}

	return ValidOptions.Last();
}

UObjectFractureDefinitionAsset* UObjectFractureComponent::ResolveFractureDefinitionForThisBreak() const
{
	TArray<UObjectFractureDefinitionAsset*> ValidDefinitions;
	if (FractureDefinition && FractureDefinition->HasUsableFragments())
	{
		ValidDefinitions.Add(FractureDefinition);
	}

	for (const TObjectPtr<UObjectFractureDefinitionAsset>& Definition : FractureDefinitions)
	{
		if (Definition && Definition->HasUsableFragments())
		{
			ValidDefinitions.AddUnique(Definition);
		}
	}

	if (ValidDefinitions.Num() == 0)
	{
		return nullptr;
	}

	if (!bSelectRandomFractureDefinition || ValidDefinitions.Num() == 1)
	{
		return ValidDefinitions[0];
	}

	const int32 SelectedIndex = FMath::RandRange(0, ValidDefinitions.Num() - 1);
	return ValidDefinitions[SelectedIndex];
}

UObjectFractureDefinitionAsset* UObjectFractureComponent::BuildSimpleStaticMeshDefinition() const
{
	if (!HasSimpleStaticMeshFragments())
	{
		return nullptr;
	}

	UObjectFractureDefinitionAsset* InlineDefinition = NewObject<UObjectFractureDefinitionAsset>(
		GetTransientPackage(),
		NAME_None,
		RF_Transient);
	if (!InlineDefinition)
	{
		return nullptr;
	}

	if (StaticMeshFragmentActorClass.Get())
	{
		InlineDefinition->DefaultFragmentActorClass = StaticMeshFragmentActorClass;
	}
	else
	{
		InlineDefinition->DefaultFragmentActorClass = AObjectFragmentActor::StaticClass();
	}
	InlineDefinition->Fragments.Reserve(StaticMeshFragments.Num());

	for (int32 Index = 0; Index < StaticMeshFragments.Num(); ++Index)
	{
		UStaticMesh* StaticMesh = StaticMeshFragments[Index];
		if (!StaticMesh)
		{
			continue;
		}

		FObjectFractureFragmentEntry FragmentEntry;
		FragmentEntry.FragmentId = FName(*FString::Printf(TEXT("StaticMeshFragment_%d"), Index));
		FragmentEntry.StaticMeshOverride = StaticMesh;
		FragmentEntry.RelativeTransform = FTransform(StaticMesh->GetBounds().Origin);
		FragmentEntry.MassRatio = static_cast<float>(EstimateStaticMeshRelativeMassWeight(StaticMesh));
		InlineDefinition->Fragments.Add(MoveTemp(FragmentEntry));
	}

	if (!InlineDefinition->HasUsableFragments())
	{
		return nullptr;
	}

	InlineDefinition->bTransferSourceVelocity = bTransferVelocityToSpawnedActor;
	InlineDefinition->bTransferSourceAngularVelocity = bTransferAngularVelocityToSpawnedActor;
	InlineDefinition->bInitializeFragmentHealthFromMass = bInitializeSpawnedActorHealthFromMass;
	InlineDefinition->bPropagateDamagedPenaltyToFragments = bPropagateDamagedPenaltyToSpawnedActor;
	InlineDefinition->OutwardImpulseStrength = SpawnedActorOutwardImpulseStrength;
	InlineDefinition->RandomImpulseStrength = SpawnedActorRandomImpulseStrength;
	InlineDefinition->bHideSourceActorOnFracture = bHideSourceActorOnSpawnedReplacement;
	InlineDefinition->bDisableSourceCollisionOnFracture = bDisableSourceCollisionOnSpawnedReplacement;
	InlineDefinition->bDestroySourceActorAfterFracture = bDestroySourceActorAfterSpawnedReplacement;
	return InlineDefinition;
}

bool UObjectFractureComponent::TriggerSimpleGeometryCollection()
{
	if (!GeometryCollection)
	{
		return false;
	}

	FObjectFractureOption SimpleGeometryCollectionOption;
	SimpleGeometryCollectionOption.OptionType = EObjectFractureOptionType::GeometryCollection;
	SimpleGeometryCollectionOption.GeometryCollectionAsset = GeometryCollection;
	SimpleGeometryCollectionOption.GeometryCollectionActorClass = GeometryCollectionActorClass;
	return TriggerSpawnedReplacement(SimpleGeometryCollectionOption);
}

UObjectFractureDefinitionAsset* UObjectFractureComponent::BuildInlineFragmentDefinition(const FObjectFractureOption& SelectedOption) const
{
	if (SelectedOption.OptionType != EObjectFractureOptionType::InlineFragments
		|| SelectedOption.InlineFragments.Num() == 0)
	{
		return nullptr;
	}

	UObjectFractureDefinitionAsset* InlineDefinition = NewObject<UObjectFractureDefinitionAsset>(
		GetTransientPackage(),
		NAME_None,
		RF_Transient);
	if (!InlineDefinition)
	{
		return nullptr;
	}

	InlineDefinition->DefaultFragmentActorClass = SelectedOption.InlineDefaultFragmentActorClass;
	InlineDefinition->Fragments = SelectedOption.InlineFragments;
	InlineDefinition->bTransferSourceVelocity = bTransferVelocityToSpawnedActor;
	InlineDefinition->bTransferSourceAngularVelocity = bTransferAngularVelocityToSpawnedActor;
	InlineDefinition->bInitializeFragmentHealthFromMass = bInitializeSpawnedActorHealthFromMass;
	InlineDefinition->bPropagateDamagedPenaltyToFragments = bPropagateDamagedPenaltyToSpawnedActor;
	InlineDefinition->OutwardImpulseStrength = SpawnedActorOutwardImpulseStrength;
	InlineDefinition->RandomImpulseStrength = SpawnedActorRandomImpulseStrength;
	InlineDefinition->bHideSourceActorOnFracture = bHideSourceActorOnSpawnedReplacement;
	InlineDefinition->bDisableSourceCollisionOnFracture = bDisableSourceCollisionOnSpawnedReplacement;
	InlineDefinition->bDestroySourceActorAfterFracture = bDestroySourceActorAfterSpawnedReplacement;
	return InlineDefinition;
}

bool UObjectFractureComponent::TriggerFractureDefinition(UObjectFractureDefinitionAsset* SelectedDefinition)
{
	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!SelectedDefinition || !OwnerActor || !World)
	{
		return false;
	}

	FResolvedObjectFractureSourceState SourceState;
	BuildSourceState(
		OwnerActor,
		ResolveSourcePrimitive(),
		ResolveMaterialComponent(),
		ResolveHealthComponent(),
		SourceState);

	TArray<int32> UsableFragmentIndices;
	TArray<double> NormalizedWeights;
	BuildNormalizedFragmentWeights(SelectedDefinition, UsableFragmentIndices, NormalizedWeights);
	if (UsableFragmentIndices.Num() == 0)
	{
		return false;
	}

	TArray<TMap<FName, int32>> SplitResourceQuantitiesScaled;
	SplitResourceQuantitiesExact(SourceState.SourceResourceQuantitiesScaled, NormalizedWeights, SplitResourceQuantitiesScaled);

	DisableSourceActorCollisionForFracture();

	int32 SpawnedFragmentCount = 0;
	for (int32 SpawnIndex = 0; SpawnIndex < UsableFragmentIndices.Num(); ++SpawnIndex)
	{
		const int32 FragmentIndex = UsableFragmentIndices[SpawnIndex];
		if (!SelectedDefinition->Fragments.IsValidIndex(FragmentIndex))
		{
			continue;
		}

		const FObjectFractureFragmentEntry& FragmentEntry = SelectedDefinition->Fragments[FragmentIndex];
		TSubclassOf<AActor> FragmentActorClass = FragmentEntry.FragmentActorClass.Get()
			? FragmentEntry.FragmentActorClass
			: SelectedDefinition->DefaultFragmentActorClass;
		if (!FragmentActorClass.Get())
		{
			continue;
		}

		const FTransform SpawnTransform = FragmentEntry.RelativeTransform * SourceState.SourceTransform;
		AActor* SpawnedFragment = World->SpawnActorDeferred<AActor>(
			FragmentActorClass,
			SpawnTransform,
			OwnerActor,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!SpawnedFragment)
		{
			continue;
		}

		ApplyStaticMeshOverride(SpawnedFragment, FragmentEntry.StaticMeshOverride);

		if (UMaterialComponent* FragmentMaterialComponent = SpawnedFragment->FindComponentByClass<UMaterialComponent>())
		{
			const float FragmentWeight = static_cast<float>(NormalizedWeights[SpawnIndex]);
			const float FragmentMaterialGlobalMultiplier = FMath::Max(KINDA_SMALL_NUMBER, FragmentMaterialComponent->GetResolvedGlobalMassMultiplier());
			const float FragmentBaseMassBeforeMultiplierKg = SourceState.SourceMaterialComponent
				? (SourceState.SourceBaseMassBeforeMultiplierKg * FragmentWeight)
				: ((SourceState.SourceCurrentMassKg * FragmentWeight) / FragmentMaterialGlobalMultiplier);
			FragmentMaterialComponent->SetExplicitBaseMassKgOverride(FragmentBaseMassBeforeMultiplierKg);

			const TMap<FName, int32>* FragmentResourcesScaled = SplitResourceQuantitiesScaled.IsValidIndex(SpawnIndex)
				? &SplitResourceQuantitiesScaled[SpawnIndex]
				: nullptr;
			if (FragmentResourcesScaled && FragmentResourcesScaled->Num() > 0)
			{
				FragmentMaterialComponent->ConfigureResourcesById(*FragmentResourcesScaled);
			}
			else
			{
				FragmentMaterialComponent->ClearConfiguredResources();
			}
		}
		else if (UPrimitiveComponent* DeferredFragmentPrimitive = ResolveBestPrimitiveOnActor(SpawnedFragment))
		{
			const float TargetFragmentMassKg = FMath::Max(0.01f, SourceState.SourceCurrentMassKg * static_cast<float>(NormalizedWeights[SpawnIndex]));
			DeferredFragmentPrimitive->SetMassOverrideInKg(NAME_None, TargetFragmentMassKg, true);
		}

		if (UObjectHealthComponent* FragmentHealthComponent = SpawnedFragment->FindComponentByClass<UObjectHealthComponent>())
		{
			FragmentHealthComponent->bHealthEnabled = true;

			if (SelectedDefinition->bPropagateDamagedPenaltyToFragments)
			{
				FragmentHealthComponent->SetInheritedDamagedPenaltyPercent(SourceState.InheritedDamagedPenaltyPercent);
			}

			if (SelectedDefinition->bInitializeFragmentHealthFromMass)
			{
				FragmentHealthComponent->InitializationMode = EObjectHealthInitializationMode::FromCurrentMass;
				FragmentHealthComponent->bAutoInitializeOnBeginPlay = true;
				FragmentHealthComponent->bDeferMassInitializationToNextTick = true;
			}
		}

		if (UObjectFractureComponent* FragmentFractureComponent = SpawnedFragment->FindComponentByClass<UObjectFractureComponent>())
		{
			if (FragmentEntry.NextFractureDefinition)
			{
				FragmentFractureComponent->bFractureEnabled = true;
				FragmentFractureComponent->FractureDefinition = FragmentEntry.NextFractureDefinition;
			}
		}

		UGameplayStatics::FinishSpawningActor(SpawnedFragment, SpawnTransform);

		if (UPrimitiveComponent* FragmentPrimitive = ResolveBestPrimitiveOnActor(SpawnedFragment))
		{
			if (SelectedDefinition->bTransferSourceVelocity)
			{
				FragmentPrimitive->SetPhysicsLinearVelocity(SourceState.SourceLinearVelocity);
			}

			if (SelectedDefinition->bTransferSourceAngularVelocity)
			{
				FragmentPrimitive->SetPhysicsAngularVelocityInDegrees(SourceState.SourceAngularVelocityDeg);
			}

			FVector ImpulseToApply = FVector::ZeroVector;
			if (SelectedDefinition->OutwardImpulseStrength > KINDA_SMALL_NUMBER)
			{
				const FVector OutwardDirection = (SpawnedFragment->GetActorLocation() - SourceState.SourceLocation).GetSafeNormal();
				ImpulseToApply += OutwardDirection * SelectedDefinition->OutwardImpulseStrength;
			}

			if (SelectedDefinition->RandomImpulseStrength > KINDA_SMALL_NUMBER)
			{
				ImpulseToApply += FMath::VRand() * SelectedDefinition->RandomImpulseStrength;
			}

			if (!FragmentEntry.AdditionalImpulse.IsNearlyZero())
			{
				ImpulseToApply += SourceState.SourceTransform.TransformVectorNoScale(FragmentEntry.AdditionalImpulse);
			}

			if (FragmentEntry.RandomImpulseMagnitude > KINDA_SMALL_NUMBER)
			{
				ImpulseToApply += FMath::VRand() * FragmentEntry.RandomImpulseMagnitude;
			}

			if (!ImpulseToApply.IsNearlyZero())
			{
				FragmentPrimitive->AddImpulse(ImpulseToApply, NAME_None, true);
			}
		}

		++SpawnedFragmentCount;
	}

	if (SpawnedFragmentCount <= 0)
	{
		return false;
	}

	bHasFractured = true;
	OnFractured.Broadcast(SpawnedFragmentCount);
	ApplySourceActorPostFracture(
		SelectedDefinition->bDisableSourceCollisionOnFracture,
		SelectedDefinition->bHideSourceActorOnFracture,
		SelectedDefinition->bDestroySourceActorAfterFracture);
	return true;
}

bool UObjectFractureComponent::TriggerSpawnedReplacement(const FObjectFractureOption& SelectedOption)
{
	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!OwnerActor || !World)
	{
		return false;
	}

	TSubclassOf<AActor> ReplacementActorClass;
	switch (SelectedOption.OptionType)
	{
	case EObjectFractureOptionType::SpawnActor:
		ReplacementActorClass = SelectedOption.ReplacementActorClass;
		break;

	case EObjectFractureOptionType::GeometryCollection:
		if (SelectedOption.GeometryCollectionActorClass.Get())
		{
			ReplacementActorClass = SelectedOption.GeometryCollectionActorClass;
		}
		else
		{
			ReplacementActorClass = AObjectGeometryCollectionActor::StaticClass();
		}
		break;

	default:
		return false;
	}

	if (!ReplacementActorClass.Get())
	{
		return false;
	}

	FResolvedObjectFractureSourceState SourceState;
	BuildSourceState(
		OwnerActor,
		ResolveSourcePrimitive(),
		ResolveMaterialComponent(),
		ResolveHealthComponent(),
		SourceState);

	DisableSourceActorCollisionForFracture();

	const FTransform SpawnTransform = SelectedOption.RelativeTransform * SourceState.SourceTransform;
	AActor* SpawnedActor = World->SpawnActorDeferred<AActor>(
		ReplacementActorClass,
		SpawnTransform,
		OwnerActor,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!SpawnedActor)
	{
		return false;
	}

	if (SelectedOption.OptionType == EObjectFractureOptionType::GeometryCollection)
	{
		ApplyGeometryCollectionOverride(SpawnedActor, SelectedOption.GeometryCollectionAsset);
	}

	ApplySpawnedActorState(
		SpawnedActor,
		SourceState.SourceResourceQuantitiesScaled,
		SourceState.SourceCurrentMassKg,
		SourceState.SourceBaseMassBeforeMultiplierKg,
		SourceState.InheritedDamagedPenaltyPercent);

	UGameplayStatics::FinishSpawningActor(SpawnedActor, SpawnTransform);
	ApplySpawnedActorVelocityAndImpulses(
		SpawnedActor,
		SourceState.SourceLinearVelocity,
		SourceState.SourceAngularVelocityDeg,
		SourceState.SourceLocation,
		SelectedOption);

	bHasFractured = true;
	OnFractured.Broadcast(1);
	ApplySourceActorPostFracture(
		bDisableSourceCollisionOnSpawnedReplacement,
		bHideSourceActorOnSpawnedReplacement,
		bDestroySourceActorAfterSpawnedReplacement);
	return true;
}

void UObjectFractureComponent::HandleOwnerHealthDepleted()
{
	TriggerFracture();
}

UObjectHealthComponent* UObjectFractureComponent::ResolveHealthComponent() const
{
	return UObjectHealthComponent::FindObjectHealthComponent(GetOwner());
}

UPrimitiveComponent* UObjectFractureComponent::ResolveSourcePrimitive() const
{
	return ResolveBestPrimitiveOnActor(GetOwner());
}

UMaterialComponent* UObjectFractureComponent::ResolveMaterialComponent() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UMaterialComponent>() : nullptr;
}

UGeometryCollectionComponent* UObjectFractureComponent::ResolveGeometryCollectionComponent(AActor* Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}

	if (UGeometryCollectionComponent* RootGeometryCollection = Cast<UGeometryCollectionComponent>(Actor->GetRootComponent()))
	{
		return RootGeometryCollection;
	}

	TArray<UGeometryCollectionComponent*> GeometryCollectionComponents;
	Actor->GetComponents<UGeometryCollectionComponent>(GeometryCollectionComponents);
	for (UGeometryCollectionComponent* GeometryCollectionComponent : GeometryCollectionComponents)
	{
		if (GeometryCollectionComponent)
		{
			return GeometryCollectionComponent;
		}
	}

	return nullptr;
}

void UObjectFractureComponent::ApplyStaticMeshOverride(AActor* SpawnedActor, UStaticMesh* StaticMeshOverride) const
{
	if (!SpawnedActor || !StaticMeshOverride)
	{
		return;
	}

	TArray<UStaticMeshComponent*> StaticMeshComponents;
	SpawnedActor->GetComponents<UStaticMeshComponent>(StaticMeshComponents);
	for (UStaticMeshComponent* StaticMeshComponent : StaticMeshComponents)
	{
		if (!StaticMeshComponent)
		{
			continue;
		}

		StaticMeshComponent->SetStaticMesh(StaticMeshOverride);
		break;
	}

	if (AObjectFragmentActor* FragmentActor = Cast<AObjectFragmentActor>(SpawnedActor))
	{
		FragmentActor->RefreshPhysicsProxyFromItemMesh();
	}
}

void UObjectFractureComponent::ApplyGeometryCollectionOverride(AActor* SpawnedActor, UGeometryCollection* GeometryCollectionAsset) const
{
	if (!SpawnedActor || !GeometryCollectionAsset)
	{
		return;
	}

	if (AObjectGeometryCollectionActor* GeometryCollectionActor = Cast<AObjectGeometryCollectionActor>(SpawnedActor))
	{
		GeometryCollectionActor->SetGeometryCollectionAsset(GeometryCollectionAsset);
		return;
	}

	if (UGeometryCollectionComponent* GeometryCollectionComponent = ResolveGeometryCollectionComponent(SpawnedActor))
	{
		GeometryCollectionComponent->SetRestCollection(GeometryCollectionAsset, true);
	}
}

void UObjectFractureComponent::ApplySpawnedActorState(
	AActor* SpawnedActor,
	const TMap<FName, int32>& SourceResourceQuantitiesScaled,
	float SourceCurrentMassKg,
	float SourceBaseMassBeforeMultiplierKg,
	float InheritedDamagedPenaltyPercent) const
{
	if (!SpawnedActor)
	{
		return;
	}

	if (UMaterialComponent* SpawnedMaterialComponent = SpawnedActor->FindComponentByClass<UMaterialComponent>())
	{
		const float SpawnedMaterialGlobalMultiplier = FMath::Max(KINDA_SMALL_NUMBER, SpawnedMaterialComponent->GetResolvedGlobalMassMultiplier());
		const float ResolvedBaseMassBeforeMultiplierKg = SourceBaseMassBeforeMultiplierKg > KINDA_SMALL_NUMBER
			? SourceBaseMassBeforeMultiplierKg
			: (SourceCurrentMassKg / SpawnedMaterialGlobalMultiplier);
		SpawnedMaterialComponent->SetExplicitBaseMassKgOverride(ResolvedBaseMassBeforeMultiplierKg);

		if (SourceResourceQuantitiesScaled.Num() > 0)
		{
			SpawnedMaterialComponent->ConfigureResourcesById(SourceResourceQuantitiesScaled);
		}
		else
		{
			SpawnedMaterialComponent->ClearConfiguredResources();
		}
	}
	else if (UPrimitiveComponent* SpawnedPrimitive = ResolveBestPrimitiveOnActor(SpawnedActor))
	{
		SpawnedPrimitive->SetMassOverrideInKg(NAME_None, FMath::Max(0.01f, SourceCurrentMassKg), true);
	}

	if (UObjectHealthComponent* SpawnedHealthComponent = SpawnedActor->FindComponentByClass<UObjectHealthComponent>())
	{
		SpawnedHealthComponent->bHealthEnabled = true;

		if (bPropagateDamagedPenaltyToSpawnedActor)
		{
			SpawnedHealthComponent->SetInheritedDamagedPenaltyPercent(InheritedDamagedPenaltyPercent);
		}

		if (bInitializeSpawnedActorHealthFromMass)
		{
			SpawnedHealthComponent->InitializationMode = EObjectHealthInitializationMode::FromCurrentMass;
			SpawnedHealthComponent->bAutoInitializeOnBeginPlay = true;
			SpawnedHealthComponent->bDeferMassInitializationToNextTick = true;
		}
	}
}

void UObjectFractureComponent::ApplySpawnedActorVelocityAndImpulses(
	AActor* SpawnedActor,
	const FVector& SourceLinearVelocity,
	const FVector& SourceAngularVelocityDeg,
	const FVector& SourceLocation,
	const FObjectFractureOption& SelectedOption) const
{
	UPrimitiveComponent* SpawnedPrimitive = ResolveBestPrimitiveOnActor(SpawnedActor);
	if (!SpawnedPrimitive)
	{
		return;
	}

	if (bTransferVelocityToSpawnedActor)
	{
		SpawnedPrimitive->SetPhysicsLinearVelocity(SourceLinearVelocity);
	}

	if (bTransferAngularVelocityToSpawnedActor)
	{
		SpawnedPrimitive->SetPhysicsAngularVelocityInDegrees(SourceAngularVelocityDeg);
	}

	FVector ImpulseToApply = FVector::ZeroVector;
	if (SpawnedActorOutwardImpulseStrength > KINDA_SMALL_NUMBER)
	{
		const FVector OutwardDirection = (SpawnedActor->GetActorLocation() - SourceLocation).GetSafeNormal();
		ImpulseToApply += OutwardDirection * SpawnedActorOutwardImpulseStrength;
	}

	if (SpawnedActorRandomImpulseStrength > KINDA_SMALL_NUMBER)
	{
		ImpulseToApply += FMath::VRand() * SpawnedActorRandomImpulseStrength;
	}

	if (!SelectedOption.AdditionalImpulse.IsNearlyZero())
	{
		ImpulseToApply += SelectedOption.RelativeTransform.TransformVectorNoScale(SelectedOption.AdditionalImpulse);
	}

	if (SelectedOption.RandomImpulseMagnitude > KINDA_SMALL_NUMBER)
	{
		ImpulseToApply += FMath::VRand() * SelectedOption.RandomImpulseMagnitude;
	}

	if (!ImpulseToApply.IsNearlyZero())
	{
		SpawnedPrimitive->AddImpulse(ImpulseToApply, NAME_None, true);
	}
}

void UObjectFractureComponent::DisableSourceActorCollisionForFracture() const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	OwnerActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent)
		{
			continue;
		}

		PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PrimitiveComponent->SetSimulatePhysics(false);
	}
}

void UObjectFractureComponent::ApplySourceActorPostFracture(bool bDisableCollision, bool bHideActor, bool bDestroyActor) const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	if (bDisableCollision)
	{
		TArray<UPrimitiveComponent*> PrimitiveComponents;
		OwnerActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (PrimitiveComponent)
			{
				PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				PrimitiveComponent->SetSimulatePhysics(false);
			}
		}
	}

	if (bHideActor)
	{
		OwnerActor->SetActorHiddenInGame(true);
	}

	if (bDestroyActor)
	{
		OwnerActor->Destroy();
	}
}
