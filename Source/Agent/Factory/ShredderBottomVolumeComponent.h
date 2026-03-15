// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factory/ShredderVolumeComponent.h"
#include "ShredderBottomVolumeComponent.generated.h"

UCLASS(ClassGroup=(Factory), meta=(BlueprintSpawnableComponent, DisplayName="Shredder Volume Bottom"))
class UShredderBottomVolumeComponent : public UShredderVolumeComponent
{
	GENERATED_BODY()

public:
	UShredderBottomVolumeComponent();
};
