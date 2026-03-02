// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DroneCompanion.generated.h"

class UCameraComponent;
class UPhysicalMaterial;
class UPrimitiveComponent;
class UStaticMeshComponent;
class USceneComponent;
class AActor;

UENUM(BlueprintType)
enum class EDroneCompanionMode : uint8
{
	ThirdPersonFollow,
	PilotControlled,
	HoldPosition,
	BuddyFollow,
	MapMode,
	MiniMapFollow
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
	void SetPilotStabilizerEnabled(bool bEnable);

	UFUNCTION(BlueprintCallable, Category="Drone")
	void TeleportDroneToTransform(const FVector& NewLocation, const FRotator& NewRotation);

	UFUNCTION(BlueprintCallable, Category="Drone")
	void SetPilotHoverModeEnabled(bool bEnable);

	UFUNCTION(BlueprintCallable, Category="Drone")
	void SetNextMapModeUsesEntryLift(bool bEnable);

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Physics")
	float DroneMassKg = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Physics")
	float DroneBodyVisualScale = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Physics")
	float DroneBodyLinearDamping = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Physics")
	float DroneBodyAngularDamping = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Physics")
	float DroneBodyFriction = 2.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Physics")
	float DroneBodyRestitution = 0.05f;

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
	float SimpleMaxTiltDegrees = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float SimpleBrakingResponse = 2.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float SimpleCollectiveRange = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float SimpleAttitudeHoldRate = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float SimpleCameraTiltAxisSpeed = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float SimpleMaxDistanceFromTarget = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float FreeFlyMaxSpeed = 2200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float FreeFlyAcceleration = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float FreeFlyDeceleration = 12000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float FreeFlyRotationResponse = 4.0f;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Camera")
	float DroneCameraTiltStep = 1.0f;

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
	float MapCameraPitchDegrees = -90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Camera")
	float PilotEntryCameraTransitionDuration = 0.35f;

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
	float DroneCrashSpeed = 600.0f;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Debug")
	bool bShowDebugOverlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Debug")
	float DroneImpactDebugHoldTime = 0.5f;

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
	void UpdateMapFlight(float DeltaSeconds);
	void UpdateMiniMapFlight(float DeltaSeconds);
	void UpdateAutonomousFlight(float DeltaSeconds);
	void UpdateBuddyDrift(float DeltaSeconds);
	void ClampVelocity() const;
	void ApplyDesiredAngularVelocity(const FVector& DesiredLocalAngularVelocity, float DeltaSeconds, float ResponseSpeed);
	FVector BuildUprightAssistAngularVelocity(float AssistStrength, float MaxCorrectionRate) const;
	FVector GetDesiredLocation() const;
	FVector GetDesiredForwardDirection() const;
	void UpdateDebugOutput() const;
	void ApplyRuntimePhysicalMaterial();
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
	USceneComponent* CameraMount;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	UCameraComponent* DroneCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	EDroneCompanionMode CompanionMode = EDroneCompanionMode::ThirdPersonFollow;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	bool bCrashed = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	bool bUseSimplePilotControls = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	bool bUseFreeFlyPilotControls = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	bool bPilotStabilizerEnabled = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	bool bPilotHoverModeEnabled = false;

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
	float BuddyDriftTimeRemaining = 0.0f;
	float ImpactDebugTimeRemaining = 0.0f;
	float CameraTransitionElapsed = 0.0f;
	float CameraTransitionTotalDuration = 0.0f;
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
	FVector FreeFlyCurrentVelocity = FVector::ZeroVector;
	FRotator CameraTransitionStartRotation = FRotator::ZeroRotator;
	FRotator CameraTransitionTargetRotation = FRotator::ZeroRotator;
};
