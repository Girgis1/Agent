// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GrabVehicleComponent.generated.h"

class UGrabVehiclePushComponent;
class UInteractVolumeComponent;

UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class AGENT_API UGrabVehicleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGrabVehicleComponent();

	UFUNCTION(BlueprintCallable, Category="Interact|GrabVehicle")
	void ApplyInteractionVolume(UInteractVolumeComponent* InInteractVolume) const;

	UFUNCTION(BlueprintCallable, Category="Interact|GrabVehicle")
	void ApplyPushTuning(UGrabVehiclePushComponent* InPushComponent, float FallbackDebugSphereRadius) const;

	UFUNCTION(BlueprintPure, Category="Interact|GrabVehicle")
	float GetMaxInteractDistance() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehicle|Interaction")
	float MaxInteractDistance = 280.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehicle|Interaction")
	FVector InteractVolumeExtent = FVector(150.0f, 120.0f, 100.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehicle|Push")
	bool bOverridePushTuning = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehicle|Push", meta=(EditCondition="bOverridePushTuning"))
	float HoldForwardOffset = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehicle|Push", meta=(EditCondition="bOverridePushTuning"))
	float HoldLateralOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehicle|Push", meta=(EditCondition="bOverridePushTuning"))
	float HoldVerticalOffset = -30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehicle|Push", meta=(EditCondition="bOverridePushTuning"))
	bool bUseInitialGrabVerticalOffset = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehicle|Push", meta=(EditCondition="bOverridePushTuning"))
	bool bAllowLiftInput = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehicle|Push", meta=(EditCondition="bOverridePushTuning && bAllowLiftInput"))
	float MaxLiftOffset = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehicle|Push", meta=(EditCondition="bOverridePushTuning"))
	float LiftInputInterpSpeed = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehicle|Push", meta=(EditCondition="bOverridePushTuning"))
	float BreakDistance = 320.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehicle|Push", meta=(EditCondition="bOverridePushTuning"))
	float MaxStartGrabDistance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehicle|Push|Handle", meta=(EditCondition="bOverridePushTuning"))
	float HandleLinearStiffness = 9200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehicle|Push|Handle", meta=(EditCondition="bOverridePushTuning"))
	float HandleLinearDamping = 560.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehicle|Push|Handle", meta=(EditCondition="bOverridePushTuning"))
	float HandleAngularStiffness = 3200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehicle|Push|Handle", meta=(EditCondition="bOverridePushTuning"))
	float HandleAngularDamping = 280.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehicle|Push|Handle", meta=(EditCondition="bOverridePushTuning"))
	float HandleInterpolationSpeed = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehicle|Push|Steer", meta=(EditCondition="bOverridePushTuning"))
	float SteerYawRate = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehicle|Push|Steer", meta=(EditCondition="bOverridePushTuning"))
	float YawTorqueStrength = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehicle|Push|Steer", meta=(EditCondition="bOverridePushTuning"))
	float YawTorqueDamping = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehicle|Push|Steer", meta=(EditCondition="bOverridePushTuning"))
	float MaxYawTorque = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehicle|Push|Debug", meta=(EditCondition="bOverridePushTuning"))
	bool bDrawDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehicle|Push|Debug", meta=(EditCondition="bOverridePushTuning"))
	bool bOverrideDebugSphereRadius = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehicle|Push|Debug", meta=(EditCondition="bOverridePushTuning && bOverrideDebugSphereRadius"))
	float DebugSphereRadius = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehicle|Push|Debug", meta=(EditCondition="bOverridePushTuning"))
	float DebugLineThickness = 1.5f;
};
