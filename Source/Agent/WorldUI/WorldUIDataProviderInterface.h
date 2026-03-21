// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "WorldUISystemTypes.h"
#include "UObject/Interface.h"
#include "WorldUIDataProviderInterface.generated.h"

UINTERFACE(BlueprintType)
class UWorldUIDataProviderInterface : public UInterface
{
	GENERATED_BODY()
};

class IWorldUIDataProviderInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="World UI")
	bool BuildWorldUIModel(FWorldUIModel& OutModel) const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="World UI")
	bool HandleWorldUIAction(const FWorldUIAction& Action);
};
