// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/ConveyorSurfaceVelocityComponent.h"

UConveyorSurfaceVelocityComponent::UConveyorSurfaceVelocityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UConveyorSurfaceVelocityComponent::SetSurfaceVelocity(const FVector& NewVelocity)
{
	SurfaceVelocity = NewVelocity;
}

void UConveyorSurfaceVelocityComponent::ClearSurfaceVelocity()
{
	SurfaceVelocity = FVector::ZeroVector;
}
