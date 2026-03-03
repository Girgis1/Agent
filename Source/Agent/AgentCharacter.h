// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "AgentCharacter.generated.h"

class ADroneCompanion;
class AConveyorBeltStraight;
class AConveyorPlacementPreview;
class AFactoryPayloadActor;
class AResourceSpawnerMachine;
class AStorageBin;
class UCameraComponent;
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
	ResourceSpawner
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
	AAgentCharacter();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	void Move(const FInputActionValue& Value);
	void StopMove(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);

	void OnViewModeButtonPressed();
	void OnViewModeButtonReleased();
	void UpdateViewModeButtonHold(float DeltaSeconds);
	void UpdateRollModeJumpHold(float DeltaSeconds);
	void CycleViewMode();
	void ApplyViewMode(EAgentViewMode NewMode, bool bBlend);
	void SpawnDroneCompanion();
	void SpawnDroneCompanionAtTransform(const FVector& SpawnLocation, const FRotator& SpawnRotation, EDroneCompanionMode InitialMode);
	void DespawnDroneCompanion();
	void ApplyDroneAssistState();
	void EnterRollTransitionMode(bool bTrackHeldReturn, bool bFromCrashRecovery);
	void ExitRollTransitionMode(bool bTryJumpLift);
	void SyncDroneCompanionControlState(bool bAllowRollControls, bool bSuppressCameraMountRefresh = false);
	void UpdateDroneCompanionThirdPersonTarget();
	void UpdateDronePilotInputs(float DeltaSeconds);
	void UpdateThirdPersonTransition(float DeltaSeconds);
	bool GetThirdPersonDroneTarget(FVector& OutLocation, FRotator& OutRotation) const;
	void ResetDroneInputState();
	void ResetCharacterCameraRoll();
	void UpdateDronePilotCameraLimits();
	void SyncControllerRotationToDroneCamera();
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
	void ApplyFactoryBuildableDefaults(AActor* SpawnedActor) const;
	bool CanUsePickupInteraction() const;
	bool GetActivePickupView(FVector& OutLocation, FRotator& OutRotation) const;
	void UpdatePickupInteraction();
	bool UpdatePickupCandidate();
	void DrawPickupInfluenceDebug(const FVector& SphereCenter, const FColor& Color) const;
	bool CanPickupComponent(const UPrimitiveComponent* PrimitiveComponent) const;
	bool TryBeginPickup();
	void EndPickup();
	void UpdateHeldPickup();
	void SyncPickupHandleSettings() const;

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
	void OnResourceSpawnerPlacementModePressed();
	void OnFactoryPlacementTogglePressed();
	void OnConveyorPlacePressed();
	void OnConveyorCancelPressed();
	void OnConveyorRotateLeftPressed();
	void OnConveyorRotateRightPressed();
	void OnPickupOrDroneYawRightPressed();
	void OnPickupOrDroneYawRightReleased();
	void OnPickupOrPlacePressed();
	void OnPickupOrPlaceReleased();

public:
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Crash")
	bool bUseCrashRollRecovery = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Crash")
	float CrashRollRecoveryStableDelay = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	FVector FirstPersonCameraOffset = FVector(0.0f, 0.0f, 64.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	float ThirdPersonDroneSwapMaxDistance = 800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	float ThirdPersonDroneTransitionDuration = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Camera")
	float ThirdPersonDroneProxyScale = 0.5f;

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
	TSubclassOf<AResourceSpawnerMachine> ResourceSpawnerMachineClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Placement")
	float ConveyorPlacementTraceDistance = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Placement")
	float ConveyorPlacementSurfaceTraceHeight = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Placement")
	float ConveyorPlacementSurfaceTraceDepth = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Placement")
	FVector ConveyorPlacementClearanceExtents = FVector(48.0f, 48.0f, 12.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Placement")
	float ConveyorMasterBeltSpeed = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Placement")
	TSubclassOf<AFactoryPayloadActor> DefaultFactoryPayloadClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupTraceDistance = 650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupTraceRadius = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupMinHoldDistance = 125.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupMaxHoldDistance = 650.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	FVector ThirdPersonPickupHoldOffset = FVector(7.5f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float CharacterPickupStrengthMultiplier = 0.012f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupLightInterpolationSpeed = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupHeavyInterpolationSpeed = 4.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupHandleLinearStiffness = 8000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupHandleLinearDamping = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupHandleAngularStiffness = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupHandleAngularDamping = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupRotationYawSpeed = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupRotationPitchSpeed = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	float PickupRotationTriggerThreshold = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interaction|Pickup")
	bool bShowPickupDebug = true;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone", meta=(AllowPrivateAccess="true"))
	TObjectPtr<ADroneCompanion> DroneCompanion = nullptr;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Placement", meta=(AllowPrivateAccess="true"))
	bool bConveyorPlacementModeActive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Placement", meta=(AllowPrivateAccess="true"))
	bool bConveyorPlacementValid = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Placement", meta=(AllowPrivateAccess="true"))
	EAgentFactoryPlacementType CurrentFactoryPlacementType = EAgentFactoryPlacementType::Conveyor;

	bool bViewModeInitialized = false;
	bool bDefaultUseControllerRotationYaw = false;
	bool bDefaultOrientRotationToMovement = true;
	bool bThirdPersonTransitionActive = false;
	bool bViewModeButtonHeld = false;
	bool bViewModeHoldTriggered = false;
	bool bHasStoredDefaultViewPitchLimits = false;
	bool bKeyboardMapButtonHeld = false;
	bool bKeyboardMapButtonTriggeredMiniMap = false;
	bool bControllerMapButtonHeld = false;
	bool bControllerMapButtonTriggeredMiniMap = false;
	bool bRollModeHoldStartedInFlight = false;
	bool bPendingRollReleaseToFlight = false;
	bool bCrashRollRecoveryActive = false;
	bool bRollModeJumpHeld = false;
	bool bPickupRotationModeActive = false;

	float ThirdPersonTransitionElapsed = 0.0f;
	float ViewModeButtonHeldDuration = 0.0f;
	float KeyboardMapButtonHeldDuration = 0.0f;
	float ControllerMapButtonHeldDuration = 0.0f;
	float DefaultViewPitchMin = -89.9f;
	float DefaultViewPitchMax = 89.9f;
	float CrashRollRecoveryStableTime = 0.0f;
	float RollToFlightAssistTimeRemaining = 0.0f;
	float RollModeJumpHeldDuration = 0.0f;
	float StoredRollJumpChargeAlpha = 0.0f;
	float HeldPickupDistance = 0.0f;
	float HeldPickupMassKg = 0.0f;
	FVector ThirdPersonTransitionStartLocation = FVector::ZeroVector;
	FRotator ThirdPersonTransitionStartRotation = FRotator::ZeroRotator;
	FVector PendingConveyorPlacementLocation = FVector::ZeroVector;
	FRotator PendingConveyorPlacementRotation = FRotator::ZeroRotator;
	int32 ConveyorPlacementRotationSteps = 0;
	TWeakObjectPtr<UPrimitiveComponent> PickupCandidateComponent;
	TWeakObjectPtr<UPrimitiveComponent> HeldPickupComponent;
	FVector PickupCandidateLocation = FVector::ZeroVector;
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

public:
	FORCEINLINE USpringArmComponent* GetCameraBoom() const { return CameraBoom; }
	FORCEINLINE UCameraComponent* GetFollowCamera() const { return FollowCamera; }
	FORCEINLINE UCameraComponent* GetFirstPersonCamera() const { return FirstPersonCamera; }
};
