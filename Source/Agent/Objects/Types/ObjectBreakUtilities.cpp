// Copyright Epic Games, Inc. All Rights Reserved.

#include "Objects/Types/ObjectBreakUtilities.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "Material/MaterialComponent.h"
#include "Objects/Components/ObjectHealthComponent.h"
#include "PhysicsEngine/BodyInstance.h"

namespace AgentObjectBreak
{
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

float ResolvePrimitiveMassKgWithoutWarning(const UPrimitiveComponent* PrimitiveComponent)
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
	FResolvedObjectBreakSourceState& OutState)
{
	OutState = FResolvedObjectBreakSourceState();
	OutState.OwnerActor = OwnerActor;
	OutState.SourcePrimitive = SourcePrimitive;
	OutState.SourceMaterialComponent = SourceMaterialComponent;
	OutState.SourceHealthComponent = SourceHealthComponent;
	OutState.SourceLocation = OwnerActor ? OwnerActor->GetActorLocation() : FVector::ZeroVector;
	OutState.SourcePrimitiveLocation = SourcePrimitive ? SourcePrimitive->GetComponentLocation() : OutState.SourceLocation;
	OutState.SourceTransform = OwnerActor ? OwnerActor->GetActorTransform() : FTransform::Identity;
	OutState.SourcePrimitiveTransform = SourcePrimitive ? SourcePrimitive->GetComponentTransform() : OutState.SourceTransform;
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

	OutState.SourceCurrentMassKg = ResolvePrimitiveMassKgWithoutWarning(SourcePrimitive);
	OutState.SourceBaseContributionKg = FMath::Max(0.0f, OutState.SourceCurrentMassKg - OutState.SourceMaterialWeightKg);
	OutState.SourceBaseMassBeforeMultiplierKg = SourceMaterialComponent
		? (OutState.SourceGlobalMassMultiplier > KINDA_SMALL_NUMBER ? (OutState.SourceBaseContributionKg / OutState.SourceGlobalMassMultiplier) : 0.0f)
		: 0.0f;
}
}
