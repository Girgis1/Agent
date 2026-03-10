// Copyright Epic Games, Inc. All Rights Reserved.

#include "Vehicle/Components/VehicleSeatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

UVehicleSeatComponent::UVehicleSeatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	bUsePossessionFlow = false;
	bAttachDriverToVehicle = true;
	bKeepDriverUprightWhileSeated = true;
	bDisableDriverMovementWhileSeated = true;
	bHideDriverWhileSeated = false;
	bDisableDriverCollisionWhileSeated = true;
	bExitAtCurrentLocation = true;
	bUseSafeFallbackIfCurrentExitBlocked = false;
}

void UVehicleSeatComponent::BeginPlay()
{
	Super::BeginPlay();

	CachedVehiclePawn = Cast<APawn>(GetOwner());
}

bool UVehicleSeatComponent::TryEnter(AActor* Interactor)
{
	if (IsOccupied())
	{
		return false;
	}

	APawn* VehiclePawn = CachedVehiclePawn.Get();
	APawn* DriverPawn = Cast<APawn>(Interactor);
	if (!VehiclePawn || !DriverPawn)
	{
		return false;
	}

	APlayerController* DriverController = Cast<APlayerController>(DriverPawn->GetController());
	if (!DriverController || DriverController->GetPawn() != DriverPawn)
	{
		return false;
	}

	ACharacter* DriverCharacter = Cast<ACharacter>(DriverPawn);
	UCharacterMovementComponent* DriverMovement = DriverCharacter
		? DriverCharacter->GetCharacterMovement()
		: nullptr;

	bDriverWasHiddenInGame = DriverPawn->IsHidden();
	bDriverCollisionWasEnabled = DriverPawn->GetActorEnableCollision();
	bDriverAttachedToVehicle = false;
	bDriverRootUsedAbsoluteRotation = false;
	if (DriverMovement)
	{
		bDriverMovementWasEnabled = DriverMovement->MovementMode != MOVE_None;
		DriverPreviousMovementMode = DriverMovement->MovementMode;
	}
	if (bHideDriverWhileSeated)
	{
		DriverPawn->SetActorHiddenInGame(true);
	}

	if (bDisableDriverCollisionWhileSeated)
	{
		DriverPawn->SetActorEnableCollision(false);
	}

	if (bDisableDriverMovementWhileSeated && DriverMovement)
	{
		DriverMovement->StopMovementImmediately();
		DriverMovement->DisableMovement();
	}

	const FVector SeatLocation = GetInteractionLocation();
	DriverPawn->SetActorLocation(SeatLocation);

	USceneComponent* AttachTarget = DriverAttachPoint
		? DriverAttachPoint.Get()
		: (SeatPoint ? SeatPoint.Get() : VehiclePawn->GetRootComponent());
	if (bAttachDriverToVehicle && AttachTarget)
	{
		DriverPawn->AttachToComponent(AttachTarget, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		bDriverAttachedToVehicle = true;
		if (bKeepDriverUprightWhileSeated)
		{
			if (USceneComponent* DriverRootComponent = DriverPawn->GetRootComponent())
			{
				bDriverRootUsedAbsoluteRotation = DriverRootComponent->IsUsingAbsoluteRotation();
				DriverRootComponent->SetAbsolute(false, true, false);
			}

			const FRotator ControlRotation = DriverController->GetControlRotation();
			DriverPawn->SetActorRotation(FRotator(0.0f, ControlRotation.Yaw, 0.0f));
		}
	}

	if (bUsePossessionFlow)
	{
		DriverController->Possess(VehiclePawn);
		if (DriverController->GetPawn() != VehiclePawn)
		{
			if (bDriverAttachedToVehicle)
			{
				if (USceneComponent* DriverRootComponent = DriverPawn->GetRootComponent())
				{
					DriverRootComponent->SetAbsolute(false, bDriverRootUsedAbsoluteRotation, false);
				}
				DriverPawn->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
				bDriverAttachedToVehicle = false;
			}
			if (bHideDriverWhileSeated)
			{
				DriverPawn->SetActorHiddenInGame(bDriverWasHiddenInGame);
			}
			if (bDisableDriverCollisionWhileSeated)
			{
				DriverPawn->SetActorEnableCollision(bDriverCollisionWasEnabled);
			}
			if (bDisableDriverMovementWhileSeated && DriverMovement)
			{
				if (bDriverMovementWasEnabled)
				{
					EMovementMode RestoredMode = static_cast<EMovementMode>(DriverPreviousMovementMode.GetValue());
					if (RestoredMode == MOVE_None)
					{
						RestoredMode = MOVE_Walking;
					}
					DriverMovement->SetMovementMode(RestoredMode);
				}
			}
			return false;
		}
	}

	ActiveDriverPawn = DriverPawn;
	ActiveDriverController = DriverController;
	return true;
}

bool UVehicleSeatComponent::TryExit(AActor* Interactor)
{
	return TryExitInternal(Interactor, false);
}

bool UVehicleSeatComponent::ForceExit(AActor* Interactor)
{
	return TryExitInternal(Interactor, true);
}

bool UVehicleSeatComponent::TryExitInternal(AActor* Interactor, bool bAllowUnsafeFallback)
{
	if (!IsOccupied())
	{
		return false;
	}

	APawn* VehiclePawn = CachedVehiclePawn.Get();
	APawn* DriverPawn = ActiveDriverPawn.Get();
	APlayerController* DriverController = ActiveDriverController.Get();
	if (!VehiclePawn || !DriverPawn || !DriverController)
	{
		return false;
	}

	if (Interactor && Interactor != VehiclePawn && Interactor != DriverPawn)
	{
		return false;
	}

	ACharacter* DriverCharacter = Cast<ACharacter>(DriverPawn);
	UCharacterMovementComponent* DriverMovement = DriverCharacter
		? DriverCharacter->GetCharacterMovement()
		: nullptr;

	FTransform ExitTransform;
	const FVector CurrentDriverLocation = DriverPawn->GetActorLocation();
	const FRotator CurrentDriverRotation = DriverPawn->GetActorRotation();
	const auto IsDriverLocationClear = [&](const FVector& TestLocation) -> bool
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			return true;
		}

		float CapsuleRadius = 34.0f;
		float CapsuleHalfHeight = 88.0f;
		if (const ACharacter* DriverCharacterForCapsule = Cast<ACharacter>(DriverPawn))
		{
			if (const UCapsuleComponent* CapsuleComponent = DriverCharacterForCapsule->GetCapsuleComponent())
			{
				CapsuleRadius = CapsuleComponent->GetScaledCapsuleRadius();
				CapsuleHalfHeight = CapsuleComponent->GetScaledCapsuleHalfHeight();
			}
		}

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VehicleExitCurrent), false, DriverPawn);
		if (const AActor* OwnerActor = GetOwner())
		{
			QueryParams.AddIgnoredActor(OwnerActor);
		}

		FHitResult BlockingHit;
		const bool bBlocked = World->SweepSingleByChannel(
			BlockingHit,
			TestLocation,
			TestLocation,
			FQuat::Identity,
			ECC_Pawn,
			FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight),
			QueryParams);
		return !bBlocked;
	};

	bool bHasExitTransform = false;
	if (bExitAtCurrentLocation)
	{
		ExitTransform = FTransform(FRotator(0.0f, CurrentDriverRotation.Yaw, 0.0f), CurrentDriverLocation);
		bHasExitTransform = true;
	}

	if (bHasExitTransform && bUseSafeFallbackIfCurrentExitBlocked && !IsDriverLocationClear(CurrentDriverLocation))
	{
		bHasExitTransform = false;
	}

	if (!bHasExitTransform && FindSafeExitTransform(DriverPawn, ExitTransform))
	{
		bHasExitTransform = true;
	}

	if (!bHasExitTransform)
	{
		if (!bAllowUnsafeFallback)
		{
			return false;
		}

		const AActor* OwnerActor = GetOwner();
		const FVector BaseExitLocation = OwnerActor
			? OwnerActor->GetActorLocation()
				+ (OwnerActor->GetActorRightVector() * EmergencyExitDistance)
				+ FVector(0.0f, 0.0f, EmergencyExitVerticalOffset)
			: DriverPawn->GetActorLocation() + FVector(EmergencyExitDistance, 0.0f, EmergencyExitVerticalOffset);
		const float ExitYaw = OwnerActor ? OwnerActor->GetActorRotation().Yaw : DriverPawn->GetActorRotation().Yaw;
		const FRotator ExitRotation(0.0f, ExitYaw, 0.0f);
		ExitTransform = FTransform(ExitRotation, BaseExitLocation);
	}

	if (bDriverAttachedToVehicle)
	{
		if (USceneComponent* DriverRootComponent = DriverPawn->GetRootComponent())
		{
			DriverRootComponent->SetAbsolute(false, bDriverRootUsedAbsoluteRotation, false);
		}
		DriverPawn->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		bDriverAttachedToVehicle = false;
	}
	const FRotator UprightExitRotation(0.0f, ExitTransform.GetRotation().Rotator().Yaw, 0.0f);
	const bool bNeedsReposition =
		!DriverPawn->GetActorLocation().Equals(ExitTransform.GetLocation(), 1.0f)
		|| !FMath::IsNearlyEqual(
			FMath::Abs(FRotator::NormalizeAxis(DriverPawn->GetActorRotation().Yaw - UprightExitRotation.Yaw)),
			0.0f,
			1.0f);
	if (bNeedsReposition)
	{
		DriverPawn->SetActorLocationAndRotation(
			ExitTransform.GetLocation(),
			UprightExitRotation,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
	}

	if (bHideDriverWhileSeated)
	{
		DriverPawn->SetActorHiddenInGame(bDriverWasHiddenInGame);
	}
	if (bDisableDriverCollisionWhileSeated)
	{
		DriverPawn->SetActorEnableCollision(bDriverCollisionWasEnabled);
	}

	if (bDisableDriverMovementWhileSeated && DriverMovement)
	{
		if (bDriverMovementWasEnabled)
		{
			EMovementMode RestoredMode = static_cast<EMovementMode>(DriverPreviousMovementMode.GetValue());
			if (RestoredMode == MOVE_None)
			{
				RestoredMode = MOVE_Walking;
			}
			DriverMovement->SetMovementMode(RestoredMode);
		}
	}

	if (bUsePossessionFlow)
	{
		DriverController->Possess(DriverPawn);
		if (DriverController->GetPawn() != DriverPawn)
		{
			return false;
		}
	}

	FRotator ExitControlRotation = DriverController->GetControlRotation();
	ExitControlRotation.Yaw = UprightExitRotation.Yaw;
	ExitControlRotation.Roll = 0.0f;
	DriverController->SetControlRotation(ExitControlRotation);
	ActiveDriverPawn.Reset();
	ActiveDriverController.Reset();
	return true;
}

bool UVehicleSeatComponent::IsOccupied() const
{
	return ActiveDriverPawn.IsValid() && ActiveDriverController.IsValid();
}

APawn* UVehicleSeatComponent::GetDriverPawn() const
{
	return ActiveDriverPawn.Get();
}

APlayerController* UVehicleSeatComponent::GetDriverController() const
{
	return ActiveDriverController.Get();
}

FVector UVehicleSeatComponent::GetInteractionLocation() const
{
	if (SeatPoint)
	{
		return SeatPoint->GetComponentLocation();
	}

	if (const AActor* OwnerActor = GetOwner())
	{
		return OwnerActor->GetActorLocation();
	}

	return FVector::ZeroVector;
}

FVector UVehicleSeatComponent::GetExitLocation() const
{
	if (ExitPoint)
	{
		return ExitPoint->GetComponentLocation();
	}

	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return FVector::ZeroVector;
	}

	return OwnerActor->GetActorLocation()
		+ (OwnerActor->GetActorForwardVector() * ExitForwardDistance)
		+ (OwnerActor->GetActorRightVector() * ExitSideDistance)
		+ FVector(0.0f, 0.0f, ExitVerticalOffset);
}

bool UVehicleSeatComponent::FindSafeExitTransform(APawn* DriverPawn, FTransform& OutExitTransform) const
{
	const AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!OwnerActor || !World || !DriverPawn)
	{
		return false;
	}

	const FRotator ExitRotation(0.0f, OwnerActor->GetActorRotation().Yaw, 0.0f);
	TArray<FVector> CandidateLocations;
	if (ExitPoint)
	{
		CandidateLocations.Add(ExitPoint->GetComponentLocation());
	}

	const FVector BaseLocation = OwnerActor->GetActorLocation() + FVector(0.0f, 0.0f, ExitVerticalOffset);
	CandidateLocations.Add(BaseLocation + (OwnerActor->GetActorRightVector() * ExitSideDistance));
	CandidateLocations.Add(BaseLocation - (OwnerActor->GetActorRightVector() * ExitSideDistance));
	CandidateLocations.Add(BaseLocation + (OwnerActor->GetActorForwardVector() * ExitForwardDistance));
	CandidateLocations.Add(BaseLocation - (OwnerActor->GetActorForwardVector() * ExitForwardDistance));
	CandidateLocations.Add(BaseLocation + FVector(ExitForwardDistance, ExitSideDistance, 0.0f));
	CandidateLocations.Add(BaseLocation + FVector(ExitForwardDistance, -ExitSideDistance, 0.0f));
	CandidateLocations.Add(BaseLocation + FVector(-ExitForwardDistance, ExitSideDistance, 0.0f));
	CandidateLocations.Add(BaseLocation + FVector(-ExitForwardDistance, -ExitSideDistance, 0.0f));

	float CapsuleRadius = 34.0f;
	float CapsuleHalfHeight = 88.0f;
	if (const ACharacter* DriverCharacter = Cast<ACharacter>(DriverPawn))
	{
		if (const UCapsuleComponent* CapsuleComponent = DriverCharacter->GetCapsuleComponent())
		{
			CapsuleRadius = CapsuleComponent->GetScaledCapsuleRadius();
			CapsuleHalfHeight = CapsuleComponent->GetScaledCapsuleHalfHeight();
		}
	}

	const FCollisionShape TestCapsule = FCollisionShape::MakeCapsule(CapsuleRadius, CapsuleHalfHeight);
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VehicleExit), false, DriverPawn);
	QueryParams.AddIgnoredActor(OwnerActor);

	for (const FVector& CandidateLocation : CandidateLocations)
	{
		FHitResult BlockingHit;
		const bool bBlocked = World->SweepSingleByChannel(
			BlockingHit,
			CandidateLocation,
			CandidateLocation,
			FQuat::Identity,
			ECC_Pawn,
			TestCapsule,
			QueryParams);
		if (!bBlocked)
		{
			OutExitTransform = FTransform(ExitRotation, CandidateLocation);
			return true;
		}
	}

	return false;
}
