// Copyright Epic Games, Inc. All Rights Reserved.

#include "AgentCharacter.h"
#include "DroneCompanion.h"
#include "Factory/ConveyorBeltStraight.h"
#include "Factory/ConveyorPlacementPreview.h"
#include "Factory/FactoryPayloadActor.h"
#include "Factory/FactoryPlacementHelpers.h"
#include "Factory/ResourceSpawnerMachine.h"
#include "Factory/StorageBin.h"
#include "Camera/CameraComponent.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "UObject/ConstructorHelpers.h"
#include "Agent.h"

AAgentCharacter::AAgentCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->InitialPushForceFactor = 0.0f;
	GetCharacterMovement()->PushForceFactor = 200000.0f;
	GetCharacterMovement()->TouchForceFactor = 0.0f;
	GetCharacterMovement()->RepulsionForce = 0.0f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->SetActive(true);

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetCapsuleComponent());
	FirstPersonCamera->SetRelativeLocation(FirstPersonCameraOffset);
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetActive(false);

	ThirdPersonTransitionCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonTransitionCamera"));
	ThirdPersonTransitionCamera->SetupAttachment(GetCapsuleComponent());
	ThirdPersonTransitionCamera->bUsePawnControlRotation = false;
	ThirdPersonTransitionCamera->SetActive(false);

	PickupPhysicsHandle = CreateDefaultSubobject<UPhysicsHandleComponent>(TEXT("PickupPhysicsHandle"));

	ThirdPersonDroneProxyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ThirdPersonDroneProxyMesh"));
	ThirdPersonDroneProxyMesh->SetupAttachment(FollowCamera);
	ThirdPersonDroneProxyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ThirdPersonDroneProxyMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	ThirdPersonDroneProxyMesh->SetGenerateOverlapEvents(false);
	ThirdPersonDroneProxyMesh->CastShadow = true;
	ThirdPersonDroneProxyMesh->bCastHiddenShadow = true;
	ThirdPersonDroneProxyMesh->SetHiddenInGame(true);
	ThirdPersonDroneProxyMesh->SetVisibility(false, true);
	ThirdPersonDroneProxyMesh->SetOwnerNoSee(true);
	ThirdPersonDroneProxyMesh->SetCanEverAffectNavigation(false);
	ThirdPersonDroneProxyMesh->SetRelativeScale3D(FVector(ThirdPersonDroneProxyScale));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DroneProxyMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (DroneProxyMesh.Succeeded())
	{
		ThirdPersonDroneProxyMesh->SetStaticMesh(DroneProxyMesh.Object);
	}

}

void AAgentCharacter::BeginPlay()
{
	Super::BeginPlay();

	bDefaultUseControllerRotationYaw = bUseControllerRotationYaw;
	bDefaultOrientRotationToMovement = GetCharacterMovement() ? GetCharacterMovement()->bOrientRotationToMovement : true;
	DronePilotControlMode = StartDronePilotControlMode;
	if (DronePilotControlMode == EAgentDronePilotControlMode::Complex)
	{
		if (bStartWithSimpleDroneControls)
		{
			DronePilotControlMode = EAgentDronePilotControlMode::Simple;
		}
		else if (bStartWithDroneStabilizerEnabled && bStartWithDroneHoverModeEnabled)
		{
			DronePilotControlMode = EAgentDronePilotControlMode::HorizonHover;
		}
		else if (bStartWithDroneStabilizerEnabled)
		{
			DronePilotControlMode = EAgentDronePilotControlMode::Horizon;
		}
	}
	SetDronePilotControlMode(DronePilotControlMode);
	CurrentViewMode = bStartInThirdPersonDroneView ? EAgentViewMode::ThirdPerson : EAgentViewMode::FirstPerson;

	if (CurrentViewMode != EAgentViewMode::ThirdPerson)
	{
		SpawnDroneCompanion();
	}
	else
	{
		SetThirdPersonProxyVisible(true);
		AttachThirdPersonProxyToComponent(FollowCamera);
	}

	if (ThirdPersonDroneProxyMesh)
	{
		ThirdPersonDroneProxyMesh->SetRelativeScale3D(FVector(FMath::Max(0.01f, ThirdPersonDroneProxyScale)));
	}

	FVector InitialThirdPersonLocation = FVector::ZeroVector;
	FRotator InitialThirdPersonRotation = FRotator::ZeroRotator;
	if (GetThirdPersonDroneTarget(InitialThirdPersonLocation, InitialThirdPersonRotation))
	{
		if (DroneCompanion && CurrentViewMode == EAgentViewMode::FirstPerson)
		{
			DroneCompanion->SetCompanionMode(EDroneCompanionMode::BuddyFollow);
		}
	}
}

void AAgentCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdateViewModeButtonHold(DeltaSeconds);
	UpdateKeyboardMapButtonHold(DeltaSeconds);
	UpdateControllerMapButtonHold(DeltaSeconds);
	UpdateRollModeJumpHold(DeltaSeconds);
	UpdateDronePilotCameraLimits();
	AConveyorBeltStraight::SetMasterConveyorSettings(ConveyorMasterBeltSpeed);

	if (bConveyorPlacementModeActive)
	{
		if (!CanUseConveyorPlacementMode())
		{
			ExitConveyorPlacementMode();
		}
		else
		{
			UpdateConveyorPlacementPreview();
		}
	}

	if (CanUsePickupInteraction())
	{
		UpdatePickupInteraction();
	}
	else
	{
		if (HeldPickupComponent.IsValid())
		{
			EndPickup();
		}

		bPickupCandidateValid = false;
		PickupCandidateComponent.Reset();
	}

	if (DroneCompanion)
	{
		SyncDroneCompanionControlState(IsDronePilotMode() && !bMapModeActive);
		UpdateDroneCompanionThirdPersonTarget();

		if (IsDroneInputModeActive())
		{
			UpdateDronePilotInputs(DeltaSeconds);
		}
		else
		{
			DroneCompanion->ResetPilotInputs();
		}
	}

	UpdateThirdPersonTransition(DeltaSeconds);

	if (!bViewModeInitialized && GetController() != nullptr)
	{
		ApplyViewMode(CurrentViewMode, false);
		bViewModeInitialized = true;
	}
}

void AAgentCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AAgentCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AAgentCharacter::DoJumpEnd);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AAgentCharacter::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &AAgentCharacter::StopMove);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Canceled, this, &AAgentCharacter::StopMove);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AAgentCharacter::Look);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AAgentCharacter::Look);
	}
	else
	{
		UE_LOG(LogAgent, Error, TEXT("'%s' Failed to find an Enhanced Input component!"), *GetNameSafe(this));
	}

	PlayerInputComponent->BindKey(EKeys::V, IE_Pressed, this, &AAgentCharacter::OnViewModeButtonPressed);
	PlayerInputComponent->BindKey(EKeys::V, IE_Released, this, &AAgentCharacter::OnViewModeButtonReleased);
	PlayerInputComponent->BindKey(EKeys::Gamepad_FaceButton_Top, IE_Pressed, this, &AAgentCharacter::OnViewModeButtonPressed);
	PlayerInputComponent->BindKey(EKeys::Gamepad_FaceButton_Top, IE_Released, this, &AAgentCharacter::OnViewModeButtonReleased);
	PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &AAgentCharacter::OnConveyorPlacementModePressed);
	PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AAgentCharacter::OnStorageBinPlacementModePressed);
	PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AAgentCharacter::OnResourceSpawnerPlacementModePressed);
	PlayerInputComponent->BindKey(EKeys::Gamepad_FaceButton_Left, IE_Pressed, this, &AAgentCharacter::OnFactoryPlacementTogglePressed);
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AAgentCharacter::OnConveyorPlacePressed);
	PlayerInputComponent->BindKey(EKeys::Gamepad_RightTrigger, IE_Pressed, this, &AAgentCharacter::OnPickupOrPlacePressed);
	PlayerInputComponent->BindKey(EKeys::Gamepad_RightTrigger, IE_Released, this, &AAgentCharacter::OnPickupOrPlaceReleased);
	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AAgentCharacter::OnConveyorCancelPressed);
	PlayerInputComponent->BindKey(EKeys::Gamepad_FaceButton_Right, IE_Pressed, this, &AAgentCharacter::OnConveyorCancelPressed);
	PlayerInputComponent->BindKey(EKeys::Gamepad_LeftShoulder, IE_Pressed, this, &AAgentCharacter::OnConveyorRotateLeftPressed);
	PlayerInputComponent->BindKey(EKeys::Gamepad_RightShoulder, IE_Pressed, this, &AAgentCharacter::OnConveyorRotateRightPressed);
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AAgentCharacter::DoJumpStart);
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Released, this, &AAgentCharacter::DoJumpEnd);
	PlayerInputComponent->BindKey(EKeys::Gamepad_FaceButton_Bottom, IE_Pressed, this, &AAgentCharacter::DoJumpStart);
	PlayerInputComponent->BindKey(EKeys::Gamepad_FaceButton_Bottom, IE_Released, this, &AAgentCharacter::DoJumpEnd);
	PlayerInputComponent->BindKey(EKeys::B, IE_Pressed, this, &AAgentCharacter::OnDroneControlModeTogglePressed);
	PlayerInputComponent->BindKey(EKeys::M, IE_Pressed, this, &AAgentCharacter::OnKeyboardMapButtonPressed);
	PlayerInputComponent->BindKey(EKeys::M, IE_Released, this, &AAgentCharacter::OnKeyboardMapButtonReleased);
	PlayerInputComponent->BindKey(EKeys::W, IE_Pressed, this, &AAgentCharacter::OnDronePitchForwardPressed);
	PlayerInputComponent->BindKey(EKeys::W, IE_Released, this, &AAgentCharacter::OnDronePitchForwardReleased);
	PlayerInputComponent->BindKey(EKeys::S, IE_Pressed, this, &AAgentCharacter::OnDronePitchBackwardPressed);
	PlayerInputComponent->BindKey(EKeys::S, IE_Released, this, &AAgentCharacter::OnDronePitchBackwardReleased);
	PlayerInputComponent->BindKey(EKeys::A, IE_Pressed, this, &AAgentCharacter::OnDroneRollLeftPressed);
	PlayerInputComponent->BindKey(EKeys::A, IE_Released, this, &AAgentCharacter::OnDroneRollLeftReleased);
	PlayerInputComponent->BindKey(EKeys::D, IE_Pressed, this, &AAgentCharacter::OnDroneRollRightPressed);
	PlayerInputComponent->BindKey(EKeys::D, IE_Released, this, &AAgentCharacter::OnDroneRollRightReleased);
	PlayerInputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AAgentCharacter::OnDroneYawLeftPressed);
	PlayerInputComponent->BindKey(EKeys::Q, IE_Released, this, &AAgentCharacter::OnDroneYawLeftReleased);
	PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, this, &AAgentCharacter::OnPickupOrDroneYawRightPressed);
	PlayerInputComponent->BindKey(EKeys::E, IE_Released, this, &AAgentCharacter::OnPickupOrDroneYawRightReleased);
	PlayerInputComponent->BindKey(EKeys::R, IE_Pressed, this, &AAgentCharacter::OnDroneThrottleUpPressed);
	PlayerInputComponent->BindKey(EKeys::R, IE_Released, this, &AAgentCharacter::OnDroneThrottleUpReleased);
	PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &AAgentCharacter::OnDroneThrottleDownPressed);
	PlayerInputComponent->BindKey(EKeys::F, IE_Released, this, &AAgentCharacter::OnDroneThrottleDownReleased);

	PlayerInputComponent->BindAxisKey(EKeys::Gamepad_LeftX, this, &AAgentCharacter::OnDroneGamepadYawAxis);
	PlayerInputComponent->BindAxisKey(EKeys::Gamepad_LeftY, this, &AAgentCharacter::OnDroneGamepadThrottleAxis);
	PlayerInputComponent->BindAxisKey(EKeys::Gamepad_RightX, this, &AAgentCharacter::OnDroneGamepadRollAxis);
	PlayerInputComponent->BindAxisKey(EKeys::Gamepad_RightY, this, &AAgentCharacter::OnDroneGamepadPitchAxis);
	PlayerInputComponent->BindAxisKey(EKeys::Gamepad_LeftTriggerAxis, this, &AAgentCharacter::OnDroneGamepadLeftTriggerAxis);
	PlayerInputComponent->BindAxisKey(EKeys::Gamepad_RightTriggerAxis, this, &AAgentCharacter::OnDroneGamepadRightTriggerAxis);
	PlayerInputComponent->BindKey(EKeys::Gamepad_DPad_Left, IE_Pressed, this, &AAgentCharacter::OnDroneHoverModePressed);
	PlayerInputComponent->BindKey(EKeys::Gamepad_DPad_Right, IE_Pressed, this, &AAgentCharacter::OnDroneStabilizerTogglePressed);
	PlayerInputComponent->BindKey(EKeys::Gamepad_Special_Left, IE_Pressed, this, &AAgentCharacter::OnControllerMapButtonPressed);
	PlayerInputComponent->BindKey(EKeys::Gamepad_Special_Left, IE_Released, this, &AAgentCharacter::OnControllerMapButtonReleased);
	PlayerInputComponent->BindKey(EKeys::Gamepad_DPad_Up, IE_Pressed, this, &AAgentCharacter::OnDroneCameraTiltUpPressed);
	PlayerInputComponent->BindKey(EKeys::Gamepad_DPad_Down, IE_Pressed, this, &AAgentCharacter::OnDroneCameraTiltDownPressed);
}

void AAgentCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();
	DoMove(MovementVector.X, MovementVector.Y);
}

void AAgentCharacter::StopMove(const FInputActionValue& Value)
{
	DoMove(0.0f, 0.0f);
}

void AAgentCharacter::Look(const FInputActionValue& Value)
{
	if (bPickupRotationModeActive && bControllerPickupHeld && HeldPickupComponent.IsValid())
	{
		return;
	}

	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AAgentCharacter::OnViewModeButtonPressed()
{
	bViewModeButtonHeld = true;
	bViewModeHoldTriggered = false;
	ViewModeButtonHeldDuration = 0.0f;
}

void AAgentCharacter::OnViewModeButtonReleased()
{
	if (!bViewModeButtonHeld)
	{
		return;
	}

	const bool bWasHoldTriggered = bViewModeHoldTriggered;
	bViewModeButtonHeld = false;
	bViewModeHoldTriggered = false;
	ViewModeButtonHeldDuration = 0.0f;

	if (!bWasHoldTriggered)
	{
		CycleViewMode();
	}
}

void AAgentCharacter::UpdateViewModeButtonHold(float DeltaSeconds)
{
	if (!bViewModeButtonHeld || bViewModeHoldTriggered)
	{
		return;
	}

	ViewModeButtonHeldDuration += FMath::Max(0.0f, DeltaSeconds);
	if (ViewModeButtonHeldDuration < FMath::Max(0.0f, DroneViewHoldTime))
	{
		return;
	}

	bViewModeHoldTriggered = true;
	if (CurrentViewMode != EAgentViewMode::DronePilot)
	{
		ApplyViewMode(EAgentViewMode::DronePilot, true);
	}
}

void AAgentCharacter::CycleViewMode()
{
	if (bMiniMapModeActive)
	{
		ExitMiniMapMode();
		return;
	}

	if (bMapModeActive)
	{
		ToggleMapMode();
		return;
	}

	EAgentViewMode NextMode = EAgentViewMode::FirstPerson;
	if (CurrentViewMode == EAgentViewMode::FirstPerson)
	{
		NextMode = EAgentViewMode::ThirdPerson;
	}

	ApplyViewMode(NextMode, true);
}

void AAgentCharacter::ApplyViewMode(EAgentViewMode NewMode, bool bBlend)
{
	if (bConveyorPlacementModeActive && NewMode != CurrentViewMode)
	{
		ExitConveyorPlacementMode();
	}

	bMapModeActive = false;
	bMiniMapModeActive = false;
	bMiniMapViewingDroneCamera = false;
	const EAgentViewMode PreviousViewMode = CurrentViewMode;
	const bool bWasDronePilot = PreviousViewMode == EAgentViewMode::DronePilot;
	if (NewMode != EAgentViewMode::DronePilot)
	{
		ResetCharacterCameraRoll();
	}
	FVector ThirdPersonTargetLocation = GetActorLocation();
	FRotator ThirdPersonTargetRotation = GetControlRotation();
	GetThirdPersonDroneTarget(ThirdPersonTargetLocation, ThirdPersonTargetRotation);
	FVector ThirdPersonSwapStartLocation = ThirdPersonTargetLocation;
	FRotator ThirdPersonSwapStartRotation = ThirdPersonTargetRotation;
	const bool bUseFirstToThirdPersonSwap = NewMode == EAgentViewMode::ThirdPerson
		&& PreviousViewMode == EAgentViewMode::FirstPerson
		&& !bMiniMapModeActive;

	if (bUseFirstToThirdPersonSwap && DroneCompanion)
	{
		DroneCompanion->GetDroneCameraTransform(ThirdPersonSwapStartLocation, ThirdPersonSwapStartRotation);
		const float AllowedDistance = FMath::Max(0.0f, ThirdPersonDroneSwapMaxDistance);
		if (AllowedDistance > 0.0f
			&& FVector::DistSquared(ThirdPersonSwapStartLocation, ThirdPersonTargetLocation) > FMath::Square(AllowedDistance))
		{
			if (GEngine)
			{
				GEngine->AddOnScreenDebugMessage(
					static_cast<uint64>(GetUniqueID()) + 14000ULL,
					2.0f,
					FColor::Red,
					TEXT("Drone camera is out of range for third person"));
			}
			return;
		}
	}

	CurrentViewMode = NewMode;
	bDroneEntryAssistActive = NewMode == EAgentViewMode::DronePilot && !bWasDronePilot;
	bThirdPersonTransitionActive = false;
	ThirdPersonTransitionElapsed = 0.0f;

	const float BlendTime = bBlend ? ViewBlendTime : 0.0f;
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	UCharacterMovementComponent* CharacterMovementComponent = GetCharacterMovement();

	if (FirstPersonCamera)
	{
		FirstPersonCamera->SetActive(false);
	}

	if (FollowCamera)
	{
		FollowCamera->SetActive(false);
	}

	if (ThirdPersonTransitionCamera)
	{
		ThirdPersonTransitionCamera->SetActive(false);
	}

	if (CharacterMovementComponent)
	{
		if (NewMode == EAgentViewMode::FirstPerson)
		{
			bUseControllerRotationYaw = true;
			CharacterMovementComponent->bOrientRotationToMovement = false;
		}
		else
		{
			bUseControllerRotationYaw = bDefaultUseControllerRotationYaw;
			CharacterMovementComponent->bOrientRotationToMovement = bDefaultOrientRotationToMovement;
		}

		if (NewMode == EAgentViewMode::DronePilot)
		{
			CharacterMovementComponent->StopMovementImmediately();
		}
	}

	switch (NewMode)
	{
	case EAgentViewMode::ThirdPerson:
	{
		if (!bUseFirstToThirdPersonSwap)
		{
			GetThirdPersonDroneTarget(ThirdPersonTargetLocation, ThirdPersonTargetRotation);
			ThirdPersonSwapStartLocation = ThirdPersonTargetLocation;
			ThirdPersonSwapStartRotation = ThirdPersonTargetRotation;
		}
		SetThirdPersonProxyVisible(true);

		if (bUseFirstToThirdPersonSwap && ThirdPersonTransitionCamera)
		{
			DespawnDroneCompanion();
			AttachThirdPersonProxyToComponent(ThirdPersonTransitionCamera);
			ThirdPersonTransitionCamera->SetWorldLocationAndRotation(ThirdPersonSwapStartLocation, ThirdPersonSwapStartRotation);
			ThirdPersonTransitionStartLocation = ThirdPersonSwapStartLocation;
			ThirdPersonTransitionStartRotation = ThirdPersonSwapStartRotation;
			bThirdPersonTransitionActive = bBlend && ThirdPersonDroneTransitionDuration > KINDA_SMALL_NUMBER;
			ThirdPersonTransitionCamera->SetActive(true);

			if (!bThirdPersonTransitionActive && FollowCamera)
			{
				AttachThirdPersonProxyToComponent(FollowCamera);
				FollowCamera->SetActive(true);
				ThirdPersonTransitionCamera->SetActive(false);
			}
		}
		else
		{
			AttachThirdPersonProxyToComponent(FollowCamera);
			if (FollowCamera)
			{
				FollowCamera->SetActive(true);
			}
		}

		if (PlayerController)
		{
			PlayerController->SetViewTargetWithBlend(this, bUseFirstToThirdPersonSwap ? 0.0f : (DroneCompanion ? 0.0f : BlendTime));
		}

		if (!bUseFirstToThirdPersonSwap)
		{
			DespawnDroneCompanion();
		}
		ResetDroneInputState();
		break;
	}
	case EAgentViewMode::DronePilot:
	{
		SetThirdPersonProxyVisible(false);
		FVector DroneSpawnLocation = FVector::ZeroVector;
		FRotator DroneSpawnRotation = FRotator::ZeroRotator;
		if (PreviousViewMode == EAgentViewMode::ThirdPerson
			&& ThirdPersonTransitionCamera
			&& ThirdPersonTransitionCamera->IsActive())
		{
			DroneSpawnLocation = ThirdPersonTransitionCamera->GetComponentLocation();
			DroneSpawnRotation = ThirdPersonTransitionCamera->GetComponentRotation();
		}
		else
		{
			GetThirdPersonDroneTarget(DroneSpawnLocation, DroneSpawnRotation);
		}

		if (!DroneCompanion)
		{
			SpawnDroneCompanionAtTransform(DroneSpawnLocation, DroneSpawnRotation, EDroneCompanionMode::PilotControlled);
		}
		else
		{
			SyncDroneCompanionControlState(true);
			DroneCompanion->SetCompanionMode(EDroneCompanionMode::PilotControlled);
		}

		if (DroneCompanion && PreviousViewMode == EAgentViewMode::ThirdPerson)
		{
			DroneCompanion->StartPilotCameraTransitionFromThirdPerson();
		}

		if (DroneCompanion && PlayerController)
		{
			PlayerController->SetViewTargetWithBlend(
				DroneCompanion.Get(),
				PreviousViewMode == EAgentViewMode::ThirdPerson ? 0.0f : BlendTime);
		}
		break;
	}
	case EAgentViewMode::FirstPerson:
	default:
		SetThirdPersonProxyVisible(false);
		if (FirstPersonCamera)
		{
			FirstPersonCamera->SetActive(true);
		}

		if (PlayerController)
		{
			PlayerController->SetViewTargetWithBlend(this, bWasDronePilot ? 0.0f : BlendTime);
		}

		if (!DroneCompanion)
		{
			FVector DroneSpawnLocation = FVector::ZeroVector;
			FRotator DroneSpawnRotation = FRotator::ZeroRotator;
			if (ThirdPersonTransitionCamera && ThirdPersonTransitionCamera->IsActive())
			{
				DroneSpawnLocation = ThirdPersonTransitionCamera->GetComponentLocation();
				DroneSpawnRotation = ThirdPersonTransitionCamera->GetComponentRotation();
			}
			else
			{
				GetThirdPersonDroneTarget(DroneSpawnLocation, DroneSpawnRotation);
			}
			SpawnDroneCompanionAtTransform(DroneSpawnLocation, DroneSpawnRotation, EDroneCompanionMode::BuddyFollow);
		}
		else
		{
			SyncDroneCompanionControlState(false);
			DroneCompanion->SetCompanionMode(EDroneCompanionMode::BuddyFollow);
		}

		ResetDroneInputState();
		break;
	}
}

void AAgentCharacter::ApplyDroneAssistState()
{
	if (!DroneCompanion)
	{
		return;
	}

	if (IsRollDronePilotMode())
	{
		DroneCompanion->SetPilotStabilizerEnabled(false);
		DroneCompanion->SetPilotHoverModeEnabled(false);
		return;
	}

	if (RollToFlightAssistTimeRemaining > 0.0f)
	{
		DroneCompanion->SetPilotStabilizerEnabled(true);
		DroneCompanion->SetPilotHoverModeEnabled(true);
		return;
	}

	if (IsFreeFlyDronePilotMode() || IsSpectatorDronePilotMode())
	{
		DroneCompanion->SetPilotStabilizerEnabled(false);
		DroneCompanion->SetPilotHoverModeEnabled(false);
		return;
	}

	DroneCompanion->SetPilotStabilizerEnabled(bDroneEntryAssistActive || bDroneStabilizerEnabled);
	DroneCompanion->SetPilotHoverModeEnabled(bDroneEntryAssistActive || bDroneHoverModeEnabled);
}

void AAgentCharacter::EnterRollTransitionMode(bool bTrackHeldReturn, bool bFromCrashRecovery)
{
	if (!DroneCompanion || !IsDronePilotMode() || bMapModeActive || bMiniMapModeActive)
	{
		return;
	}

	CrashRollRecoveryStableTime = 0.0f;
	bRollModeHoldStartedInFlight = bTrackHeldReturn;
	bCrashRollRecoveryActive = bFromCrashRecovery;
	RollModeJumpHeldDuration = 0.0f;
	SetDronePilotControlMode(EAgentDronePilotControlMode::Roll);
}

void AAgentCharacter::ExitRollTransitionMode(bool bTryJumpLift)
{
	if (!DroneCompanion || !IsDronePilotMode())
	{
		return;
	}

	bRollModeJumpHeld = false;
	bRollModeHoldStartedInFlight = false;
	bCrashRollRecoveryActive = false;
	CrashRollRecoveryStableTime = 0.0f;
	RollModeJumpHeldDuration = 0.0f;

	const EAgentDronePilotControlMode ReturnMode = LastFlightDronePilotControlMode == EAgentDronePilotControlMode::Roll
		? EAgentDronePilotControlMode::Complex
		: LastFlightDronePilotControlMode;
	RollToFlightAssistTimeRemaining = bTryJumpLift ? FMath::Max(0.0f, RollToFlightAssistDuration) : 0.0f;
	SetDronePilotControlMode(ReturnMode);
}

void AAgentCharacter::SyncDroneCompanionControlState(bool bAllowRollControls, bool bSuppressCameraMountRefresh)
{
	if (!DroneCompanion)
	{
		return;
	}

	DroneCompanion->SetFollowTarget(this);
	DroneCompanion->SetViewReferenceRotation(GetControlRotation());
	if (bSuppressCameraMountRefresh)
	{
		DroneCompanion->SetSuppressCameraMountRefresh(true);
	}

	DroneCompanion->SetUseSimplePilotControls(bUseSimpleDronePilotControls);
	DroneCompanion->SetUseFreeFlyPilotControls(IsFreeFlyDronePilotMode());
	DroneCompanion->SetUseSpectatorPilotControls(IsSpectatorDronePilotMode());
	DroneCompanion->SetUseRollPilotControls(bAllowRollControls && IsRollDronePilotMode());
	DroneCompanion->SetUseCrashRollRecoveryMode(bUseCrashRollRecovery);

	if (bSuppressCameraMountRefresh)
	{
		DroneCompanion->SetSuppressCameraMountRefresh(false);
	}

	ApplyDroneAssistState();
}

void AAgentCharacter::UpdateDroneCompanionThirdPersonTarget()
{
	if (!DroneCompanion)
	{
		return;
	}

	FVector ThirdPersonTargetLocation = FVector::ZeroVector;
	FRotator ThirdPersonTargetRotation = FRotator::ZeroRotator;
	if (GetThirdPersonDroneTarget(ThirdPersonTargetLocation, ThirdPersonTargetRotation))
	{
		DroneCompanion->SetThirdPersonCameraTarget(ThirdPersonTargetLocation, ThirdPersonTargetRotation);
	}
}

bool AAgentCharacter::GetThirdPersonDroneTarget(FVector& OutLocation, FRotator& OutRotation) const
{
	if (FollowCamera)
	{
		OutLocation = FollowCamera->GetComponentLocation();
		OutRotation = FollowCamera->GetComponentRotation();
		return true;
	}

	OutLocation = GetActorLocation();
	OutRotation = GetControlRotation();
	return true;
}

void AAgentCharacter::SpawnDroneCompanion()
{
	const FRotator SpawnYawRotation(0.0f, GetControlRotation().Yaw, 0.0f);
	const FVector SpawnLocation = GetActorLocation() + FRotationMatrix(SpawnYawRotation).TransformVector(DroneSpawnOffset);
	const EDroneCompanionMode InitialMode = CurrentViewMode == EAgentViewMode::DronePilot
		? EDroneCompanionMode::PilotControlled
		: EDroneCompanionMode::BuddyFollow;
	SpawnDroneCompanionAtTransform(SpawnLocation, SpawnYawRotation, InitialMode);
}

void AAgentCharacter::SpawnDroneCompanionAtTransform(
	const FVector& SpawnLocation,
	const FRotator& SpawnRotation,
	EDroneCompanionMode InitialMode)
{
	if (DroneCompanion || !GetWorld())
	{
		return;
	}

	UClass* SpawnClass = DroneCompanionClass.Get();
	if (!SpawnClass)
	{
		SpawnClass = ADroneCompanion::StaticClass();
	}
	if (!SpawnClass)
	{
		return;
	}

	FActorSpawnParameters SpawnParameters{};
	SpawnParameters.Owner = this;
	SpawnParameters.Instigator = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	DroneCompanion = GetWorld()->SpawnActor<ADroneCompanion>(SpawnClass, SpawnLocation, SpawnRotation, SpawnParameters);

	if (!DroneCompanion)
	{
		return;
	}

	SyncDroneCompanionControlState(CurrentViewMode == EAgentViewMode::DronePilot && !bMapModeActive);
	UpdateDroneCompanionThirdPersonTarget();
	DroneCompanion->SetCompanionMode(InitialMode);
}

void AAgentCharacter::DespawnDroneCompanion()
{
	if (!DroneCompanion)
	{
		return;
	}

	DroneCompanion->Destroy();
	DroneCompanion = nullptr;
}

void AAgentCharacter::UpdateThirdPersonTransition(float DeltaSeconds)
{
	if (!bThirdPersonTransitionActive || !ThirdPersonTransitionCamera || !FollowCamera)
	{
		return;
	}

	ThirdPersonTransitionElapsed = FMath::Min(
		ThirdPersonTransitionElapsed + FMath::Max(0.0f, DeltaSeconds),
		FMath::Max(KINDA_SMALL_NUMBER, ThirdPersonDroneTransitionDuration));
	const float RawAlpha = ThirdPersonTransitionElapsed / FMath::Max(KINDA_SMALL_NUMBER, ThirdPersonDroneTransitionDuration);
	const float Alpha = FMath::InterpEaseInOut(0.0f, 1.0f, RawAlpha, 2.0f);
	const FVector TargetLocation = FollowCamera->GetComponentLocation();
	const FQuat StartRotation = ThirdPersonTransitionStartRotation.Quaternion();
	const FQuat TargetRotation = FollowCamera->GetComponentRotation().Quaternion();

	ThirdPersonTransitionCamera->SetWorldLocationAndRotation(
		FMath::Lerp(ThirdPersonTransitionStartLocation, TargetLocation, Alpha),
		FQuat::Slerp(StartRotation, TargetRotation, Alpha));

	if (RawAlpha >= 1.0f)
	{
		bThirdPersonTransitionActive = false;
		AttachThirdPersonProxyToComponent(FollowCamera);
		FollowCamera->SetActive(true);
		ThirdPersonTransitionCamera->SetActive(false);
	}
}

void AAgentCharacter::SetThirdPersonProxyVisible(bool bVisible)
{
	if (!ThirdPersonDroneProxyMesh)
	{
		return;
	}

	ThirdPersonDroneProxyMesh->SetHiddenInGame(!bVisible, true);
	ThirdPersonDroneProxyMesh->SetVisibility(bVisible, true);
	ThirdPersonDroneProxyMesh->SetCastShadow(bVisible);
	ThirdPersonDroneProxyMesh->bCastHiddenShadow = bVisible;
}

void AAgentCharacter::AttachThirdPersonProxyToComponent(USceneComponent* AttachmentParent)
{
	if (!ThirdPersonDroneProxyMesh || !AttachmentParent)
	{
		return;
	}

	ThirdPersonDroneProxyMesh->AttachToComponent(
		AttachmentParent,
		FAttachmentTransformRules::SnapToTargetIncludingScale);
	ThirdPersonDroneProxyMesh->SetRelativeLocation(FVector::ZeroVector);
	ThirdPersonDroneProxyMesh->SetRelativeRotation(FRotator::ZeroRotator);
}

bool AAgentCharacter::CanUseConveyorPlacementMode() const
{
	return !bMapModeActive
		&& !bMiniMapModeActive
		&& (CurrentViewMode == EAgentViewMode::ThirdPerson || CurrentViewMode == EAgentViewMode::FirstPerson);
}

void AAgentCharacter::ToggleConveyorPlacementMode()
{
	if (bConveyorPlacementModeActive)
	{
		ExitConveyorPlacementMode();
		return;
	}

	EnterConveyorPlacementMode();
}

void AAgentCharacter::EnterConveyorPlacementMode()
{
	if (bConveyorPlacementModeActive)
	{
		return;
	}

	if (!CanUseConveyorPlacementMode())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				static_cast<uint64>(GetUniqueID()) + 18000ULL,
				1.5f,
				FColor::Yellow,
				TEXT("Factory placement only works in first and third person."));
		}
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UClass* PreviewClass = ConveyorPlacementPreviewClass.Get();
	if (!PreviewClass)
	{
		PreviewClass = AConveyorPlacementPreview::StaticClass();
	}

	FActorSpawnParameters SpawnParameters{};
	SpawnParameters.Owner = this;
	SpawnParameters.Instigator = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ConveyorPlacementPreview = World->SpawnActor<AConveyorPlacementPreview>(
		PreviewClass,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParameters);

	bConveyorPlacementModeActive = ConveyorPlacementPreview != nullptr;
	bConveyorPlacementValid = false;
	ConveyorPlacementRotationSteps = 0;
	PendingConveyorPlacementLocation = FVector::ZeroVector;
	PendingConveyorPlacementRotation = FRotator::ZeroRotator;

	if (ConveyorPlacementPreview)
	{
		ConveyorPlacementPreview->SetActorHiddenInGame(true);
	}

	UpdateConveyorPlacementPreview();
}

void AAgentCharacter::ExitConveyorPlacementMode()
{
	bConveyorPlacementModeActive = false;
	bConveyorPlacementValid = false;
	PendingConveyorPlacementLocation = FVector::ZeroVector;
	PendingConveyorPlacementRotation = FRotator::ZeroRotator;

	if (ConveyorPlacementPreview)
	{
		ConveyorPlacementPreview->Destroy();
		ConveyorPlacementPreview = nullptr;
	}
}

void AAgentCharacter::UpdateConveyorPlacementPreview()
{
	if (!bConveyorPlacementModeActive || !ConveyorPlacementPreview)
	{
		return;
	}

	FVector PreviewLocation = PendingConveyorPlacementLocation;
	FRotator PreviewRotation = PendingConveyorPlacementRotation;
	bool bPlacementIsValid = false;
	const bool bHasPreviewLocation = EvaluateConveyorPlacement(PreviewLocation, PreviewRotation, bPlacementIsValid);

	bConveyorPlacementValid = bHasPreviewLocation && bPlacementIsValid;

	if (!bHasPreviewLocation)
	{
		ConveyorPlacementPreview->SetActorHiddenInGame(true);
		return;
	}

	PendingConveyorPlacementLocation = PreviewLocation;
	PendingConveyorPlacementRotation = PreviewRotation;

	ConveyorPlacementPreview->SetActorHiddenInGame(false);
	ConveyorPlacementPreview->SetActorLocationAndRotation(PendingConveyorPlacementLocation, PendingConveyorPlacementRotation);
	ConveyorPlacementPreview->SetPlacementState(bConveyorPlacementValid);

	if (UWorld* World = GetWorld())
	{
		const FColor DebugColor = bConveyorPlacementValid ? FColor::Green : FColor::Red;
		const FVector BoxCenter = PendingConveyorPlacementLocation + FVector(0.0f, 0.0f, 20.0f + ConveyorPlacementClearanceExtents.Z);
		DrawDebugBox(
			World,
			BoxCenter,
			ConveyorPlacementClearanceExtents,
			PendingConveyorPlacementRotation.Quaternion(),
			DebugColor,
			false,
			0.0f,
			0,
			1.5f);

		const FVector ArrowStart = PendingConveyorPlacementLocation + FVector(0.0f, 0.0f, 50.0f);
		const FVector ArrowEnd = ArrowStart + PendingConveyorPlacementRotation.Vector() * 70.0f;
		DrawDebugDirectionalArrow(
			World,
			ArrowStart,
			ArrowEnd,
			25.0f,
			DebugColor,
			false,
			0.0f,
			0,
			2.0f);
	}
}

UCameraComponent* AAgentCharacter::GetActivePlacementCamera() const
{
	if (CurrentViewMode == EAgentViewMode::FirstPerson && FirstPersonCamera)
	{
		return FirstPersonCamera;
	}

	if (CurrentViewMode == EAgentViewMode::ThirdPerson)
	{
		if (ThirdPersonTransitionCamera && ThirdPersonTransitionCamera->IsActive())
		{
			return ThirdPersonTransitionCamera;
		}

		if (FollowCamera)
		{
			return FollowCamera;
		}
	}

	return nullptr;
}

void AAgentCharacter::TryPlaceConveyor()
{
	if (!bConveyorPlacementModeActive)
	{
		return;
	}

	FVector SpawnLocation = FVector::ZeroVector;
	FRotator SpawnRotation = FRotator::ZeroRotator;
	bool bPlacementIsValid = false;
	const bool bHasPreviewLocation = EvaluateConveyorPlacement(SpawnLocation, SpawnRotation, bPlacementIsValid);

	if (!bHasPreviewLocation || !bPlacementIsValid)
	{
		bConveyorPlacementValid = false;
		if (ConveyorPlacementPreview)
		{
			ConveyorPlacementPreview->SetPlacementState(false);
		}

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				static_cast<uint64>(GetUniqueID()) + 18001ULL,
				1.0f,
				FColor::Red,
				TEXT("Invalid factory placement."));
		}
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UClass* BuildableClass = nullptr;
	switch (CurrentFactoryPlacementType)
	{
	case EAgentFactoryPlacementType::StorageBin:
		BuildableClass = StorageBinClass.Get() ? StorageBinClass.Get() : AStorageBin::StaticClass();
		break;
	case EAgentFactoryPlacementType::ResourceSpawner:
		BuildableClass = ResourceSpawnerMachineClass.Get() ? ResourceSpawnerMachineClass.Get() : AResourceSpawnerMachine::StaticClass();
		break;
	case EAgentFactoryPlacementType::Conveyor:
	default:
		BuildableClass = ConveyorBeltClass.Get() ? ConveyorBeltClass.Get() : AConveyorBeltStraight::StaticClass();
		break;
	}

	if (!BuildableClass)
	{
		return;
	}

	FActorSpawnParameters SpawnParameters{};
	SpawnParameters.Owner = this;
	SpawnParameters.Instigator = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* SpawnedActor = World->SpawnActor<AActor>(
		BuildableClass,
		SpawnLocation,
		SpawnRotation,
		SpawnParameters);

	ApplyFactoryBuildableDefaults(SpawnedActor);

	UpdateConveyorPlacementPreview();
}

void AAgentCharacter::CancelConveyorPlacement()
{
	if (bConveyorPlacementModeActive)
	{
		ExitConveyorPlacementMode();
	}
}

void AAgentCharacter::RotateConveyorPlacement(int32 Direction)
{
	if (!bConveyorPlacementModeActive)
	{
		return;
	}

	ConveyorPlacementRotationSteps = (ConveyorPlacementRotationSteps + Direction) % 4;
	if (ConveyorPlacementRotationSteps < 0)
	{
		ConveyorPlacementRotationSteps += 4;
	}

	UpdateConveyorPlacementPreview();
}

bool AAgentCharacter::EvaluateConveyorPlacement(FVector& OutLocation, FRotator& OutRotation, bool& bOutIsValid) const
{
	OutLocation = FVector::ZeroVector;
	OutRotation = FRotator::ZeroRotator;
	bOutIsValid = false;

	if (!CanUseConveyorPlacementMode())
	{
		return false;
	}

	UWorld* World = GetWorld();
	UCameraComponent* PlacementCamera = GetActivePlacementCamera();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ConveyorPlacementTrace), false, this);
	QueryParams.AddIgnoredActor(this);

	if (DroneCompanion)
	{
		QueryParams.AddIgnoredActor(DroneCompanion);
	}

	if (ConveyorPlacementPreview)
	{
		QueryParams.AddIgnoredActor(ConveyorPlacementPreview);
	}

	OutRotation = AgentFactory::SnapYawToGrid(static_cast<float>(ConveyorPlacementRotationSteps) * AgentFactory::QuarterTurnDegrees);

	if (!PlacementCamera)
	{
		return false;
	}

	const FVector TraceStart = PlacementCamera->GetComponentLocation();
	const FVector TraceDirection = PlacementCamera->GetForwardVector();
	const FVector TraceEnd = TraceStart + TraceDirection * FMath::Max(100.0f, ConveyorPlacementTraceDistance);

	FHitResult AimHit;
	bool bHasAimHit = World->LineTraceSingleByChannel(
		AimHit,
		TraceStart,
		TraceEnd,
		AgentFactory::BuildPlacementTraceChannel,
		QueryParams);

	if (!bHasAimHit)
	{
		bHasAimHit = World->LineTraceSingleByChannel(AimHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
	}

	if (!bHasAimHit)
	{
		return false;
	}

	const bool bSnappedToConveyorFace = TryGetFactoryBuildableFaceSnapLocation(AimHit, OutLocation);
	if (!bSnappedToConveyorFace)
	{
		const FVector PlacementProbeLocation = AimHit.ImpactPoint;

		const FVector SurfaceTraceStart = PlacementProbeLocation + FVector(0.0f, 0.0f, FMath::Max(100.0f, ConveyorPlacementSurfaceTraceHeight));
		const FVector SurfaceTraceEnd = PlacementProbeLocation - FVector(0.0f, 0.0f, FMath::Max(100.0f, ConveyorPlacementSurfaceTraceDepth));

		FHitResult SurfaceHit;
		bool bHasSurfaceHit = World->LineTraceSingleByChannel(
			SurfaceHit,
			SurfaceTraceStart,
			SurfaceTraceEnd,
			AgentFactory::BuildPlacementTraceChannel,
			QueryParams);

		if (!bHasSurfaceHit)
		{
			bHasSurfaceHit = World->LineTraceSingleByChannel(SurfaceHit, SurfaceTraceStart, SurfaceTraceEnd, ECC_Visibility, QueryParams);
		}

		if (!bHasSurfaceHit)
		{
			return false;
		}

		OutLocation = AgentFactory::SnapLocationToGrid(SurfaceHit.ImpactPoint);

		if (SurfaceHit.ImpactNormal.Z < 0.85f)
		{
			return true;
		}
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQueryParams.AddObjectTypesToQuery(AgentFactory::FactoryBuildableChannel);

	FCollisionQueryParams OverlapParams(SCENE_QUERY_STAT(ConveyorPlacementOverlap), false, this);
	OverlapParams.AddIgnoredActor(this);

	if (DroneCompanion)
	{
		OverlapParams.AddIgnoredActor(DroneCompanion);
	}

	if (ConveyorPlacementPreview)
	{
		OverlapParams.AddIgnoredActor(ConveyorPlacementPreview);
	}

	TArray<FOverlapResult> OverlapResults;
	const FVector OverlapCenter = OutLocation + FVector(0.0f, 0.0f, 20.0f + ConveyorPlacementClearanceExtents.Z);
	const bool bHasOverlap = World->OverlapMultiByObjectType(
		OverlapResults,
		OverlapCenter,
		OutRotation.Quaternion(),
		ObjectQueryParams,
		FCollisionShape::MakeBox(ConveyorPlacementClearanceExtents),
		OverlapParams);

	if (bHasOverlap)
	{
		for (const FOverlapResult& OverlapResult : OverlapResults)
		{
			if (OverlapResult.GetActor() == nullptr)
			{
				continue;
			}

			if (OverlapResult.GetActor() == this
				|| OverlapResult.GetActor() == DroneCompanion.Get()
				|| OverlapResult.GetActor() == ConveyorPlacementPreview.Get())
			{
				continue;
			}

			return true;
		}
	}

	bOutIsValid = true;
	return true;
}

bool AAgentCharacter::TryGetFactoryBuildableFaceSnapLocation(const FHitResult& AimHit, FVector& OutLocation) const
{
	AActor* HitActor = AimHit.GetActor();
	if (!HitActor)
	{
		return false;
	}

	const bool bIsSupportedBuildable = HitActor->IsA<AConveyorBeltStraight>()
		|| HitActor->IsA<AStorageBin>()
		|| HitActor->IsA<AResourceSpawnerMachine>();
	if (!bIsSupportedBuildable)
	{
		return false;
	}

	const FTransform BuildableTransform = HitActor->GetActorTransform();
	FVector LocalImpact = BuildableTransform.InverseTransformPosition(AimHit.ImpactPoint);
	LocalImpact.Z = 0.0f;

	if (LocalImpact.IsNearlyZero())
	{
		LocalImpact.X = AgentFactory::GridSize * 0.5f;
	}

	FVector SnapDirection = HitActor->GetActorForwardVector();
	if (FMath::Abs(LocalImpact.X) >= FMath::Abs(LocalImpact.Y))
	{
		SnapDirection *= (LocalImpact.X >= 0.0f) ? 1.0f : -1.0f;
	}
	else
	{
		SnapDirection = HitActor->GetActorRightVector() * ((LocalImpact.Y >= 0.0f) ? 1.0f : -1.0f);
	}

	OutLocation = AgentFactory::SnapLocationToGrid(HitActor->GetActorLocation() + SnapDirection * AgentFactory::GridSize);
	return true;
}

void AAgentCharacter::SelectFactoryPlacementType(EAgentFactoryPlacementType NewType, bool bToggleIfAlreadySelected)
{
	const bool bWasActive = bConveyorPlacementModeActive;
	const bool bWasSameType = CurrentFactoryPlacementType == NewType;
	CurrentFactoryPlacementType = NewType;

	if (!bWasActive)
	{
		EnterConveyorPlacementMode();
		return;
	}

	if (bToggleIfAlreadySelected && bWasSameType)
	{
		ExitConveyorPlacementMode();
		return;
	}

	UpdateConveyorPlacementPreview();
}

void AAgentCharacter::ApplyFactoryBuildableDefaults(AActor* SpawnedActor) const
{
	if (!SpawnedActor)
	{
		return;
	}

	if (AConveyorBeltStraight* Conveyor = Cast<AConveyorBeltStraight>(SpawnedActor))
	{
		Conveyor->SetMasterConveyorSettings(ConveyorMasterBeltSpeed);
		return;
	}

	if (AResourceSpawnerMachine* ResourceSpawner = Cast<AResourceSpawnerMachine>(SpawnedActor))
	{
		if (DefaultFactoryPayloadClass.Get())
		{
			ResourceSpawner->PayloadActorClass = DefaultFactoryPayloadClass;
		}
	}
}

bool AAgentCharacter::CanUsePickupInteraction() const
{
	if (bMiniMapModeActive || bConveyorPlacementModeActive || bMapModeActive)
	{
		return false;
	}

	return CurrentViewMode == EAgentViewMode::ThirdPerson
		|| CurrentViewMode == EAgentViewMode::FirstPerson;
}

bool AAgentCharacter::GetActivePickupView(FVector& OutLocation, FRotator& OutRotation) const
{
	if (CurrentViewMode == EAgentViewMode::FirstPerson && FirstPersonCamera)
	{
		OutLocation = FirstPersonCamera->GetComponentLocation();
		OutRotation = FirstPersonCamera->GetComponentRotation();
		return true;
	}

	if (CurrentViewMode == EAgentViewMode::ThirdPerson)
	{
		if (ThirdPersonTransitionCamera && ThirdPersonTransitionCamera->IsActive())
		{
			OutLocation = ThirdPersonTransitionCamera->GetComponentLocation();
			OutRotation = ThirdPersonTransitionCamera->GetComponentRotation();
			return true;
		}

		if (FollowCamera)
		{
			OutLocation = FollowCamera->GetComponentLocation();
			OutRotation = FollowCamera->GetComponentRotation();
			return true;
		}
	}

	return false;
}

void AAgentCharacter::UpdatePickupInteraction()
{
	if (HeldPickupComponent.IsValid())
	{
		UpdateHeldPickup();
		if (HeldPickupComponent.IsValid())
		{
			return;
		}
	}

	UpdatePickupCandidate();
}

bool AAgentCharacter::UpdatePickupCandidate()
{
	bPickupCandidateValid = false;
	PickupCandidateComponent.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	if (!GetActivePickupView(ViewLocation, ViewRotation))
	{
		return false;
	}

	const float TraceDistance = FMath::Max(0.0f, PickupTraceDistance);
	const float TraceRadius = FMath::Max(0.0f, PickupTraceRadius);
	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * TraceDistance;

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PickupTrace), false, this);
	QueryParams.bFindInitialOverlaps = true;
	QueryParams.AddIgnoredActor(this);

	if (DroneCompanion)
	{
		QueryParams.AddIgnoredActor(DroneCompanion.Get());
	}

	if (ConveyorPlacementPreview)
	{
		QueryParams.AddIgnoredActor(ConveyorPlacementPreview.Get());
	}

	TArray<FHitResult> Hits;
	const bool bHitAnything = World->SweepMultiByObjectType(
		Hits,
		ViewLocation,
		TraceEnd,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(TraceRadius),
		QueryParams);

	FHitResult BestHit;
	float BestDistanceSq = TNumericLimits<float>::Max();

	if (bHitAnything)
	{
		for (const FHitResult& Hit : Hits)
		{
			UPrimitiveComponent* HitComponent = Hit.GetComponent();
			if (!CanPickupComponent(HitComponent))
			{
				continue;
			}

			const FVector HitLocation = Hit.ImpactPoint.IsNearlyZero() ? Hit.Location : Hit.ImpactPoint;
			const float DistanceSq = FVector::DistSquared(ViewLocation, HitLocation);
			if (DistanceSq < BestDistanceSq)
			{
				BestDistanceSq = DistanceSq;
				BestHit = Hit;
			}
		}
	}

	if (BestDistanceSq < TNumericLimits<float>::Max())
	{
		PickupCandidateComponent = BestHit.GetComponent();
		PickupCandidateLocation = BestHit.ImpactPoint.IsNearlyZero() ? BestHit.Location : BestHit.ImpactPoint;
		bPickupCandidateValid = true;
		DrawPickupInfluenceDebug(PickupCandidateLocation, FColor::Green);
		return true;
	}

	PickupCandidateLocation = TraceEnd;
	DrawPickupInfluenceDebug(TraceEnd, FColor(200, 200, 200));
	return false;
}

void AAgentCharacter::DrawPickupInfluenceDebug(
	const FVector& SphereCenter,
	const FColor& Color) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!bShowPickupDebug)
	{
		return;
	}

	DrawDebugSphere(World, SphereCenter, FMath::Max(0.0f, PickupTraceRadius), 18, Color, false, 0.0f, 0, 1.25f);
}

bool AAgentCharacter::CanPickupComponent(const UPrimitiveComponent* PrimitiveComponent) const
{
	return PrimitiveComponent
		&& PrimitiveComponent->IsSimulatingPhysics()
		&& PrimitiveComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision;
}

bool AAgentCharacter::TryBeginPickup()
{
	if (!PickupPhysicsHandle)
	{
		return false;
	}

	if (HeldPickupComponent.IsValid())
	{
		return true;
	}

	if ((!bPickupCandidateValid || !PickupCandidateComponent.IsValid()) && !UpdatePickupCandidate())
	{
		return false;
	}

	UPrimitiveComponent* PrimitiveComponent = PickupCandidateComponent.Get();
	if (!CanPickupComponent(PrimitiveComponent))
	{
		return false;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	if (!GetActivePickupView(ViewLocation, ViewRotation))
	{
		return false;
	}

	PrimitiveComponent->WakeAllRigidBodies();
	PickupPhysicsHandle->ReleaseComponent();

	HeldPickupComponent = PrimitiveComponent;
	HeldPickupDistance = FMath::Clamp(
		FVector::Dist(ViewLocation, PickupCandidateLocation),
		FMath::Max(0.0f, PickupMinHoldDistance),
		FMath::Max(PickupMinHoldDistance, PickupMaxHoldDistance));
	HeldPickupMassKg = FMath::Max(0.0f, PrimitiveComponent->GetMass());
	HeldPickupTargetRotation = PrimitiveComponent->GetComponentRotation();
	bPickupRotationModeActive = false;
	SyncPickupHandleSettings();
	PickupPhysicsHandle->GrabComponentAtLocationWithRotation(
		PrimitiveComponent,
		NAME_None,
		PickupCandidateLocation,
		HeldPickupTargetRotation);
	return true;
}

void AAgentCharacter::EndPickup()
{
	if (PickupPhysicsHandle && PickupPhysicsHandle->GetGrabbedComponent() != nullptr)
	{
		PickupPhysicsHandle->ReleaseComponent();
	}

	HeldPickupComponent.Reset();
	HeldPickupDistance = 0.0f;
	HeldPickupMassKg = 0.0f;
	HeldPickupTargetRotation = FRotator::ZeroRotator;
	bPickupRotationModeActive = false;
}

void AAgentCharacter::UpdateHeldPickup()
{
	if (!PickupPhysicsHandle)
	{
		EndPickup();
		return;
	}

	UPrimitiveComponent* HeldComponent = HeldPickupComponent.Get();
	if (!CanPickupComponent(HeldComponent))
	{
		EndPickup();
		return;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	if (!GetActivePickupView(ViewLocation, ViewRotation))
	{
		EndPickup();
		return;
	}

	HeldPickupMassKg = FMath::Max(0.0f, HeldComponent->GetMass());
	SyncPickupHandleSettings();

	const float HoldDistance = FMath::Clamp(
		HeldPickupDistance,
		FMath::Max(0.0f, PickupMinHoldDistance),
		FMath::Max(PickupMinHoldDistance, PickupMaxHoldDistance));
	FVector DesiredTargetLocation = ViewLocation + ViewRotation.Vector() * HoldDistance;
	if (CurrentViewMode == EAgentViewMode::ThirdPerson)
	{
		DesiredTargetLocation += ViewRotation.RotateVector(ThirdPersonPickupHoldOffset);
	}

	PickupPhysicsHandle->SetTargetLocationAndRotation(DesiredTargetLocation, HeldPickupTargetRotation);
	DrawPickupInfluenceDebug(DesiredTargetLocation, FColor::Cyan);
}

void AAgentCharacter::SyncPickupHandleSettings() const
{
	if (!PickupPhysicsHandle)
	{
		return;
	}

	PickupPhysicsHandle->bInterpolateTarget = true;
	PickupPhysicsHandle->bSoftLinearConstraint = true;
	PickupPhysicsHandle->bSoftAngularConstraint = false;
	PickupPhysicsHandle->LinearStiffness = FMath::Max(0.0f, PickupHandleLinearStiffness);
	PickupPhysicsHandle->LinearDamping = FMath::Max(0.0f, PickupHandleLinearDamping);
	PickupPhysicsHandle->AngularStiffness = FMath::Max(0.0f, PickupHandleAngularStiffness);
	PickupPhysicsHandle->AngularDamping = FMath::Max(0.0f, PickupHandleAngularDamping);

	const float MassAlpha = FMath::Clamp(HeldPickupMassKg * FMath::Max(0.0f, CharacterPickupStrengthMultiplier), 0.0f, 1.0f);
	const float InterpolationSpeed = FMath::Lerp(
		PickupLightInterpolationSpeed,
		PickupHeavyInterpolationSpeed,
		MassAlpha);
	PickupPhysicsHandle->SetInterpolationSpeed(InterpolationSpeed);
}

void AAgentCharacter::UpdateDronePilotInputs(float DeltaSeconds)
{
	if (!DroneCompanion || !IsDroneInputModeActive())
	{
		return;
	}

	const bool bSimplePilotMode = !bMapModeActive && IsSimpleDronePilotMode();
	const bool bFreeFlyPilotMode = !bMapModeActive && IsFreeFlyDronePilotMode();
	const bool bSpectatorPilotMode = !bMapModeActive && IsSpectatorDronePilotMode();
	const bool bRollPilotMode = !bMapModeActive && IsRollDronePilotMode();
	const float KeyboardForwardInput = (bDronePitchForwardHeld ? 1.0f : 0.0f) - (bDronePitchBackwardHeld ? 1.0f : 0.0f);
	float KeyboardRightInput = (bDroneRollRightHeld ? 1.0f : 0.0f) - (bDroneRollLeftHeld ? 1.0f : 0.0f);
	float KeyboardVerticalInput = 0.0f;
	float KeyboardYawInput = 0.0f;

	if (bMapModeActive)
	{
		const float KeyboardUpInput = (bDroneYawRightHeld ? 1.0f : 0.0f) + (bDroneThrottleUpHeld ? 1.0f : 0.0f);
		const float KeyboardDownInput = (bDroneYawLeftHeld ? 1.0f : 0.0f) + (bDroneThrottleDownHeld ? 1.0f : 0.0f);
		KeyboardVerticalInput = FMath::Clamp(KeyboardUpInput - KeyboardDownInput, -1.0f, 1.0f);
	}
	else if (bSimplePilotMode)
	{
		KeyboardYawInput = (bDroneYawRightHeld ? 1.0f : 0.0f) - (bDroneYawLeftHeld ? 1.0f : 0.0f);
		KeyboardVerticalInput = (bDroneThrottleUpHeld ? 1.0f : 0.0f) - (bDroneThrottleDownHeld ? 1.0f : 0.0f);
	}
	else if (bFreeFlyPilotMode || bSpectatorPilotMode)
	{
		const float KeyboardUpInput = (bDroneYawRightHeld ? 1.0f : 0.0f) + (bDroneThrottleUpHeld ? 1.0f : 0.0f);
		const float KeyboardDownInput = (bDroneYawLeftHeld ? 1.0f : 0.0f) + (bDroneThrottleDownHeld ? 1.0f : 0.0f);
		KeyboardVerticalInput = FMath::Clamp(KeyboardUpInput - KeyboardDownInput, -1.0f, 1.0f);
	}
	else if (bRollPilotMode)
	{
		KeyboardYawInput = 0.0f;
		KeyboardVerticalInput = 0.0f;
	}
	else
	{
		KeyboardYawInput = (bDroneYawRightHeld ? 1.0f : 0.0f) - (bDroneYawLeftHeld ? 1.0f : 0.0f);
		KeyboardVerticalInput = (bDroneThrottleUpHeld ? 1.0f : 0.0f) - (bDroneThrottleDownHeld ? 1.0f : 0.0f);
	}

	float GamepadVerticalInput = DroneGamepadThrottleInput;
	float GamepadYawInput = DroneGamepadYawInput;
	float GamepadRightInput = DroneGamepadRollInput;
	float GamepadForwardInput = DroneGamepadPitchInput;

	if (bMapModeActive)
	{
		GamepadVerticalInput = DroneGamepadLeftTriggerInput - DroneGamepadRightTriggerInput;
		GamepadYawInput = 0.0f;
		GamepadRightInput = DroneGamepadYawInput;
		GamepadForwardInput = DroneGamepadThrottleInput;
	}
	else if (bSimplePilotMode)
	{
		GamepadVerticalInput = DroneGamepadRightTriggerInput - DroneGamepadLeftTriggerInput;
		GamepadYawInput = DroneGamepadRollInput;
		GamepadRightInput = DroneGamepadYawInput;
		GamepadForwardInput = DroneGamepadThrottleInput;

		if (FMath::Abs(DroneGamepadPitchInput) > KINDA_SMALL_NUMBER)
		{
			DroneCompanion->AdjustCameraTilt(DroneGamepadPitchInput * DroneCompanion->SimpleCameraTiltAxisSpeed * DeltaSeconds);
		}
	}
	else if (bFreeFlyPilotMode || bSpectatorPilotMode)
	{
		GamepadVerticalInput = DroneGamepadRightTriggerInput - DroneGamepadLeftTriggerInput;
		GamepadYawInput = 0.0f;
		GamepadRightInput = DroneGamepadYawInput;
		GamepadForwardInput = DroneGamepadThrottleInput;
	}
	else if (bRollPilotMode)
	{
		GamepadVerticalInput = 0.0f;
		GamepadYawInput = 0.0f;
		GamepadRightInput = DroneGamepadYawInput;
		GamepadForwardInput = DroneGamepadThrottleInput;
	}

	const float VerticalInput = FMath::Clamp(KeyboardVerticalInput + GamepadVerticalInput, -1.0f, 1.0f);
	const float YawInput = FMath::Clamp(KeyboardYawInput + GamepadYawInput, -1.0f, 1.0f);
	const float RightInput = FMath::Clamp(KeyboardRightInput + GamepadRightInput, -1.0f, 1.0f);
	const float ForwardInput = FMath::Clamp(KeyboardForwardInput + GamepadForwardInput, -1.0f, 1.0f);

	if (bDroneEntryAssistActive
		&& IsDronePilotMode()
		&& !bFreeFlyPilotMode
		&& !bSpectatorPilotMode
		&& !bRollPilotMode
		&& FMath::Abs(VerticalInput) > FMath::Max(0.0f, DroneEntryAssistReleaseThreshold))
	{
		bDroneEntryAssistActive = false;
		ApplyDroneAssistState();
	}

	DroneCompanion->SetPilotInputs(VerticalInput, YawInput, RightInput, ForwardInput);
}

void AAgentCharacter::ResetDroneInputState()
{
	DroneGamepadThrottleInput = 0.0f;
	DroneGamepadYawInput = 0.0f;
	DroneGamepadRollInput = 0.0f;
	DroneGamepadPitchInput = 0.0f;
	DroneGamepadLeftTriggerInput = 0.0f;
	DroneGamepadRightTriggerInput = 0.0f;
	bDronePitchForwardHeld = false;
	bDronePitchBackwardHeld = false;
	bDroneRollLeftHeld = false;
	bDroneRollRightHeld = false;
	bDroneYawLeftHeld = false;
	bDroneYawRightHeld = false;
	bDroneThrottleUpHeld = false;
	bDroneThrottleDownHeld = false;
	bRollModeHoldStartedInFlight = false;
	bPendingRollReleaseToFlight = false;
	bCrashRollRecoveryActive = false;
	bRollModeJumpHeld = false;
	CrashRollRecoveryStableTime = 0.0f;
	RollToFlightAssistTimeRemaining = 0.0f;
	RollModeJumpHeldDuration = 0.0f;
	StoredRollJumpChargeAlpha = 0.0f;

	if (DroneCompanion)
	{
		DroneCompanion->ResetPilotInputs();
	}
}

void AAgentCharacter::ResetCharacterCameraRoll()
{
	if (AController* LocalController = GetController())
	{
		FRotator ResetRotation = LocalController->GetControlRotation();
		ResetRotation.Pitch = 0.0f;
		ResetRotation.Roll = 0.0f;
		LocalController->SetControlRotation(ResetRotation);
	}

	if (CameraBoom)
	{
		FRotator BoomRotation = CameraBoom->GetRelativeRotation();
		BoomRotation.Pitch = 0.0f;
		BoomRotation.Roll = 0.0f;
		CameraBoom->SetRelativeRotation(BoomRotation);
	}

	if (FirstPersonCamera)
	{
		FRotator CameraRotation = FirstPersonCamera->GetRelativeRotation();
		CameraRotation.Pitch = 0.0f;
		CameraRotation.Roll = 0.0f;
		FirstPersonCamera->SetRelativeRotation(CameraRotation);
	}

	if (ThirdPersonTransitionCamera)
	{
		FRotator CameraRotation = ThirdPersonTransitionCamera->GetRelativeRotation();
		CameraRotation.Pitch = 0.0f;
		CameraRotation.Roll = 0.0f;
		ThirdPersonTransitionCamera->SetRelativeRotation(CameraRotation);
	}
}

void AAgentCharacter::UpdateDronePilotCameraLimits()
{
	APlayerController* const PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController || !PlayerController->PlayerCameraManager)
	{
		return;
	}

	APlayerCameraManager* const CameraManager = PlayerController->PlayerCameraManager;
	if (!bHasStoredDefaultViewPitchLimits)
	{
		DefaultViewPitchMin = CameraManager->ViewPitchMin;
		DefaultViewPitchMax = CameraManager->ViewPitchMax;
		bHasStoredDefaultViewPitchLimits = true;
	}

	const float FreeFlyPitchLimit = FMath::Clamp(FreeFlyUnlockedPitchLimit, 90.0f, 179.0f);
	const float CurrentPitch = FRotator::NormalizeAxis(GetControlRotation().Pitch);
	const float MinPitch = FMath::Min(DefaultViewPitchMin, DefaultViewPitchMax);
	const float MaxPitch = FMath::Max(DefaultViewPitchMin, DefaultViewPitchMax);
	const bool bNeedPitchUnwind = CurrentPitch < MinPitch || CurrentPitch > MaxPitch;
	const bool bUseUnlockedPitch = IsDronePilotMode()
		&& !bMapModeActive
		&& !bMiniMapModeActive
		&& (IsFreeFlyDronePilotMode() || bNeedPitchUnwind);

	CameraManager->ViewPitchMin = bUseUnlockedPitch ? -FreeFlyPitchLimit : DefaultViewPitchMin;
	CameraManager->ViewPitchMax = bUseUnlockedPitch ? FreeFlyPitchLimit : DefaultViewPitchMax;
}

void AAgentCharacter::UpdateRollModeJumpHold(float DeltaSeconds)
{
	RollToFlightAssistTimeRemaining = FMath::Max(0.0f, RollToFlightAssistTimeRemaining - DeltaSeconds);

	if (DroneCompanion
		&& bUseCrashRollRecovery
		&& IsDronePilotMode()
		&& !bMapModeActive
		&& !bMiniMapModeActive
		&& DroneCompanion->ConsumePendingCrashRollRecovery())
	{
		EnterRollTransitionMode(false, true);
	}

	if (bCrashRollRecoveryActive)
	{
		if (!DroneCompanion || !IsDronePilotMode() || !IsRollDronePilotMode())
		{
			bCrashRollRecoveryActive = false;
			CrashRollRecoveryStableTime = 0.0f;
		}
		else if (DroneCompanion->IsStableForRollRecovery())
		{
			CrashRollRecoveryStableTime += FMath::Max(0.0f, DeltaSeconds);
			if (CrashRollRecoveryStableTime >= FMath::Max(0.0f, CrashRollRecoveryStableDelay))
			{
				ExitRollTransitionMode(true);
				return;
			}
		}
		else
		{
			CrashRollRecoveryStableTime = 0.0f;
		}
	}

	if (!bRollModeJumpHeld)
	{
		return;
	}

	if (!IsDronePilotMode() || !IsRollDronePilotMode() || !DroneCompanion)
	{
		bRollModeJumpHeld = false;
		CrashRollRecoveryStableTime = 0.0f;
		RollModeJumpHeldDuration = 0.0f;
		return;
	}

	RollModeJumpHeldDuration += FMath::Max(0.0f, DeltaSeconds);
}

void AAgentCharacter::SyncControllerRotationToDroneCamera()
{
	if (!DroneCompanion || !GetController())
	{
		return;
	}

	FVector DroneCameraLocation = FVector::ZeroVector;
	FRotator DroneCameraRotation = FRotator::ZeroRotator;
	if (!DroneCompanion->GetDroneCameraTransform(DroneCameraLocation, DroneCameraRotation))
	{
		return;
	}

	GetController()->SetControlRotation(DroneCameraRotation);
	DroneCompanion->SetViewReferenceRotation(DroneCameraRotation);
}

bool AAgentCharacter::IsDronePilotMode() const
{
	return CurrentViewMode == EAgentViewMode::DronePilot;
}

bool AAgentCharacter::IsDroneInputModeActive() const
{
	return IsDronePilotMode() || bMapModeActive;
}

bool AAgentCharacter::IsSimpleDronePilotMode() const
{
	return DronePilotControlMode == EAgentDronePilotControlMode::Simple;
}

bool AAgentCharacter::IsFreeFlyDronePilotMode() const
{
	return DronePilotControlMode == EAgentDronePilotControlMode::FreeFly;
}

bool AAgentCharacter::IsSpectatorDronePilotMode() const
{
	return DronePilotControlMode == EAgentDronePilotControlMode::SpectatorCam;
}

bool AAgentCharacter::IsRollDronePilotMode() const
{
	return DronePilotControlMode == EAgentDronePilotControlMode::Roll;
}

void AAgentCharacter::ToggleDronePilotControlMode()
{
	CycleDronePilotControlMode(1);
}

void AAgentCharacter::SetDronePilotControlMode(EAgentDronePilotControlMode NewMode)
{
	if (DroneCompanion && IsDroneInputModeActive())
	{
		SyncControllerRotationToDroneCamera();
	}

	if (NewMode != EAgentDronePilotControlMode::Roll)
	{
		bRollModeHoldStartedInFlight = false;
		bPendingRollReleaseToFlight = false;
		bRollModeJumpHeld = false;
		RollModeJumpHeldDuration = 0.0f;
		bCrashRollRecoveryActive = false;
		CrashRollRecoveryStableTime = 0.0f;
		StoredRollJumpChargeAlpha = 0.0f;
		LastFlightDronePilotControlMode = NewMode;
	}

	DronePilotControlMode = NewMode;

	switch (DronePilotControlMode)
	{
	case EAgentDronePilotControlMode::Roll:
		bUseSimpleDronePilotControls = false;
		bDroneStabilizerEnabled = false;
		bDroneHoverModeEnabled = false;
		break;
	case EAgentDronePilotControlMode::FreeFly:
	case EAgentDronePilotControlMode::SpectatorCam:
		bUseSimpleDronePilotControls = false;
		bDroneStabilizerEnabled = false;
		bDroneHoverModeEnabled = false;
		break;
	case EAgentDronePilotControlMode::Simple:
		bUseSimpleDronePilotControls = true;
		bDroneStabilizerEnabled = true;
		bDroneHoverModeEnabled = true;
		break;
	case EAgentDronePilotControlMode::HorizonHover:
		bUseSimpleDronePilotControls = false;
		bDroneStabilizerEnabled = true;
		bDroneHoverModeEnabled = true;
		break;
	case EAgentDronePilotControlMode::Horizon:
		bUseSimpleDronePilotControls = false;
		bDroneStabilizerEnabled = true;
		bDroneHoverModeEnabled = false;
		break;
	case EAgentDronePilotControlMode::Complex:
	default:
		bUseSimpleDronePilotControls = false;
		bDroneStabilizerEnabled = false;
		bDroneHoverModeEnabled = false;
		break;
	}

	if (DroneCompanion)
	{
		SyncDroneCompanionControlState(IsDronePilotMode() && !bMapModeActive);
	}
}

void AAgentCharacter::CycleDronePilotControlMode(int32 Direction)
{
	constexpr int32 PilotModeCount = 7;
	const int32 CurrentIndex = static_cast<int32>(DronePilotControlMode);
	const int32 WrappedIndex = (CurrentIndex + Direction + PilotModeCount) % PilotModeCount;
	SetDronePilotControlMode(static_cast<EAgentDronePilotControlMode>(WrappedIndex));

	if (DroneCompanion
		&& (DroneCompanion->GetCompanionMode() == EDroneCompanionMode::MapMode
			|| DroneCompanion->GetCompanionMode() == EDroneCompanionMode::MiniMapFollow))
	{
		SyncDroneCompanionControlState(false, true);
	}
}

void AAgentCharacter::ToggleMapMode()
{
	APlayerController* PlayerController = Cast<APlayerController>(GetController());

	if (bMiniMapModeActive)
	{
		ExitMiniMapMode();
		return;
	}

	if (!bMapModeActive)
	{
		SyncControllerRotationToDroneCamera();

		if (bConveyorPlacementModeActive)
		{
			ExitConveyorPlacementMode();
		}

		if (!DroneCompanion)
		{
			FVector DroneSpawnLocation = FVector::ZeroVector;
			FRotator DroneSpawnRotation = FRotator::ZeroRotator;
			if (CurrentViewMode == EAgentViewMode::ThirdPerson
				&& ThirdPersonTransitionCamera
				&& ThirdPersonTransitionCamera->IsActive())
			{
				DroneSpawnLocation = ThirdPersonTransitionCamera->GetComponentLocation();
				DroneSpawnRotation = ThirdPersonTransitionCamera->GetComponentRotation();
			}
			else
			{
				GetThirdPersonDroneTarget(DroneSpawnLocation, DroneSpawnRotation);
			}
			SpawnDroneCompanionAtTransform(DroneSpawnLocation, DroneSpawnRotation, EDroneCompanionMode::MapMode);
		}

		if (!DroneCompanion)
		{
			return;
		}

		SetThirdPersonProxyVisible(false);
		bMapModeActive = true;
		SyncDroneCompanionControlState(false, true);
		DroneCompanion->SetNextMapModeUsesEntryLift(CurrentViewMode != EAgentViewMode::DronePilot);
		DroneCompanion->SetCompanionMode(EDroneCompanionMode::MapMode);

		if (PlayerController)
		{
			PlayerController->SetViewTargetWithBlend(DroneCompanion.Get(), ViewBlendTime);
		}

		return;
	}

	bMapModeActive = false;
	ApplyViewMode(CurrentViewMode, true);
}

void AAgentCharacter::EnterMiniMapMode()
{
	if (bMiniMapModeActive || bMapModeActive)
	{
		return;
	}

	if (bConveyorPlacementModeActive)
	{
		ExitConveyorPlacementMode();
	}

	SyncControllerRotationToDroneCamera();
	MiniMapReturnViewMode = CurrentViewMode;
	ApplyViewMode(EAgentViewMode::FirstPerson, true);

	if (!DroneCompanion)
	{
		return;
	}

	bMiniMapModeActive = true;
	bMiniMapViewingDroneCamera = false;
	SetThirdPersonProxyVisible(false);
	SyncDroneCompanionControlState(false, true);
	DroneCompanion->SetCompanionMode(EDroneCompanionMode::MiniMapFollow);
}

void AAgentCharacter::ExitMiniMapMode()
{
	if (!bMiniMapModeActive)
	{
		return;
	}

	ApplyViewMode(MiniMapReturnViewMode, true);
}

void AAgentCharacter::FocusMiniMapDroneCamera()
{
	if (!bMiniMapModeActive || !DroneCompanion)
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return;
	}

	bMiniMapViewingDroneCamera = true;
	PlayerController->SetViewTargetWithBlend(DroneCompanion.Get(), ViewBlendTime);
}

void AAgentCharacter::UpdateControllerMapButtonHold(float DeltaSeconds)
{
	if (!bControllerMapButtonHeld || bControllerMapButtonTriggeredMiniMap)
	{
		return;
	}

	ControllerMapButtonHeldDuration += FMath::Max(0.0f, DeltaSeconds);
	if (!bMapModeActive
		&& !bMiniMapModeActive
		&& ControllerMapButtonHeldDuration >= FMath::Max(0.0f, ControllerMapButtonHoldTime))
	{
		bControllerMapButtonTriggeredMiniMap = true;
		EnterMiniMapMode();
	}
}

void AAgentCharacter::UpdateKeyboardMapButtonHold(float DeltaSeconds)
{
	if (!bKeyboardMapButtonHeld || bKeyboardMapButtonTriggeredMiniMap)
	{
		return;
	}

	KeyboardMapButtonHeldDuration += FMath::Max(0.0f, DeltaSeconds);
	if (!bMapModeActive
		&& !bMiniMapModeActive
		&& KeyboardMapButtonHeldDuration >= FMath::Max(0.0f, ControllerMapButtonHoldTime))
	{
		bKeyboardMapButtonTriggeredMiniMap = true;
		EnterMiniMapMode();
	}
}

void AAgentCharacter::DoMove(float Right, float Forward)
{
	if (IsDroneInputModeActive() && bLockCharacterMovementDuringDronePilot)
	{
		return;
	}

	if (GetController() == nullptr)
	{
		return;
	}

	const FRotator Rotation = GetController()->GetControlRotation();
	const FRotator YawRotation(0.0f, Rotation.Yaw, 0.0f);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDirection, Forward);
	AddMovementInput(RightDirection, Right);
}

void AAgentCharacter::DoLook(float Yaw, float Pitch)
{
	if (IsDroneInputModeActive()
		&& bLockCharacterMovementDuringDronePilot
		&& !(IsDronePilotMode() && (IsFreeFlyDronePilotMode() || IsSpectatorDronePilotMode() || IsRollDronePilotMode())))
	{
		return;
	}

	if (GetController() == nullptr)
	{
		return;
	}

	AddControllerYawInput(Yaw);
	AddControllerPitchInput(Pitch);
}

void AAgentCharacter::DoJumpStart()
{
	if (IsDronePilotMode() && DroneCompanion && !bMapModeActive && !bMiniMapModeActive)
	{
		if (!IsRollDronePilotMode())
		{
			EnterRollTransitionMode(true, false);
		}

		if (!bRollModeJumpHeld)
		{
			bRollModeJumpHeld = true;
			RollModeJumpHeldDuration = 0.0f;
		}
		return;
	}

	if (IsDroneInputModeActive() && bLockCharacterMovementDuringDronePilot)
	{
		return;
	}

	Jump();
}

void AAgentCharacter::DoJumpEnd()
{
	if (bRollModeJumpHeld)
	{
		const float HeldDuration = RollModeJumpHeldDuration;
		const float ChargeAlpha = RollModeJumpHoldTime > KINDA_SMALL_NUMBER
			? FMath::Clamp(HeldDuration / RollModeJumpHoldTime, 0.0f, 1.0f)
			: 1.0f;
		bRollModeJumpHeld = false;
		RollModeJumpHeldDuration = 0.0f;

		if (IsDronePilotMode() && IsRollDronePilotMode() && DroneCompanion)
		{
			if (bRollModeHoldStartedInFlight)
			{
				bRollModeHoldStartedInFlight = false;
				StoredRollJumpChargeAlpha = FMath::Max(StoredRollJumpChargeAlpha, ChargeAlpha);
				return;
			}

			const float EffectiveChargeAlpha = FMath::Max(StoredRollJumpChargeAlpha, ChargeAlpha);
			StoredRollJumpChargeAlpha = 0.0f;
			if (!DroneCompanion->TryRollJump(EffectiveChargeAlpha))
			{
				ExitRollTransitionMode(true);
			}
		}
		return;
	}

	if (IsDronePilotMode() && IsRollDronePilotMode())
	{
		return;
	}

	if (IsDroneInputModeActive() && bLockCharacterMovementDuringDronePilot)
	{
		return;
	}

	StopJumping();
}

void AAgentCharacter::OnDronePitchForwardPressed()
{
	bDronePitchForwardHeld = true;
}

void AAgentCharacter::OnDronePitchForwardReleased()
{
	bDronePitchForwardHeld = false;
}

void AAgentCharacter::OnDronePitchBackwardPressed()
{
	bDronePitchBackwardHeld = true;
}

void AAgentCharacter::OnDronePitchBackwardReleased()
{
	bDronePitchBackwardHeld = false;
}

void AAgentCharacter::OnDroneRollLeftPressed()
{
	bDroneRollLeftHeld = true;
}

void AAgentCharacter::OnDroneRollLeftReleased()
{
	bDroneRollLeftHeld = false;
}

void AAgentCharacter::OnDroneRollRightPressed()
{
	bDroneRollRightHeld = true;
}

void AAgentCharacter::OnDroneRollRightReleased()
{
	bDroneRollRightHeld = false;
}

void AAgentCharacter::OnDroneYawLeftPressed()
{
	bDroneYawLeftHeld = true;
}

void AAgentCharacter::OnDroneYawLeftReleased()
{
	bDroneYawLeftHeld = false;
}

void AAgentCharacter::OnDroneYawRightPressed()
{
	bDroneYawRightHeld = true;
}

void AAgentCharacter::OnDroneYawRightReleased()
{
	bDroneYawRightHeld = false;
}

void AAgentCharacter::OnDroneThrottleUpPressed()
{
	if (bConveyorPlacementModeActive)
	{
		RotateConveyorPlacement(1);
		return;
	}

	bDroneThrottleUpHeld = true;
}

void AAgentCharacter::OnDroneThrottleUpReleased()
{
	if (bConveyorPlacementModeActive)
	{
		return;
	}

	bDroneThrottleUpHeld = false;
}

void AAgentCharacter::OnDroneThrottleDownPressed()
{
	bDroneThrottleDownHeld = true;
}

void AAgentCharacter::OnDroneThrottleDownReleased()
{
	bDroneThrottleDownHeld = false;
}

void AAgentCharacter::OnDroneGamepadYawAxis(float Value)
{
	DroneGamepadYawInput = Value;
}

void AAgentCharacter::OnDroneGamepadThrottleAxis(float Value)
{
	DroneGamepadThrottleInput = Value;
}

void AAgentCharacter::OnDroneGamepadRollAxis(float Value)
{
	if (bPickupRotationModeActive && bControllerPickupHeld && HeldPickupComponent.IsValid())
	{
		const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : (1.0f / 60.0f);
		HeldPickupTargetRotation.Yaw = FRotator::NormalizeAxis(
			HeldPickupTargetRotation.Yaw + (Value * PickupRotationYawSpeed * DeltaSeconds));
		DroneGamepadRollInput = 0.0f;
		return;
	}

	DroneGamepadRollInput = Value;
}

void AAgentCharacter::OnDroneGamepadPitchAxis(float Value)
{
	if (bPickupRotationModeActive && bControllerPickupHeld && HeldPickupComponent.IsValid())
	{
		const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : (1.0f / 60.0f);
		HeldPickupTargetRotation.Pitch = FRotator::NormalizeAxis(
			HeldPickupTargetRotation.Pitch + (Value * PickupRotationPitchSpeed * DeltaSeconds));
		DroneGamepadPitchInput = 0.0f;
		return;
	}

	DroneGamepadPitchInput = Value;
}

void AAgentCharacter::OnDroneGamepadLeftTriggerAxis(float Value)
{
	DroneGamepadLeftTriggerInput = FMath::Clamp(Value, 0.0f, 1.0f);
	bPickupRotationModeActive = bControllerPickupHeld
		&& HeldPickupComponent.IsValid()
		&& CanUsePickupInteraction()
		&& DroneGamepadLeftTriggerInput >= FMath::Max(0.0f, PickupRotationTriggerThreshold);
}

void AAgentCharacter::OnDroneGamepadRightTriggerAxis(float Value)
{
	if (bControllerPickupHeld && CanUsePickupInteraction())
	{
		DroneGamepadRightTriggerInput = 0.0f;
		return;
	}

	DroneGamepadRightTriggerInput = FMath::Clamp(Value, 0.0f, 1.0f);
}

void AAgentCharacter::OnDroneCameraTiltUpPressed()
{
	if (DroneCompanion)
	{
		DroneCompanion->AdjustCameraTilt(DroneCompanion->DroneCameraTiltStep);
	}
}

void AAgentCharacter::OnDroneCameraTiltDownPressed()
{
	if (DroneCompanion)
	{
		DroneCompanion->AdjustCameraTilt(-DroneCompanion->DroneCameraTiltStep);
	}
}

void AAgentCharacter::OnDroneControlModeTogglePressed()
{
	if (bConveyorPlacementModeActive)
	{
		return;
	}

	ToggleDronePilotControlMode();
}

void AAgentCharacter::OnDroneHoverModePressed()
{
	if (bConveyorPlacementModeActive)
	{
		return;
	}

	CycleDronePilotControlMode(-1);
}

void AAgentCharacter::OnDroneStabilizerTogglePressed()
{
	if (bConveyorPlacementModeActive)
	{
		return;
	}

	CycleDronePilotControlMode(1);
}

void AAgentCharacter::OnMapModePressed()
{
	if (bMiniMapModeActive)
	{
		FocusMiniMapDroneCamera();
		return;
	}

	ToggleMapMode();
}

void AAgentCharacter::OnKeyboardMapButtonPressed()
{
	bKeyboardMapButtonHeld = true;
	bKeyboardMapButtonTriggeredMiniMap = false;
	KeyboardMapButtonHeldDuration = 0.0f;
}

void AAgentCharacter::OnKeyboardMapButtonReleased()
{
	const bool bWasMiniMapHold = bKeyboardMapButtonTriggeredMiniMap;
	bKeyboardMapButtonHeld = false;
	bKeyboardMapButtonTriggeredMiniMap = false;
	KeyboardMapButtonHeldDuration = 0.0f;

	if (bWasMiniMapHold)
	{
		return;
	}

	if (bMiniMapModeActive)
	{
		FocusMiniMapDroneCamera();
		return;
	}

	ToggleMapMode();
}

void AAgentCharacter::OnControllerMapButtonPressed()
{
	bControllerMapButtonHeld = true;
	bControllerMapButtonTriggeredMiniMap = false;
	ControllerMapButtonHeldDuration = 0.0f;
}

void AAgentCharacter::OnControllerMapButtonReleased()
{
	const bool bWasMiniMapHold = bControllerMapButtonTriggeredMiniMap;
	bControllerMapButtonHeld = false;
	bControllerMapButtonTriggeredMiniMap = false;
	ControllerMapButtonHeldDuration = 0.0f;

	if (bWasMiniMapHold)
	{
		return;
	}

	if (bMiniMapModeActive)
	{
		FocusMiniMapDroneCamera();
		return;
	}

	ToggleMapMode();
}

void AAgentCharacter::OnConveyorPlacementModePressed()
{
	SelectFactoryPlacementType(EAgentFactoryPlacementType::Conveyor, true);
}

void AAgentCharacter::OnStorageBinPlacementModePressed()
{
	SelectFactoryPlacementType(EAgentFactoryPlacementType::StorageBin, true);
}

void AAgentCharacter::OnResourceSpawnerPlacementModePressed()
{
	SelectFactoryPlacementType(EAgentFactoryPlacementType::ResourceSpawner, true);
}

void AAgentCharacter::OnFactoryPlacementTogglePressed()
{
	ToggleConveyorPlacementMode();
}

void AAgentCharacter::OnConveyorPlacePressed()
{
	TryPlaceConveyor();
}

void AAgentCharacter::OnConveyorCancelPressed()
{
	CancelConveyorPlacement();
}

void AAgentCharacter::OnConveyorRotateLeftPressed()
{
	RotateConveyorPlacement(-1);
}

void AAgentCharacter::OnConveyorRotateRightPressed()
{
	RotateConveyorPlacement(1);
}

void AAgentCharacter::OnPickupOrDroneYawRightPressed()
{
	if (CanUsePickupInteraction() && TryBeginPickup())
	{
		bKeyboardPickupHeld = true;
		return;
	}

	if (IsDroneInputModeActive())
	{
		OnDroneYawRightPressed();
	}
}

void AAgentCharacter::OnPickupOrDroneYawRightReleased()
{
	if (bKeyboardPickupHeld)
	{
		bKeyboardPickupHeld = false;
		EndPickup();
		return;
	}

	OnDroneYawRightReleased();
}

void AAgentCharacter::OnPickupOrPlacePressed()
{
	if (bConveyorPlacementModeActive)
	{
		OnConveyorPlacePressed();
		return;
	}

	if (CanUsePickupInteraction() && TryBeginPickup())
	{
		bControllerPickupHeld = true;
		bPickupRotationModeActive = HeldPickupComponent.IsValid()
			&& DroneGamepadLeftTriggerInput >= FMath::Max(0.0f, PickupRotationTriggerThreshold);
	}
}

void AAgentCharacter::OnPickupOrPlaceReleased()
{
	if (!bControllerPickupHeld)
	{
		return;
	}

	bControllerPickupHeld = false;
	bPickupRotationModeActive = false;
	EndPickup();
}
