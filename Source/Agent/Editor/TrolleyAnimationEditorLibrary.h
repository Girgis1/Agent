// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "TrolleyAnimationEditorLibrary.generated.h"

UCLASS()
class AGENT_API UTrolleyAnimationEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Trolley|Animation")
	static bool FixTrolleyPostProcessAnimBlueprint();
};
