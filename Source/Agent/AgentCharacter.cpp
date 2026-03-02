// Copyright Epic Games, Inc. All Rights Reserved.

#include "AgentCharacter.h"
#include "DroneCompanion.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
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

	UpdateKeyboardMapButtonHold(DeltaSeconds);
	UpdateControllerMapButtonHold(DeltaSeconds);

	if (DroneCompanion)
	{
		DroneCompanion->SetFollowTarget(this);
		DroneCompanion->SetViewReferenceRotation(GetControlRotation());
		DroneCompanion->SetUseSimplePilotControls(bUseSimpleDronePilotControls);
		DroneCompanion->SetUseFreeFlyPilotControls(IsFreeFlyDronePilotMode());
		ApplyDroneAssistState();
		FVector ThirdPersonTargetLocation = FVector::ZeroVector;
		FRotator ThirdPersonTargetRotation = FRotator::ZeroRotator;
		if (GetThirdPersonDroneTarget(ThirdPersonTargetLocation, ThirdPersonTargetRotation))
		{
			DroneCompanion->SetThirdPersonCameraTarget(ThirdPersonTargetLocation, ThirdPersonTargetRotation);
		}

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

	PlayerInputComponent->BindKey(EKeys::V, IE_Pressed, this, &AAgentCharacter::CycleViewMode);
	PlayerInputComponent->BindKey(EKeys::Gamepad_FaceButton_Top, IE_Pressed, this, &AAgentCharacter::CycleViewMode);
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
	PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, this, &AAgentCharacter::OnDroneYawRightPressed);
	PlayerInputComponent->BindKey(EKeys::E, IE_Released, this, &AAgentCharacter::OnDroneYawRightReleased);
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
	PlayerInputComponent->BindKey(EKeys::Gamepad_DPad_Left, IE_Pressed, this, &AAgentCharacter::OnDroneControlModeTogglePressed);
	PlayerInputComponent->BindKey(EKeys::Gamepad_LeftShoulder, IE_Pressed, this, &AAgentCharacter::OnDroneHoverModePressed);
	PlayerInputComponent->BindKey(EKeys::Gamepad_RightShoulder, IE_Pressed, this, &AAgentCharacter::OnDroneStabilizerTogglePressed);
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
	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	DoLook(LookAxisVector.X, LookAxisVector.Y);
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

	EAgentViewMode NextMode = EAgentViewMode::ThirdPerson;

	switch (CurrentViewMode)
	{
	case EAgentViewMode::ThirdPerson:
		NextMode = EAgentViewMode::DronePilot;
		break;
	case EAgentViewMode::DronePilot:
		NextMode = EAgentViewMode::FirstPerson;
		break;
	case EAgentViewMode::FirstPerson:
	default:
		NextMode = EAgentViewMode::ThirdPerson;
		break;
	}

	ApplyViewMode(NextMode, true);
}

void AAgentCharacter::ApplyViewMode(EAgentViewMode NewMode, bool bBlend)
{
	bMapModeActive = false;
	bMiniMapModeActive = false;
	bMiniMapViewingDroneCamera = false;
	const EAgentViewMode PreviousViewMode = CurrentViewMode;
	const bool bWasDronePilot = PreviousViewMode == EAgentViewMode::DronePilot;
	FVector ThirdPersonTargetLocation = GetActorLocation();
	FRotator ThirdPersonTargetRotation = GetControlRotation();
	GetThirdPersonDroneTarget(ThirdPersonTargetLocation, ThirdPersonTargetRotation);
	FVector ThirdPersonSwapStartLocation = ThirdPersonTargetLocation;
	FRotator ThirdPersonSwapStartRotation = ThirdPersonTargetRotation;
	const bool bUseFirstToThirdPersonSwap = NewMode == EAgentViewMode::ThirdPerson
		&& PreviousViewMode == EAgentViewMode::FirstPerson;

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
			DroneCompanion->SetFollowTarget(this);
			DroneCompanion->SetViewReferenceRotation(GetControlRotation());
			DroneCompanion->SetUseSimplePilotControls(bUseSimpleDronePilotControls);
			DroneCompanion->SetUseFreeFlyPilotControls(IsFreeFlyDronePilotMode());
			ApplyDroneAssistState();
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
			DroneCompanion->SetFollowTarget(this);
			DroneCompanion->SetViewReferenceRotation(GetControlRotation());
			DroneCompanion->SetUseSimplePilotControls(bUseSimpleDronePilotControls);
			DroneCompanion->SetUseFreeFlyPilotControls(IsFreeFlyDronePilotMode());
			ApplyDroneAssistState();
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

	if (IsFreeFlyDronePilotMode())
	{
		DroneCompanion->SetPilotStabilizerEnabled(false);
		DroneCompanion->SetPilotHoverModeEnabled(false);
		return;
	}

	DroneCompanion->SetPilotStabilizerEnabled(bDroneEntryAssistActive || bDroneStabilizerEnabled);
	DroneCompanion->SetPilotHoverModeEnabled(bDroneEntryAssistActive || bDroneHoverModeEnabled);
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

	DroneCompanion->SetFollowTarget(this);
	DroneCompanion->SetViewReferenceRotation(GetControlRotation());
	DroneCompanion->SetUseSimplePilotControls(bUseSimpleDronePilotControls);
	DroneCompanion->SetUseFreeFlyPilotControls(IsFreeFlyDronePilotMode());
	ApplyDroneAssistState();

	FVector ThirdPersonTargetLocation = FVector::ZeroVector;
	FRotator ThirdPersonTargetRotation = FRotator::ZeroRotator;
	GetThirdPersonDroneTarget(ThirdPersonTargetLocation, ThirdPersonTargetRotation);
	DroneCompanion->SetThirdPersonCameraTarget(ThirdPersonTargetLocation, ThirdPersonTargetRotation);
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

void AAgentCharacter::UpdateDronePilotInputs(float DeltaSeconds)
{
	if (!DroneCompanion || !IsDroneInputModeActive())
	{
		return;
	}

	const bool bSimplePilotMode = !bMapModeActive && IsSimpleDronePilotMode();
	const bool bFreeFlyPilotMode = !bMapModeActive && IsFreeFlyDronePilotMode();
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
	else if (bFreeFlyPilotMode)
	{
		const float KeyboardUpInput = (bDroneYawRightHeld ? 1.0f : 0.0f) + (bDroneThrottleUpHeld ? 1.0f : 0.0f);
		const float KeyboardDownInput = (bDroneYawLeftHeld ? 1.0f : 0.0f) + (bDroneThrottleDownHeld ? 1.0f : 0.0f);
		KeyboardVerticalInput = FMath::Clamp(KeyboardUpInput - KeyboardDownInput, -1.0f, 1.0f);
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
	else if (bFreeFlyPilotMode)
	{
		GamepadVerticalInput = DroneGamepadRightTriggerInput - DroneGamepadLeftTriggerInput;
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

	if (DroneCompanion)
	{
		DroneCompanion->ResetPilotInputs();
	}
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

void AAgentCharacter::ToggleDronePilotControlMode()
{
	CycleDronePilotControlMode(1);
}

void AAgentCharacter::SetDronePilotControlMode(EAgentDronePilotControlMode NewMode)
{
	DronePilotControlMode = NewMode;

	switch (DronePilotControlMode)
	{
	case EAgentDronePilotControlMode::FreeFly:
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
		DroneCompanion->SetUseSimplePilotControls(bUseSimpleDronePilotControls);
		DroneCompanion->SetUseFreeFlyPilotControls(IsFreeFlyDronePilotMode());
		ApplyDroneAssistState();
	}
}

void AAgentCharacter::CycleDronePilotControlMode(int32 Direction)
{
	constexpr int32 PilotModeCount = 5;
	const int32 CurrentIndex = static_cast<int32>(DronePilotControlMode);
	const int32 WrappedIndex = (CurrentIndex + Direction + PilotModeCount) % PilotModeCount;
	SetDronePilotControlMode(static_cast<EAgentDronePilotControlMode>(WrappedIndex));

	if (DroneCompanion && DroneCompanion->GetCompanionMode() == EDroneCompanionMode::MapMode)
	{
		DroneCompanion->SetUseSimplePilotControls(bUseSimpleDronePilotControls);
		DroneCompanion->SetUseFreeFlyPilotControls(IsFreeFlyDronePilotMode());
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
		DroneCompanion->SetFollowTarget(this);
		DroneCompanion->SetViewReferenceRotation(GetControlRotation());
		DroneCompanion->SetUseSimplePilotControls(bUseSimpleDronePilotControls);
		DroneCompanion->SetUseFreeFlyPilotControls(IsFreeFlyDronePilotMode());
		ApplyDroneAssistState();
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

	ApplyViewMode(EAgentViewMode::FirstPerson, true);

	if (!DroneCompanion)
	{
		return;
	}

	bMiniMapModeActive = true;
	bMiniMapViewingDroneCamera = false;
	SetThirdPersonProxyVisible(false);
	DroneCompanion->SetFollowTarget(this);
	DroneCompanion->SetViewReferenceRotation(GetControlRotation());
	DroneCompanion->SetUseSimplePilotControls(bUseSimpleDronePilotControls);
	DroneCompanion->SetUseFreeFlyPilotControls(IsFreeFlyDronePilotMode());
	ApplyDroneAssistState();
	DroneCompanion->SetCompanionMode(EDroneCompanionMode::MiniMapFollow);
}

void AAgentCharacter::ExitMiniMapMode()
{
	if (!bMiniMapModeActive)
	{
		return;
	}

	ApplyViewMode(EAgentViewMode::FirstPerson, true);
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
		&& !(IsDronePilotMode() && IsFreeFlyDronePilotMode()))
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
	if (IsDroneInputModeActive() && bLockCharacterMovementDuringDronePilot)
	{
		return;
	}

	Jump();
}

void AAgentCharacter::DoJumpEnd()
{
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
	bDroneThrottleUpHeld = true;
}

void AAgentCharacter::OnDroneThrottleUpReleased()
{
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
	DroneGamepadRollInput = Value;
}

void AAgentCharacter::OnDroneGamepadPitchAxis(float Value)
{
	DroneGamepadPitchInput = Value;
}

void AAgentCharacter::OnDroneGamepadLeftTriggerAxis(float Value)
{
	DroneGamepadLeftTriggerInput = FMath::Clamp(Value, 0.0f, 1.0f);
}

void AAgentCharacter::OnDroneGamepadRightTriggerAxis(float Value)
{
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
	ToggleDronePilotControlMode();
}

void AAgentCharacter::OnDroneHoverModePressed()
{
	CycleDronePilotControlMode(-1);
}

void AAgentCharacter::OnDroneStabilizerTogglePressed()
{
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
