// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/ToolDefinition.h"
#include "BroomToolDefinition.generated.h"

UCLASS(BlueprintType)
class AGENT_API UBroomToolDefinition : public UToolDefinition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Broom|Pose")
	FVector RestHeadOffset = FVector(145.0f, 12.0f, -56.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Broom|Pose")
	FVector SweepHeadOffset = FVector(168.0f, 12.0f, -64.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Broom|Pose", meta=(ClampMin="0.01", UIMin="0.01"))
	float HeadFollowSpeedIdle = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Broom|Pose", meta=(ClampMin="0.01", UIMin="0.01"))
	float HeadFollowSpeedSweep = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Broom|Pose", meta=(ClampMin="0.01", UIMin="0.01"))
	float HeadFollowSpeedCatch = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Broom|Collision", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float HeadCollisionRadius = 9.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Broom|Collision", meta=(ClampMin="0.0", UIMin="0.0", ClampMax="1.0", UIMax="1.0"))
	float CollisionSlideFraction = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Broom|Ground")
	bool bEnableGroundFollow = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Broom|Ground", meta=(ClampMin="0.0", UIMin="0.0", ClampMax="1.0", UIMax="1.0"))
	float GroundFollowStrength = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Broom|Ground", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float GroundTraceUpHeightCm = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Broom|Ground", meta=(ClampMin="1.0", UIMin="1.0", Units="cm"))
	float GroundTraceDownDepthCm = 130.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Broom|Ground", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float GroundClearanceCm = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Broom|Push", meta=(ClampMin="0.0", UIMin="0.0", Units="s"))
	float PushApplyIntervalSeconds = 0.04f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Broom|Push", meta=(ClampMin="0.0", UIMin="0.0"))
	float PushImpulse = 90000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Broom|Push", meta=(ClampMin="0.0", UIMin="0.0", ClampMax="1.0", UIMax="1.0"))
	float MinPushTransferFraction = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Broom|Push", meta=(ClampMin="0.1", UIMin="0.1"))
	float StrengthToPushExponent = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Broom|Catch", meta=(ClampMin="0.0", UIMin="0.0", Units="s"))
	float CatchSoftDurationSeconds = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Broom|Catch", meta=(ClampMin="0.1", UIMin="0.1"))
	float CatchMassOverStrengthRatio = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Broom|Dirty")
	bool bAllowDirtWhileEquipped = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Broom|Dirty")
	bool bKeepBrushActiveWhenDropped = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Broom|Debug")
	bool bDrawDebug = false;
};

