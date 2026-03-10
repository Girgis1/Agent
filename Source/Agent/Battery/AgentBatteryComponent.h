// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AgentBatteryComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAgentBatteryChargeChangedSignature, float, NewCharge);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAgentBatteryDepletedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAgentBatteryRestoredSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAgentBatteryFullyChargedSignature);

UCLASS(ClassGroup=(Power), meta=(BlueprintSpawnableComponent))
class UAgentBatteryComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAgentBatteryComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category="Battery")
	bool IsBatteryEnabled() const { return bBatteryEnabled; }

	UFUNCTION(BlueprintCallable, Category="Battery")
	void SetBatteryEnabled(bool bInEnabled);

	UFUNCTION(BlueprintPure, Category="Battery")
	float GetMaxCharge() const { return MaxCharge; }

	UFUNCTION(BlueprintPure, Category="Battery")
	float GetCurrentCharge() const { return CurrentCharge; }

	UFUNCTION(BlueprintPure, Category="Battery")
	float GetChargePercent() const;

	UFUNCTION(BlueprintPure, Category="Battery")
	float GetFullThresholdCharge() const { return FullThresholdCharge; }

	UFUNCTION(BlueprintPure, Category="Battery")
	bool IsDepleted() const;

	UFUNCTION(BlueprintPure, Category="Battery")
	bool IsFullyCharged() const;

	UFUNCTION(BlueprintPure, Category="Battery")
	bool IsDrainEnabled() const { return bAutoDrainEnabled; }

	UFUNCTION(BlueprintPure, Category="Battery")
	bool IsChargeEnabled() const { return bAutoChargeEnabled; }

	UFUNCTION(BlueprintPure, Category="Battery")
	float GetDrainRatePerSecond() const { return DrainRatePerSecond; }

	UFUNCTION(BlueprintPure, Category="Battery")
	float GetChargeRatePerSecond() const { return ChargeRatePerSecond; }

	UFUNCTION(BlueprintCallable, Category="Battery")
	void SetMaxCharge(float NewMaxCharge);

	UFUNCTION(BlueprintCallable, Category="Battery")
	void SetCurrentCharge(float NewCurrentCharge);

	UFUNCTION(BlueprintCallable, Category="Battery")
	void SetChargePercent(float NewChargePercent);

	UFUNCTION(BlueprintCallable, Category="Battery")
	void SetFullThresholdCharge(float NewFullThresholdCharge);

	UFUNCTION(BlueprintCallable, Category="Battery")
	void SetDrainEnabled(bool bInEnabled);

	UFUNCTION(BlueprintCallable, Category="Battery")
	void SetChargeEnabled(bool bInEnabled);

	UFUNCTION(BlueprintCallable, Category="Battery")
	void SetDrainRatePerSecond(float NewDrainRatePerSecond);

	UFUNCTION(BlueprintCallable, Category="Battery")
	void SetChargeRatePerSecond(float NewChargeRatePerSecond);

	UFUNCTION(BlueprintCallable, Category="Battery")
	float ConsumeCharge(float ChargeToConsume);

	UFUNCTION(BlueprintCallable, Category="Battery")
	float AddCharge(float ChargeToAdd);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battery")
	bool bBatteryEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battery", meta=(ClampMin="0.01", UIMin="0.01"))
	float MaxCharge = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battery", meta=(ClampMin="0.0", UIMin="0.0"))
	float CurrentCharge = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battery", meta=(ClampMin="0.0", UIMin="0.0"))
	float FullThresholdCharge = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battery")
	bool bAutoDrainEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battery")
	bool bAutoChargeEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battery", meta=(ClampMin="0.0", UIMin="0.0"))
	float DrainRatePerSecond = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Battery", meta=(ClampMin="0.0", UIMin="0.0"))
	float ChargeRatePerSecond = 0.0f;

	UPROPERTY(BlueprintAssignable, Category="Battery")
	FOnAgentBatteryChargeChangedSignature OnBatteryChargeChanged;

	UPROPERTY(BlueprintAssignable, Category="Battery")
	FOnAgentBatteryDepletedSignature OnBatteryDepleted;

	UPROPERTY(BlueprintAssignable, Category="Battery")
	FOnAgentBatteryRestoredSignature OnBatteryRestored;

	UPROPERTY(BlueprintAssignable, Category="Battery")
	FOnAgentBatteryFullyChargedSignature OnBatteryFullyCharged;

private:
	void ClampAndBroadcastIfChanged(float PreviousCharge);

	bool bWasDepleted = false;
	bool bWasFullyCharged = false;
};

