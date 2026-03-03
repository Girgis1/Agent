// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"

namespace AgentFactory
{
	inline constexpr float GridSize = 100.0f;
	inline constexpr float QuarterTurnDegrees = 90.0f;
	inline constexpr ECollisionChannel BuildPlacementTraceChannel = ECC_GameTraceChannel1;
	inline constexpr ECollisionChannel FactoryBuildableChannel = ECC_GameTraceChannel2;

	inline FVector SnapLocationToGrid(const FVector& WorldLocation)
	{
		return FVector(
			FMath::GridSnap(WorldLocation.X, GridSize),
			FMath::GridSnap(WorldLocation.Y, GridSize),
			FMath::GridSnap(WorldLocation.Z, GridSize));
	}

	inline FRotator SnapYawToGrid(float RawYawDegrees)
	{
		return FRotator(0.0f, FMath::GridSnap(FRotator::NormalizeAxis(RawYawDegrees), QuarterTurnDegrees), 0.0f);
	}
}
