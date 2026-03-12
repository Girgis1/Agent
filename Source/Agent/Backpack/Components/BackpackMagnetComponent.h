// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Curves/CurveFloat.h"
#include "BackpackMagnetComponent.generated.h"

class USceneComponent;

UCLASS(ClassGroup=(Backpack), meta=(BlueprintSpawnableComponent))
class AGENT_API UBackpackMagnetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBackpackMagnetComponent();

	/** Allows this actor to be influenced by the player backpack magnet field. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Backpack|Magnet")
	bool bEligibleForPlayerMagnet = true;

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

	/** Optional socket on the backpack mesh for this module when snapped (for example: "Backpack Socket"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Backpack|Magnet|Attach")
	FName BackpackSocketName = TEXT("Backpack Socket");

	/** Optional component tag used to find this object's attach point for snapping. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Backpack|Magnet|Attach")
	FName AttachPointComponentTag = TEXT("BackpackAttachPoint");

	/** Optional name hint used to find this object's attach point when no tag is set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Backpack|Magnet|Attach")
	FString AttachPointNameHint = TEXT("BackpackAttachPoint");

	/** Optional socket on the object's attach-point component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Backpack|Magnet|Attach")
	FName AttachPointSocketName = NAME_None;

	UFUNCTION(BlueprintPure, Category="Backpack|Magnet")
	float ComputeDistanceStrength(float DistanceToTarget, float MaxDistance) const;

	UFUNCTION(BlueprintCallable, Category="Backpack|Magnet|Attach")
	bool ResolveAttachPoint(USceneComponent*& OutAttachPointComponent, FName& OutAttachSocketName) const;

	UFUNCTION(BlueprintPure, Category="Backpack|Magnet|Attach")
	FTransform GetAttachPointWorldTransform() const;
};
