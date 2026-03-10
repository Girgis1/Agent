// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "RecipeAsset.generated.h"

class AActor;
class UMaterialDefinitionAsset;

UENUM(BlueprintType)
enum class ERecipeInputType : uint8
{
	SpecificMaterial UMETA(DisplayName="Specific Material"),
	AnyMaterial UMETA(DisplayName="Any Material"),
	ItemBlueprint UMETA(DisplayName="Item Blueprint")
};

USTRUCT(BlueprintType)
struct FRecipeMaterialInputEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Recipe|Inputs")
	ERecipeInputType InputType = ERecipeInputType::SpecificMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Recipe|Inputs")
	TObjectPtr<UMaterialDefinitionAsset> Material = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Recipe|Inputs", meta=(DisplayName="Item Blueprint Class", EditCondition="InputType==ERecipeInputType::ItemBlueprint", EditConditionHides))
	TSoftClassPtr<AActor> ItemActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Recipe|Inputs", meta=(DisplayName="Units", ClampMin="1", UIMin="1"))
	int32 QuantityUnits = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Recipe|Inputs|Any Material", meta=(EditCondition="InputType==ERecipeInputType::AnyMaterial", EditConditionHides))
	bool bUseWhitelist = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Recipe|Inputs|Any Material", meta=(EditCondition="InputType==ERecipeInputType::AnyMaterial&&bUseWhitelist", EditConditionHides))
	TArray<TObjectPtr<UMaterialDefinitionAsset>> WhitelistMaterials;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Recipe|Inputs|Any Material", meta=(EditCondition="InputType==ERecipeInputType::AnyMaterial", EditConditionHides))
	bool bUseBlacklist = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Recipe|Inputs|Any Material", meta=(EditCondition="InputType==ERecipeInputType::AnyMaterial&&bUseBlacklist", EditConditionHides))
	TArray<TObjectPtr<UMaterialDefinitionAsset>> BlacklistMaterials;

	bool IsDefined() const;
	bool IsSpecificMaterialInput() const;
	bool IsAnyMaterialInput() const;
	bool IsItemBlueprintInput() const;
	FName GetResolvedMaterialId() const;
	TSubclassOf<AActor> ResolveItemActorClass() const;
	int32 GetQuantityScaled() const;
	int32 GetRequiredItemCount() const;
	void BuildResolvedWhitelist(TSet<FName>& OutWhitelistMaterialIds) const;
	void BuildResolvedBlacklist(TSet<FName>& OutBlacklistMaterialIds) const;
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
