// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/ConveyorCharacterMovementComponent.h"

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

	if (!PendingConveyorVelocity.IsNearlyZero() && IsMovingOnGround())
	{
		Velocity += PendingConveyorVelocity;
	}

	PendingConveyorVelocity = FVector::ZeroVector;
}
