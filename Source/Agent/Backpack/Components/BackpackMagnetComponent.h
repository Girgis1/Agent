// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Curves/CurveFloat.h"
#include "BackpackMagnetComponent.generated.h"

UCLASS(ClassGroup=(Backpack), meta=(BlueprintSpawnableComponent))
class AGENT_API UBackpackMagnetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBackpackMagnetComponent();

	/** Global pull strength multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Backpack|Magnet", meta=(ClampMin="0.1", UIMin="0.1"))
	float MagnetStrengthMultiplier = 1.0f;

	/** Pull speed cap in cm/s when strength is at full value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Backpack|Magnet", meta=(ClampMin="100.0", UIMin="100.0"))
	float PullMaxSpeed = 2500.0f;

	/** How quickly velocity steers toward desired pull velocity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Backpack|Magnet", meta=(ClampMin="0.1", UIMin="0.1"))
	float PullVelocityInterpSpeed = 14.0f;

	/** How quickly the actor rotates toward target while recalling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Backpack|Magnet", meta=(ClampMin="0.1", UIMin="0.1"))
	float RotationInterpSpeed = 10.0f;

	/** Distance that counts as attached/held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Backpack|Magnet", meta=(ClampMin="0.1", UIMin="0.1"))
	float AttachDistance = 10.0f;

	/** Distance at which an attached item detaches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Backpack|Magnet", meta=(ClampMin="1.0", UIMin="1.0"))
	float DetachDistance = 35.0f;

	/** Brief grace time before detaching to avoid jitter. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Backpack|Magnet", meta=(ClampMin="0.0", UIMin="0.0"))
	float DetachGraceTime = 0.08f;

	/** Strength at max recall range before the curve is applied. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Backpack|Magnet", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float StrengthAtMaxDistance = 0.2f;

	/** X: 0=far, 1=close. Y: 0..1 strength blend. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Backpack|Magnet")
	FRuntimeFloatCurve StrengthByDistanceCurve;

	/** Keep camera from colliding with magnet-held actor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Backpack|Magnet|Collision")
	bool bIgnoreCameraCollisionWhenMagnetHeld = true;

	/** Allow the recalled actor to collide with pawns while magnet-held/recalling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Backpack|Magnet|Collision")
	bool bBlockPawnCollisionWhenMagnetHeld = true;

	UFUNCTION(BlueprintPure, Category="Backpack|Magnet")
	float ComputeDistanceStrength(float DistanceToTarget, float MaxDistance) const;
};

