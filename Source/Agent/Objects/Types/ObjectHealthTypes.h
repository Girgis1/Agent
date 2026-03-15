// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ObjectHealthTypes.generated.h"

UENUM(BlueprintType)
enum class EObjectHealthInitializationMode : uint8
{
	Manual UMETA(DisplayName="Manual"),
	FromCurrentMass UMETA(DisplayName="From Current Mass")
};

USTRUCT(BlueprintType)
struct FObjectHealthState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|Health")
	float MaxHealth = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|Health")
	float CurrentHealth = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|Health")
	float CurrentPhaseDamagedPenaltyPercent = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|Health")
	float TotalDamagedPenaltyPercent = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|Health")
	bool bInitialized = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|Health")
	bool bDepleted = false;

	float GetHealthPercent() const
	{
		const float SafeMaxHealth = FMath::Max(0.01f, MaxHealth);
		return (CurrentHealth / SafeMaxHealth) * 100.0f;
	}
};
