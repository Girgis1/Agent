// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/ConveyorCharacterMovementComponent.h"
#include "Factory/ConveyorSurfaceVelocityComponent.h"
#include "GameFramework/Actor.h"

UConveyorCharacterMovementComponent::UConveyorCharacterMovementComponent()
{
}

void UConveyorCharacterMovementComponent::SetPendingConveyorVelocity(const FVector& NewVelocity)
{
	PendingConveyorVelocity = NewVelocity;
}

void UConveyorCharacterMovementComponent::ClearPendingConveyorVelocity()
{
	PendingConveyorVelocity = FVector::ZeroVector;
}

void UConveyorCharacterMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);

	if (!UpdatedComponent || DeltaSeconds <= 0.0f)
	{
		PendingConveyorVelocity = FVector::ZeroVector;
		return;
	}

	if (bEmitStepUpEvents && IsMovingOnGround())
	{
		const FVector CharacterMoveLocation = UpdatedComponent->GetComponentLocation();
		const float StepUpHeight = CharacterMoveLocation.Z - OldLocation.Z;
		const FVector HorizontalDelta = FVector(CharacterMoveLocation.X - OldLocation.X, CharacterMoveLocation.Y - OldLocation.Y, 0.0f);
		const float OldVerticalSpeed = FMath::Abs(OldVelocity.Z);
		const float NewVerticalSpeed = FMath::Abs(Velocity.Z);
		if (StepUpHeight >= StepUpEventMinHeightCm
			&& StepUpHeight <= StepUpEventMaxHeightCm
			&& HorizontalDelta.SizeSquared() >= FMath::Square(StepUpEventMinHorizontalMoveCm)
			&& OldVerticalSpeed <= StepUpEventMaxVerticalSpeedCmPerSecond
			&& NewVerticalSpeed <= StepUpEventMaxVerticalSpeedCmPerSecond)
		{
			OnCharacterStepUp.Broadcast(StepUpHeight);
		}
	}

	FVector ConveyorVelocity = PendingConveyorVelocity;
	if (AActor* OwnerActor = GetOwner())
	{
		if (UConveyorSurfaceVelocityComponent* SurfaceVelocityComponent = OwnerActor->FindComponentByClass<UConveyorSurfaceVelocityComponent>())
		{
			ConveyorVelocity += SurfaceVelocityComponent->GetConveyorSurfaceVelocity();
		}
	}

	if (!ConveyorVelocity.IsNearlyZero())
	{
		const FVector ConveyorDelta = ConveyorVelocity * DeltaSeconds;
		FHitResult ConveyorHit;
		SafeMoveUpdatedComponent(ConveyorDelta, UpdatedComponent->GetComponentQuat(), true, ConveyorHit);
		if (ConveyorHit.IsValidBlockingHit())
		{
			SlideAlongSurface(ConveyorDelta, 1.0f - ConveyorHit.Time, ConveyorHit.Normal, ConveyorHit, true);
		}
	}

	PendingConveyorVelocity = FVector::ZeroVector;
}
