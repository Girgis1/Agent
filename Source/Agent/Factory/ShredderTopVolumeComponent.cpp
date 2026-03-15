// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/ShredderTopVolumeComponent.h"

UShredderTopVolumeComponent::UShredderTopVolumeComponent()
{
	DebugName = TEXT("Shredder Top");
	bUseShredDelay = true;
	ShredDelayMinSeconds = 1.0f;
	ShredDelayMaxSeconds = 5.0f;
	bRequireHealthDepletionBeforeShredding = false;
}
