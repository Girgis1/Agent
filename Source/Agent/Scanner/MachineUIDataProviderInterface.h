// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MachineUISystemTypes.h"
#include "UObject/Interface.h"
#include "MachineUIDataProviderInterface.generated.h"

UINTERFACE(BlueprintType)
class UMachineUIDataProviderInterface : public UInterface
{
	GENERATED_BODY()
};

class IMachineUIDataProviderInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Machine|UI")
	bool BuildMachineUIState(FMachineUIState& OutState) const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Machine|UI")
	bool ExecuteMachineUICommand(const FMachineUICommand& Command);
};

