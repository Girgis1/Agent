// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "AgentCharacter.generated.h"

class ADroneCompanion;
class AConveyorBeltStraight;
class AConveyorPlacementPreview;
class AMachineActor;
class AStorageBin;
class ATrolleyVehiclePawn;
class UVehicleInteractionComponent;
class UCameraComponent;
class UDroneSwarmComponent;
class UAnimInstance;
class UInputAction;
class UPhysicsHandleComponent;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMeshComponent;
class USpringArmComponent;
struct FHitResult;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

enum class EDroneCompanionMode : uint8;
enum class EDroneSwarmRoleSlot : uint8;

UENUM(BlueprintType)
enum class EAgentViewMode : uint8
{
	ThirdPerson,
	DronePilot,
	FirstPerson
};

UENUM(BlueprintType)
enum class EAgentDronePilotControlMode : uint8
{
	Complex,
	Horizon,
	HorizonHover,
	Simple,
	FreeFly,
	Roll,
	SpectatorCam
};

UENUM(BlueprintType)
enum class EAgentFactoryPlacementType : uint8
{
	Conveyor,
	StorageBin,
	Machine
};

/**
 *  Player character that coordinates character movement and the persistent companion drone.
 */
UCLASS(abstract)
class AAgentCharacter : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom retained for compatibility with the template and as a fallback camera mount */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	USpringArmComponent* CameraBoom;

	/** Third-person fallback camera on the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	UCameraComponent* FollowCamera;

	/** First-person camera on the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	UCameraComponent* FirstPersonCamera;

	/** Temporary camera used to fake the drone-to-third-person handoff */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	UCameraComponent* ThirdPersonTransitionCamera;

	/** Hidden local proxy mesh that rides with the third-person camera path and only exists for shadowing */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	UStaticMeshComponent* ThirdPersonDroneProxyMesh;

	/** Reusable physics handle used for picking up and dragging objects */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	UPhysicsHandleComponent* PickupPhysicsHandle;

	/** Dedicated vehicle entry interaction for possession-based vehicles */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	UVehicleInteractionComponent* VehicleInteractionComponent;

	/** Swarm registry and role-slot coordinator for all known drones */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	UDroneSwarmComponent* DroneSwarmComponent;

protected:
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

public:
	AAgentCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void Move(const FInputActionValue& Value);
	void StopMove(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

	void OnViewModeButtonPressed();
	void OnViewModeButtonReleased();
	void OnCycleActiveDronePressed();
	void OnDroneTorchTogglePressed();
	void OnLeftMouseButtonPressed();
	void OnGamepadFaceButtonLeftPressed();
	void OnGamepadLeftShoulderPressed();
	void UpdateViewModeButtonHold(float DeltaSeconds);
	void UpdateRollModeJumpHold(float DeltaSeconds);
	void CycleViewMode();
	void ApplyViewMode(EAgentViewMode NewMode, bool bBlend);
	void SpawnInitialDroneFleet();
	void DiscoverWorldDrones();
	void SyncSwarmFromDroneFleet();
	EDroneSwarmRoleSlot ResolveDesiredActiveSwarmRole() const;
	void RefreshSwarmRoleAssignments();
	void CleanupInvalidDroneJobLocks();
	bool IsDroneLockedToPersistentJob(const ADroneCompanion* CandidateDrone) const;
	bool IsDroneInHookJobLock(const ADroneCompanion* CandidateDrone) const;
	EDroneCompanionMode ResolveCompanionModeForRole(EDroneSwarmRoleSlot InRole) const;
	void SpawnDroneCompanion();
	ADroneCompanion* SpawnDroneCompanionAtTransform(
		const FVector& SpawnLocation,
		const FRotator& SpawnRotation,
		EDroneCompanionMode InitialMode,
		bool bMakeActive = true);
	void DespawnDroneCompanion();
	void CleanupInvalidDroneCompanions();
	void EnsureActiveDroneSelection();
	bool IsDroneAvailableForActivation(const ADroneCompanion* CandidateDrone) const;
	int32 FindNextDroneIndex(int32 StartIndex, int32 Direction, bool bPreferAvailable, bool bSkipJobLockedDrones) const;
	bool SetActiveDroneIndex(int32 NewActiveIndex, bool bUpdateViewTarget = true, bool bBlendViewTarget = true);
	void CycleActiveDrone(int32 Direction = 1);
	void ApplyDroneFleetContext(bool bUpdateViewTarget = false, bool bBlendViewTarget = true);
	void ApplyDroneAssistState();
	void EnterRollTransitionMode(bool bTrackHeldReturn, bool bFromCrashRecovery);
	void ExitRollTransitionMode(bool bTryJumpLift);
	void SyncDroneCompanionControlState(bool bAllowRollControls, bool bSuppressCameraMountRefresh = false);
	void UpdateDroneCompanionThirdPersonTarget();
	void UpdateDroneTorchTarget();
	void UpdateDronePilotInputs(float DeltaSeconds);
	void UpdateThirdPersonTransition(float DeltaSeconds);
	bool GetThirdPersonDroneTarget(FVector& OutLocation, FRotator& OutRotation) const;
	void ResetDroneInputState();
	void ResetCharacterCameraRoll();
	void UpdateDronePilotCameraLimits();
	void SyncControllerRotationToDroneCamera();
	void RefreshPrimaryDroneAvailabilityFromCompanion();
	bool IsDronePilotMode() const;
	bool IsDroneInputModeActive() const;
	bool IsSimpleDronePilotMode() const;
	bool IsFreeFlyDronePilotMode() const;
	bool IsSpectatorDronePilotMode() const;
	bool IsRollDronePilotMode() const;
	void SetDronePilotControlMode(EAgentDronePilotControlMode NewMode);
	void CycleDronePilotControlMode(int32 Direction);
	void ToggleDronePilotControlMode();
	void ToggleMapMode();
	void EnterMiniMapMode();
	void ExitMiniMapMode();
	void FocusMiniMapDroneCamera();
	void UpdateKeyboardMapButtonHold(float DeltaSeconds);
	void UpdateControllerMapButtonHold(float DeltaSeconds);
	void SetThirdPersonProxyVisible(bool bVisible);
	void AttachThirdPersonProxyToComponent(USceneComponent* AttachmentParent);
	bool CanUseConveyorPlacementMode() const;
	bool CanUseCharacterInteraction() const;
	void ToggleConveyorPlacementMode();
	void EnterConveyorPlacementMode();
	void ExitConveyorPlacementMode();
	void UpdateConveyorPlacementPreview();
	UCameraComponent* GetActivePlacementCamera() const;
	void TryPlaceConveyor();
	void CancelConveyorPlacement();
	void RotateConveyorPlacement(int32 Direction);
	bool EvaluateConveyorPlacement(FVector& OutLocation, FRotator& OutRotation, bool& bOutIsValid) const;
	bool TryGetFactoryBuildableFaceSnapLocation(const FHitResult& AimHit, FVector& OutLocation) const;
	void SelectFactoryPlacementType(EAgentFactoryPlacementType NewType, bool bToggleIfAlreadySelected);
	bool CanUsePickupInteraction() const;
	bool CanMaintainHeldPickup() const;
	bool CanUseDroneLiftAssist() const;
	bool CanMaintainDroneLiftAssist() const;
	bool CanUseMapModeDronePickup() const;
	bool GetCharacterPickupView(FVector& OutLocation, FRotator& OutRotation) const;
	bool GetActivePickupView(FVector& OutLocation, FRotator& OutRotation) const;
	void UpdatePickupInteraction();
	bool UpdatePickupCandidate();
	void DrawPickupInfluenceDebug(const FVector& SphereCenter, const FColor& Color) const;
	UPrimitiveComponent* ResolvePickupPhysicsComponent(UPrimitiveComponent* PrimitiveComponent) const;
	bool CanPickupComponent(const UPrimitiveComponent* PrimitiveComponent) const;
	bool TryBeginPickup();
	void EndPickup();
	void UpdateHeldPickup();
	void RebaseHeldPickupRotationToView();
	void RefreshHeldPickupConstraintMode();
	void SyncPickupHandleSettings() const;
	float GetActivePickupStrengthValue() const;
	float GetHeldPickupResponsivenessScale() const;
	float GetHeldPickupHeavyDampingMultiplier() const;
	float GetHeldPickupRotationLeverageMultiplier() const;
	float GetHeldPickupRotationDriveScale() const;
	void ApplyHeldPickupRotationPhysicsInput(const FVector& RotationAxis, float InputValue, float DeltaSeconds);
	ADroneCompanion* GetHeldDroneFromPickup() const;
	bool TryToggleHeldDronePowerFromPickup();
	bool TryToggleDroneLiftAssist();
	void SyncDroneLiftAssistTuning() const;
	void AdjustDroneLiftAssistStrength(float Delta);
	void AdjustPickupStrength(float Delta);
	void ShowPickupStrengthDebug() const;
	bool TryAssignActiveDroneToHookJob(bool bCycleAfterAssign);
	bool TryReleaseAllHookJobDrones();
	void UpdateHookJobButtonHold(float DeltaSeconds);

	void OnDronePitchForwardPressed();
	void OnDronePitchForwardReleased();
	void OnDronePitchBackwardPressed();
	void OnDronePitchBackwardReleased();
	void OnDroneRollLeftPressed();
	void OnDroneRollLeftReleased();
	void OnDroneRollRightPressed();
	void OnDroneRollRightReleased();
	void OnDroneYawLeftPressed();
	void OnDroneYawLeftReleased();
	void OnDroneYawRightPressed();
	void OnDroneYawRightReleased();
	void OnDroneThrottleUpPressed();
	void OnDroneThrottleUpReleased();
	void OnDroneThrottleDownPressed();
	void OnDroneThrottleDownReleased();

	void OnDroneGamepadYawAxis(float Value);
	void OnDroneGamepadThrottleAxis(float Value);
	void OnDroneGamepadRollAxis(float Value);
	void OnDroneGamepadPitchAxis(float Value);
	void OnDroneGamepadLeftTriggerAxis(float Value);
	void OnDroneGamepadRightTriggerAxis(float Value);
	void OnDroneCameraTiltUpPressed();
	void OnDroneCameraTiltDownPressed();
	void OnDroneControlModeTogglePressed();
	void OnDroneHoverModePressed();
	void OnDroneStabilizerTogglePressed();
	void OnMapModePressed();
	void OnKeyboardMapButtonPressed();
	void OnKeyboardMapButtonReleased();
	void OnControllerMapButtonPressed();
	void OnControllerMapButtonReleased();
	void OnConveyorPlacementModePressed();
	void OnStorageBinPlacementModePressed();
	void OnMachinePlacementModePressed();
	void OnFactoryPlacementTogglePressed();
	void OnConveyorPlacePressed();
	void OnConveyorCancelPressed();
	void OnConveyorRotateLeftPressed();
	void OnConveyorRotateRightPressed();
	void OnHookJobButtonPressed();
	void OnHookJobButtonReleased();
	void OnPickupOrDroneYawRightPressed();
	void OnPickupOrDroneYawRightReleased();
	void OnPickupOrPlacePressed();
	void OnPickupOrPlaceReleased();
	void OnPickupStrengthDecreasePressed();
	void OnPickupStrengthIncreasePressed();
	void OnInteractPressed();
	void OnInteractReleased();
	void OnVehicleInteractPressed();
	void ConfigureTrolleyPosePostProcess();
	void UpdateTrolleyDriverPose(float DeltaSeconds);
	ATrolleyVehiclePawn* GetControlledTrolleyVehicle() const;
	FVector ResolveMeshSocketWorldLocation(FName SocketName) const;
	FVector TransformWorldToMeshSpace(const FVector& WorldLocation) const;
	FTransform TransformWorldToMeshSpace(const FTransform& WorldTransform) const;
	void ResetTrolleyDriverPoseState();

public:
	virtual FVector GetVelocity() const override;

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone")
	TSubclassOf<ADroneCompanion> DroneCompanionClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Fleet", meta=(ClampMin="1", UIMin="1"))
	int32 InitialDroneCount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Fleet", meta=(ClampMin="0.0", UIMin="0.0"))
	float InitialInactiveDroneSpawnRadius = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Fleet")
	bool bCycleOnlyAvailableDrones = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Swarm")
	bool bAutoAssignInactiveDronesAsHookWorkers = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Swarm", meta=(ClampMin="0.05", UIMin="0.05"))
	float HookJobReleaseHoldTime = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Fleet", meta=(ClampMin="0.1", UIMin="0.1"))
	float DroneWorldDiscoveryInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone")
	FVector DroneSpawnOffset = FVector(-180.0f, 0.0f, 120.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone")
	float ViewBlendTime = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone")
	bool bStartInThirdPersonDroneView = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone")
	bool bLockCharacterMovementDuringDronePilot = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone")
	bool bStartWithSimpleDroneControls = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone")
	bool bStartWithDroneStabilizerEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone")
	bool bStartWithDroneHoverModeEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone")
	EAgentDronePilotControlMode StartDronePilotControlMode = EAgentDronePilotControlMode::Complex;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone")
	float DroneEntryAssistReleaseThreshold = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Torch")
	bool bStartWithDroneTorchEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Torch", meta=(ClampMin="100.0", UIMin="100.0"))
	float DroneTorchAimTraceDistance = 8000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Crash")
	bool bUseCrashRollRecovery = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Crash")
	float CrashRollRecoveryStableDelay = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Availability")
	bool bPrimaryDroneAvailable = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Availability")
	bool bForceFirstPersonWhenDroneUnavailable = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Battery")
	float PersistedPrimaryDroneBatteryPercent = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	FVector FirstPersonCameraOffset = FVector(0.0f, 0.0f, 64.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	float ThirdPersonDroneSwapMaxDistance = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	float ThirdPersonDroneTransitionDuration = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	float ThirdPersonDroneProxyScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	float DroneViewHoldTime = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float RollModeJumpHoldTime = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float RollToFlightAssistDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Pilot")
	float FreeFlyUnlockedPitchLimit = 179.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Map")
	float ControllerMapButtonHoldTime = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Placement")
	TSubclassOf<AConveyorBeltStraight> ConveyorBeltClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Placement")
	TSubclassOf<AConveyorPlacementPreview> ConveyorPlacementPreviewClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Placement")
	TSubclassOf<AStorageBin> StorageBinClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Placement")
	TSubclassOf<AMachineActor> MachineClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Placement")
	float ConveyorPlacementTraceDistance = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Placement")
	float ConveyorPlacementSurfaceTraceHeight = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Placement")
	float ConveyorPlacementSurfaceTraceDepth = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Placement")
	FVector ConveyorPlacementClearanceExtents = FVector(48.0f, 48.0f, 12.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupTraceDistance = 650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupTraceRadius = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupMinHoldDistance = 125.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupMaxHoldDistance = 650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float MapModePickupDownwardDotThreshold = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	FVector ThirdPersonPickupHoldOffset = FVector(7.5f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup", meta=(DisplayName="Character Pickup Strength"))
	float CharacterPickupStrengthMultiplier = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup", meta=(DisplayName="Drone Pickup Strength"))
	float DronePickupStrengthMultiplier = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup", meta=(DisplayName="Pickup Strength Mass Scale (kg)"))
	float PickupStrengthMassScaleKg = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup", meta=(DisplayName="Pickup Mass Feel Multiplier"))
	float PickupMassFeelMultiplier = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup", meta=(DisplayName="Pickup Soft Cap Response Fraction"))
	float PickupSoftCapResponseMassFraction = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupLightInterpolationSpeed = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupHeavyInterpolationSpeed = 4.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupHandleLinearStiffness = 8000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupHandleLinearDamping = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupHandleAngularStiffness = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupHandleAngularDamping = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupLightSwingDampingMultiplier = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupHeldBodyLinearDamping = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupHeldBodyAngularDamping = 7.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupHeavyDampingStartMassKg = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupHeavyDampingReferenceStrength = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupHeavyDampingMultiplier = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupRotationComfortMassFraction = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupRotationLeverageReferenceCm = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupRotationResistanceMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupRotationGuideStrength = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupRotationGuideInputScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupRotationAngularImpulse = 9000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupRotationYawSpeed = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupRotationPitchSpeed = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupRotationTriggerThreshold = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	bool bShowPickupDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Pose")
	bool bEnableTrolleyDriverPoseSync = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Pose")
	TSubclassOf<UAnimInstance> TrolleyPosePostProcessAnimClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Pose")
	bool bEnableTrolleyBodyFacing = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Pose", meta=(ClampMin="0.0"))
	float TrolleyBodyYawFollowSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Pose")
	float TrolleyBodyFacingYawOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Pose")
	bool bUseHandleRotationForHandTargets = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Pose")
	bool bEnableTrolleyHandIK = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Pose")
	FRotator TrolleyLeftHandTargetRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Pose")
	FRotator TrolleyRightHandTargetRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Pose")
	bool bSmoothTrolleyRigTargets = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Pose", meta=(ClampMin="0.0"))
	float TrolleyRigTargetLocationSmoothingSpeed = 22.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Pose", meta=(ClampMin="0.0"))
	float TrolleyRigTargetRotationSmoothingSpeed = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Pose", meta=(ClampMin="0.0"))
	float TrolleyPoseBlendInSpeed = 9.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Pose", meta=(ClampMin="0.0"))
	float TrolleyPoseBlendOutSpeed = 11.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Pose", meta=(ClampMin="0.0"))
	float TrolleyHandIKBlendSpeed = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Pose", meta=(ClampMin="0.0"))
	float TrolleyLocomotionBlendSpeed = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Pose", meta=(ClampMin="0.0"))
	float TrolleyLeanBlendSpeed = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Pose", meta=(ClampMin="0.0"))
	float TrolleyMaxLeanDegrees = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Pose", meta=(ClampMin="1.0"))
	float TrolleyAccelerationForMaxLean = 1600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Pose", meta=(ClampMin="0.0"))
	float TrolleyGripBreakDistance = 48.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Pose", meta=(ClampMin="0.0"))
	float TrolleyGripRegrabDistance = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Pose", meta=(ClampMin="0.0"))
	float TrolleyGripBreakAngularSpeedDeg = 240.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Pose", meta=(ClampMin="0.0"))
	float TrolleyGripRegrabDelay = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Pose", meta=(ClampMin="1.0"))
	float TrolleySpeedForFullStrain = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Pose")
	FName TrolleyLeftHandSocketName = TEXT("hand_l");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Pose")
	FName TrolleyRightHandSocketName = TEXT("hand_r");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Pose")
	bool bDrawTrolleyPoseDebug = false;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	TObjectPtr<ADroneCompanion> DroneCompanion = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Fleet", meta=(AllowPrivateAccess="true"))
	TArray<TObjectPtr<ADroneCompanion>> DroneCompanions;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Fleet", meta=(AllowPrivateAccess="true"))
	int32 ActiveDroneIndex = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Swarm", meta=(AllowPrivateAccess="true"))
	TObjectPtr<ADroneCompanion> ThirdPersonLockedDrone = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Swarm", meta=(AllowPrivateAccess="true"))
	TObjectPtr<ADroneCompanion> MiniMapLockedDrone = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Swarm", meta=(AllowPrivateAccess="true"))
	TArray<TObjectPtr<ADroneCompanion>> HookJobLockedDrones;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Swarm", meta=(AllowPrivateAccess="true"))
	TObjectPtr<ADroneCompanion> LastHookJobLockedDrone = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Placement", meta=(AllowPrivateAccess="true"))
	TObjectPtr<AConveyorPlacementPreview> ConveyorPlacementPreview = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera", meta=(AllowPrivateAccess="true"))
	EAgentViewMode CurrentViewMode = EAgentViewMode::ThirdPerson;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	bool bUseSimpleDronePilotControls = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	bool bMapModeActive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	bool bMiniMapModeActive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	bool bMiniMapViewingDroneCamera = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	EAgentViewMode MiniMapReturnViewMode = EAgentViewMode::FirstPerson;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	EAgentDronePilotControlMode DronePilotControlMode = EAgentDronePilotControlMode::Complex;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	EAgentDronePilotControlMode LastFlightDronePilotControlMode = EAgentDronePilotControlMode::Complex;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	bool bDroneStabilizerEnabled = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	bool bDroneHoverModeEnabled = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	bool bDroneEntryAssistActive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Torch", meta=(AllowPrivateAccess="true"))
	bool bPlayerDroneTorchEnabled = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Placement", meta=(AllowPrivateAccess="true"))
	bool bConveyorPlacementModeActive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Placement", meta=(AllowPrivateAccess="true"))
	bool bConveyorPlacementValid = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Pose", meta=(AllowPrivateAccess="true"))
	bool bTrolleyPoseActive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Pose", meta=(AllowPrivateAccess="true"))
	bool bTrolleyGripBroken = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Pose", meta=(AllowPrivateAccess="true"))
	float TrolleyPoseBlendAlpha = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Pose", meta=(AllowPrivateAccess="true"))
	float TrolleyLeftHandIKAlpha = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Pose", meta=(AllowPrivateAccess="true"))
	float TrolleyRightHandIKAlpha = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Pose", meta=(AllowPrivateAccess="true"))
	FVector TrolleyHandleLeftWorld = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Pose", meta=(AllowPrivateAccess="true"))
	FVector TrolleyHandleRightWorld = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Pose", meta=(AllowPrivateAccess="true"))
	FVector TrolleyChestTargetWorld = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Pose", meta=(AllowPrivateAccess="true"))
	FTransform TrolleyHandleLeftWorldTransform = FTransform::Identity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Pose", meta=(AllowPrivateAccess="true"))
	FTransform TrolleyHandleRightWorldTransform = FTransform::Identity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Pose", meta=(AllowPrivateAccess="true"))
	FTransform TrolleyChestTargetWorldTransform = FTransform::Identity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Pose", meta=(AllowPrivateAccess="true"))
	FVector TrolleyHandleLeftMeshSpace = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Pose", meta=(AllowPrivateAccess="true"))
	FVector TrolleyHandleRightMeshSpace = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Pose", meta=(AllowPrivateAccess="true"))
	FVector TrolleyChestTargetMeshSpace = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Pose", meta=(AllowPrivateAccess="true"))
	FTransform TrolleyHandleLeftMeshTransform = FTransform::Identity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Pose", meta=(AllowPrivateAccess="true"))
	FTransform TrolleyHandleRightMeshTransform = FTransform::Identity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Pose", meta=(AllowPrivateAccess="true"))
	FTransform TrolleyChestTargetMeshTransform = FTransform::Identity;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Pose", meta=(AllowPrivateAccess="true"))
	FRotator TrolleyDesiredBodyFacing = FRotator::ZeroRotator;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Pose", meta=(AllowPrivateAccess="true"))
	float TrolleyForwardSpeedLocal = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Pose", meta=(AllowPrivateAccess="true"))
	float TrolleyRightSpeedLocal = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Pose", meta=(AllowPrivateAccess="true"))
	float TrolleySpeedNormalized = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Pose", meta=(AllowPrivateAccess="true"))
	float TrolleyStrainAlpha = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Pose", meta=(AllowPrivateAccess="true"))
	float TrolleyForwardBlend = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Pose", meta=(AllowPrivateAccess="true"))
	float TrolleyRightBlend = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Pose", meta=(AllowPrivateAccess="true"))
	float TrolleyLeanPitchDeg = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Pose", meta=(AllowPrivateAccess="true"))
	float TrolleyLeanRollDeg = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Pose", meta=(AllowPrivateAccess="true"))
	float TrolleyForwardInput = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Pose", meta=(AllowPrivateAccess="true"))
	float TrolleyRightInput = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Placement", meta=(AllowPrivateAccess="true"))
	EAgentFactoryPlacementType CurrentFactoryPlacementType = EAgentFactoryPlacementType::Conveyor;

	bool bViewModeInitialized = false;
	bool bDefaultUseControllerRotationYaw = false;
	bool bDefaultOrientRotationToMovement = true;
	bool bThirdPersonTransitionActive = false;
	bool bPendingThirdPersonSwapFromDroneCamera = false;
	bool bViewModeButtonHeld = false;
	bool bViewModeHoldTriggered = false;
	bool bHasStoredDefaultViewPitchLimits = false;
	bool bKeyboardMapButtonHeld = false;
	bool bKeyboardMapButtonTriggeredMiniMap = false;
	bool bControllerMapButtonHeld = false;
	bool bControllerMapButtonTriggeredMiniMap = false;
	bool bHookJobButtonHeld = false;
	bool bHookJobButtonTriggeredHoldRelease = false;
	bool bRollModeHoldStartedInFlight = false;
	bool bPendingRollReleaseToFlight = false;
	bool bCrashRollRecoveryActive = false;
	bool bRollModeJumpHeld = false;
	bool bPickupRotationModeActive = false;
	bool bHeldPickupUsesDroneView = false;

	float ThirdPersonTransitionElapsed = 0.0f;
	float ViewModeButtonHeldDuration = 0.0f;
	float KeyboardMapButtonHeldDuration = 0.0f;
	float ControllerMapButtonHeldDuration = 0.0f;
	float HookJobButtonHeldDuration = 0.0f;
	float DroneWorldDiscoveryTimeRemaining = 0.0f;
	float DefaultViewPitchMin = -89.9f;
	float DefaultViewPitchMax = 89.9f;
	float CrashRollRecoveryStableTime = 0.0f;
	float RollToFlightAssistTimeRemaining = 0.0f;
	float RollModeJumpHeldDuration = 0.0f;
	float StoredRollJumpChargeAlpha = 0.0f;
	float HeldPickupDistance = 0.0f;
	float HeldPickupMassKg = 0.0f;
	float HeldPickupOriginalLinearDamping = 0.0f;
	float HeldPickupOriginalAngularDamping = 0.0f;
	FVector ThirdPersonTransitionStartLocation = FVector::ZeroVector;
	FRotator ThirdPersonTransitionStartRotation = FRotator::ZeroRotator;
	FVector PendingConveyorPlacementLocation = FVector::ZeroVector;
	FRotator PendingConveyorPlacementRotation = FRotator::ZeroRotator;
	int32 ConveyorPlacementRotationSteps = 0;
	TWeakObjectPtr<UPrimitiveComponent> PickupCandidateComponent;
	TWeakObjectPtr<UPrimitiveComponent> HeldPickupComponent;
	FVector PickupCandidateLocation = FVector::ZeroVector;
	FVector HeldPickupLocalGrabOffset = FVector::ZeroVector;
	FRotator HeldPickupViewRelativeRotationOffset = FRotator::ZeroRotator;
	FRotator HeldPickupTargetRotation = FRotator::ZeroRotator;
	bool bPickupCandidateValid = false;
	bool bKeyboardPickupHeld = false;
	bool bControllerPickupHeld = false;

	float DroneGamepadThrottleInput = 0.0f;
	float DroneGamepadYawInput = 0.0f;
	float DroneGamepadRollInput = 0.0f;
	float DroneGamepadPitchInput = 0.0f;
	float DroneGamepadLeftTriggerInput = 0.0f;
	float DroneGamepadRightTriggerInput = 0.0f;

	bool bDronePitchForwardHeld = false;
	bool bDronePitchBackwardHeld = false;
	bool bDroneRollLeftHeld = false;
	bool bDroneRollRightHeld = false;
	bool bDroneYawLeftHeld = false;
	bool bDroneYawRightHeld = false;
	bool bDroneThrottleUpHeld = false;
	bool bDroneThrottleDownHeld = false;
	bool bInteractKeyDrivingDroneThrottle = false;
	float TrolleyGripRegrabCooldownRemaining = 0.0f;
	float TrolleyPrevForwardSpeedLocal = 0.0f;
	float TrolleyPrevRightSpeedLocal = 0.0f;

public:
	UFUNCTION(BlueprintPure, Category="Vehicle|Pose")
	bool IsTrolleyPoseActive() const { return bTrolleyPoseActive; }

	UFUNCTION(BlueprintPure, Category="Vehicle|Pose")
	bool IsTrolleyGripBroken() const { return bTrolleyGripBroken; }

	UFUNCTION(BlueprintPure, Category="Vehicle|Pose")
	float GetTrolleyPoseBlendAlpha() const { return TrolleyPoseBlendAlpha; }

	UFUNCTION(BlueprintPure, Category="Vehicle|Pose")
	float GetTrolleyLeftHandIKAlpha() const { return TrolleyLeftHandIKAlpha; }

	UFUNCTION(BlueprintPure, Category="Vehicle|Pose")
	float GetTrolleyRightHandIKAlpha() const { return TrolleyRightHandIKAlpha; }

	UFUNCTION(BlueprintPure, Category="Vehicle|Pose")
	FVector GetTrolleyHandleLeftWorld() const { return TrolleyHandleLeftWorld; }

	UFUNCTION(BlueprintPure, Category="Vehicle|Pose")
	FVector GetTrolleyHandleRightWorld() const { return TrolleyHandleRightWorld; }

	UFUNCTION(BlueprintPure, Category="Vehicle|Pose")
	FVector GetTrolleyChestTargetWorld() const { return TrolleyChestTargetWorld; }

	UFUNCTION(BlueprintPure, Category="Vehicle|Pose")
	FVector GetTrolleyHandleLeftMeshSpace() const { return TrolleyHandleLeftMeshSpace; }

	UFUNCTION(BlueprintPure, Category="Vehicle|Pose")
	FVector GetTrolleyHandleRightMeshSpace() const { return TrolleyHandleRightMeshSpace; }

	UFUNCTION(BlueprintPure, Category="Vehicle|Pose")
	FVector GetTrolleyChestTargetMeshSpace() const { return TrolleyChestTargetMeshSpace; }

	UFUNCTION(BlueprintPure, Category="Vehicle|Pose")
	FTransform GetTrolleyHandleLeftWorldTransform() const { return TrolleyHandleLeftWorldTransform; }

	UFUNCTION(BlueprintPure, Category="Vehicle|Pose")
	FTransform GetTrolleyHandleRightWorldTransform() const { return TrolleyHandleRightWorldTransform; }

	UFUNCTION(BlueprintPure, Category="Vehicle|Pose")
	FTransform GetTrolleyChestTargetWorldTransform() const { return TrolleyChestTargetWorldTransform; }

	UFUNCTION(BlueprintPure, Category="Vehicle|Pose")
	FTransform GetTrolleyHandleLeftMeshTransform() const { return TrolleyHandleLeftMeshTransform; }

	UFUNCTION(BlueprintPure, Category="Vehicle|Pose")
	FTransform GetTrolleyHandleRightMeshTransform() const { return TrolleyHandleRightMeshTransform; }

	UFUNCTION(BlueprintPure, Category="Vehicle|Pose")
	FTransform GetTrolleyChestTargetMeshTransform() const { return TrolleyChestTargetMeshTransform; }

	UFUNCTION(BlueprintPure, Category="Vehicle|Pose")
	float GetTrolleyForwardBlend() const { return TrolleyForwardBlend; }

	UFUNCTION(BlueprintPure, Category="Vehicle|Pose")
	float GetTrolleyRightBlend() const { return TrolleyRightBlend; }

	UFUNCTION(BlueprintPure, Category="Vehicle|Pose")
	float GetTrolleyLeanPitchDeg() const { return TrolleyLeanPitchDeg; }

	UFUNCTION(BlueprintPure, Category="Vehicle|Pose")
	float GetTrolleyLeanRollDeg() const { return TrolleyLeanRollDeg; }

	UFUNCTION(BlueprintPure, Category="Vehicle|Pose")
	float GetTrolleyStrainAlpha() const { return TrolleyStrainAlpha; }

	UFUNCTION(BlueprintPure, Category="Vehicle|Pose")
	float GetTrolleySpeedNormalized() const { return TrolleySpeedNormalized; }

	UFUNCTION(BlueprintPure, Category="Vehicle|Pose")
	float GetTrolleyForwardSpeedLocal() const { return TrolleyForwardSpeedLocal; }

	UFUNCTION(BlueprintPure, Category="Vehicle|Pose")
	float GetTrolleyRightSpeedLocal() const { return TrolleyRightSpeedLocal; }

	UFUNCTION(BlueprintPure, Category="Vehicle|Pose")
	float GetTrolleyForwardInput() const { return TrolleyForwardInput; }

	UFUNCTION(BlueprintPure, Category="Vehicle|Pose")
	float GetTrolleyRightInput() const { return TrolleyRightInput; }

	UFUNCTION(BlueprintPure, Category="Drone|Availability")
	bool IsPrimaryDroneAvailable() const { return bPrimaryDroneAvailable; }

	UFUNCTION(BlueprintPure, Category="Drone|Battery")
	float GetPersistedPrimaryDroneBatteryPercent() const { return PersistedPrimaryDroneBatteryPercent; }

	UFUNCTION(BlueprintCallable, Category="Drone|Availability")
	void SetPrimaryDroneAvailable(bool bNewAvailable, bool bForceFirstPersonIfUnavailable = true);

	bool ShouldUseAttachedLocomotionProxy() const;
	FVector GetAttachedLocomotionVelocityWorld() const;

	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	FORCEINLINE UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }
};
