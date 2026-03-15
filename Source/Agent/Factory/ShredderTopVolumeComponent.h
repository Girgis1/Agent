// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factory/ShredderVolumeComponent.h"
#include "ShredderTopVolumeComponent.generated.h"

UCLASS(ClassGroup=(Factory), meta=(BlueprintSpawnableComponent, DisplayName="Shredder Volume Top"))
class UShredderTopVolumeComponent : public UShredderVolumeComponent
{
	GENERATED_BODY()

public:
	UShredderTopVolumeComponent();
};
