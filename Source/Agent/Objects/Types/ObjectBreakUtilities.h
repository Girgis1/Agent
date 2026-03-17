// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;
class UMaterialComponent;
class UObjectHealthComponent;
class UPrimitiveComponent;

struct FResolvedObjectBreakSourceState
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
	FVector SourcePrimitiveLocation = FVector::ZeroVector;
	FTransform SourceTransform = FTransform::Identity;
	FTransform SourcePrimitiveTransform = FTransform::Identity;
	float InheritedDamagedPenaltyPercent = 0.0f;
};

namespace AgentObjectBreak
{
	AGENT_API UPrimitiveComponent* ResolveBestPrimitiveOnActor(AActor* Actor);
	AGENT_API float ResolvePrimitiveMassKgWithoutWarning(const UPrimitiveComponent* PrimitiveComponent);
	AGENT_API void SplitResourceQuantitiesExact(
		const TMap<FName, int32>& SourceQuantitiesScaled,
		const TArray<double>& NormalizedWeights,
		TArray<TMap<FName, int32>>& OutSplitQuantitiesScaled);
	AGENT_API void BuildSourceState(
		AActor* OwnerActor,
		UPrimitiveComponent* SourcePrimitive,
		UMaterialComponent* SourceMaterialComponent,
		UObjectHealthComponent* SourceHealthComponent,
		FResolvedObjectBreakSourceState& OutState);
}
