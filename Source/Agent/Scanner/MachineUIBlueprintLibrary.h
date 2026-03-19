// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MachineUISystemTypes.h"
#include "MachineUIBlueprintLibrary.generated.h"

class AActor;
class UMachineComponent;

UCLASS()
class UMachineUIBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="Machine|UI", meta=(DefaultToSelf="SourceActor"))
	static bool ResolveMachineComponentFromActor(
		const AActor* SourceActor,
		UMachineComponent*& OutMachineComponent,
		AActor*& OutMachineActor);

	UFUNCTION(BlueprintCallable, Category="Machine|UI")
	static bool BuildMachineUIStateFromMachineComponent(const UMachineComponent* MachineComponent, FMachineUIState& OutState);

	UFUNCTION(BlueprintCallable, Category="Machine|UI")
	static bool BuildMachineUIStateFromActor(const AActor* SourceActor, FMachineUIState& OutState);

	UFUNCTION(BlueprintCallable, Category="Machine|UI")
	static bool ApplyMachineUICommandToMachineComponent(
		UMachineComponent* MachineComponent,
		const FMachineUICommand& Command,
		float DefaultSpeedStep = 0.1f);

	UFUNCTION(BlueprintCallable, Category="Machine|UI", meta=(DefaultToSelf="SourceActor"))
	static bool ApplyMachineUICommandToActor(
		AActor* SourceActor,
		const FMachineUICommand& Command,
		float DefaultSpeedStep = 0.1f);
};

