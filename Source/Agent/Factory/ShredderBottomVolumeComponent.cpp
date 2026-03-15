// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/ShredderBottomVolumeComponent.h"

UShredderBottomVolumeComponent::UShredderBottomVolumeComponent()
{
	DebugName = TEXT("Shredder Bottom");
	bUseShredDelay = false;
	ShredDelayMinSeconds = 0.0f;
	ShredDelayMaxSeconds = 0.0f;
	ProcessingInterval = 0.0f;
	bRequireHealthDepletionBeforeShredding = false;
	bConsumeOverlapsImmediately = true;
}
