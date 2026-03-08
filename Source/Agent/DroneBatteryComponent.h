// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DroneBatteryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDroneBatteryChangedSignature, float, NewBatteryPercent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDroneBatteryDepletedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDroneBatteryRestoredSignature);

UCLASS(ClassGroup=(Drone), meta=(BlueprintSpawnableComponent))
class UDroneBatteryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDroneBatteryComponent();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category="Drone|Battery")
	float GetBatteryPercent() const { return BatteryPercent; }

	UFUNCTION(BlueprintPure, Category="Drone|Battery")
	float GetMaxBatteryPercent() const { return MaxBatteryPercent; }

	UFUNCTION(BlueprintPure, Category="Drone|Battery")
	float GetDeadThresholdPercent() const { return DeadThresholdPercent; }

	UFUNCTION(BlueprintPure, Category="Drone|Battery")
	bool IsDepleted() const;

	UFUNCTION(BlueprintCallable, Category="Drone|Battery")
	void SetBatteryPercent(float NewBatteryPercent);

	UFUNCTION(BlueprintCallable, Category="Drone|Battery")
	void SetDeadThresholdPercent(float NewDeadThresholdPercent);

	UFUNCTION(BlueprintCallable, Category="Drone|Battery")
	float ConsumePercent(float PercentToConsume);

	UFUNCTION(BlueprintCallable, Category="Drone|Battery")
	float ChargePercent(float PercentToCharge);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Battery")
	float MaxBatteryPercent = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Battery", meta=(ClampMin="0.0", UIMin="0.0"))
	float BatteryPercent = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Drone|Battery", meta=(ClampMin="0.0", UIMin="0.0"))
	float DeadThresholdPercent = 0.0f;

	UPROPERTY(BlueprintAssignable, Category="Drone|Battery")
	FOnDroneBatteryChangedSignature OnBatteryChanged;

	UPROPERTY(BlueprintAssignable, Category="Drone|Battery")
	FOnDroneBatteryDepletedSignature OnBatteryDepleted;

	UPROPERTY(BlueprintAssignable, Category="Drone|Battery")
	FOnDroneBatteryRestoredSignature OnBatteryRestored;

private:
	void ClampAndBroadcastIfChanged(float PreviousBatteryPercent);

	bool bWasDepleted = false;
};
