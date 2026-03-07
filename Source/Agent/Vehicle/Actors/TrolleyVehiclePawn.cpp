// Copyright Epic Games, Inc. All Rights Reserved.

#include "Vehicle/Actors/TrolleyVehiclePawn.h"
#include "AgentCharacter.h"
#include "Factory/ConveyorSurfaceVelocityComponent.h"
#include "Vehicle/Components/TrolleyMovementComponent.h"
#include "Vehicle/Components/VehicleSeatComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "InputCoreTypes.h"
#include "UObject/ConstructorHelpers.h"

ATrolleyVehiclePawn::ATrolleyVehiclePawn()
{
	PrimaryActorTick.bCanEverTick = true;
	bUseControllerRotationYaw = false;
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;

	PhysicsBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PhysicsBody"));
	SetRootComponent(PhysicsBody);
	PhysicsBody->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	PhysicsBody->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PhysicsBody->SetSimulatePhysics(true);
	PhysicsBody->SetEnableGravity(true);
	PhysicsBody->SetLinearDamping(0.25f);
	PhysicsBody->SetAngularDamping(0.45f);
	PhysicsBody->BodyInstance.COMNudge = FVector(-12.0f, 0.0f, -8.0f);
	PhysicsBody->SetCanEverAffectNavigation(false);
	PhysicsBody->SetGenerateOverlapEvents(true);
	PhysicsBody->SetNotifyRigidBodyCollision(true);
	PhysicsBody->SetRelativeScale3D(FVector::OneVector);

	// Keep both pointers for BP compatibility, but they intentionally reference the same component.
	VehicleBody = PhysicsBody;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (DefaultMesh.Succeeded())
	{
		if (!PhysicsBody->GetStaticMesh())
		{
			PhysicsBody->SetStaticMesh(DefaultMesh.Object);
		}
	}

	SeatPoint = CreateDefaultSubobject<USceneComponent>(TEXT("SeatPoint"));
	SeatPoint->SetupAttachment(PhysicsBody);
	SeatPoint->SetRelativeLocation(FVector(-15.0f, 0.0f, 72.0f));

	// Use the same point for enter/exit so the transition feels consistent.
	ExitPoint = SeatPoint;

	DriverAttachPoint = CreateDefaultSubobject<USceneComponent>(TEXT("DriverAttachPoint"));
	DriverAttachPoint->SetupAttachment(PhysicsBody);
	DriverAttachPoint->SetRelativeLocation(FVector(-105.0f, 0.0f, 64.0f));

	HandleLeftPoint = CreateDefaultSubobject<USceneComponent>(TEXT("HandleLeftPoint"));
	HandleLeftPoint->SetupAttachment(PhysicsBody);
	HandleLeftPoint->SetRelativeLocation(FVector(-52.0f, -24.0f, 96.0f));

	HandleRightPoint = CreateDefaultSubobject<USceneComponent>(TEXT("HandleRightPoint"));
	HandleRightPoint->SetupAttachment(PhysicsBody);
	HandleRightPoint->SetRelativeLocation(FVector(-52.0f, 24.0f, 96.0f));

	DriverChestTargetPoint = CreateDefaultSubobject<USceneComponent>(TEXT("DriverChestTargetPoint"));
	DriverChestTargetPoint->SetupAttachment(PhysicsBody);
	DriverChestTargetPoint->SetRelativeLocation(FVector(-72.0f, 0.0f, 96.0f));

	CargoMassVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("CargoMassVolume"));
	CargoMassVolume->SetupAttachment(PhysicsBody);
	CargoMassVolume->SetBoxExtent(CargoVolumeExtent);
	CargoMassVolume->SetRelativeLocation(CargoVolumeOffset);
	CargoMassVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CargoMassVolume->SetCollisionObjectType(ECC_WorldDynamic);
	CargoMassVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	CargoMassVolume->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	CargoMassVolume->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	CargoMassVolume->SetGenerateOverlapEvents(true);
	CargoMassVolume->SetCanEverAffectNavigation(false);

	TrolleyMovementComponent = CreateDefaultSubobject<UTrolleyMovementComponent>(TEXT("TrolleyMovementComponent"));
	TrolleyMovementComponent->SetUpdatedComponent(PhysicsBody);

	ConveyorSurfaceVelocityComponent = CreateDefaultSubobject<UConveyorSurfaceVelocityComponent>(TEXT("ConveyorSurfaceVelocity"));

	VehicleSeatComponent = CreateDefaultSubobject<UVehicleSeatComponent>(TEXT("VehicleSeatComponent"));
	VehicleSeatComponent->SeatPoint = SeatPoint;
	VehicleSeatComponent->ExitPoint = ExitPoint;
	VehicleSeatComponent->DriverAttachPoint = DriverAttachPoint;
	VehicleSeatComponent->bUsePossessionFlow = false;
	VehicleSeatComponent->bAttachDriverToVehicle = true;
	VehicleSeatComponent->bDisableDriverMovementWhileSeated = false;
	VehicleSeatComponent->bHideDriverWhileSeated = false;
	VehicleSeatComponent->bDisableDriverCollisionWhileSeated = false;
	VehicleSeatComponent->bUseCollisionAwareDriverSync = true;
	VehicleSeatComponent->bFollowSeatVerticalAxis = false;
	VehicleSeatComponent->bForceExitWhenDriverBlocked = true;
	VehicleSeatComponent->DriverBlockedExitDistance = 45.0f;

	CargoMassVolume->OnComponentBeginOverlap.AddDynamic(this, &ATrolleyVehiclePawn::OnCargoBeginOverlap);
	CargoMassVolume->OnComponentEndOverlap.AddDynamic(this, &ATrolleyVehiclePawn::OnCargoEndOverlap);
}

void ATrolleyVehiclePawn::BeginPlay()
{
	Super::BeginPlay();

	if (VehicleSeatComponent)
	{
		// Re-apply seat settings at runtime so placed BP instances cannot drift onto stale defaults.
		VehicleSeatComponent->SeatPoint = SeatPoint;
		VehicleSeatComponent->ExitPoint = ExitPoint;
		VehicleSeatComponent->DriverAttachPoint = DriverAttachPoint;
		VehicleSeatComponent->bUsePossessionFlow = false;
		VehicleSeatComponent->bAttachDriverToVehicle = true;
		VehicleSeatComponent->bDisableDriverMovementWhileSeated = false;
		VehicleSeatComponent->bHideDriverWhileSeated = false;
		VehicleSeatComponent->bDisableDriverCollisionWhileSeated = false;
		VehicleSeatComponent->bUseCollisionAwareDriverSync = true;
		VehicleSeatComponent->bFollowSeatVerticalAxis = false;
		VehicleSeatComponent->bForceExitWhenDriverBlocked = true;
		VehicleSeatComponent->DriverBlockedExitDistance = 45.0f;
	}

	TipOverElapsed = 0.0f;
	TipOverRetryCooldownRemaining = 0.0f;
	SetActorScale3D(FVector::OneVector);
	if (PhysicsBody)
	{
		PhysicsBody->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
		PhysicsBody->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		PhysicsBody->SetSimulatePhysics(true);
		PhysicsBody->SetEnableGravity(true);
		PhysicsBody->SetLinearDamping(0.25f);
		PhysicsBody->SetAngularDamping(0.45f);
		// Keep the trolley body visible without forcing child component visibility.
		PhysicsBody->SetHiddenInGame(false, false);
		PhysicsBody->SetVisibility(true, false);
		PhysicsBody->SetRelativeScale3D(FVector::OneVector);
	}

	if (CargoMassVolume)
	{
		CargoMassVolume->SetBoxExtent(CargoVolumeExtent);
		CargoMassVolume->SetRelativeLocation(CargoVolumeOffset);
	}

	if (TrolleyMovementComponent)
	{
		TrolleyMovementComponent->SetUpdatedComponent(PhysicsBody);
		TrolleyMovementComponent->SetDriverStrengthSpeedMultiplier(ResolveDriverStrengthSpeedMultiplier(nullptr));
	}

	UpdateCargoMassTracking();
	UpdatePhysicsMaterial();
}

void ATrolleyVehiclePawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateCargoMassTracking();
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

	PlayerInputComponent->BindAxisKey(EKeys::Gamepad_LeftY, this, &ATrolleyVehiclePawn::OnThrottleAxis);
	PlayerInputComponent->BindAxisKey(EKeys::Gamepad_LeftX, this, &ATrolleyVehiclePawn::OnSteeringAxis);
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

	const bool bEnteredVehicle = VehicleSeatComponent->TryEnter(Interactor);
	if (bEnteredVehicle)
	{
		ResetDriveInput();
		bUsingExternalInput = true;
		if (TrolleyMovementComponent)
		{
			TrolleyMovementComponent->SetDriverStrengthSpeedMultiplier(ResolveDriverStrengthSpeedMultiplier(Interactor));
		}
		TipOverElapsed = 0.0f;
		TipOverRetryCooldownRemaining = 0.0f;
		if (PhysicsBody)
		{
			PhysicsBody->WakeAllRigidBodies();
		}
	}

	return bEnteredVehicle;
}

bool ATrolleyVehiclePawn::ExitVehicle_Implementation(AActor* Interactor)
{
	bool bExitedVehicle = false;
	if (VehicleSeatComponent)
	{
		bExitedVehicle = VehicleSeatComponent->TryExit(Interactor);
		if (!bExitedVehicle)
		{
			bExitedVehicle = VehicleSeatComponent->ForceExit(Interactor);
		}
	}

	if (bExitedVehicle)
	{
		ResetDriveInput();
		if (TrolleyMovementComponent)
		{
			TrolleyMovementComponent->SetDriverStrengthSpeedMultiplier(ResolveDriverStrengthSpeedMultiplier(nullptr));
		}
		TipOverElapsed = 0.0f;
		TipOverRetryCooldownRemaining = 0.0f;
	}

	return bExitedVehicle;
}

bool ATrolleyVehiclePawn::IsVehicleOccupied_Implementation() const
{
	return VehicleSeatComponent && VehicleSeatComponent->IsOccupied();
}

bool ATrolleyVehiclePawn::IsVehicleControlledBy_Implementation(AActor* Interactor) const
{
	const APawn* DriverPawn = VehicleSeatComponent ? VehicleSeatComponent->GetDriverPawn() : nullptr;
	return DriverPawn && DriverPawn == Interactor;
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

void ATrolleyVehiclePawn::SetVehicleMoveInput_Implementation(AActor* Interactor, float ForwardInput, float RightInput)
{
	if (!IsVehicleControlledBy_Implementation(Interactor))
	{
		return;
	}

	bUsingExternalInput = true;
	ExternalThrottleInput = FMath::Clamp(ForwardInput, -1.0f, 1.0f);
	ExternalSteeringInput = FMath::Clamp(RightInput, -1.0f, 1.0f);
	if (TrolleyMovementComponent)
	{
		TrolleyMovementComponent->SetDriverStrengthSpeedMultiplier(ResolveDriverStrengthSpeedMultiplier(Interactor));
	}
	ApplyDriveInput();
}

void ATrolleyVehiclePawn::SetVehicleHandbrakeInput_Implementation(AActor* Interactor, bool bHandbrakeActive)
{
	if (!IsVehicleControlledBy_Implementation(Interactor))
	{
		return;
	}

	bUsingExternalInput = true;
	bExternalHandbrakeHeld = bHandbrakeActive;
	if (TrolleyMovementComponent)
	{
		TrolleyMovementComponent->SetDriverStrengthSpeedMultiplier(ResolveDriverStrengthSpeedMultiplier(Interactor));
	}
	ApplyDriveInput();
}

void ATrolleyVehiclePawn::ApplyDriveInput()
{
	if (!TrolleyMovementComponent)
	{
		return;
	}

	if (bUsingExternalInput)
	{
		TrolleyMovementComponent->SetThrottleInput(ExternalThrottleInput);
		TrolleyMovementComponent->SetSteeringInput(ExternalSteeringInput);
		TrolleyMovementComponent->SetHandbrakeInput(bExternalHandbrakeHeld);
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

void ATrolleyVehiclePawn::ResetDriveInput()
{
	bUsingExternalInput = false;
	ExternalThrottleInput = 0.0f;
	ExternalSteeringInput = 0.0f;
	bExternalHandbrakeHeld = false;
	bThrottleForwardHeld = false;
	bThrottleReverseHeld = false;
	bSteerLeftHeld = false;
	bSteerRightHeld = false;
	bHandbrakeHeld = false;
	ThrottleAxisValue = 0.0f;
	SteeringAxisValue = 0.0f;
	if (TrolleyMovementComponent)
	{
		TrolleyMovementComponent->SetThrottleInput(0.0f);
		TrolleyMovementComponent->SetSteeringInput(0.0f);
		TrolleyMovementComponent->SetHandbrakeInput(false);
	}
}

void ATrolleyVehiclePawn::UpdatePhysicsMaterial()
{
	if (!PhysicsBody)
	{
		return;
	}

	if (!RuntimePhysicsMaterial)
	{
		RuntimePhysicsMaterial = NewObject<UPhysicalMaterial>(this, TEXT("RuntimeTrolleyPhysicsMaterial"));
	}

	if (!RuntimePhysicsMaterial)
	{
		return;
	}

	RuntimePhysicsMaterial->Friction = FMath::Max(0.0f, PhysicsSurfaceFriction);
	RuntimePhysicsMaterial->StaticFriction = FMath::Max(0.0f, PhysicsSurfaceFriction);
	RuntimePhysicsMaterial->Restitution = FMath::Clamp(PhysicsSurfaceRestitution, 0.0f, 1.0f);
	RuntimePhysicsMaterial->bOverrideFrictionCombineMode = true;
	RuntimePhysicsMaterial->FrictionCombineMode = EFrictionCombineMode::Min;
	RuntimePhysicsMaterial->bOverrideRestitutionCombineMode = true;
	RuntimePhysicsMaterial->RestitutionCombineMode = EFrictionCombineMode::Min;
	PhysicsBody->SetPhysMaterialOverride(RuntimePhysicsMaterial);
}

void ATrolleyVehiclePawn::UpdateCargoMassTracking()
{
	float TotalCargoMassKg = 0.0f;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> ComponentsToRemove;

	for (const TWeakObjectPtr<UPrimitiveComponent>& CargoEntry : TrackedCargoComponents)
	{
		UPrimitiveComponent* CargoComponent = CargoEntry.Get();
		if (!IsTrackedCargoComponent(CargoComponent))
		{
			ComponentsToRemove.Add(CargoEntry);
			continue;
		}

		if (CargoMassVolume && !CargoMassVolume->IsOverlappingComponent(CargoComponent))
		{
			ComponentsToRemove.Add(CargoEntry);
			continue;
		}

		TotalCargoMassKg += FMath::Max(0.0f, CargoComponent->GetMass());
	}

	for (const TWeakObjectPtr<UPrimitiveComponent>& CargoToRemove : ComponentsToRemove)
	{
		TrackedCargoComponents.Remove(CargoToRemove);
	}

	CurrentCargoMassKg = FMath::Max(0.0f, TotalCargoMassKg);
	if (TrolleyMovementComponent)
	{
		TrolleyMovementComponent->SetPayloadMassKg(CurrentCargoMassKg);
	}
}

float ATrolleyVehiclePawn::ResolveDriverStrengthSpeedMultiplier(AActor* Interactor) const
{
	float DefaultMultiplier = 1.0f;
	float InputMin = 0.1f;
	float InputMax = 10.0f;
	float CurveExponent = 2.0f;
	float MaxMultiplier = 5.0f;
	if (TrolleyMovementComponent)
	{
		DefaultMultiplier = FMath::Max(0.0f, TrolleyMovementComponent->DefaultStrengthSpeedMultiplier);
		InputMin = FMath::Max(0.0f, TrolleyMovementComponent->StrengthInputMin);
		InputMax = FMath::Max(0.0f, TrolleyMovementComponent->StrengthInputMax);
		CurveExponent = FMath::Max(0.01f, TrolleyMovementComponent->StrengthCurveExponent);
		MaxMultiplier = FMath::Max(0.0f, TrolleyMovementComponent->MaxStrengthSpeedMultiplier);
	}

	float StrengthSpeedMultiplier = DefaultMultiplier;
	if (const AAgentCharacter* AgentCharacter = Cast<AAgentCharacter>(Interactor))
	{
		const float StrengthValue = FMath::Max(0.0f, AgentCharacter->CharacterPickupStrengthMultiplier);
		const float SafeInputMax = FMath::Max(InputMin + KINDA_SMALL_NUMBER, InputMax);
		const float Alpha = FMath::Clamp((StrengthValue - InputMin) / (SafeInputMax - InputMin), 0.0f, 1.0f);
		const float EasedAlpha = 1.0f - FMath::Pow(1.0f - Alpha, CurveExponent);
		const float SafeMaxMultiplier = FMath::Max(DefaultMultiplier, MaxMultiplier);
		StrengthSpeedMultiplier = FMath::Lerp(DefaultMultiplier, SafeMaxMultiplier, EasedAlpha);
	}

	return FMath::Clamp(
		StrengthSpeedMultiplier,
		FMath::Max(0.0f, DefaultMultiplier),
		FMath::Max(DefaultMultiplier, MaxMultiplier));
}

bool ATrolleyVehiclePawn::IsTrackedCargoComponent(const UPrimitiveComponent* PrimitiveComponent) const
{
	return IsValid(PrimitiveComponent)
		&& PrimitiveComponent != PhysicsBody
		&& PrimitiveComponent->GetOwner() != this
		&& PrimitiveComponent->IsRegistered()
		&& PrimitiveComponent->IsSimulatingPhysics();
}

void ATrolleyVehiclePawn::OnCargoBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	(void)OverlappedComponent;
	(void)OtherActor;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;

	if (IsTrackedCargoComponent(OtherComp))
	{
		TrackedCargoComponents.Add(OtherComp);
		UpdateCargoMassTracking();
	}
}

void ATrolleyVehiclePawn::OnCargoEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	(void)OverlappedComponent;
	(void)OtherActor;
	(void)OtherBodyIndex;

	if (OtherComp)
	{
		TrackedCargoComponents.Remove(OtherComp);
		UpdateCargoMassTracking();
	}
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

FVector ATrolleyVehiclePawn::GetDriverAttachPointLocation() const
{
	return DriverAttachPoint ? DriverAttachPoint->GetComponentLocation() : GetActorLocation();
}

FTransform ATrolleyVehiclePawn::GetDriverAttachPointTransform() const
{
	return DriverAttachPoint ? DriverAttachPoint->GetComponentTransform() : GetActorTransform();
}

FVector ATrolleyVehiclePawn::GetHandleLeftPointLocation() const
{
	return HandleLeftPoint
		? HandleLeftPoint->GetComponentLocation()
		: (DriverAttachPoint ? DriverAttachPoint->GetComponentLocation() : GetActorLocation());
}

FTransform ATrolleyVehiclePawn::GetHandleLeftPointTransform() const
{
	return HandleLeftPoint
		? HandleLeftPoint->GetComponentTransform()
		: GetDriverAttachPointTransform();
}

FVector ATrolleyVehiclePawn::GetHandleRightPointLocation() const
{
	return HandleRightPoint
		? HandleRightPoint->GetComponentLocation()
		: (DriverAttachPoint ? DriverAttachPoint->GetComponentLocation() : GetActorLocation());
}

FTransform ATrolleyVehiclePawn::GetHandleRightPointTransform() const
{
	return HandleRightPoint
		? HandleRightPoint->GetComponentTransform()
		: GetDriverAttachPointTransform();
}

FVector ATrolleyVehiclePawn::GetDriverChestTargetLocation() const
{
	return DriverChestTargetPoint
		? DriverChestTargetPoint->GetComponentLocation()
		: (DriverAttachPoint ? DriverAttachPoint->GetComponentLocation() : GetActorLocation());
}

FTransform ATrolleyVehiclePawn::GetDriverChestTargetTransform() const
{
	return DriverChestTargetPoint
		? DriverChestTargetPoint->GetComponentTransform()
		: GetDriverAttachPointTransform();
}

FVector ATrolleyVehiclePawn::GetPhysicsBodyLinearVelocity() const
{
	if (PhysicsBody && PhysicsBody->IsSimulatingPhysics())
	{
		return PhysicsBody->GetPhysicsLinearVelocity();
	}

	return GetVelocity();
}

FVector ATrolleyVehiclePawn::GetPhysicsBodyAngularVelocityDeg() const
{
	if (PhysicsBody && PhysicsBody->IsSimulatingPhysics())
	{
		return PhysicsBody->GetPhysicsAngularVelocityInDegrees();
	}

	return FVector::ZeroVector;
}
