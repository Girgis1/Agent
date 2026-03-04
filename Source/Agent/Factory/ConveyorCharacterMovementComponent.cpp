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
