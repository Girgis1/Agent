// Copyright Epic Games, Inc. All Rights Reserved.

#include "DroneBatteryComponent.h"

UDroneBatteryComponent::UDroneBatteryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDroneBatteryComponent::BeginPlay()
{
	Super::BeginPlay();

	MaxBatteryPercent = FMath::Clamp(MaxBatteryPercent, 0.01f, 100.0f);
	FullThresholdPercent = FMath::Clamp(FullThresholdPercent, 0.0f, MaxBatteryPercent);
	DeadThresholdPercent = FMath::Clamp(DeadThresholdPercent, 0.0f, MaxBatteryPercent);
	BatteryDrainDurationSeconds = FMath::Max(0.01f, BatteryDrainDurationSeconds);
	PassiveChargeRatePercentPerSecond = FMath::Max(0.0f, PassiveChargeRatePercentPerSecond);
	BatteryPercent = FMath::Clamp(BatteryPercent, 0.0f, MaxBatteryPercent);
	bWasDepleted = IsDepleted();
}

bool UDroneBatteryComponent::IsDepleted() const
{
	return BatteryPercent <= FMath::Max(0.0f, DeadThresholdPercent);
}

bool UDroneBatteryComponent::IsFullyCharged() const
{
	return BatteryPercent >= FMath::Max(0.0f, FullThresholdPercent) - KINDA_SMALL_NUMBER;
}

float UDroneBatteryComponent::GetDrainRatePercentPerSecond() const
{
	if (!bDrainBatteryOverTime || BatteryDrainDurationSeconds <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	return MaxBatteryPercent / FMath::Max(0.01f, BatteryDrainDurationSeconds);
}

void UDroneBatteryComponent::SetBatteryPercent(float NewBatteryPercent)
{
	const float PreviousBatteryPercent = BatteryPercent;
	BatteryPercent = NewBatteryPercent;
	ClampAndBroadcastIfChanged(PreviousBatteryPercent);
}

void UDroneBatteryComponent::SetMaxBatteryPercent(float NewMaxBatteryPercent)
{
	const float PreviousBatteryPercent = BatteryPercent;
	MaxBatteryPercent = NewMaxBatteryPercent;
	ClampAndBroadcastIfChanged(PreviousBatteryPercent);
}

void UDroneBatteryComponent::SetFullThresholdPercent(float NewFullThresholdPercent)
{
	const float PreviousBatteryPercent = BatteryPercent;
	FullThresholdPercent = NewFullThresholdPercent;
	ClampAndBroadcastIfChanged(PreviousBatteryPercent);
}

void UDroneBatteryComponent::SetDeadThresholdPercent(float NewDeadThresholdPercent)
{
	const float PreviousBatteryPercent = BatteryPercent;
	DeadThresholdPercent = FMath::Clamp(NewDeadThresholdPercent, 0.0f, MaxBatteryPercent);
	ClampAndBroadcastIfChanged(PreviousBatteryPercent);
}

float UDroneBatteryComponent::ConsumePercent(float PercentToConsume)
{
	if (PercentToConsume <= 0.0f)
	{
		return 0.0f;
	}

	const float PreviousBatteryPercent = BatteryPercent;
	BatteryPercent = FMath::Max(0.0f, BatteryPercent - PercentToConsume);
	ClampAndBroadcastIfChanged(PreviousBatteryPercent);
	return FMath::Max(0.0f, PreviousBatteryPercent - BatteryPercent);
}

float UDroneBatteryComponent::ChargePercent(float PercentToCharge)
{
	if (PercentToCharge <= 0.0f)
	{
		return 0.0f;
	}

	const float PreviousBatteryPercent = BatteryPercent;
	BatteryPercent = FMath::Min(MaxBatteryPercent, BatteryPercent + PercentToCharge);
	ClampAndBroadcastIfChanged(PreviousBatteryPercent);
	return FMath::Max(0.0f, BatteryPercent - PreviousBatteryPercent);
}

void UDroneBatteryComponent::ClampAndBroadcastIfChanged(float PreviousBatteryPercent)
{
	MaxBatteryPercent = FMath::Clamp(MaxBatteryPercent, 0.01f, 100.0f);
	FullThresholdPercent = FMath::Clamp(FullThresholdPercent, 0.0f, MaxBatteryPercent);
	DeadThresholdPercent = FMath::Clamp(DeadThresholdPercent, 0.0f, MaxBatteryPercent);
	BatteryDrainDurationSeconds = FMath::Max(0.01f, BatteryDrainDurationSeconds);
	PassiveChargeRatePercentPerSecond = FMath::Max(0.0f, PassiveChargeRatePercentPerSecond);
	BatteryPercent = FMath::Clamp(BatteryPercent, 0.0f, MaxBatteryPercent);

	if (!FMath::IsNearlyEqual(BatteryPercent, PreviousBatteryPercent))
	{
		OnBatteryChanged.Broadcast(BatteryPercent);
	}

	const bool bIsDepletedNow = IsDepleted();
	if (bIsDepletedNow != bWasDepleted)
	{
		if (bIsDepletedNow)
		{
			OnBatteryDepleted.Broadcast();
		}
		else
		{
			OnBatteryRestored.Broadcast();
		}

		bWasDepleted = bIsDepletedNow;
	}
}
