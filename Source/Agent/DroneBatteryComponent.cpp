// Copyright Epic Games, Inc. All Rights Reserved.

#include "DroneBatteryComponent.h"

UDroneBatteryComponent::UDroneBatteryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDroneBatteryComponent::BeginPlay()
{
	Super::BeginPlay();

	MaxBatteryPercent = 100.0f;
	DeadThresholdPercent = FMath::Clamp(DeadThresholdPercent, 0.0f, MaxBatteryPercent);
	BatteryPercent = FMath::Clamp(BatteryPercent, 0.0f, MaxBatteryPercent);
	bWasDepleted = IsDepleted();
}

bool UDroneBatteryComponent::IsDepleted() const
{
	return BatteryPercent <= FMath::Max(0.0f, DeadThresholdPercent);
}

void UDroneBatteryComponent::SetBatteryPercent(float NewBatteryPercent)
{
	const float PreviousBatteryPercent = BatteryPercent;
	BatteryPercent = NewBatteryPercent;
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
	MaxBatteryPercent = 100.0f;
	DeadThresholdPercent = FMath::Clamp(DeadThresholdPercent, 0.0f, MaxBatteryPercent);
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
