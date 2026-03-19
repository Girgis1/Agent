// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MachineUISystemTypes.generated.h"

UENUM(BlueprintType)
enum class EMachineUICommandType : uint8
{
	None,
	TogglePower,
	SetPowerEnabled,
	AdjustSpeed,
	SetSpeed,
	CycleRecipe,
	SetRecipeAny,
	SelectRecipeIndex,
	InstallUpgrade,
	RemoveUpgrade,
	Custom
};

USTRUCT(BlueprintType)
struct FMachineUICommand
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Command")
	EMachineUICommandType CommandType = EMachineUICommandType::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Command")
	float FloatValue = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Command")
	int32 IntValue = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Command")
	FName NameValue = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Command")
	FString StringValue;
};

USTRUCT(BlueprintType)
struct FMachineUIRecipeOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Recipe")
	int32 RecipeIndex = INDEX_NONE;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Recipe")
	FName RecipeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Recipe")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Recipe")
	float CraftTimeSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Recipe")
	bool bIsActive = false;
};

USTRUCT(BlueprintType)
struct FMachineUIUpgradeSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Upgrade")
	FName SlotId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Upgrade")
	FName UpgradeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Upgrade")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Upgrade")
	int32 Level = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Upgrade")
	bool bOccupied = false;
};

USTRUCT(BlueprintType)
struct FMachineUIState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|State")
	FText MachineName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|State")
	FText RuntimeStatus;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|State")
	bool bPowerEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|State")
	float CurrentSpeed = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|State")
	float MinSpeed = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|State")
	float MaxSpeed = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|State")
	float EffectiveSpeed = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|State")
	float StoredUnits = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|State")
	float CapacityUnits = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|State")
	float InputFillPercent01 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|State")
	float BatteryPercent01 = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|State")
	float BatteryCurrent = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|State")
	float BatteryCapacity = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|State")
	float PowerUsageWatts = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|State")
	float PowerSupplyWatts = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|State")
	float NetPowerWatts = -1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|State")
	float CraftProgress01 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|State")
	float CraftRemainingSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|State")
	int32 RecipeIndex = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|State")
	int32 RecipeCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|State")
	bool bRecipeAny = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|State")
	FText ActiveRecipeName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|State")
	TArray<FMachineUIRecipeOption> RecipeOptions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|State")
	TArray<FMachineUIUpgradeSlot> UpgradeSlots;
};
