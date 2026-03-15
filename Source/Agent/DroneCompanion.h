// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DroneCompanion.generated.h"

class UCameraComponent;
class UAgentBeamToolComponent;
class UConveyorSurfaceVelocityComponent;
class UDroneBatteryComponent;
class UMaterialInstanceDynamic;
class UPhysicalMaterial;
class UPointLightComponent;
class UPrimitiveComponent;
class USpotLightComponent;
class USphereComponent;
class UStaticMeshComponent;
class USceneComponent;
class AActor;

UENUM(BlueprintType)
enum class EDroneCompanionMode : uint8
{
	ThirdPersonFollow,
	PilotControlled,
	Idle,
	HoldPosition,
	BuddyFollow,
	MapMode,
	MiniMapFollow
};

UENUM(BlueprintType)
enum class EDronePowerState : uint8
{
	Active,
	PoweredOff,
	Depleted
};

UENUM(BlueprintType)
enum class EDroneMobilityState : uint8
{
	Flight,
	PhysicsBody,
	Anchored
};

UENUM(BlueprintType)
enum class EDroneRoleState : uint8
{
	ThirdPersonCamera,
	Pilot,
	Idle,
	Hold,
	Buddy,
	Map,
	MiniMap
};

UENUM(BlueprintType)
enum class EDroneControlState : uint8
{
	Autonomous,
	PlayerControlled,
	ScriptControlled
};

struct FDroneLiftAssistForceTuning
{
	float StrengthValue = 0.0f;
	float StrengthMassScaleKg = 500.0f;
};

UCLASS()
class ADroneCompanion : public AActor
{
	GENERATED_BODY()

public:
	ADroneCompanion();

	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category="Drone")
	void SetFollowTarget(AActor* NewFollowTarget);

	UFUNCTION(BlueprintCallable, Category="Drone")
	void SetCompanionMode(EDroneCompanionMode NewMode);

	UFUNCTION(BlueprintCallable, Category="Drone")
	void SetViewReferenceRotation(const FRotator& NewRotation);

	UFUNCTION(BlueprintCallable, Category="Drone")
	void SetThirdPersonCameraTarget(const FVector& NewLocation, const FRotator& NewRotation);

	UFUNCTION(BlueprintCallable, Category="Drone")
	void SetHoldTransform(const FVector& NewLocation, const FRotator& NewRotation);

	UFUNCTION(BlueprintCallable, Category="Drone")
	void SetPilotInputs(float InThrottleInput, float InYawInput, float InRollInput, float InPitchInput);

	UFUNCTION(BlueprintCallable, Category="Drone")
	void ResetPilotInputs();

	UFUNCTION(BlueprintCallable, Category="Drone")
	void SetUseSimplePilotControls(bool bEnable);

	UFUNCTION(BlueprintCallable, Category="Drone")
	void SetUseFreeFlyPilotControls(bool bEnable);

	UFUNCTION(BlueprintCallable, Category="Drone")
	void SetUseSpectatorPilotControls(bool bEnable);

	UFUNCTION(BlueprintCallable, Category="Drone")
	void SetUseRollPilotControls(bool bEnable);

	UFUNCTION(BlueprintCallable, Category="Drone")
	void SetPilotStabilizerEnabled(bool bEnable);

	UFUNCTION(BlueprintCallable, Category="Drone")
	void TeleportDroneToTransform(const FVector& NewLocation, const FRotator& NewRotation);

	UFUNCTION(BlueprintCallable, Category="Drone")
	void SetPilotHoverModeEnabled(bool bEnable);

	UFUNCTION(BlueprintCallable, Category="Drone")
	bool TryRollJump(float ChargeAlpha = 0.0f);

	UFUNCTION(BlueprintCallable, Category="Drone")
	void SetUseCrashRollRecoveryMode(bool bEnable);

	UFUNCTION(BlueprintCallable, Category="Drone")
	void SetNextMapModeUsesEntryLift(bool bEnable);

	UFUNCTION(BlueprintCallable, Category="Drone")
	void SetSuppressCameraMountRefresh(bool bSuppress);

	UFUNCTION(BlueprintCallable, Category="Drone|Visual")
	void SetHideStaticMeshesFromOwnerCamera(bool bHide);

	UFUNCTION(BlueprintPure, Category="Drone|Visual")
	bool IsHidingStaticMeshesFromOwnerCamera() const { return bHideStaticMeshesFromOwnerCamera; }

	UFUNCTION(BlueprintCallable, Category="Drone|Torch")
	void SetTorchEnabled(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category="Drone|Torch")
	void ToggleTorchEnabled();

	UFUNCTION(BlueprintPure, Category="Drone|Torch")
	bool IsTorchEnabled() const { return bTorchEnabled; }

	UFUNCTION(BlueprintPure, Category="Drone|Torch")
	bool IsTorchRuntimeActive() const { return bTorchRuntimeActive; }

	UFUNCTION(BlueprintCallable, Category="Drone|Torch")
	void SetTorchAimTarget(const FVector& NewAimTargetLocation);

	UFUNCTION(BlueprintCallable, Category="Drone|Torch")
	void ClearTorchAimTarget();

	UFUNCTION(BlueprintCallable, Category="Drone")
	bool StartLiftAssist(UPrimitiveComponent* InTargetComponent, const FVector& InGrabLocation);

	UFUNCTION(BlueprintCallable, Category="Drone")
	void StopLiftAssist();

	void SetLiftAssistForceTuning(const FDroneLiftAssistForceTuning& InTuning);

	UFUNCTION(BlueprintCallable, Category="Drone")
	void StartPilotCameraTransitionFromThirdPerson();

	UFUNCTION(BlueprintCallable, Category="Drone")
	void AdjustCameraTilt(float DeltaDegrees);

	UFUNCTION(BlueprintCallable, Category="Drone")
	bool GetDroneCameraTransform(FVector& OutLocation, FRotator& OutRotation) const;

	UFUNCTION(BlueprintPure, Category="Drone")
	float GetCameraTiltDegrees() const { return CameraTiltDegrees; }

	UFUNCTION(BlueprintPure, Category="Drone")
	EDroneCompanionMode GetCompanionMode() const { return CompanionMode; }

	UFUNCTION(BlueprintPure, Category="Drone")
	bool IsCrashed() const { return bCrashed; }

	UFUNCTION(BlueprintCallable, Category="Drone")
	bool ConsumePendingCrashRollRecovery();

	UFUNCTION(BlueprintPure, Category="Drone")
	bool IsStableForRollRecovery() const;

	UFUNCTION(BlueprintPure, Category="Drone")
	bool IsLiftAssistActive() const { return bLiftAssistActive; }

	UFUNCTION(BlueprintPure, Category="Drone|Battery")
	float GetBatteryPercent() const;

	UFUNCTION(BlueprintCallable, Category="Drone|Battery")
	void SetBatteryPercent(float NewBatteryPercent);

	UFUNCTION(BlueprintCallable, Category="Drone|Battery")
	float ChargeBatteryPercent(float PercentToCharge);

	UFUNCTION(BlueprintPure, Category="Drone|Battery")
	bool IsBatteryDepleted() const;

	UFUNCTION(BlueprintPure, Category="Drone|Battery")
	bool IsBatteryFullyCharged() const;

	UFUNCTION(BlueprintPure, Category="Drone|Battery")
	bool IsDronePoweredOff() const { return bDronePoweredOff; }

	UFUNCTION(BlueprintPure, Category="Drone|Battery")
	bool IsInChargerVolume() const { return bIsInChargerVolume; }

	UFUNCTION(BlueprintPure, Category="Drone|Battery")
	bool IsChargingLockActive() const { return bChargingLockActive; }

	UFUNCTION(BlueprintCallable, Category="Drone|State")
	bool SetManualPowerOff(bool bPowerOff);

	UFUNCTION(BlueprintCallable, Category="Drone|State")
	void ToggleManualPowerOff();

	UFUNCTION(BlueprintPure, Category="Drone|State")
	bool IsManualPowerOffRequested() const { return bManualPowerOffRequested; }

	UFUNCTION(BlueprintPure, Category="Drone|State")
	EDronePowerState GetPowerState() const { return CurrentPowerState; }

	UFUNCTION(BlueprintPure, Category="Drone|State")
	EDroneMobilityState GetMobilityState() const { return CurrentMobilityState; }

	UFUNCTION(BlueprintPure, Category="Drone|State")
	EDroneRoleState GetRoleState() const { return CurrentRoleState; }

	UFUNCTION(BlueprintPure, Category="Drone|State")
	EDroneControlState GetControlState() const { return CurrentControlState; }

	UFUNCTION(BlueprintCallable, Category="Drone|Battery")
	void NotifyEnteredChargerVolume();

	UFUNCTION(BlueprintCallable, Category="Drone|Battery")
	void NotifyExitedChargerVolume();

	UFUNCTION(BlueprintPure, Category="Drone|Pickup")
	UPrimitiveComponent* GetPickupPhysicsComponent() const { return DroneBody; }

	UFUNCTION(BlueprintPure, Category="Drone|Beam")
	USceneComponent* GetBeamOriginComponent() const;

	UFUNCTION(BlueprintPure, Category="Drone|Beam")
	UAgentBeamToolComponent* GetBeamToolComponent() const { return BeamToolComponent; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Beam")
	FName BeamOriginTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Physics")
	float DroneMassKg = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Physics")
	float DroneBodyVisualScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Physics")
	float DroneBodyLinearDamping = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Physics")
	float DroneBodyAngularDamping = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Physics")
	float DroneBodyFriction = 2.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Physics")
	float DroneBodyRestitution = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pickup")
	bool bUsePickupProxySphere = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pickup", meta=(ClampMin="1.0", UIMin="1.0"))
	float PickupProxySphereRadius = 52.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Physics")
	float DroneMaxLinearSpeed = 2600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Physics")
	float DroneMaxAngularSpeed = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float PilotMaxThrustAcceleration = 2600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float PilotPitchRate = 480.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float PilotRollRate = 540.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float PilotYawRate = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float PilotAngularResponse = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float PilotThrottleResponse = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float PilotInputDeadzone = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float PilotUprightAssist = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float PilotStabilizerMaxCorrectionRate = 360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float PilotHoverCollectiveRange = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float PilotHoverMinUpDot = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float PilotHoverSelfRightActivationSpeed = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float PilotHoverSelfRightMinUpDot = -0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float PilotHoverSelfRightRestoreUpDot = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float PilotHoverSelfRightAssist = 7.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float PilotHoverSelfRightMaxCorrectionRate = 540.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float SimpleMaxHorizontalSpeed = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float SimpleMaxVerticalSpeed = 750.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float SimpleVelocityResponse = 1.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float SimpleRotationResponse = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float SimpleYawFollowSpeed = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float SimpleInputExpo = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float SimpleMaxTiltDegrees = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float SimpleBrakingResponse = 2.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float SimpleCollectiveRange = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float SimpleAttitudeHoldRate = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float SimpleCameraTiltAxisSpeed = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float SimpleMaxDistanceFromTarget = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float FreeFlyMaxSpeed = 2200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float FreeFlyAcceleration = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float FreeFlyDeceleration = 4000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float FreeFlyRotationResponse = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float RollDriveAcceleration = 3200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float RollBrakingAcceleration = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float RollJumpMinLaunchVelocity = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float RollJumpMaxLaunchVelocity = 950.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float RollJumpChargeExponent = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float RollJumpCooldown = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float RollGroundCheckDistance = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float RollGroundedMaxClearance = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float RollMaxSpeed = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float RollMaxAngularSpeed = 360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Autopilot")
	FVector ThirdPersonOffset = FVector(-340.0f, 0.0f, 140.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Autopilot")
	float ThirdPersonLookAtHeight = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Autopilot")
	float ThirdPersonPositionGain = 9.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Autopilot")
	float ThirdPersonVelocityDamping = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Autopilot")
	float ThirdPersonYawFollowSpeed = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Autopilot")
	float ThirdPersonCameraPositionGain = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Autopilot")
	float ThirdPersonCameraVelocityDamping = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Autopilot")
	float ThirdPersonCameraRotationResponse = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Autopilot")
	float BuddyForwardOffset = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Autopilot")
	float BuddyHeightOffset = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Autopilot")
	float BuddyMinLateralDistance = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Autopilot")
	float BuddyMaxLateralDistance = 210.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Autopilot")
	float BuddyForwardJitter = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Autopilot")
	float BuddyVerticalJitter = 22.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Autopilot")
	float BuddyDriftUpdateInterval = 1.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Autopilot")
	float BuddyPositionGain = 7.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Autopilot")
	float BuddyVelocityDamping = 4.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Autopilot")
	float BuddyMaxLinearSpeed = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Autopilot")
	float BuddyFacingBlend = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Autopilot")
	float AutopilotAngularResponse = 5.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|LiftAssist")
	FVector LiftAssistCarryLocalOffset = FVector(80.0f, -140.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|LiftAssist")
	float LiftAssistApproachHeight = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|LiftAssist")
	float LiftAssistDesiredClearance = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|LiftAssist")
	float LiftAssistFollowRange = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|LiftAssist")
	float LiftAssistLatchDistance = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|LiftAssist")
	float LiftAssistRopeLength = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|LiftAssist")
	float LiftAssistPreRopeDelay = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|LiftAssist")
	float LiftAssistPreLiftDelay = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|LiftAssist")
	float LiftAssistPreFollowDelay = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|LiftAssist")
	float LiftAssistClearanceResponse = 3.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|LiftAssist")
	float LiftAssistClearanceDeadband = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|LiftAssist")
	float LiftAssistDemandResponse = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|LiftAssist")
	float LiftAssistMaxRaiseAcceleration = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|LiftAssist")
	float LiftAssistPDMaxVerticalSpeed = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|LiftAssist")
	float LiftAssistPDVelocityResponse = 2.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|LiftAssist")
	float LiftAssistPDMaxVerticalAcceleration = 650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|LiftAssist")
	float LiftAssistControlLiftOverhead = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|LiftAssist")
	float LiftAssistVerticalDamping = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|LiftAssist")
	float LiftAssistRopeSlack = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|LiftAssist")
	float LiftAssistRopeOutwardDamping = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|LiftAssist")
	float LiftAssistStrengthScale = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|LiftAssist")
	float LiftAssistReactionScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Camera")
	float DroneCameraTiltStep = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Camera")
	float DroneMinCameraTilt = -45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Camera")
	float DroneMaxCameraTilt = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Camera")
	float PilotCameraFieldOfView = 105.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Camera")
	float ThirdPersonCameraFieldOfView = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Camera")
	float MapCameraFieldOfView = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Camera")
	float DroneMinCameraFieldOfView = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Camera")
	float DroneMaxCameraFieldOfView = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Camera")
	float RollCameraLeanDegrees = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Camera")
	float RollCameraLeanResponse = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Camera")
	float RollCameraPitchInfluenceDegrees = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Camera")
	float RollCameraInfluenceVelocity = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Camera")
	float MapCameraPitchDegrees = -90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Camera")
	float PilotEntryCameraTransitionDuration = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Camera")
	float PilotModeCameraTransitionDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Map")
	float MapEntryRiseHeight = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Map")
	float MapCameraTransitionDuration = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Map")
	float MapPanSpeed = 2600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Map")
	float MapVerticalSpeed = 2600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Map")
	float MapPositionGain = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Map")
	float MapVelocityDamping = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Map")
	float MapMinHeightAboveTarget = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Map")
	float MapMaxHeightAboveTarget = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Map")
	float MiniMapHeightAboveTarget = 2600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Map")
	float MiniMapPositionGain = 5.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Map")
	float MiniMapVelocityDamping = 3.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Crash")
	float DroneCrashSpeed = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Crash")
	float DroneCrashMinRecoveryTime = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Crash")
	float DroneCrashRecoveryTimePerSpeed = 0.0006f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Crash")
	float DroneCrashMaxRecoveryTime = 2.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Crash")
	float DroneCrashSettleLinearSpeed = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Crash")
	float DroneCrashSettleAngularSpeed = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Crash")
	float CrashSelfRightAssist = 6.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Crash")
	float CrashSelfRightMaxCorrectionRate = 540.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Crash")
	float CrashSelfRightActivationSpeed = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Battery", meta=(ClampMin="0.0", UIMin="0.0", ClampMax="100.0", UIMax="100.0"))
	float BatteryFlickerStartPercent = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Battery", meta=(ClampMin="0.0", UIMin="0.0", ClampMax="100.0", UIMax="100.0"))
	float BatteryRedLightStartPercent = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Battery", meta=(ClampMin="0.0", UIMin="0.0", ClampMax="1.0", UIMax="1.0"))
	float PoweredOffDrainMultiplier = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Battery|Torch", meta=(ClampMin="1.0", UIMin="1.0"))
	float TorchDrainMultiplier = 1.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|State")
	bool bAnchorThirdPersonRoleToCamera = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|State")
	bool bDisableCollisionWhenAnchored = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|State|Physics")
	TEnumAsByte<ECollisionEnabled::Type> FlightCollisionEnabled = ECollisionEnabled::QueryAndPhysics;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|State|Physics")
	TEnumAsByte<ECollisionEnabled::Type> PassiveCollisionEnabled = ECollisionEnabled::QueryAndPhysics;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|State|Physics", meta=(ClampMin="0.0", UIMin="0.0"))
	float PassivePhysicsLinearDamping = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|State|Physics", meta=(ClampMin="0.0", UIMin="0.0"))
	float PassivePhysicsAngularDamping = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|State")
	bool bLogStateTransitions = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light", meta=(ClampMin="0.0", UIMin="0.0"))
	float StatusLightBaseIntensity = 8000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light", meta=(ClampMin="0.0", UIMin="0.0"))
	float StatusLightMinIntensity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light", meta=(ClampMin="0.0", UIMin="0.0"))
	float StatusLightAttenuationRadius = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light")
	FLinearColor StatusLightNormalColor = FLinearColor(0.14f, 0.82f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light")
	FLinearColor StatusLightChargingFullColor = FLinearColor(0.15f, 1.0f, 0.25f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light")
	FLinearColor StatusLightLowBatteryColor = FLinearColor(1.0f, 0.06f, 0.03f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light")
	FLinearColor StatusLightLowBatteryWarningColor = FLinearColor(1.0f, 0.46f, 0.06f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|PoweredOff")
	bool bUseManualOffHeartbeat = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|PoweredOff", meta=(ClampMin="0.01", UIMin="0.01"))
	float ManualOffHeartbeatFrequencyHz = 0.28f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|PoweredOff", meta=(ClampMin="0.0", UIMin="0.0", ClampMax="1.0", UIMax="1.0"))
	float ManualOffHeartbeatMinBrightnessAlpha = 0.015f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|PoweredOff", meta=(ClampMin="0.0", UIMin="0.0", ClampMax="1.0", UIMax="1.0"))
	float ManualOffHeartbeatMaxBrightnessAlpha = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Spotlight")
	bool bForceTorchSpotlightsWhite = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Spotlight")
	FLinearColor TorchSpotlightColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Spotlight", meta=(ClampMin="0.0", UIMin="0.0", ClampMax="100.0", UIMax="100.0"))
	float TorchDisableAtLowBatteryPercent = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Spotlight")
	FLinearColor TorchLowBatteryColor = FLinearColor(1.0f, 0.05f, 0.03f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Spotlight", meta=(ClampMin="0.0", UIMin="0.0", ClampMax="1.0", UIMax="1.0"))
	float TorchLowBatteryBrightnessAlpha = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Spotlight|Flicker", meta=(ClampMin="0.0", UIMin="0.0", ClampMax="100.0", UIMax="100.0"))
	float TorchFlickerStartPercent = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Spotlight|Flicker", meta=(ClampMin="0.1", UIMin="0.1"))
	float TorchFlickerRampExponent = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Spotlight|Flicker", meta=(ClampMin="0.0", UIMin="0.0", ClampMax="1.0", UIMax="1.0"))
	float TorchFlickerOutageChanceAtStart = 0.06f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Spotlight|Flicker", meta=(ClampMin="0.0", UIMin="0.0", ClampMax="1.0", UIMax="1.0"))
	float TorchFlickerOutageChanceAtCutoff = 0.82f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Spotlight|Flicker", meta=(ClampMin="0.01", UIMin="0.01"))
	float TorchFlickerGapMinAtStartSeconds = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Spotlight|Flicker", meta=(ClampMin="0.01", UIMin="0.01"))
	float TorchFlickerGapMaxAtStartSeconds = 4.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Spotlight|Flicker", meta=(ClampMin="0.01", UIMin="0.01"))
	float TorchFlickerGapMinAtCutoffSeconds = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Spotlight|Flicker", meta=(ClampMin="0.01", UIMin="0.01"))
	float TorchFlickerGapMaxAtCutoffSeconds = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Spotlight|Flicker", meta=(ClampMin="0.005", UIMin="0.005"))
	float TorchFlickerOffMinAtStartSeconds = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Spotlight|Flicker", meta=(ClampMin="0.005", UIMin="0.005"))
	float TorchFlickerOffMaxAtStartSeconds = 0.07f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Spotlight|Flicker", meta=(ClampMin="0.005", UIMin="0.005"))
	float TorchFlickerOffMinAtCutoffSeconds = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Spotlight|Flicker", meta=(ClampMin="0.005", UIMin="0.005"))
	float TorchFlickerOffMaxAtCutoffSeconds = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Spotlight", meta=(ClampMin="0.0", UIMin="0.0"))
	float TorchAimInterpSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Emissive")
	bool bUseDynamicEmissiveMaterials = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Emissive")
	FName AccessoryEmissiveSlotName = TEXT("AccessoryLight");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Emissive")
	FName AccessoryEmissiveLegacySlotName = TEXT("AccesorryLight");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Emissive")
	FName EyeEmissiveSlotName = TEXT("Eye");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Emissive")
	FName EyeEmissiveAltSlotName = TEXT("DroneEye");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Emissive")
	FName AccessoryEmissiveColorParameterName = TEXT("EmissiveColor");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Emissive")
	FName AccessoryEmissiveIntensityParameterName = TEXT("EmissiveIntensity");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Emissive")
	FName EyeEmissiveColorParameterName = TEXT("EmissiveColor");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Emissive")
	FName EyeEmissiveIntensityParameterName = TEXT("EmissiveIntensity");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Emissive")
	FLinearColor EyeEmissiveColor = FLinearColor(1.0f, 0.05f, 0.03f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Emissive")
	bool bEyeBlinkGreenWhenFullyCharged = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Emissive")
	bool bEyeFullChargeBlinkRequiresChargerVolume = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Emissive")
	FLinearColor EyeFullyChargedBlinkColor = FLinearColor(0.15f, 1.0f, 0.25f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Emissive", meta=(ClampMin="0.01", UIMin="0.01"))
	float EyeFullChargeBlinkFrequencyHz = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Emissive", meta=(ClampMin="0.0", UIMin="0.0", ClampMax="1.0", UIMax="1.0"))
	float EyeFullChargeBlinkMinAlpha = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Emissive", meta=(ClampMin="0.0", UIMin="0.0", ClampMax="1.0", UIMax="1.0"))
	float EyeFullChargeBlinkMaxAlpha = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Emissive", meta=(ClampMin="0.0", UIMin="0.0"))
	float AccessoryEmissiveMinIntensity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Emissive", meta=(ClampMin="0.0", UIMin="0.0"))
	float AccessoryEmissiveMaxIntensity = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Emissive", meta=(ClampMin="0.0", UIMin="0.0"))
	float EyeEmissiveMinIntensity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Emissive", meta=(ClampMin="0.0", UIMin="0.0"))
	float EyeEmissiveMaxIntensity = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Emissive", meta=(ClampMin="0.1", UIMin="0.1"))
	float EyeEmissiveBatteryRampExponent = 1.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light", meta=(ClampMin="0.0", UIMin="0.0", ClampMax="1.0", UIMax="1.0"))
	float StatusLightLowBatteryMinBrightnessAlpha = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Flat", meta=(ClampMin="0.01", UIMin="0.01"))
	float FlatBatteryPulseFrequencyHz = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Flat", meta=(ClampMin="0.0", UIMin="0.0", ClampMax="1.0", UIMax="1.0"))
	float FlatBatteryPulseMinBrightnessAlpha = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Flat", meta=(ClampMin="0.0", UIMin="0.0", ClampMax="1.0", UIMax="1.0"))
	float FlatBatteryPulseMaxBrightnessAlpha = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Flicker", meta=(ClampMin="0.0", UIMin="0.0"))
	float LowBatteryFlickerMinFrequencyHz = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Flicker", meta=(ClampMin="0.0", UIMin="0.0"))
	float LowBatteryFlickerMaxFrequencyHz = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Flicker", meta=(ClampMin="0.0", UIMin="0.0", ClampMax="1.0", UIMax="1.0"))
	float LowBatteryFlickerMinDepth = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Flicker", meta=(ClampMin="0.0", UIMin="0.0", ClampMax="1.0", UIMax="1.0"))
	float LowBatteryFlickerMaxDepth = 0.32f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Light|Flicker", meta=(ClampMin="0.1", UIMin="0.1"))
	float LowBatteryFlickerRampExponent = 2.75f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Battery", meta=(AllowPrivateAccess="true"))
	bool bDronePoweredOff = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|State", meta=(AllowPrivateAccess="true"))
	EDronePowerState CurrentPowerState = EDronePowerState::Active;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|State", meta=(AllowPrivateAccess="true"))
	EDroneMobilityState CurrentMobilityState = EDroneMobilityState::Flight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|State", meta=(AllowPrivateAccess="true"))
	EDroneRoleState CurrentRoleState = EDroneRoleState::ThirdPersonCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|State", meta=(AllowPrivateAccess="true"))
	EDroneControlState CurrentControlState = EDroneControlState::Autonomous;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Battery", meta=(AllowPrivateAccess="true"))
	bool bIsInChargerVolume = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Battery", meta=(AllowPrivateAccess="true"))
	bool bChargingLockActive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Battery", meta=(AllowPrivateAccess="true"))
	float LowBatteryFlickerAlpha = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Debug")
	bool bShowDebugOverlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Debug")
	bool bLogRollModeDiagnostics = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Debug")
	float DroneImpactDebugHoldTime = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Debug")
	float RollModeLogInterval = 0.2f;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnDroneBodyHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& Hit);

	void UpdateCrashRecovery(float DeltaSeconds);
	void UpdateComplexFlight(float DeltaSeconds);
	void UpdateSimpleFlight(float DeltaSeconds);
	void UpdateFreeFlyFlight(float DeltaSeconds);
	void UpdateSpectatorFlight(float DeltaSeconds);
	void UpdateRollFlight(float DeltaSeconds);
	void UpdateMapFlight(float DeltaSeconds);
	void UpdateMiniMapFlight(float DeltaSeconds);
	void UpdateAutonomousFlight(float DeltaSeconds);
	void UpdateLiftAssistFlight(float DeltaSeconds);
	void UpdateBuddyDrift(float DeltaSeconds);
	void ClampVelocity() const;
	void ApplyDesiredAngularVelocity(const FVector& DesiredLocalAngularVelocity, float DeltaSeconds, float ResponseSpeed);
	FVector BuildUprightAssistAngularVelocity(float AssistStrength, float MaxCorrectionRate) const;
	FVector GetDesiredLocation() const;
	FVector GetDesiredForwardDirection() const;
	void UpdateRollCamera(float DeltaSeconds);
	bool QueryRollGroundContact(float& OutGroundClearance) const;
	void LogRollModeState(const TCHAR* Context, bool bProbeGround);
	void UpdateDebugOutput() const;
	bool IsLocalPlayerDebugTarget() const;
	void RemoveAppliedConveyorSurfaceVelocity();
	void ApplyConveyorSurfaceVelocity();
	void ApplyRuntimePhysicalMaterial();
	void UpdateBatteryState(float DeltaSeconds);
	void HandleDronePowerLoss();
	void HandleDronePowerRestore();
	void UpdateStatusLight(float DeltaSeconds);
	void InitializeDynamicEmissiveMaterials();
	void UpdateDynamicEmissiveMaterials(
		const FLinearColor& StatusColor,
		float StatusBrightnessAlpha,
		float BatteryAlpha,
		bool bBatteryFlat);
	void UpdateSpotlightVisuals(float DeltaSeconds = 0.0f);
	void ApplyPickupProxyVisualizationSettings();
	void ApplyEditorCameraVisualizationSettings();
	void ApplyStaticMeshOwnerVisibility();
	void RefreshTorchRuntimeState();
	void RefreshStateDomains(FName Reason);
	EDronePowerState ResolveDesiredPowerState() const;
	EDroneRoleState ResolveRoleStateFromCompanionMode(EDroneCompanionMode Mode) const;
	EDroneControlState ResolveControlStateFromCompanionMode(EDroneCompanionMode Mode) const;
	EDroneMobilityState ResolveDesiredMobilityState() const;
	bool RequestPowerState(EDronePowerState NewState, FName Reason);
	bool RequestMobilityState(EDroneMobilityState NewState, FName Reason);
	bool RequestRoleState(EDroneRoleState NewState, FName Reason);
	bool RequestControlState(EDroneControlState NewState, FName Reason);
	bool CanTransitionPowerState(EDronePowerState FromState, EDronePowerState ToState) const;
	bool CanTransitionMobilityState(EDroneMobilityState FromState, EDroneMobilityState ToState) const;
	bool CanTransitionRoleState(EDroneRoleState FromState, EDroneRoleState ToState) const;
	bool CanTransitionControlState(EDroneControlState FromState, EDroneControlState ToState) const;
	void ApplyPhysicsProfile(bool bResetKinematics);
	bool CanApplyMovementForces() const;
	bool ShouldProcessConveyorAssist() const;
	bool ShouldClampVelocityForCurrentState() const;
	void UpdateAnchoredTransform();
	void LogStateTransition(const TCHAR* Domain, int32 PreviousValue, int32 NewValue, FName Reason, bool bAccepted) const;
	void RefreshCameraForPilotControlModeChange(bool bWasRollControls);
	void UpdateCameraTransition(float DeltaSeconds);
	void StartCameraTransition(
		const FRotator& TargetRotation,
		float TargetFieldOfView,
		bool bInstant,
		float TransitionDuration = -1.0f);
	FRotator GetDesiredCameraMountRotationForMode(EDroneCompanionMode ForMode) const;
	float GetDesiredCameraFieldOfViewForMode(EDroneCompanionMode ForMode) const;
	void RefreshCameraMountRotation();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	UStaticMeshComponent* DroneBody;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	USphereComponent* DronePickupProxySphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	USceneComponent* CameraMount;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	USceneComponent* BeamOrigin;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	UCameraComponent* DroneCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	UPointLightComponent* DroneStatusLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	UDroneBatteryComponent* BatteryComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	UAgentBeamToolComponent* BeamToolComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Conveyor", meta=(AllowPrivateAccess="true"))
	UConveyorSurfaceVelocityComponent* ConveyorSurfaceVelocity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	EDroneCompanionMode CompanionMode = EDroneCompanionMode::ThirdPersonFollow;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	bool bCrashed = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	bool bUseSimplePilotControls = false;

	FVector AppliedConveyorSurfaceVelocity = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	bool bUseFreeFlyPilotControls = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	bool bUseSpectatorPilotControls = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	bool bUseRollPilotControls = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	bool bPilotStabilizerEnabled = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	bool bPilotHoverModeEnabled = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	bool bUseCrashRollRecoveryMode = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	bool bPilotHoverAssistTemporarilySuppressed = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	float CrashRecoveryTimeRemaining = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	bool bImpactDetected = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	bool bImpactWouldCrash = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	float LastImpactSpeed = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	float CameraTiltDegrees = 0.0f;

	UPROPERTY()
	TObjectPtr<AActor> FollowTarget;

	UPROPERTY(Transient)
	UPhysicalMaterial* RuntimePhysicalMaterial = nullptr;

	FRotator ViewReferenceRotation = FRotator::ZeroRotator;
	FVector ThirdPersonCameraTargetLocation = FVector::ZeroVector;
	FRotator ThirdPersonCameraTargetRotation = FRotator::ZeroRotator;
	bool bHasThirdPersonCameraTarget = false;
	FVector MapTargetLocation = FVector::ZeroVector;
	FVector HoldTargetLocation = FVector::ZeroVector;
	FRotator HoldTargetRotation = FRotator::ZeroRotator;
	FVector BuddyLocalOffset = FVector(0.0f, -160.0f, 250.0f);
	bool bNextMapModeUsesEntryLift = true;
	bool bMapModeUsesHeightLimits = true;
	bool bCameraTransitionActive = false;
	bool bPendingCrashRollRecovery = false;
	bool bSuppressCameraMountRefresh = false;
	bool bLiftAssistActive = false;
	bool bLiftAssistLatched = false;
	bool bLiftAssistRopeVisible = false;
	bool bLiftAssistLiftEngaged = false;
	bool bLiftAssistFollowEngaged = false;
	float BuddyDriftTimeRemaining = 0.0f;
	float ImpactDebugTimeRemaining = 0.0f;
	float CameraTransitionElapsed = 0.0f;
	float CameraTransitionTotalDuration = 0.0f;
	float RollJumpCooldownRemaining = 0.0f;
	float RollModeLogTimeRemaining = 0.0f;
	FVector PreviousLinearVelocity = FVector::ZeroVector;
	float PilotThrottleInput = 0.0f;
	float AppliedThrottleInput = 0.0f;
	float PilotYawInput = 0.0f;
	float PilotRollInput = 0.0f;
	float PilotPitchInput = 0.0f;
	float CameraTransitionStartFieldOfView = 0.0f;
	float CameraTransitionTargetFieldOfView = 0.0f;
	float CurrentHoverBaseAcceleration = 0.0f;
	float CurrentHoverCommandInput = 0.0f;
	float CurrentHoverVerticalAcceleration = 0.0f;
	float CurrentHoverLiftDot = 1.0f;
	float CurrentRollCameraPitchOffset = 0.0f;
	float CurrentRollCameraLean = 0.0f;
	float LiftAssistStageTimeRemaining = 0.0f;
	float LiftAssistForceRampTime = 0.0f;
	float LiftAssistSmoothedVerticalAcceleration = 0.0f;
	float ManualOffHeartbeatTime = 0.0f;
	float StatusLightFlickerTime = 0.0f;
	float TorchFlickerTimeRemaining = 0.0f;
	float FlatBatteryPulseTime = 0.0f;
	FVector FreeFlyCurrentVelocity = FVector::ZeroVector;
	FRotator CameraTransitionStartRotation = FRotator::ZeroRotator;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> AccessoryEmissiveMaterialInstances;
	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> EyeEmissiveMaterialInstances;
	bool bHideStaticMeshesFromOwnerCamera = false;
	bool bTorchEnabled = false;
	bool bTorchRuntimeActive = false;
	bool bTorchHasAimTarget = false;
	FVector TorchAimTargetLocation = FVector::ZeroVector;
	bool bManualPowerOffRequested = false;
	TWeakObjectPtr<UPrimitiveComponent> LiftAssistTargetComponent;
	FVector LiftAssistLocalGrabOffset = FVector::ZeroVector;
	FDroneLiftAssistForceTuning LiftAssistForceTuning;
	FRotator CameraTransitionTargetRotation = FRotator::ZeroRotator;
	int32 ChargerVolumeOverlapCount = 0;
	bool bStatusLightFlickerOn = true;
	bool bTorchFlickerOn = true;
	TMap<TWeakObjectPtr<USpotLightComponent>, float> TorchSpotlightBaseIntensityByComponent;
};
