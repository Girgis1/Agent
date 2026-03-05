// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "VehicleInteractable.generated.h"

class AActor;

UINTERFACE(BlueprintType)
class AGENT_API UVehicleInteractable : public UInterface
{
	GENERATED_BODY()
};

class AGENT_API IVehicleInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Vehicle")
	bool CanEnterVehicle(AActor* Interactor) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Vehicle")
	bool EnterVehicle(AActor* Interactor);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Vehicle")
	bool ExitVehicle(AActor* Interactor);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Vehicle")
	bool IsVehicleOccupied() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Vehicle")
	bool IsVehicleControlledBy(AActor* Interactor) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Vehicle")
	FVector GetVehicleInteractionLocation(AActor* Interactor) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Vehicle")
	FText GetVehicleInteractionPrompt() const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Vehicle")
	void SetVehicleMoveInput(AActor* Interactor, float ForwardInput, float RightInput);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Vehicle")
	void SetVehicleHandbrakeInput(AActor* Interactor, bool bHandbrakeActive);
};
