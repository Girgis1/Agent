// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ResourceDefinitionAsset.generated.h"

class AActor;
class UTexture2D;

UCLASS(BlueprintType)
class UResourceDefinitionAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Factory|Resource")
	FName GetResolvedResourceId() const;

	UFUNCTION(BlueprintPure, Category="Factory|Resource")
	float GetMassPerUnitKg() const { return FMath::Max(0.0f, MassPerUnitKg); }

	UFUNCTION(BlueprintPure, Category="Factory|Resource")
	TSubclassOf<AActor> ResolveOutputActorClass() const;

	UFUNCTION(BlueprintPure, Category="Factory|Resource|Processing")
	FName GetRefiningGroup() const { return RefiningGroup; }

	UFUNCTION(BlueprintPure, Category="Factory|Resource|Processing")
	float GetRefiningEquivalentPerUnit() const { return FMath::Max(0.0f, RefiningEquivalentPerUnit); }

	UFUNCTION(BlueprintPure, Category="Factory|Resource|Processing")
	float GetScrapUnitsPerUnit() const { return FMath::Max(0.0f, ScrapUnitsPerUnit); }

	UFUNCTION(BlueprintPure, Category="Factory|Resource|Processing")
	FName GetResolvedScrapResourceId() const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Resource")
	FName ResourceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Resource")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Resource", meta=(ClampMin="0.0", UIMin="0.0"))
	float MassPerUnitKg = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Resource")
	FLinearColor DebugColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Resource")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Resource|Output")
	TSoftClassPtr<AActor> OutputActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Resource|Processing")
	FName RefiningGroup = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Resource|Processing", meta=(ClampMin="0.0", UIMin="0.0"))
	float RefiningEquivalentPerUnit = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Resource|Processing")
	TObjectPtr<UResourceDefinitionAsset> ScrapResource = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Resource|Processing", meta=(ClampMin="0.0", UIMin="0.0"))
	float ScrapUnitsPerUnit = 1.0f;
};
