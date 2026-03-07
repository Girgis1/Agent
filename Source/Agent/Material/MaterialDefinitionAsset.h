// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MaterialDefinitionAsset.generated.h"

class AActor;
class UTexture2D;

UCLASS(BlueprintType)
class UMaterialDefinitionAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Factory|Material")
	FName GetResolvedResourceId() const;

	UFUNCTION(BlueprintPure, Category="Factory|Material")
	float GetMassPerUnitKg() const { return FMath::Max(0.0f, MassPerUnitKg); }

	UFUNCTION(BlueprintPure, Category="Factory|Material")
	TSubclassOf<AActor> ResolveOutputActorClass() const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Material")
	FName ResourceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Material")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Material", meta=(ClampMin="0.0", UIMin="0.0"))
	float MassPerUnitKg = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Material")
	FLinearColor DebugColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Material")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Material|Output")
	TSoftClassPtr<AActor> OutputActorClass;
};

