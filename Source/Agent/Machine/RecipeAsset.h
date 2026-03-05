// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RecipeAsset.generated.h"

class UResourceDefinitionAsset;

USTRUCT(BlueprintType)
struct FRecipeResourceEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Machine|Recipe")
	TObjectPtr<UResourceDefinitionAsset> Resource = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Machine|Recipe", meta=(DisplayName="Units", ClampMin="1", UIMin="1"))
	int32 QuantityUnits = 1;

	bool IsDefined() const;
	FName GetResolvedResourceId() const;
	int32 GetQuantityScaled() const;
};

USTRUCT(BlueprintType)
struct FRecipeGroupInputEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Machine|Recipe|Groups")
	FName GroupId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Machine|Recipe|Groups", meta=(DisplayName="Equivalent Units", ClampMin="1", UIMin="1"))
	int32 RequiredEquivalentUnits = 1;

	bool IsDefined() const;
	int32 GetRequiredEquivalentScaled() const;
};

UCLASS(BlueprintType)
class URecipeAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Machine|Recipe")
	bool IsRecipeConfigured() const;

	UFUNCTION(BlueprintPure, Category="Machine|Recipe")
	void BuildInputRequirementsScaled(TMap<FName, int32>& OutRequirementsScaled) const;

	UFUNCTION(BlueprintPure, Category="Machine|Recipe")
	void BuildInputGroupRequirementsScaled(TMap<FName, int32>& OutGroupRequirementsScaled) const;

	UFUNCTION(BlueprintPure, Category="Machine|Recipe")
	void BuildOutputAmountsScaled(TMap<FName, int32>& OutOutputScaled) const;

	UFUNCTION(BlueprintPure, Category="Machine|Recipe")
	float GetResolvedCraftTimeSeconds() const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Machine|Recipe")
	FName RecipeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Machine|Recipe")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Machine|Recipe")
	FName AllowedMachineTag = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Machine|Recipe", meta=(ClampMin="0.01", UIMin="0.01"))
	float CraftTimeSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Machine|Recipe")
	TArray<FRecipeResourceEntry> Inputs;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Machine|Recipe")
	TArray<FRecipeGroupInputEntry> InputGroups;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Machine|Recipe")
	TArray<FRecipeResourceEntry> Outputs;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Machine|Recipe|Byproduct")
	bool bConvertOtherInputsToScrap = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Machine|Recipe|Byproduct", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bConvertOtherInputsToScrap"))
	float ScrapRecoveryScalar = 1.0f;
};
