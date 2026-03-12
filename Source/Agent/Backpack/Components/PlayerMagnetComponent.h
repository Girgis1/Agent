// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Curves/CurveFloat.h"
#include "PlayerMagnetComponent.generated.h"

class ACharacter;
class USceneComponent;
class USkeletalMeshComponent;
class UShapeComponent;

UCLASS(ClassGroup=(Backpack), meta=(BlueprintSpawnableComponent))
class AGENT_API UPlayerMagnetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPlayerMagnetComponent();

	/** Prefer attaching backpack to this mesh socket (for example: "Backpack Socket"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Attach")
	bool bUseBackpackSocket = true;

	/** Socket used for backpack magnetic target while equipped. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Attach", meta=(EditCondition="bUseBackpackSocket"))
	FName BackpackSocketName = TEXT("Backpack Socket");

	/** Optional component tag used as alternate attach target when socket mode is disabled or unavailable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Attach")
	FName AttachComponentTag = TEXT("PlayerMagnet");

	/** Optional actor tag used when the magnetic anchor lives on a child actor (for example BP_PlayerMagnet). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Attach")
	FName AttachActorTag = TEXT("PlayerMagnet");

	/** Optional actor/class/component name hint when attach tags are not configured (for example "PlayerMagnet"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Attach")
	FString AttachActorNameHint = TEXT("PlayerMagnet");

	/** Global pull strength multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Recall", meta=(ClampMin="0.1", UIMin="0.1"))
	float MagnetStrengthMultiplier = 1.0f;

	/** Pull speed cap in cm/s when strength is full. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Recall", meta=(ClampMin="100.0", UIMin="100.0"))
	float PullMaxSpeed = 2500.0f;

	/** How quickly recall velocity steers toward desired velocity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Recall", meta=(ClampMin="0.1", UIMin="0.1"))
	float PullVelocityInterpSpeed = 14.0f;

	/** How quickly backpack rotates toward magnet target while recalling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Recall", meta=(ClampMin="0.1", UIMin="0.1"))
	float RotationInterpSpeed = 10.0f;

	/** Strength floor at max range before curve is applied. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Recall", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float StrengthAtMaxDistance = 0.2f;

	/** X: 0=far, 1=close. Y: 0..1 strength blend. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Recall")
	FRuntimeFloatCurve StrengthByDistanceCurve;

	/** Field range where nearby magnetic objects get weak attraction/noise while Hold-B is active. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Field", meta=(ClampMin="1.0", UIMin="1.0"))
	float AttractRange = 200.0f;

	/** Scan interval for overlap-based candidate collection while magnet is active. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Field", meta=(ClampMin="0.01", UIMin="0.01"))
	float CandidateScanInterval = 0.08f;

	/** Time in seconds before unseen candidates are forgotten. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Field", meta=(ClampMin="0.01", UIMin="0.01"))
	float CandidateForgetDelay = 0.35f;

	/** Limits per-frame force processing cost in heavy chaos scenarios. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Field", meta=(ClampMin="1", UIMin="1"))
	int32 MaxAffectedItems = 24;

	/** Closest object inside this range is captured toward the backpack magnet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Field", meta=(ClampMin="1.0", UIMin="1.0"))
	float CaptureRange = 100.0f;

	/** Shapes distance response (higher values pull harder only when close). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Field", meta=(ClampMin="0.1", UIMin="0.1"))
	float DistanceStrengthExponent = 1.35f;

	/** Minimum attraction speed in cm/s when object is near the edge of the magnetic field. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Field", meta=(ClampMin="0.0", UIMin="0.0"))
	float MinPullSpeed = 140.0f;

	/** Maximum attraction speed in cm/s for nearby objects. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Field", meta=(ClampMin="0.0", UIMin="0.0"))
	float MaxPullSpeed = 1900.0f;

	/** Upward lift added while attracting to create the rising magnetic feel. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Field", meta=(ClampMin="0.0", UIMin="0.0"))
	float LiftSpeed = 160.0f;

	/** Velocity error gain for attraction force solving. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Field", meta=(ClampMin="0.0", UIMin="0.0"))
	float VelocityGain = 10.0f;

	/** Hard force clamp per kg to keep chaotic piles from exploding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Field", meta=(ClampMin="0.0", UIMin="0.0"))
	float MaxForcePerKg = 2600.0f;

	/** Pull scale applied in field phase before capture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Field", meta=(ClampMin="0.0", UIMin="0.0"))
	float FieldPullScale = 0.35f;

	/** Scale applied to pull while item is in-air/unlocked (1.0 = unchanged, 0.5 = 50% slower). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Field", meta=(ClampMin="0.0", UIMin="0.0", ClampMax="1.0", UIMax="1.0"))
	float InAirPullStrengthScale = 0.5f;

	/** Tangential linear noise in field phase for unstable magnetic wobble. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Field", meta=(ClampMin="0.0", UIMin="0.0"))
	float FieldLinearNoiseScale = 0.2f;

	/** Angular noise (deg/s) in field phase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Field", meta=(ClampMin="0.0", UIMin="0.0"))
	float FieldAngularNoiseDegPerSec = 90.0f;

	/** Noise frequency used for field wobble. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Field", meta=(ClampMin="0.0", UIMin="0.0"))
	float FieldNoiseFrequency = 3.5f;

	/** Distance that counts as magnet-held/attached. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Hold", meta=(ClampMin="0.1", UIMin="0.1"))
	float AttachDistance = 10.0f;

	/** Spring gain for MagLock hold force (keeps physics active while holding). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Hold", meta=(ClampMin="0.0", UIMin="0.0"))
	float HoldSpringStrength = 850.0f;

	/** Damping applied while item is MagLock-held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Hold", meta=(ClampMin="0.0", UIMin="0.0"))
	float HoldDamping = 70.0f;

	/** Hard MagLock force clamp per kg. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Hold", meta=(ClampMin="0.0", UIMin="0.0"))
	float MaxHoldForcePerKg = 4200.0f;

	/** Delay after release before items can re-enter MagLock. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Hold", meta=(ClampMin="0.0", UIMin="0.0"))
	float LockReacquireDelay = 0.15f;

	/** Keep MagLock'd items held after entering the zone, even if they drift outside briefly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Hold")
	bool bStickyLockInZone = true;

	/** Torque gain for orienting item attach points toward the magnet anchor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Hold", meta=(ClampMin="0.0", UIMin="0.0"))
	float AlignTorqueGain = 16.0f;

	/** Angular damping used during magnet alignment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Hold", meta=(ClampMin="0.0", UIMin="0.0"))
	float AlignAngularDamping = 2.5f;

	/** Hard alignment torque clamp per kg. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Hold", meta=(ClampMin="0.0", UIMin="0.0"))
	float MaxTorquePerKg = 2200.0f;

	/** Optional component tag used to resolve a MagLock zone shape on the player magnet actor hierarchy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Hold")
	FName MagLockZoneComponentTag = TEXT("MagLockZone");

	/** Optional component tag used to resolve a dedicated MagLock anchor scene component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Hold")
	FName MagLockAnchorComponentTag = TEXT("MagLockAnchor");

	/** When no dedicated MagLock anchor is found, use the normal attach target as lock anchor. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Hold")
	bool bUseAttachTargetAsMagLockAnchor = true;

	/** Freeze recall physics once backpack is close enough. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Hold")
	bool bFreezePhysicsWhenClose = true;

	/** Distance where freeze engages. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Hold", meta=(ClampMin="0.1", UIMin="0.1", EditCondition="bFreezePhysicsWhenClose"))
	float FreezeDistance = 8.0f;

	/** Distance where a held backpack detaches from magnet. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Hold", meta=(ClampMin="1.0", UIMin="1.0"))
	float DetachDistance = 35.0f;

	/** Grace time before detaching once beyond DetachDistance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Hold", meta=(ClampMin="0.0", UIMin="0.0"))
	float DetachGraceTime = 0.08f;

	/** Release a frozen backpack when player impact impulse exceeds threshold. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Hold")
	bool bReleaseFrozenOnImpact = true;

	/** Minimum impact impulse to release frozen backpack. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Hold", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bReleaseFrozenOnImpact"))
	float ReleaseImpactImpulse = 30000.0f;

	/** Keep third-person camera from retracting due to backpack collision while magnet-held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Collision")
	bool bIgnoreCameraCollisionWhenMagnetHeld = true;

	/** Whether backpack blocks pawns while magnet-held/recalling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Collision")
	bool bBlockPawnCollisionWhenMagnetHeld = true;

	/** Includes actors with BackpackMagnetComponent in magnetic eligibility. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Filter")
	bool bAllowBackpackMagnetItems = true;

	/** Includes actors/components with any configured magnetic tag in magnetic eligibility. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Filter")
	bool bAllowTaggedMagneticItems = true;

	/** Includes actors with MaterialComponent entries matching MagneticMaterialIds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Filter")
	bool bAllowMaterialMagneticItems = true;

	/** Actor tags treated as magnetic (for example "Magnetic", "BackpackItem"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Filter")
	TArray<FName> MagneticActorTags;

	/** Primitive component tags treated as magnetic. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Filter")
	TArray<FName> MagneticComponentTags;

	/** Material ids treated as magnetic (for example "Iron"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PlayerMagnet|Filter")
	TArray<FName> MagneticMaterialIds;

	UFUNCTION(BlueprintPure, Category="PlayerMagnet|Recall")
	float ComputeDistanceStrength(float DistanceToTarget, float MaxDistance) const;

	UFUNCTION(BlueprintCallable, Category="PlayerMagnet|Attach")
	bool ResolveAttachTarget(USceneComponent*& OutAttachParent, FName& OutAttachSocketName) const;

	UFUNCTION(BlueprintCallable, Category="PlayerMagnet|Hold")
	bool ResolveMagLockAnchor(USceneComponent*& OutAnchorComponent, FName& OutAnchorSocketName) const;

	UFUNCTION(BlueprintCallable, Category="PlayerMagnet|Hold")
	UShapeComponent* ResolveMagLockZoneComponent() const;

	UFUNCTION(BlueprintPure, Category="PlayerMagnet|Filter")
	bool IsMaterialIdMagnetic(FName MaterialId) const;

protected:
	void GatherOwnerAndAttachedActors(TArray<const AActor*>& OutActors) const;
	bool ResolveTaggedSceneComponent(FName ComponentTag, USceneComponent*& OutSceneComponent) const;
	bool ResolveTaggedShapeComponent(FName ComponentTag, UShapeComponent*& OutShapeComponent) const;
	USkeletalMeshComponent* ResolveOwnerMesh() const;
};
