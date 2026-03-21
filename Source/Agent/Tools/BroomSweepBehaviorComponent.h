// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/ToolBehaviorComponent.h"
#include "BroomSweepBehaviorComponent.generated.h"

class UBroomToolDefinition;

UCLASS(ClassGroup=(Tools), meta=(BlueprintSpawnableComponent))
class AGENT_API UBroomSweepBehaviorComponent : public UToolBehaviorComponent
{
	GENERATED_BODY()

public:
	virtual void OnToolEquipped(UToolSystemComponent* InToolSystem, UToolWorldComponent* InToolWorld) override;
	virtual void OnToolDropped() override;
	virtual void TickEquipped(float DeltaTime) override;

private:
	const UBroomToolDefinition* ResolveDefinition() const;
	bool ResolvePivotAndView(FVector& OutPivotLocation, FRotator& OutViewRotation) const;
	void TickPushInteraction(const UBroomToolDefinition& Definition, const FVector& CurrentHeadLocation, bool bCanPush, float DeltaTime);
	void ResetRuntimeState();

	FVector LocalGripPosition = FVector::ZeroVector;
	FVector LocalHeadPosition = FVector::ZeroVector;
	FVector LocalGripToHeadDirection = FVector::ForwardVector;
	float LocalGripToHeadLength = 100.0f;

	FVector SolvedHeadWorld = FVector::ZeroVector;
	FVector PreviousHeadWorld = FVector::ZeroVector;
	FVector PreviousPushHeadWorld = FVector::ZeroVector;
	bool bHasSolvedHead = false;
	bool bHasPreviousHead = false;
	bool bHasPreviousPushHead = false;
	bool bContactOnGround = false;
	float PushApplyAccumulator = 0.0f;
	float CatchSoftTimeRemaining = 0.0f;
};

