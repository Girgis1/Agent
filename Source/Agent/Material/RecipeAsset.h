// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RecipeAsset.generated.h"

class AActor;
class UMaterialDefinitionAsset;

USTRUCT(BlueprintType)
struct FRecipeMaterialInputEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Recipe|Inputs")
	TObjectPtr<UMaterialDefinitionAsset> Material = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Recipe|Inputs", meta=(DisplayName="Units", ClampMin="1", UIMin="1"))
	int32 QuantityUnits = 1;

	bool IsDefined() const;
	FName GetResolvedMaterialId() const;
	int32 GetQuantityScaled() const;
};

USTRUCT(BlueprintType)
struct FRecipeBlueprintOutputEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Recipe|Outputs", meta=(DisplayName="Output Blueprint Class"))
	TSoftClassPtr<AActor> OutputActorClass;

	bool IsDefined() const;
	TSubclassOf<AActor> ResolveOutputActorClass() const;
};

UCLASS(BlueprintType)
class URecipeAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Factory|Recipe")
	bool IsRecipeConfigured() const;

	UFUNCTION(BlueprintPure, Category="Factory|Recipe")
	void BuildInputRequirementsScaled(TMap<FName, int32>& OutRequirementsScaled) const;

	UFUNCTION(BlueprintPure, Category="Factory|Recipe")
	void BuildResolvedOutputs(TArray<TSubclassOf<AActor>>& OutOutputActorClasses) const;

	UFUNCTION(BlueprintPure, Category="Factory|Recipe")
	float GetResolvedCraftTimeSeconds() const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Recipe")
	FName RecipeId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Recipe")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Recipe", meta=(ClampMin="0.01", UIMin="0.01"))
	float CraftTimeSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Recipe")
	TArray<FRecipeMaterialInputEntry> Inputs;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Recipe")
	TArray<FRecipeBlueprintOutputEntry> Outputs;
};
