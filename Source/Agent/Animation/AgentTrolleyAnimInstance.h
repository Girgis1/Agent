// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "AgentTrolleyAnimInstance.generated.h"

class AAgentCharacter;

UCLASS(BlueprintType, Blueprintable, Transient)
class AGENT_API UAgentTrolleyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	UPROPERTY(BlueprintReadOnly, Category="Trolley|Runtime")
	bool bTrolleyPoseActive = false;

	UPROPERTY(BlueprintReadOnly, Category="Trolley|Runtime")
	bool bTrolleyGripBroken = false;

	UPROPERTY(BlueprintReadOnly, Category="Trolley|Runtime")
	float TrolleyPoseBlendAlpha = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Trolley|Runtime")
	float TrolleyLeftHandIKAlpha = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Trolley|Runtime")
	float TrolleyRightHandIKAlpha = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Trolley|Runtime")
	FVector TrolleyHandleLeftMeshSpace = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="Trolley|Runtime")
	FVector TrolleyHandleRightMeshSpace = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="Trolley|Runtime")
	FVector TrolleyChestTargetMeshSpace = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="Trolley|Runtime")
	FTransform TrolleyHandleLeftWorldTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category="Trolley|Runtime")
	FTransform TrolleyHandleRightWorldTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category="Trolley|Runtime")
	FTransform TrolleyChestTargetWorldTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category="Trolley|Runtime")
	FTransform TrolleyHandleLeftMeshTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category="Trolley|Runtime")
	FTransform TrolleyHandleRightMeshTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category="Trolley|Runtime")
	FTransform TrolleyChestTargetMeshTransform = FTransform::Identity;

	UPROPERTY(BlueprintReadOnly, Category="Trolley|Runtime")
	float TrolleyForwardBlend = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Trolley|Runtime")
	float TrolleyRightBlend = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Trolley|Runtime")
	float TrolleyStrainAlpha = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Trolley|Runtime")
	float TrolleyLeanPitchDeg = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Trolley|Runtime")
	float TrolleyLeanRollDeg = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Trolley|Runtime")
	float TrolleySpeedNormalized = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Trolley|Runtime")
	float TrolleyForwardSpeedLocal = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Trolley|Runtime")
	float TrolleyRightSpeedLocal = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Trolley|Runtime")
	float TrolleyForwardInput = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category="Trolley|Runtime")
	float TrolleyRightInput = 0.0f;

private:
	void RefreshCachedCharacter();
	void ResetRuntimeValues();

	UPROPERTY(Transient)
	TWeakObjectPtr<AAgentCharacter> CachedAgentCharacter;
};
