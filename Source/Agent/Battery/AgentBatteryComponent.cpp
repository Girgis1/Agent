// Copyright Epic Games, Inc. All Rights Reserved.

#include "Battery/AgentBatteryComponent.h"

UAgentBatteryComponent::UAgentBatteryComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UAgentBatteryComponent::BeginPlay()
{
	Super::BeginPlay();

	ClampAndBroadcastIfChanged(CurrentCharge);
	bWasDepleted = IsDepleted();
	bWasFullyCharged = IsFullyCharged();
}

void UAgentBatteryComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bBatteryEnabled)
	{
		return;
	}

	const float SafeDeltaTime = FMath::Max(0.0f, DeltaTime);
	if (SafeDeltaTime <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	if (bAutoDrainEnabled && DrainRatePerSecond > KINDA_SMALL_NUMBER && !IsDepleted())
	{
		ConsumeCharge(DrainRatePerSecond * SafeDeltaTime);
	}

	if (bAutoChargeEnabled && ChargeRatePerSecond > KINDA_SMALL_NUMBER && !IsFullyCharged())
	{
		AddCharge(ChargeRatePerSecond * SafeDeltaTime);
	}
}

void UAgentBatteryComponent::SetBatteryEnabled(bool bInEnabled)
{
	bBatteryEnabled = bInEnabled;
}

float UAgentBatteryComponent::GetChargePercent() const
{
	const float SafeMaxCharge = FMath::Max(0.01f, MaxCharge);
	return (CurrentCharge / SafeMaxCharge) * 100.0f;
}

bool UAgentBatteryComponent::IsDepleted() const
{
	return CurrentCharge <= KINDA_SMALL_NUMBER;
}

bool UAgentBatteryComponent::IsFullyCharged() const
{
	return CurrentCharge >= FMath::Max(0.0f, FullThresholdCharge) - KINDA_SMALL_NUMBER;
}

void UAgentBatteryComponent::SetMaxCharge(float NewMaxCharge)
{
	const float PreviousCharge = CurrentCharge;
	MaxCharge = NewMaxCharge;
	ClampAndBroadcastIfChanged(PreviousCharge);
}

void UAgentBatteryComponent::SetCurrentCharge(float NewCurrentCharge)
{
	const float PreviousCharge = CurrentCharge;
	CurrentCharge = NewCurrentCharge;
	ClampAndBroadcastIfChanged(PreviousCharge);
}

void UAgentBatteryComponent::SetChargePercent(float NewChargePercent)
{
	const float PreviousCharge = CurrentCharge;
	const float SafeMaxCharge = FMath::Max(0.01f, MaxCharge);
	const float ClampedPercent = FMath::Clamp(NewChargePercent, 0.0f, 100.0f);
	CurrentCharge = SafeMaxCharge * (ClampedPercent / 100.0f);
	ClampAndBroadcastIfChanged(PreviousCharge);
}

void UAgentBatteryComponent::SetFullThresholdCharge(float NewFullThresholdCharge)
{
	const float PreviousCharge = CurrentCharge;
	FullThresholdCharge = NewFullThresholdCharge;
	ClampAndBroadcastIfChanged(PreviousCharge);
}

void UAgentBatteryComponent::SetDrainEnabled(bool bInEnabled)
{
	bAutoDrainEnabled = bInEnabled;
}

void UAgentBatteryComponent::SetChargeEnabled(bool bInEnabled)
{
	bAutoChargeEnabled = bInEnabled;
}

void UAgentBatteryComponent::SetDrainRatePerSecond(float NewDrainRatePerSecond)
{
	const float PreviousCharge = CurrentCharge;
	DrainRatePerSecond = NewDrainRatePerSecond;
	ClampAndBroadcastIfChanged(PreviousCharge);
}

void UAgentBatteryComponent::SetChargeRatePerSecond(float NewChargeRatePerSecond)
{
	const float PreviousCharge = CurrentCharge;
	ChargeRatePerSecond = NewChargeRatePerSecond;
	ClampAndBroadcastIfChanged(PreviousCharge);
}

float UAgentBatteryComponent::ConsumeCharge(float ChargeToConsume)
{
	if (ChargeToConsume <= 0.0f)
	{
		return 0.0f;
	}

	const float PreviousCharge = CurrentCharge;
	CurrentCharge = FMath::Max(0.0f, CurrentCharge - ChargeToConsume);
	ClampAndBroadcastIfChanged(PreviousCharge);
	return FMath::Max(0.0f, PreviousCharge - CurrentCharge);
}

float UAgentBatteryComponent::AddCharge(float ChargeToAdd)
{
	if (ChargeToAdd <= 0.0f)
	{
		return 0.0f;
	}

	const float PreviousCharge = CurrentCharge;
	CurrentCharge = FMath::Min(FMath::Max(0.01f, MaxCharge), CurrentCharge + ChargeToAdd);
	ClampAndBroadcastIfChanged(PreviousCharge);
	return FMath::Max(0.0f, CurrentCharge - PreviousCharge);
}

void UAgentBatteryComponent::ClampAndBroadcastIfChanged(float PreviousCharge)
{
	MaxCharge = FMath::Max(0.01f, MaxCharge);
	CurrentCharge = FMath::Clamp(CurrentCharge, 0.0f, MaxCharge);
	FullThresholdCharge = FMath::Clamp(FullThresholdCharge, 0.0f, MaxCharge);
	DrainRatePerSecond = FMath::Max(0.0f, DrainRatePerSecond);
	ChargeRatePerSecond = FMath::Max(0.0f, ChargeRatePerSecond);

	if (!FMath::IsNearlyEqual(CurrentCharge, PreviousCharge))
	{
		OnBatteryChargeChanged.Broadcast(CurrentCharge);
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

	const bool bIsFullyChargedNow = IsFullyCharged();
	if (bIsFullyChargedNow && !bWasFullyCharged)
	{
		OnBatteryFullyCharged.Broadcast();
	}
	bWasFullyCharged = bIsFullyChargedNow;
}

