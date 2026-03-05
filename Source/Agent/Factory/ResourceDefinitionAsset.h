// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ResourceDefinitionAsset.generated.h"

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
};
