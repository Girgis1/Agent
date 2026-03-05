// Copyright Epic Games, Inc. All Rights Reserved.

#include "Vehicle/Components/VehicleSeatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

UVehicleSeatComponent::UVehicleSeatComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
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

	const FVector SeatLocation = GetInteractionLocation();
	DriverPawn->SetActorLocation(SeatLocation);

	bDriverWasHiddenInGame = DriverPawn->IsHidden();
	bDriverCollisionWasEnabled = DriverPawn->GetActorEnableCollision();
	if (bHideDriverWhileSeated)
	{
		DriverPawn->SetActorHiddenInGame(true);
	}

	if (bDisableDriverCollisionWhileSeated)
	{
		DriverPawn->SetActorEnableCollision(false);
	}

	DriverController->Possess(VehiclePawn);
	if (DriverController->GetPawn() != VehiclePawn)
	{
		if (bHideDriverWhileSeated)
		{
			DriverPawn->SetActorHiddenInGame(bDriverWasHiddenInGame);
		}
		if (bDisableDriverCollisionWhileSeated)
		{
			DriverPawn->SetActorEnableCollision(bDriverCollisionWasEnabled);
		}
		return false;
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

	FTransform ExitTransform;
	if (!FindSafeExitTransform(DriverPawn, ExitTransform))
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
		const FRotator ExitRotation = OwnerActor ? OwnerActor->GetActorRotation() : DriverPawn->GetActorRotation();
		ExitTransform = FTransform(ExitRotation, BaseExitLocation);
	}

	DriverPawn->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	DriverPawn->SetActorLocationAndRotation(
		ExitTransform.GetLocation(),
		ExitTransform.GetRotation().Rotator(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	if (bHideDriverWhileSeated)
	{
		DriverPawn->SetActorHiddenInGame(bDriverWasHiddenInGame);
	}
	if (bDisableDriverCollisionWhileSeated)
	{
		DriverPawn->SetActorEnableCollision(bDriverCollisionWasEnabled);
	}

	DriverController->Possess(DriverPawn);
	if (DriverController->GetPawn() != DriverPawn)
	{
		return false;
	}

	DriverController->SetControlRotation(ExitTransform.GetRotation().Rotator());
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

	const FRotator ExitRotation = OwnerActor->GetActorRotation();
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
