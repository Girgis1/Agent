// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dirty/DirtDecalSubsystem.h"

#include "Dirty/DirtDecalActor.h"

void UDirtDecalSubsystem::Deinitialize()
{
	RegisteredDirtDecals.Reset();
	Super::Deinitialize();
}

void UDirtDecalSubsystem::RegisterDirtDecal(ADirtDecalActor* DirtDecal)
{
	if (!IsValid(DirtDecal))
	{
		return;
	}

	CompactRegisteredDecals();
	RegisteredDirtDecals.AddUnique(DirtDecal);
}

void UDirtDecalSubsystem::UnregisterDirtDecal(ADirtDecalActor* DirtDecal)
{
	if (!DirtDecal)
	{
		return;
	}

	RegisteredDirtDecals.RemoveAll(
		[DirtDecal](const TWeakObjectPtr<ADirtDecalActor>& Entry)
		{
			return !Entry.IsValid() || Entry.Get() == DirtDecal;
		});
}

bool UDirtDecalSubsystem::ApplyBrushAtHit(const FHitResult& Hit, const FDirtBrushStamp& Stamp, float DeltaTime)
{
	const FVector WorldPoint = Hit.ImpactPoint.IsNearlyZero() ? Hit.Location : Hit.ImpactPoint;
	return ApplyBrushAtWorldPoint(WorldPoint, Stamp, DeltaTime);
}

bool UDirtDecalSubsystem::ApplyBrushAtWorldPoint(const FVector& WorldPoint, const FDirtBrushStamp& Stamp, float DeltaTime)
{
	CompactRegisteredDecals();

	bool bAnyApplied = false;
	for (const TWeakObjectPtr<ADirtDecalActor>& DirtDecalEntry : RegisteredDirtDecals)
	{
		ADirtDecalActor* DirtDecal = DirtDecalEntry.Get();
		if (!IsValid(DirtDecal))
		{
			continue;
		}

		bAnyApplied |= DirtDecal->ApplyBrushAtWorldPoint(WorldPoint, Stamp, DeltaTime);
	}

	return bAnyApplied;
}

void UDirtDecalSubsystem::FindDecalsNearPoint(const FVector& WorldPoint, float BrushRadiusCm, TArray<ADirtDecalActor*>& OutDecals)
{
	OutDecals.Reset();
	CompactRegisteredDecals();

	for (const TWeakObjectPtr<ADirtDecalActor>& DirtDecalEntry : RegisteredDirtDecals)
	{
		ADirtDecalActor* DirtDecal = DirtDecalEntry.Get();
		if (IsValid(DirtDecal) && DirtDecal->CanBrushAffectWorldPoint(WorldPoint, BrushRadiusCm))
		{
			OutDecals.Add(DirtDecal);
		}
	}
}

void UDirtDecalSubsystem::CompactRegisteredDecals()
{
	RegisteredDirtDecals.RemoveAll(
		[](const TWeakObjectPtr<ADirtDecalActor>& Entry)
		{
			return !Entry.IsValid();
		});
}
