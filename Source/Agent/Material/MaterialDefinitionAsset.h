// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MaterialDefinitionAsset.generated.h"

class AActor;
class UMaterialInterface;
class UTexture2D;

UENUM(BlueprintType)
enum class EMaterialOutputForm : uint8
{
	Raw UMETA(DisplayName="Raw"),
	Pure UMETA(DisplayName="Pure")
};

UCLASS(BlueprintType)
class UMaterialDefinitionAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Factory|Material")
	FName GetResolvedMaterialId() const;

	UFUNCTION(BlueprintPure, Category="Factory|Material", meta=(DeprecatedFunction, DeprecationMessage="Use GetResolvedMaterialId."))
	FName GetResolvedResourceId() const { return GetResolvedMaterialId(); }

	UFUNCTION(BlueprintPure, Category="Factory|Material")
	FName GetResolvedColorId() const;

	UFUNCTION(BlueprintPure, Category="Factory|Material")
	float GetMassPerUnitKg() const { return FMath::Max(0.0f, MassPerUnitKg); }

	UFUNCTION(BlueprintPure, Category="Factory|Material")
	TSubclassOf<AActor> ResolveOutputActorClass(EMaterialOutputForm OutputForm = EMaterialOutputForm::Raw) const;

	UFUNCTION(BlueprintPure, Category="Factory|Material")
	FLinearColor GetResolvedVisualColor() const;

	UFUNCTION(BlueprintPure, Category="Factory|Material")
	UMaterialInterface* ResolveVisualMaterial() const;

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Material")
	FName MaterialId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Material")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Material", meta=(ClampMin="0.0", UIMin="0.0"))
	float MassPerUnitKg = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Material")
	FLinearColor DebugColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Material|Visual")
	FName ColorId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Material|Visual")
	FLinearColor VisualColor = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Material|Visual")
	TSoftObjectPtr<UMaterialInterface> VisualMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Material")
	TSoftObjectPtr<UTexture2D> Icon;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Material|Output")
	TSoftClassPtr<AActor> RawOutputActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Material|Output")
	TSoftClassPtr<AActor> PureOutputActorClass;
};

