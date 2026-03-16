// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentBeamToolComponent.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Logging/LogMacros.h"
#include "AgentCharacter.generated.h"

class ADroneCompanion;
class AConveyorBeltStraight;
class AConveyorPlacementPreview;
class AMaterialNodeActor;
class AMachineActor;
class AMinerActor;
class AMiningSwarmMachine;
class AStorageBin;
class AActor;
class UVehicleInteractionComponent;
class UBackAttachmentComponent;
class UPlayerMagnetComponent;
class UAgentScannerComponent;
class UCameraComponent;
class UDroneSwarmComponent;
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
	Machine,
	Miner,
	MiningSwarm
};

UENUM(BlueprintType)
enum class EAgentRagdollState : uint8
{
	Normal,
	Ragdoll,
	Recovering
};

UENUM(BlueprintType)
enum class EAgentRagdollReason : uint8
{
	ManualInput,
	ClumsinessTrip,
	ClumsinessSlip,
	Impact,
	Scripted
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

	/** Native first-person fallback camera; gameplay prefers FirstPersonCameraComponentName when available. */
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

	/** Reusable backpack deploy/recall manager used by carried world items */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	UBackAttachmentComponent* BackAttachmentComponent;

	/** Player-owned backpack magnet settings and attach target. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	UPlayerMagnetComponent* PlayerMagnetComponent;

	/** Reusable beam tool for player-held drain/heal interactions. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	UAgentBeamToolComponent* PlayerBeamToolComponent;

	/** Passive scanner that reveals target health and composition while aiming. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	UAgentScannerComponent* ScannerComponent;

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
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode) override;
	virtual void Landed(const FHitResult& Hit) override;
	virtual void NotifyHit(
		UPrimitiveComponent* MyComp,
		AActor* Other,
		UPrimitiveComponent* OtherComp,
		bool bSelfMoved,
		FVector HitLocation,
		FVector HitNormal,
		FVector NormalImpulse,
		const FHitResult& Hit) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
	void RefreshFirstPersonCameraReference();
	void RefreshFirstPersonCameraAttachment();
	void RefreshFirstPersonCameraControlRotation();
	UCameraComponent* ResolveFirstPersonCamera();
	UCameraComponent* ResolveFirstPersonCamera() const;

	void Move(const FInputActionValue& Value);
	void StopMove(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void OnRagdollTogglePressed();
	void OnWalkModifierPressed();
	void OnSprintModifierPressed();
	void OnSprintModifierReleased();
	void UpdateMovementSpeedState(float DeltaSeconds);
	float ResolveTargetGroundSpeed() const;
	bool IsWalkModeActive() const;
	bool IsSprintModeActive() const;
	bool CanEnterRagdoll() const;
	bool CanExitRagdoll() const;
	void EnterRagdollInternal(EAgentRagdollReason Reason);
	void ExitRagdollInternal(EAgentRagdollReason Reason);
	void SetRagdollState(EAgentRagdollState NewState, EAgentRagdollReason Reason);
	EAgentViewMode ResolveRagdollEntryViewMode() const;
	FName ResolveRagdollAnchorBoneName() const;
	FVector GetRagdollAnchorLocation() const;
	FRotator GetRagdollAnchorRotation() const;
	void UpdateRagdollAutoRecovery(float DeltaSeconds);
	void ResetRagdollInputState();

	void OnViewModeButtonPressed();
	void OnViewModeButtonReleased();
	void OnCycleActiveDronePressed();
	void OnDroneTorchTogglePressed();
	void OnLeftMouseButtonPressed();
	void OnLeftMouseButtonReleased();
	void OnRightMouseButtonPressed();
	void OnRightMouseButtonReleased();
	void OnMouseScrollUpPressed();
	void OnMouseScrollDownPressed();
	void OnGamepadFaceButtonLeftPressed();
	void OnGamepadFaceButtonRightPressed();
	void OnGamepadLeftShoulderPressed();
	void CycleBeamMode(int32 Direction = 1);
	void ApplyBeamModeToEmitters();
	void StopAllBeamTools();
	void UpdateBeamSystems(float DeltaSeconds);
	void UpdateScannerSystems(float DeltaSeconds);
	void UpdateBeamAimZoom(float DeltaSeconds);
	bool IsRawMouseBeamAimModifierHeld() const;
	bool IsRawControllerBeamAimModifierHeld() const;
	bool IsBeamAimModifierActive() const;
	bool IsBeamFireInputHeld() const;
	bool CanUseBeamTool() const;
	UAgentBeamToolComponent* ResolveActiveBeamToolComponent() const;
	USceneComponent* ResolveActiveBeamOriginComponent() const;
	bool ResolveActiveBeamPose(FVector& OutViewOrigin, FVector& OutViewDirection, FVector& OutVisualOrigin) const;
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
	void UpdateThirdPersonCameraChase(float DeltaSeconds);
	void UpdateDroneCompanionThirdPersonTarget();
	void UpdateDroneTorchTarget();
	void UpdateDronePilotInputs(float DeltaSeconds);
	void UpdateThirdPersonTransition(float DeltaSeconds);
	bool GetThirdPersonDroneTarget(FVector& OutLocation, FRotator& OutRotation) const;
	FVector ResolveThirdPersonCameraFocusLocation() const;
	float ComputeThirdPersonCameraChaseAlpha(float DistanceToTarget) const;
	void ResetThirdPersonCameraChaseState();
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
	AMaterialNodeActor* ResolveMaterialNodeFromPlacementHit(const FHitResult& AimHit) const;
	AMaterialNodeActor* ResolveMaterialNodeAtPlacementLocation(const FVector& PlacementLocation) const;
	void SelectFactoryPlacementType(EAgentFactoryPlacementType NewType, bool bToggleIfAlreadySelected);
	UClass* ResolveFactoryBuildableClass(EAgentFactoryPlacementType PlacementType) const;
	bool IsFreeFactoryPlacementActive() const;
	void UpdateFactoryPlacementRotationHold(float DeltaSeconds);
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
	bool TryToggleHeldBackpackPortalFromPickup();
	bool TryToggleHeldDronePowerFromPickup();
	bool TryToggleDroneLiftAssist();
	void SyncDroneLiftAssistTuning() const;
	void AdjustDroneLiftAssistStrength(float Delta);
	void AdjustPickupStrength(float Delta);
	void AdjustClumsiness(float Delta, bool bShowFeedback = true);
	float GetMostClumsyAlpha() const;
	float ComputeTripChance(float StepUpHeightCm, float HorizontalSpeedCmPerSec, bool bMovingBackward) const;
	float ComputeTripRagdollFollowThroughChance() const;
	float ComputeDroneImpactKnockdownChance(float ClosingSpeed, bool bHeadHit, bool bLegHit) const;
	void HandleStepUpEvent(float StepUpHeightCm);
	void UpdateClumsinessSystems(float DeltaSeconds);
	void UpdateFallHeightTracking();
	void AddInstability(float InstabilityDelta, EAgentRagdollReason ThresholdReason, FName SourceTag, bool bAllowThresholdRagdoll = true);
	bool TryTriggerAutoRagdoll(EAgentRagdollReason Reason, FName SourceTag);
	float ComputeImpactClosingSpeed(const UPrimitiveComponent* OtherComp, const FVector& HitNormal) const;
	float ComputeImpactMagnitude(const FVector& NormalImpulse, const UPrimitiveComponent* OtherComp, const FVector& HitNormal) const;
	void ShowClumsinessDebug() const;
	void ShowPickupStrengthDebug() const;
	bool TryAssignActiveDroneToHookJob(bool bCycleAfterAssign);
	bool TryReleaseAllHookJobDrones();
	void UpdateHookJobButtonHold(float DeltaSeconds);
	bool CanUseBackpackDeployInput() const;
	void UpdateBackpackDeployButtonHold(float DeltaSeconds);
	void ResetBackpackDeployButtonHoldState();

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
	void OnBackpackOrDroneModePressed();
	void OnBackpackOrDroneModeReleased();
	void OnBackpackOrDroneCameraDownPressed();
	void OnBackpackOrDroneCameraDownReleased();
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
	void OnMinerPlacementModePressed();
	void OnMiningSwarmPlacementModePressed();
	void OnFactoryPlacementTogglePressed();
	void OnFactoryFreePlacementTogglePressed();
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

public:
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	UFUNCTION(BlueprintCallable, Category="Ragdoll")
	void ToggleRagdoll();

	UFUNCTION(BlueprintCallable, Category="Ragdoll")
	bool RequestEnterRagdoll(EAgentRagdollReason Reason);

	UFUNCTION(BlueprintCallable, Category="Ragdoll")
	bool RequestExitRagdoll(EAgentRagdollReason Reason);

	UFUNCTION(BlueprintPure, Category="Ragdoll")
	bool IsRagdolling() const { return RagdollState == EAgentRagdollState::Ragdoll; }

	UFUNCTION(BlueprintPure, Category="Ragdoll")
	EAgentRagdollState GetRagdollState() const { return RagdollState; }

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam")
	EAgentBeamMode CurrentBeamMode = EAgentBeamMode::Damage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam")
	bool bAllowBeamInDronePilotMode = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam")
	bool bAllowBeamInMapMode = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam", meta=(ClampMin="0.0", UIMin="0.0", ClampMax="1.0", UIMax="1.0"))
	float ControllerBeamAimTriggerThreshold = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam|Zoom", meta=(ClampMin="0.0", UIMin="0.0"))
	float BeamAimZoomFovReduction = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam|Zoom", meta=(ClampMin="0.0", UIMin="0.0"))
	float BeamAimZoomInInterpSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam|Zoom", meta=(ClampMin="0.0", UIMin="0.0"))
	float BeamAimZoomOutInterpSpeed = 8.0f;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|FirstPerson")
	bool bUsePawnControlRotationInFirstPerson = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|FirstPerson")
	bool bUsePawnControlRotationWhileRagdoll = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|FirstPerson")
	FName FirstPersonCameraComponentName = FName(TEXT("FirstPersonCamera1"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|FirstPerson")
	FName FirstPersonCameraSocketName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	float ThirdPersonDroneSwapMaxDistance = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	float ThirdPersonDroneTransitionDuration = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	float ThirdPersonDroneProxyScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|ThirdPerson")
	bool bEnableThirdPersonCameraChase = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|ThirdPerson")
	bool bThirdPersonCameraTargetTracksCharacterMesh = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|ThirdPerson", meta=(ClampMin="50.0", UIMin="50.0"))
	float ThirdPersonCameraRagdollMinDistance = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|ThirdPerson", meta=(ClampMin="100.0", UIMin="100.0"))
	float ThirdPersonCameraRagdollMaxDistance = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|ThirdPerson")
	FName ThirdPersonCameraTrackBoneName = FName(TEXT("pelvis"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|ThirdPerson")
	bool bThirdPersonCameraLookAtCharacter = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|ThirdPerson")
	float ThirdPersonCameraLookAtHeightOffset = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|ThirdPerson", meta=(ClampMin="0.0", UIMin="0.0"))
	float ThirdPersonCameraChaseNearDistance = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|ThirdPerson", meta=(ClampMin="1.0", UIMin="1.0"))
	float ThirdPersonCameraChaseFarDistance = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|ThirdPerson", meta=(ClampMin="0.1", UIMin="0.1"))
	float ThirdPersonCameraChaseDistanceExponent = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|ThirdPerson", meta=(ClampMin="0.0", UIMin="0.0"))
	float ThirdPersonCameraChaseNearLocationInterpSpeed = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|ThirdPerson", meta=(ClampMin="0.0", UIMin="0.0"))
	float ThirdPersonCameraChaseFarLocationInterpSpeed = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|ThirdPerson", meta=(ClampMin="0.0", UIMin="0.0"))
	float ThirdPersonCameraChaseNearRotationInterpSpeed = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera|ThirdPerson", meta=(ClampMin="0.0", UIMin="0.0"))
	float ThirdPersonCameraChaseFarRotationInterpSpeed = 14.0f;

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

	/** Time in seconds to ramp Hold-B backpack magnet strength from 0 to 100%. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Backpack", meta=(ClampMin="0.05", UIMin="0.05", DisplayName="Backpack Magnet Charge Time"))
	float BackpackDeployHoldTime = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Speed", meta=(ClampMin="0.0", UIMin="0.0"))
	float WalkModeSpeed = 230.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Speed", meta=(ClampMin="0.0", UIMin="0.0"))
	float RunModeSpeed = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Speed", meta=(ClampMin="0.0", UIMin="0.0"))
	float SprintModeSpeed = 900.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Speed", meta=(ClampMin="0.0", UIMin="0.0"))
	float MovementSpeedInterpSpeed = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Movement|Safety")
	bool bEnableControllerSafeModeToggle = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ragdoll|Input")
	bool bEnableRagdollToggleInput = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ragdoll|Entry")
	bool bAttemptVehicleExitOnRagdollEntry = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ragdoll|Entry")
	bool bForceCharacterViewOnRagdollEntry = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ragdoll|Entry")
	EAgentViewMode RagdollEntryViewMode = EAgentViewMode::ThirdPerson;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ragdoll|Entry")
	bool bExitConveyorPlacementOnRagdollEntry = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ragdoll|Entry")
	bool bReleaseHeldPickupOnRagdollEntry = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ragdoll|Entry")
	bool bReleaseHookJobDronesOnRagdollEntry = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ragdoll|Physics")
	FName RagdollAnchorBoneName = FName(TEXT("pelvis"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ragdoll|Physics")
	FName RagdollCollisionProfileName = FName(TEXT("Ragdoll"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ragdoll|Recovery")
	float RagdollRecoveryHeightOffset = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ragdoll|Recovery")
	bool bSweepCapsuleOnRagdollRecovery = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ragdoll|Recovery")
	bool bAllowTeleportRecoveryFallback = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ragdoll|Recovery")
	bool bUseRagdollYawOnRecovery = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ragdoll|Recovery")
	bool bRestorePreRagdollMovementMode = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ragdoll|Recovery")
	bool bEnableAutoRagdollRecovery = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ragdoll|Recovery", meta=(ClampMin="0.0", UIMin="0.0"))
	float AutoRagdollRecoveryMinDuration = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ragdoll|Recovery", meta=(ClampMin="0.0", UIMin="0.0"))
	float AutoRagdollRecoveryMaxLinearSpeed = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ragdoll|Recovery", meta=(ClampMin="0.0", UIMin="0.0"))
	float AutoRagdollRecoveryMaxAngularSpeedDeg = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ragdoll|Recovery", meta=(ClampMin="0.0", UIMin="0.0"))
	float AutoRagdollRecoveryStableDuration = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ragdoll|Control")
	bool bAllowLookInputWhileRagdoll = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ragdoll|Control")
	bool bAllowViewModeChangesWhileRagdoll = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ragdoll|Debug")
	bool bLogRagdollStateChanges = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Ragdoll|Debug")
	bool bLogAutoRagdollRecovery = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Skill", meta=(ClampMin="1.0", UIMin="1.0"))
	float ClumsinessMinValue = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Skill", meta=(ClampMin="1.0", UIMin="1.0"))
	float ClumsinessMaxValue = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Skill", meta=(ClampMin="1.0", UIMin="1.0"))
	float ClumsinessValue = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Skill")
	bool bStartClumsinessAtMinimum = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Skill")
	bool bHigherClumsinessValueMeansMoreClumsy = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Input", meta=(ClampMin="0.0", UIMin="0.0"))
	float ClumsinessBracketStep = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Trip")
	bool bEnableClumsinessTrip = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Trip")
	bool bDisableTripWhileWalkMode = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Trip", meta=(ClampMin="0.0", UIMin="0.0"))
	float TripBaseChance = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Trip", meta=(ClampMin="0.0", UIMin="0.0"))
	float TripLeastClumsyMultiplier = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Trip", meta=(ClampMin="0.0", UIMin="0.0"))
	float TripMostClumsyMultiplier = 2.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Trip", meta=(ClampMin="0.0", UIMin="0.0"))
	float TripStepUpMinHeightCm = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Trip", meta=(ClampMin="0.0", UIMin="0.0"))
	float TripStepUpMaxHeightCm = 36.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Trip", meta=(ClampMin="0.0", UIMin="0.0"))
	float TripStepUpMinMultiplier = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Trip", meta=(ClampMin="0.0", UIMin="0.0"))
	float TripStepUpMaxMultiplier = 3.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Trip", meta=(ClampMin="0.1", UIMin="0.1"))
	float TripStepUpExponent = 1.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Trip", meta=(ClampMin="0.0", UIMin="0.0"))
	float TripSpeedMinCmPerSecond = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Trip", meta=(ClampMin="0.0", UIMin="0.0"))
	float TripSpeedMaxCmPerSecond = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Trip", meta=(ClampMin="0.0", UIMin="0.0"))
	float TripSpeedMinMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Trip", meta=(ClampMin="0.0", UIMin="0.0"))
	float TripSpeedMaxMultiplier = 2.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Trip", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float TripBackwardDotThreshold = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Trip", meta=(ClampMin="1.0", UIMin="1.0"))
	float TripBackwardMultiplier = 1.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Trip", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float TripChanceMaxClamp = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Trip", meta=(ClampMin="0.0", UIMin="0.0"))
	float TripEventCooldownSeconds = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Trip")
	bool bEnableTripFollowThroughRoll = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Trip", meta=(ClampMin="1.0", UIMin="1.0"))
	float TripFollowThroughLowSkillLevel = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Trip", meta=(ClampMin="1.0", UIMin="1.0"))
	float TripFollowThroughHighSkillLevel = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Trip", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float TripFollowThroughRagdollChanceAtLowSkill = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Trip", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float TripFollowThroughRagdollChanceAtHighSkill = 0.0001f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Trip", meta=(ClampMin="0.0", UIMin="0.0", DisplayName="Trip Skill Gain Per 1.0 Chance"))
	float TripSkillGainOnTrigger = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Instability")
	bool bEnableInstabilityMeter = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Instability", meta=(ClampMin="1.0", UIMin="1.0"))
	float InstabilityMaxValue = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Instability", meta=(ClampMin="0.0", UIMin="0.0"))
	float InstabilityRagdollThreshold = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Instability", meta=(ClampMin="0.0", UIMin="0.0"))
	float InstabilityDecayPerSecond = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Instability", meta=(ClampMin="0.0", UIMin="0.0"))
	float AutoRagdollCooldownSeconds = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Instability")
	bool bResetInstabilityOnRagdollEntry = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Fall")
	bool bEnableFallInstability = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Fall", meta=(ClampMin="0.0", UIMin="0.0"))
	float MinFallHeightForRagdollCm = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Fall", meta=(ClampMin="0.0", UIMin="0.0"))
	float FallInstabilityStartSpeed = 550.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Fall", meta=(ClampMin="0.0", UIMin="0.0"))
	float FallInstabilityMaxSpeed = 1400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Fall", meta=(ClampMin="0.0", UIMin="0.0"))
	float FallInstabilityMinAdd = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Fall", meta=(ClampMin="0.0", UIMin="0.0"))
	float FallInstabilityMaxAdd = 82.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Fall", meta=(ClampMin="0.0", UIMin="0.0"))
	float FallHardRagdollSpeed = 1850.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Impact")
	bool bEnableImpactInstability = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Impact")
	bool bEnableImpactVelocityRagdoll = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Impact")
	bool bImpactVelocityRequiresMovingOther = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Impact", meta=(ClampMin="0.0", UIMin="0.0"))
	float ImpactVelocityMovingOtherMinSpeed = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Impact", meta=(ClampMin="0.0", UIMin="0.0"))
	float ImpactVelocityRagdollThreshold = 1100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Impact|Drone")
	bool bEnableDroneImpactKnockdownChance = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Impact|Drone")
	bool bDroneImpactRequiresDroneMovement = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Impact|Drone", meta=(ClampMin="0.0", UIMin="0.0"))
	float DroneImpactMinDroneSpeed = 140.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Impact|Drone", meta=(ClampMin="0.0", UIMin="0.0"))
	float DroneImpactChanceMinClosingSpeed = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Impact|Drone", meta=(ClampMin="0.0", UIMin="0.0"))
	float DroneImpactChanceMaxClosingSpeed = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Impact|Drone", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float DroneImpactMinKnockdownChance = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Impact|Drone", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float DroneImpactMaxKnockdownChance = 0.92f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Impact|Drone", meta=(ClampMin="0.0", UIMin="0.0"))
	float DroneImpactHeadHitChanceMultiplier = 1.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Impact|Drone", meta=(ClampMin="0.0", UIMin="0.0"))
	float DroneImpactLegHitChanceMultiplier = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Impact|Drone", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float DroneImpactChanceMaxClamp = 0.98f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Impact|Drone")
	FName DroneImpactHeadBoneName = FName(TEXT("head"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Impact|Drone", meta=(ClampMin="0.0", UIMin="0.0"))
	float DroneImpactHeadRadiusCm = 32.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Impact|Drone", meta=(ClampMin="0.0", UIMin="0.0"))
	float DroneImpactLegMaxHeightFromFeetCm = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Impact|Drone")
	bool bShowDroneImpactKnockdownDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Impact", meta=(ClampMin="0.0", UIMin="0.0"))
	float ImpactInstabilityStartImpulse = 12000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Impact", meta=(ClampMin="0.0", UIMin="0.0"))
	float ImpactInstabilityMaxImpulse = 52000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Impact", meta=(ClampMin="0.0", UIMin="0.0"))
	float ImpactInstabilityMinAdd = 16.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Impact", meta=(ClampMin="0.0", UIMin="0.0"))
	float ImpactInstabilityMaxAdd = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Impact", meta=(ClampMin="0.0", UIMin="0.0"))
	float ImpactHardRagdollImpulse = 68000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Impact", meta=(ClampMin="0.1", UIMin="0.1"))
	float ImpactPhysicsBodyMultiplier = 1.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Debug")
	bool bShowClumsinessDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Debug")
	bool bShowTripEventDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Debug", meta=(ClampMin="0.0", UIMin="0.0"))
	float TripEventDebugDuration = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Clumsiness|Debug")
	bool bLogClumsinessEvents = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Placement")
	TSubclassOf<AConveyorBeltStraight> ConveyorBeltClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Placement")
	TSubclassOf<AConveyorPlacementPreview> ConveyorPlacementPreviewClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Placement")
	TSubclassOf<AStorageBin> StorageBinClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Placement")
	TSubclassOf<AMachineActor> MachineClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Placement")
	TSubclassOf<AMinerActor> MinerClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Placement")
	TSubclassOf<AMiningSwarmMachine> MiningSwarmClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Placement")
	float ConveyorPlacementTraceDistance = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Placement")
	float ConveyorPlacementSurfaceTraceHeight = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Placement")
	float ConveyorPlacementSurfaceTraceDepth = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Placement")
	FVector ConveyorPlacementClearanceExtents = FVector(48.0f, 48.0f, 12.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Placement", meta=(ClampMin="0.1", UIMin="0.1"))
	float FreePlacementRotationStepDegrees = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Placement", meta=(ClampMin="0.0", UIMin="0.0"))
	float FreePlacementRotationHoldDelay = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Placement", meta=(ClampMin="0.01", UIMin="0.01"))
	float FreePlacementRotationHoldInterval = 0.06f;

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

protected:
	UPROPERTY(Transient)
	TObjectPtr<UCameraComponent> ResolvedFirstPersonCamera = nullptr;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ragdoll", meta=(AllowPrivateAccess="true"))
	EAgentRagdollState RagdollState = EAgentRagdollState::Normal;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ragdoll", meta=(AllowPrivateAccess="true"))
	float RagdollElapsedDuration = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ragdoll", meta=(AllowPrivateAccess="true"))
	float RagdollStableDuration = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ragdoll", meta=(AllowPrivateAccess="true"))
	float LastRagdollLinearSpeed = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Ragdoll", meta=(AllowPrivateAccess="true"))
	float LastRagdollAngularSpeedDeg = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Clumsiness", meta=(AllowPrivateAccess="true"))
	float InstabilityValue = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Clumsiness", meta=(AllowPrivateAccess="true"))
	float LastComputedTripChance = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Clumsiness", meta=(AllowPrivateAccess="true"))
	float LastStepUpHeightCm = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Clumsiness", meta=(AllowPrivateAccess="true"))
	float LastFallImpactSpeed = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Clumsiness", meta=(AllowPrivateAccess="true"))
	float LastFallHeightCm = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Clumsiness", meta=(AllowPrivateAccess="true"))
	float LastImpactMagnitude = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Clumsiness", meta=(AllowPrivateAccess="true"))
	float LastImpactClosingSpeed = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Clumsiness", meta=(AllowPrivateAccess="true"))
	FName LastClumsinessEventSource = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Clumsiness", meta=(AllowPrivateAccess="true"))
	float LastTripRoll = -1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Clumsiness", meta=(AllowPrivateAccess="true"))
	float LastTripHorizontalSpeed = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Clumsiness", meta=(AllowPrivateAccess="true"))
	bool bLastTripMovingBackward = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Clumsiness", meta=(AllowPrivateAccess="true"))
	bool bLastTripTriggeredRagdoll = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Clumsiness", meta=(AllowPrivateAccess="true"))
	float LastTripFollowThroughChance = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Clumsiness", meta=(AllowPrivateAccess="true"))
	float LastTripFollowThroughRoll = -1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Clumsiness", meta=(AllowPrivateAccess="true"))
	bool bLastTripFollowThroughPreventedRagdoll = false;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Placement", meta=(AllowPrivateAccess="true"))
	bool bFactoryFreePlacementEnabled = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Placement", meta=(AllowPrivateAccess="true"))
	EAgentFactoryPlacementType CurrentFactoryPlacementType = EAgentFactoryPlacementType::Conveyor;

	bool bViewModeInitialized = false;
	bool bDefaultUseControllerRotationYaw = false;
	bool bDefaultOrientRotationToMovement = true;
	bool bThirdPersonTransitionActive = false;
	bool bPendingThirdPersonSwapFromDroneCamera = false;
	bool bHasThirdPersonCameraChaseState = false;
	bool bViewModeButtonHeld = false;
	bool bViewModeHoldTriggered = false;
	bool bHasStoredDefaultViewPitchLimits = false;
	bool bKeyboardMapButtonHeld = false;
	bool bKeyboardMapButtonTriggeredMiniMap = false;
	bool bControllerMapButtonHeld = false;
	bool bControllerMapButtonTriggeredMiniMap = false;
	bool bBackpackKeyboardButtonHeld = false;
	bool bBackpackControllerButtonHeld = false;
	bool bHookJobButtonHeld = false;
	bool bHookJobButtonTriggeredHoldRelease = false;
	bool bRollModeHoldStartedInFlight = false;
	bool bPendingRollReleaseToFlight = false;
	bool bCrashRollRecoveryActive = false;
	bool bRollModeJumpHeld = false;
	bool bPickupRotationModeActive = false;
	bool bHeldPickupUsesDroneView = false;
	bool bTrackingFallHeight = false;
	bool bSprintModifierHeld = false;
	bool bSafeModeToggled = false;

	float ThirdPersonTransitionElapsed = 0.0f;
	float ViewModeButtonHeldDuration = 0.0f;
	float KeyboardMapButtonHeldDuration = 0.0f;
	float ControllerMapButtonHeldDuration = 0.0f;
	float BackpackKeyboardButtonHeldDuration = 0.0f;
	float BackpackControllerButtonHeldDuration = 0.0f;
	float HookJobButtonHeldDuration = 0.0f;
	float DroneWorldDiscoveryTimeRemaining = 0.0f;
	float AutoRagdollCooldownRemaining = 0.0f;
	float TripEventCooldownRemaining = 0.0f;
	float FallTrackingStartZ = 0.0f;
	float FallTrackingMinZ = 0.0f;
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
	FVector ThirdPersonCameraChaseLocation = FVector::ZeroVector;
	FRotator ThirdPersonCameraChaseRotation = FRotator::ZeroRotator;
	FVector PendingConveyorPlacementLocation = FVector::ZeroVector;
	FRotator PendingConveyorPlacementRotation = FRotator::ZeroRotator;
	float FactoryPlacementRotationYawDegrees = 0.0f;
	float FactoryPlacementRotationHoldTimeRemaining = 0.0f;
	int32 FactoryPlacementRotationHoldDirection = 0;
	TWeakObjectPtr<UPrimitiveComponent> PickupCandidateComponent;
	TWeakObjectPtr<UPrimitiveComponent> HeldPickupComponent;
	FVector PickupCandidateLocation = FVector::ZeroVector;
	FVector HeldPickupLocalGrabOffset = FVector::ZeroVector;
	FRotator HeldPickupViewRelativeRotationOffset = FRotator::ZeroRotator;
	FRotator HeldPickupTargetRotation = FRotator::ZeroRotator;
	bool bPickupCandidateValid = false;
	bool bKeyboardPickupHeld = false;
	bool bControllerPickupHeld = false;
	bool bMouseBeamFireHeld = false;
	bool bControllerBeamFireHeld = false;
	bool bRightMouseBeamAimHeld = false;
	bool bBeamAimZoomLocked = false;

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
	bool bPlacementRotateLeftHeld = false;
	bool bPlacementRotateRightHeld = false;
	bool bDroneThrottleUpHeld = false;
	bool bDroneThrottleDownHeld = false;
	bool bInteractKeyDrivingDroneThrottle = false;
	EAgentRagdollReason LastRagdollReason = EAgentRagdollReason::ManualInput;
	FTransform CachedMeshRelativeTransform = FTransform::Identity;
	FName CachedMeshCollisionProfileName = NAME_None;
	ECollisionEnabled::Type CachedMeshCollisionEnabled = ECollisionEnabled::NoCollision;
	ECollisionEnabled::Type CachedCapsuleCollisionEnabled = ECollisionEnabled::QueryAndPhysics;
	EMovementMode CachedMovementMode = MOVE_Walking;
	uint8 CachedCustomMovementMode = 0;
	EAgentViewMode PreRagdollViewMode = EAgentViewMode::ThirdPerson;
	float BeamAimBaseFov = 0.0f;
	float BeamAimCurrentFov = 0.0f;
	TWeakObjectPtr<AActor> BeamAimZoomViewTarget;

public:
	UFUNCTION(BlueprintPure, Category="Clumsiness")
	float GetClumsinessValue() const { return ClumsinessValue; }

	UFUNCTION(BlueprintPure, Category="Clumsiness")
	float GetInstabilityValue() const { return InstabilityValue; }

	UFUNCTION(BlueprintPure, Category="Drone|Availability")
	bool IsPrimaryDroneAvailable() const { return bPrimaryDroneAvailable; }

	UFUNCTION(BlueprintPure, Category="Drone|Battery")
	float GetPersistedPrimaryDroneBatteryPercent() const { return PersistedPrimaryDroneBatteryPercent; }

	UFUNCTION(BlueprintCallable, Category="Drone|Availability")
	void SetPrimaryDroneAvailable(bool bNewAvailable, bool bForceFirstPersonIfUnavailable = true);

	UFUNCTION(BlueprintPure, Category="Interaction|Backpack")
	UBackAttachmentComponent* GetBackAttachmentComponent() const { return BackAttachmentComponent; }

	UFUNCTION(BlueprintPure, Category="Interaction|Backpack")
	UPlayerMagnetComponent* GetPlayerMagnetComponent() const { return PlayerMagnetComponent; }

	UFUNCTION(BlueprintPure, Category="Scanner")
	UAgentScannerComponent* GetScannerComponent() const { return ScannerComponent; }

	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	FORCEINLINE UCameraComponent* GetFirstPersonCamera() const { return ResolveFirstPersonCamera(); }
};
