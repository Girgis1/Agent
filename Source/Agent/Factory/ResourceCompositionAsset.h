// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Factory/ResourceTypes.h"
#include "ResourceCompositionAsset.generated.h"

UCLASS(BlueprintType)
class UResourceCompositionAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Factory|Resource")
	bool HasDefinedResources() const;

	UFUNCTION(BlueprintPure, Category="Factory|Resource")
	float GetTotalMassFraction() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Resource")
	TArray<FResourceCompositionEntry> Entries;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Resource", meta=(ClampMin="0.0", UIMin="0.0"))
	float DefaultRecoveryScalar = 1.0f;
};
