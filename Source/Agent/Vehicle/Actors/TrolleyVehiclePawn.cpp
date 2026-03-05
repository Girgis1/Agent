// Copyright Epic Games, Inc. All Rights Reserved.

#include "Vehicle/Actors/TrolleyVehiclePawn.h"
#include "AgentCharacter.h"
#include "Vehicle/Components/TrolleyMovementComponent.h"
#include "Vehicle/Components/VehicleSeatComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputCoreTypes.h"
#include "UObject/ConstructorHelpers.h"

ATrolleyVehiclePawn::ATrolleyVehiclePawn()
{
	PrimaryActorTick.bCanEverTick = true;
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	VehicleBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VehicleBody"));
	SetRootComponent(VehicleBody);
	VehicleBody->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	VehicleBody->SetSimulatePhysics(true);
	VehicleBody->SetEnableGravity(true);
	VehicleBody->SetLinearDamping(0.45f);
	VehicleBody->SetAngularDamping(0.95f);
	VehicleBody->BodyInstance.COMNudge = FVector(-18.0f, 0.0f, -12.0f);
	VehicleBody->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (DefaultMesh.Succeeded())
	{
		VehicleBody->SetStaticMesh(DefaultMesh.Object);
		VehicleBody->SetRelativeScale3D(FVector(1.8f, 1.1f, 0.65f));
	}

	SeatPoint = CreateDefaultSubobject<USceneComponent>(TEXT("SeatPoint"));
	SeatPoint->SetupAttachment(VehicleBody);
	SeatPoint->SetRelativeLocation(FVector(-15.0f, 0.0f, 72.0f));

	ExitPoint = CreateDefaultSubobject<USceneComponent>(TEXT("ExitPoint"));
	ExitPoint->SetupAttachment(VehicleBody);
	ExitPoint->SetRelativeLocation(FVector(0.0f, 115.0f, 35.0f));

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(VehicleBody);
	CameraBoom->TargetArmLength = 340.0f;
	CameraBoom->SetRelativeLocation(FVector(0.0f, 0.0f, 85.0f));
	CameraBoom->bUsePawnControlRotation = true;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 8.0f;

	ThirdPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonCamera"));
	ThirdPersonCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	ThirdPersonCamera->bUsePawnControlRotation = false;
	ThirdPersonCamera->SetActive(true);

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(SeatPoint);
	FirstPersonCamera->SetRelativeLocation(FVector(45.0f, 0.0f, 34.0f));
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetActive(false);

	TrolleyMovementComponent = CreateDefaultSubobject<UTrolleyMovementComponent>(TEXT("TrolleyMovementComponent"));
	TrolleyMovementComponent->SetUpdatedComponent(VehicleBody);

	VehicleSeatComponent = CreateDefaultSubobject<UVehicleSeatComponent>(TEXT("VehicleSeatComponent"));
	VehicleSeatComponent->SeatPoint = SeatPoint;
	VehicleSeatComponent->ExitPoint = ExitPoint;
}

void ATrolleyVehiclePawn::BeginPlay()
{
	Super::BeginPlay();

	SetUseFirstPersonCamera(bStartInFirstPerson);
	TipOverElapsed = 0.0f;
	TipOverRetryCooldownRemaining = 0.0f;
	if (TrolleyMovementComponent)
	{
		TrolleyMovementComponent->SetUpdatedComponent(VehicleBody);
	}
}

void ATrolleyVehiclePawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateTipExit(DeltaSeconds);
}

void ATrolleyVehiclePawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (!PlayerInputComponent)
	{
		return;
	}

	PlayerInputComponent->BindKey(EKeys::W, IE_Pressed, this, &ATrolleyVehiclePawn::OnThrottleForwardPressed);
	PlayerInputComponent->BindKey(EKeys::W, IE_Released, this, &ATrolleyVehiclePawn::OnThrottleForwardReleased);
	PlayerInputComponent->BindKey(EKeys::S, IE_Pressed, this, &ATrolleyVehiclePawn::OnThrottleReversePressed);
	PlayerInputComponent->BindKey(EKeys::S, IE_Released, this, &ATrolleyVehiclePawn::OnThrottleReverseReleased);
	PlayerInputComponent->BindKey(EKeys::A, IE_Pressed, this, &ATrolleyVehiclePawn::OnSteerLeftPressed);
	PlayerInputComponent->BindKey(EKeys::A, IE_Released, this, &ATrolleyVehiclePawn::OnSteerLeftReleased);
	PlayerInputComponent->BindKey(EKeys::D, IE_Pressed, this, &ATrolleyVehiclePawn::OnSteerRightPressed);
	PlayerInputComponent->BindKey(EKeys::D, IE_Released, this, &ATrolleyVehiclePawn::OnSteerRightReleased);
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ATrolleyVehiclePawn::OnHandbrakePressed);
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Released, this, &ATrolleyVehiclePawn::OnHandbrakeReleased);
	PlayerInputComponent->BindKey(EKeys::Gamepad_RightTrigger, IE_Pressed, this, &ATrolleyVehiclePawn::OnHandbrakePressed);
	PlayerInputComponent->BindKey(EKeys::Gamepad_RightTrigger, IE_Released, this, &ATrolleyVehiclePawn::OnHandbrakeReleased);
	PlayerInputComponent->BindKey(EKeys::T, IE_Pressed, this, &ATrolleyVehiclePawn::OnExitVehiclePressed);
	PlayerInputComponent->BindKey(EKeys::V, IE_Pressed, this, &ATrolleyVehiclePawn::OnToggleCameraPressed);

	PlayerInputComponent->BindAxisKey(EKeys::Gamepad_LeftY, this, &ATrolleyVehiclePawn::OnThrottleAxis);
	PlayerInputComponent->BindAxisKey(EKeys::Gamepad_LeftX, this, &ATrolleyVehiclePawn::OnSteeringAxis);
	PlayerInputComponent->BindAxisKey(EKeys::MouseX, this, &ATrolleyVehiclePawn::OnLookYawAxis);
	PlayerInputComponent->BindAxisKey(EKeys::MouseY, this, &ATrolleyVehiclePawn::OnLookPitchAxis);
	PlayerInputComponent->BindAxisKey(EKeys::Gamepad_RightX, this, &ATrolleyVehiclePawn::OnLookYawAxis);
	PlayerInputComponent->BindAxisKey(EKeys::Gamepad_RightY, this, &ATrolleyVehiclePawn::OnLookPitchAxis);
}

UPawnMovementComponent* ATrolleyVehiclePawn::GetMovementComponent() const
{
	return TrolleyMovementComponent;
}

bool ATrolleyVehiclePawn::CanEnterVehicle_Implementation(AActor* Interactor) const
{
	const APawn* InteractorPawn = Cast<APawn>(Interactor);
	if (!InteractorPawn || !VehicleSeatComponent || VehicleSeatComponent->IsOccupied())
	{
		return false;
	}

	const float AllowedDistance = FMath::Max(0.0f, MaxEnterDistance);
	if (AllowedDistance <= 0.0f)
	{
		return true;
	}

	return FVector::DistSquared(InteractorPawn->GetActorLocation(), GetVehicleInteractionLocation_Implementation(Interactor))
		<= FMath::Square(AllowedDistance);
}

bool ATrolleyVehiclePawn::EnterVehicle_Implementation(AActor* Interactor)
{
	if (!CanEnterVehicle_Implementation(Interactor) || !VehicleSeatComponent)
	{
		return false;
	}

	if (const AAgentCharacter* AgentCharacter = Cast<AAgentCharacter>(Interactor))
	{
		if (const UCameraComponent* CharacterFirstPersonCamera = AgentCharacter->GetFirstPersonCamera())
		{
			SetUseFirstPersonCamera(CharacterFirstPersonCamera->IsActive());
		}
	}

	const bool bEnteredVehicle = VehicleSeatComponent->TryEnter(Interactor);
	if (bEnteredVehicle)
	{
		TipOverElapsed = 0.0f;
		TipOverRetryCooldownRemaining = 0.0f;
		if (VehicleBody)
		{
			VehicleBody->WakeAllRigidBodies();
		}
	}

	return bEnteredVehicle;
}

bool ATrolleyVehiclePawn::ExitVehicle_Implementation(AActor* Interactor)
{
	const bool bExitedVehicle = VehicleSeatComponent ? VehicleSeatComponent->TryExit(Interactor) : false;
	if (bExitedVehicle)
	{
		TipOverElapsed = 0.0f;
		TipOverRetryCooldownRemaining = 0.0f;
	}

	return bExitedVehicle;
}

bool ATrolleyVehiclePawn::IsVehicleOccupied_Implementation() const
{
	return VehicleSeatComponent && VehicleSeatComponent->IsOccupied();
}

FVector ATrolleyVehiclePawn::GetVehicleInteractionLocation_Implementation(AActor* Interactor) const
{
	(void)Interactor;
	return VehicleSeatComponent ? VehicleSeatComponent->GetInteractionLocation() : GetActorLocation();
}

FText ATrolleyVehiclePawn::GetVehicleInteractionPrompt_Implementation() const
{
	return IsVehicleOccupied_Implementation() ? ExitPrompt : EnterPrompt;
}

void ATrolleyVehiclePawn::SetUseFirstPersonCamera(bool bUseFirstPerson)
{
	bUseFirstPersonCamera = bUseFirstPerson;
	ApplyCameraMode();
}

void ATrolleyVehiclePawn::ToggleCameraMode()
{
	SetUseFirstPersonCamera(!bUseFirstPersonCamera);
}

void ATrolleyVehiclePawn::ApplyCameraMode()
{
	if (FirstPersonCamera)
	{
		FirstPersonCamera->SetActive(bUseFirstPersonCamera);
	}

	if (ThirdPersonCamera)
	{
		ThirdPersonCamera->SetActive(!bUseFirstPersonCamera);
	}
}

void ATrolleyVehiclePawn::ApplyDriveInput()
{
	if (!TrolleyMovementComponent)
	{
		return;
	}

	const float DigitalThrottle = (bThrottleForwardHeld ? 1.0f : 0.0f) + (bThrottleReverseHeld ? -1.0f : 0.0f);
	const float DigitalSteer = (bSteerRightHeld ? 1.0f : 0.0f) + (bSteerLeftHeld ? -1.0f : 0.0f);

	const float TargetThrottle = !FMath::IsNearlyZero(ThrottleAxisValue, 0.01f)
		? ThrottleAxisValue
		: DigitalThrottle;
	const float TargetSteer = !FMath::IsNearlyZero(SteeringAxisValue, 0.01f)
		? SteeringAxisValue
		: DigitalSteer;

	TrolleyMovementComponent->SetThrottleInput(TargetThrottle);
	TrolleyMovementComponent->SetSteeringInput(TargetSteer);
	TrolleyMovementComponent->SetHandbrakeInput(bHandbrakeHeld);
}

void ATrolleyVehiclePawn::UpdateTipExit(float DeltaSeconds)
{
	if (!bExitOnTipOver || !VehicleSeatComponent || !VehicleSeatComponent->IsOccupied())
	{
		TipOverElapsed = 0.0f;
		TipOverRetryCooldownRemaining = 0.0f;
		return;
	}

	if (TipOverRetryCooldownRemaining > 0.0f)
	{
		TipOverRetryCooldownRemaining = FMath::Max(0.0f, TipOverRetryCooldownRemaining - DeltaSeconds);
		return;
	}

	const float UpDot = FVector::DotProduct(GetActorUpVector(), FVector::UpVector);
	if (UpDot >= TipOverUpDotThreshold)
	{
		TipOverElapsed = 0.0f;
		return;
	}

	TipOverElapsed += FMath::Max(0.0f, DeltaSeconds);
	if (TipOverElapsed < FMath::Max(0.0f, TipOverExitDelay))
	{
		return;
	}

	TipOverElapsed = 0.0f;
	const bool bExitedVehicle = ExitVehicle_Implementation(this)
		|| (VehicleSeatComponent && VehicleSeatComponent->ForceExit(this));
	if (!bExitedVehicle)
	{
		TipOverRetryCooldownRemaining = FMath::Max(0.0f, TipOverExitRetryCooldown);
	}
}

void ATrolleyVehiclePawn::OnThrottleForwardPressed()
{
	bThrottleForwardHeld = true;
	ApplyDriveInput();
}

void ATrolleyVehiclePawn::OnThrottleForwardReleased()
{
	bThrottleForwardHeld = false;
	ApplyDriveInput();
}

void ATrolleyVehiclePawn::OnThrottleReversePressed()
{
	bThrottleReverseHeld = true;
	ApplyDriveInput();
}

void ATrolleyVehiclePawn::OnThrottleReverseReleased()
{
	bThrottleReverseHeld = false;
	ApplyDriveInput();
}

void ATrolleyVehiclePawn::OnSteerLeftPressed()
{
	bSteerLeftHeld = true;
	ApplyDriveInput();
}

void ATrolleyVehiclePawn::OnSteerLeftReleased()
{
	bSteerLeftHeld = false;
	ApplyDriveInput();
}

void ATrolleyVehiclePawn::OnSteerRightPressed()
{
	bSteerRightHeld = true;
	ApplyDriveInput();
}

void ATrolleyVehiclePawn::OnSteerRightReleased()
{
	bSteerRightHeld = false;
	ApplyDriveInput();
}

void ATrolleyVehiclePawn::OnHandbrakePressed()
{
	bHandbrakeHeld = true;
	ApplyDriveInput();
}

void ATrolleyVehiclePawn::OnHandbrakeReleased()
{
	bHandbrakeHeld = false;
	ApplyDriveInput();
}

void ATrolleyVehiclePawn::OnExitVehiclePressed()
{
	ExitVehicle_Implementation(this);
}

void ATrolleyVehiclePawn::OnToggleCameraPressed()
{
	if (!bAllowCameraToggle)
	{
		return;
	}

	ToggleCameraMode();
}

void ATrolleyVehiclePawn::OnThrottleAxis(float Value)
{
	ThrottleAxisValue = FMath::Clamp(Value, -1.0f, 1.0f);
	ApplyDriveInput();
}

void ATrolleyVehiclePawn::OnSteeringAxis(float Value)
{
	SteeringAxisValue = FMath::Clamp(Value, -1.0f, 1.0f);
	ApplyDriveInput();
}

void ATrolleyVehiclePawn::OnLookYawAxis(float Value)
{
	AddControllerYawInput(Value);
}

void ATrolleyVehiclePawn::OnLookPitchAxis(float Value)
{
	AddControllerPitchInput(Value * -1.0f);
}
