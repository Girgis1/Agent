// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "FactoryWorldConfig.generated.h"

UCLASS()
class AFactoryWorldConfig : public AInfo
{
	GENERATED_BODY()

public:
	AFactoryWorldConfig();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Conveyor")
	float ConveyorMasterBeltSpeed = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Machines", meta=(ClampMin="0.0", UIMin="0.0", DisplayName="Global Factory Speed"))
	float FactoryCraftSpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Resources", meta=(ClampMin="0.0", UIMin="0.0"))
	float ResourceBaseMassMultiplier = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Resources|GlobalStorage")
	bool bGlobalStorageInfinite = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Resources|GlobalStorage", meta=(ClampMin="0", UIMin="0", EditCondition="!bGlobalStorageInfinite"))
	int32 GlobalStorageCapacityUnits = 0;
};
