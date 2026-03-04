// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/ConveyorSurfaceVelocityComponent.h"

UConveyorSurfaceVelocityComponent::UConveyorSurfaceVelocityComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UConveyorSurfaceVelocityComponent::SetConveyorSurfaceVelocity(UObject* Source, const FVector& NewVelocity)
{
	if (!Source)
	{
		Source = this;
	}

	if (NewVelocity.IsNearlyZero())
	{
		ClearConveyorSurfaceVelocity(Source);
		return;
	}

	CompactInvalidSources();

	for (FSurfaceVelocitySource& Entry : SurfaceVelocitySources)
	{
		if (Entry.Source.Get() == Source)
		{
			Entry.Velocity = NewVelocity;
			UpdateCachedSurfaceVelocity();
			return;
		}
	}

	FSurfaceVelocitySource& NewEntry = SurfaceVelocitySources.AddDefaulted_GetRef();
	NewEntry.Source = Source;
	NewEntry.Velocity = NewVelocity;
	UpdateCachedSurfaceVelocity();
}

void UConveyorSurfaceVelocityComponent::ClearConveyorSurfaceVelocity(UObject* Source)
{
	if (!Source)
	{
		SurfaceVelocitySources.Reset();
		UpdateCachedSurfaceVelocity();
		return;
	}

	SurfaceVelocitySources.RemoveAll(
		[Source](const FSurfaceVelocitySource& Entry)
		{
			return !Entry.Source.IsValid() || Entry.Source.Get() == Source;
		});
	UpdateCachedSurfaceVelocity();
}

FVector UConveyorSurfaceVelocityComponent::GetConveyorSurfaceVelocity() const
{
	FVector AccumulatedVelocity = FVector::ZeroVector;
	int32 ValidSourceCount = 0;

	for (const FSurfaceVelocitySource& Entry : SurfaceVelocitySources)
	{
		if (!Entry.Source.IsValid() || Entry.Velocity.IsNearlyZero())
		{
			continue;
		}

		AccumulatedVelocity += Entry.Velocity;
		++ValidSourceCount;
	}

	if (ValidSourceCount <= 0)
	{
		return FVector::ZeroVector;
	}

	return AccumulatedVelocity / static_cast<float>(ValidSourceCount);
}

bool UConveyorSurfaceVelocityComponent::HasConveyorSurfaceVelocity() const
{
	return !GetConveyorSurfaceVelocity().IsNearlyZero();
}

void UConveyorSurfaceVelocityComponent::SetSurfaceVelocity(const FVector& NewVelocity)
{
	SetConveyorSurfaceVelocity(GetOwner() ? static_cast<UObject*>(GetOwner()) : static_cast<UObject*>(this), NewVelocity);
}

void UConveyorSurfaceVelocityComponent::ClearSurfaceVelocity()
{
	ClearConveyorSurfaceVelocity(GetOwner() ? static_cast<UObject*>(GetOwner()) : static_cast<UObject*>(this));
}

void UConveyorSurfaceVelocityComponent::CompactInvalidSources()
{
	SurfaceVelocitySources.RemoveAll(
		[](const FSurfaceVelocitySource& Entry)
		{
			return !Entry.Source.IsValid() || Entry.Velocity.IsNearlyZero();
		});
}

void UConveyorSurfaceVelocityComponent::UpdateCachedSurfaceVelocity()
{
	CompactInvalidSources();
	SurfaceVelocity = GetConveyorSurfaceVelocity();
}
