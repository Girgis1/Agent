// Copyright Epic Games, Inc. All Rights Reserved.

#include "AgentCharacter.h"
#include "DroneCompanion.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
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

	if (FirstPersonCamera)
	{
		FirstPersonCamera->SetRelativeLocation(FirstPersonCameraOffset);
	}

	SpawnDroneCompanion();

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

	if (DroneCompanion)
	{
		DroneCompanion->SetFollowTarget(this);
		DroneCompanion->SetViewReferenceRotation(GetControlRotation());
		DroneCompanion->SetUseSimplePilotControls(bUseSimpleDronePilotControls);
		ApplyDroneAssistState();
		FVector ThirdPersonTargetLocation = FVector::ZeroVector;
		FRotator ThirdPersonTargetRotation = FRotator::ZeroRotator;
		if (GetThirdPersonDroneTarget(ThirdPersonTargetLocation, ThirdPersonTargetRotation))
		{
			DroneCompanion->SetThirdPersonCameraTarget(ThirdPersonTargetLocation, ThirdPersonTargetRotation);
		}

		if (IsDroneInputModeActive())
		{
			UpdateDronePilotInputs();
		}
		else
		{
			DroneCompanion->ResetPilotInputs();
		}
	}

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
	PlayerInputComponent->BindKey(EKeys::B, IE_Pressed, this, &AAgentCharacter::OnDroneControlModeTogglePressed);
	PlayerInputComponent->BindKey(EKeys::M, IE_Pressed, this, &AAgentCharacter::OnMapModePressed);
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
	PlayerInputComponent->BindKey(EKeys::Gamepad_Special_Left, IE_Pressed, this, &AAgentCharacter::OnMapModePressed);
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
	const bool bWasDronePilot = CurrentViewMode == EAgentViewMode::DronePilot;
	CurrentViewMode = NewMode;
	bDroneEntryAssistActive = NewMode == EAgentViewMode::DronePilot && !bWasDronePilot;

	const float BlendTime = bBlend ? ViewBlendTime : 0.0f;
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	UCharacterMovementComponent* CharacterMovementComponent = GetCharacterMovement();

	if (FirstPersonCamera)
	{
		FirstPersonCamera->SetActive(NewMode == EAgentViewMode::FirstPerson);
	}

	if (FollowCamera)
	{
		FollowCamera->SetActive(
			NewMode == EAgentViewMode::ThirdPerson
			|| (!DroneCompanion && NewMode != EAgentViewMode::FirstPerson));
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

	if (!DroneCompanion)
	{
		if (PlayerController)
		{
			PlayerController->SetViewTargetWithBlend(this, BlendTime);
		}

		ResetDroneInputState();
		return;
	}

	DroneCompanion->SetFollowTarget(this);
	DroneCompanion->SetViewReferenceRotation(GetControlRotation());
	DroneCompanion->SetUseSimplePilotControls(bUseSimpleDronePilotControls);
	ApplyDroneAssistState();

	switch (NewMode)
	{
	case EAgentViewMode::ThirdPerson:
	{
		FVector ThirdPersonTargetLocation = FVector::ZeroVector;
		FRotator ThirdPersonTargetRotation = FRotator::ZeroRotator;
		GetThirdPersonDroneTarget(ThirdPersonTargetLocation, ThirdPersonTargetRotation);
		DroneCompanion->SetThirdPersonCameraTarget(ThirdPersonTargetLocation, ThirdPersonTargetRotation);
		if (!bBlend)
		{
			DroneCompanion->TeleportDroneToTransform(ThirdPersonTargetLocation, ThirdPersonTargetRotation);
		}
		DroneCompanion->SetCompanionMode(EDroneCompanionMode::ThirdPersonFollow);
		if (PlayerController)
		{
			PlayerController->SetViewTargetWithBlend(this, BlendTime);
		}
		ResetDroneInputState();
		break;
	}
	case EAgentViewMode::DronePilot:
		DroneCompanion->SetCompanionMode(EDroneCompanionMode::PilotControlled);
		if (PlayerController)
		{
			PlayerController->SetViewTargetWithBlend(DroneCompanion.Get(), BlendTime);
		}
		break;
	case EAgentViewMode::FirstPerson:
	default:
		if (PlayerController)
		{
			PlayerController->SetViewTargetWithBlend(this, BlendTime);
		}

		DroneCompanion->SetCompanionMode(EDroneCompanionMode::BuddyFollow);
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

	const FRotator SpawnYawRotation(0.0f, GetControlRotation().Yaw, 0.0f);
	const FVector SpawnLocation = GetActorLocation() + FRotationMatrix(SpawnYawRotation).TransformVector(DroneSpawnOffset);

	FActorSpawnParameters SpawnParameters{};
	SpawnParameters.Owner = this;
	SpawnParameters.Instigator = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	DroneCompanion = GetWorld()->SpawnActor<ADroneCompanion>(SpawnClass, SpawnLocation, SpawnYawRotation, SpawnParameters);

	if (DroneCompanion)
	{
		DroneCompanion->SetFollowTarget(this);
		DroneCompanion->SetViewReferenceRotation(GetControlRotation());
		DroneCompanion->SetUseSimplePilotControls(bUseSimpleDronePilotControls);
		ApplyDroneAssistState();
		FVector ThirdPersonTargetLocation = FVector::ZeroVector;
		FRotator ThirdPersonTargetRotation = FRotator::ZeroRotator;
		GetThirdPersonDroneTarget(ThirdPersonTargetLocation, ThirdPersonTargetRotation);
		DroneCompanion->SetThirdPersonCameraTarget(ThirdPersonTargetLocation, ThirdPersonTargetRotation);
		DroneCompanion->SetCompanionMode(
			CurrentViewMode == EAgentViewMode::DronePilot
				? EDroneCompanionMode::PilotControlled
				: (CurrentViewMode == EAgentViewMode::ThirdPerson
					? EDroneCompanionMode::ThirdPersonFollow
					: EDroneCompanionMode::BuddyFollow));
	}
}

void AAgentCharacter::UpdateDronePilotInputs()
{
	if (!DroneCompanion || !IsDroneInputModeActive())
	{
		return;
	}

	const bool bUseSimpleStyleInputs = bMapModeActive || bUseSimpleDronePilotControls;
	const float KeyboardForwardInput = (bDronePitchForwardHeld ? 1.0f : 0.0f) - (bDronePitchBackwardHeld ? 1.0f : 0.0f);
	const float KeyboardRightInput = (bDroneRollRightHeld ? 1.0f : 0.0f) - (bDroneRollLeftHeld ? 1.0f : 0.0f);
	float KeyboardVerticalInput = 0.0f;
	float KeyboardYawInput = 0.0f;

	if (bUseSimpleStyleInputs)
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
	if (bUseSimpleStyleInputs)
	{
		GamepadVerticalInput = bMapModeActive
			? (DroneGamepadLeftTriggerInput - DroneGamepadRightTriggerInput)
			: (DroneGamepadRightTriggerInput - DroneGamepadLeftTriggerInput);
	}
	const float GamepadYawInput = bUseSimpleStyleInputs ? 0.0f : DroneGamepadYawInput;
	const float GamepadRightInput = bUseSimpleStyleInputs ? DroneGamepadYawInput : DroneGamepadRollInput;
	const float GamepadForwardInput = bUseSimpleStyleInputs ? DroneGamepadThrottleInput : DroneGamepadPitchInput;

	const float VerticalInput = FMath::Clamp(KeyboardVerticalInput + GamepadVerticalInput, -1.0f, 1.0f);
	const float YawInput = FMath::Clamp(KeyboardYawInput + GamepadYawInput, -1.0f, 1.0f);
	const float RightInput = FMath::Clamp(KeyboardRightInput + GamepadRightInput, -1.0f, 1.0f);
	const float ForwardInput = FMath::Clamp(KeyboardForwardInput + GamepadForwardInput, -1.0f, 1.0f);

	if (bDroneEntryAssistActive && IsDronePilotMode() && FMath::Abs(VerticalInput) > FMath::Max(0.0f, DroneEntryAssistReleaseThreshold))
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

void AAgentCharacter::ToggleDronePilotControlMode()
{
	CycleDronePilotControlMode(1);
}

void AAgentCharacter::SetDronePilotControlMode(EAgentDronePilotControlMode NewMode)
{
	DronePilotControlMode = NewMode;

	switch (DronePilotControlMode)
	{
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
		ApplyDroneAssistState();
	}
}

void AAgentCharacter::CycleDronePilotControlMode(int32 Direction)
{
	constexpr int32 PilotModeCount = 4;
	const int32 CurrentIndex = static_cast<int32>(DronePilotControlMode);
	const int32 WrappedIndex = (CurrentIndex + Direction + PilotModeCount) % PilotModeCount;
	SetDronePilotControlMode(static_cast<EAgentDronePilotControlMode>(WrappedIndex));

	if (DroneCompanion && DroneCompanion->GetCompanionMode() == EDroneCompanionMode::MapMode)
	{
		DroneCompanion->SetUseSimplePilotControls(bUseSimpleDronePilotControls);
	}
}

void AAgentCharacter::ToggleMapMode()
{
	if (!DroneCompanion)
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());

	if (!bMapModeActive)
	{
		bMapModeActive = true;
		DroneCompanion->SetFollowTarget(this);
		DroneCompanion->SetViewReferenceRotation(GetControlRotation());
		DroneCompanion->SetUseSimplePilotControls(bUseSimpleDronePilotControls);
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
	if (IsDroneInputModeActive() && bLockCharacterMovementDuringDronePilot)
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
	ToggleMapMode();
}
