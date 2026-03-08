// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DroneBatteryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDroneBatteryChangedSignature, float, NewBatteryPercent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDroneBatteryDepletedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnDroneBatteryRestoredSignature);

UCLASS(ClassGroup=(Battery), meta=(BlueprintSpawnableComponent))
class UDroneBatteryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDroneBatteryComponent();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category="Battery")
	bool IsBatteryEnabled() const { return bBatteryEnabled; }

	UFUNCTION(BlueprintPure, Category="Battery")
	float GetBatteryPercent() const { return BatteryPercent; }

	UFUNCTION(BlueprintPure, Category="Battery")
	float GetMaxBatteryPercent() const { return MaxBatteryPercent; }

	UFUNCTION(BlueprintPure, Category="Battery")
	float GetFullThresholdPercent() const { return FullThresholdPercent; }

	UFUNCTION(BlueprintPure, Category="Battery")
	float GetDeadThresholdPercent() const { return DeadThresholdPercent; }

	UFUNCTION(BlueprintPure, Category="Battery")
	bool IsDepleted() const;

	UFUNCTION(BlueprintPure, Category="Battery")
	bool IsFullyCharged() const;

	UFUNCTION(BlueprintPure, Category="Battery")
	bool ShouldDrainOverTime() const { return bDrainBatteryOverTime; }

	UFUNCTION(BlueprintPure, Category="Battery")
	float GetDrainDurationSeconds() const { return BatteryDrainDurationSeconds; }

	UFUNCTION(BlueprintPure, Category="Battery")
	float GetDrainRatePercentPerSecond() const;

	UFUNCTION(BlueprintPure, Category="Battery")
	float GetPassiveChargeRatePercentPerSecond() const { return PassiveChargeRatePercentPerSecond; }

	UFUNCTION(BlueprintCallable, Category="Battery")
	void SetBatteryPercent(float NewBatteryPercent);

	UFUNCTION(BlueprintCallable, Category="Battery")
	void SetMaxBatteryPercent(float NewMaxBatteryPercent);

	UFUNCTION(BlueprintCallable, Category="Battery")
	void SetFullThresholdPercent(float NewFullThresholdPercent);

	UFUNCTION(BlueprintCallable, Category="Battery")
	void SetDeadThresholdPercent(float NewDeadThresholdPercent);

	UFUNCTION(BlueprintCallable, Category="Battery")
	float ConsumePercent(float PercentToConsume);

	UFUNCTION(BlueprintCallable, Category="Battery")
	float ChargePercent(float PercentToCharge);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battery")
	bool bBatteryEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battery", meta=(ClampMin="0.01", UIMin="0.01", ClampMax="100.0", UIMax="100.0"))
	float MaxBatteryPercent = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battery", meta=(ClampMin="0.0", UIMin="0.0"))
	float BatteryPercent = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battery", meta=(ClampMin="0.0", UIMin="0.0"))
	float FullThresholdPercent = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battery", meta=(ClampMin="0.0", UIMin="0.0"))
	float DeadThresholdPercent = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battery")
	bool bDrainBatteryOverTime = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battery", meta=(ClampMin="0.01", UIMin="0.01"))
	float BatteryDrainDurationSeconds = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battery", meta=(ClampMin="0.0", UIMin="0.0"))
	float PassiveChargeRatePercentPerSecond = 0.0f;

	UPROPERTY(BlueprintAssignable, Category="Battery")
	FOnDroneBatteryChangedSignature OnBatteryChanged;

	UPROPERTY(BlueprintAssignable, Category="Battery")
	FOnDroneBatteryDepletedSignature OnBatteryDepleted;

	UPROPERTY(BlueprintAssignable, Category="Battery")
	FOnDroneBatteryRestoredSignature OnBatteryRestored;

private:
	void ClampAndBroadcastIfChanged(float PreviousBatteryPercent);

	bool bWasDepleted = false;
};
