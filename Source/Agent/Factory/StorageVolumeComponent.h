// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factory/FactoryVolumeComponentBase.h"
#include "StorageVolumeComponent.generated.h"

class AFactoryPayloadActor;

UCLASS(ClassGroup=(Factory), meta=(BlueprintSpawnableComponent))
class UStorageVolumeComponent : public UFactoryVolumeComponentBase
{
	GENERATED_BODY()

public:
	UStorageVolumeComponent();

	UFUNCTION(BlueprintPure, Category="Factory|Storage")
	int32 GetStoredItemCount(FName ResourceId) const;

	UFUNCTION(BlueprintPure, Category="Factory|Storage")
	float GetStoredUnits(FName ResourceId) const;

	UFUNCTION(BlueprintPure, Category="Factory|Storage")
	float GetTotalStoredUnits() const;

	UFUNCTION(BlueprintPure, Category="Factory|Storage")
	int32 GetTotalStoredItemCount() const { return TotalStoredItemCount; }

protected:
	virtual bool TryProcessOverlappingActor(AActor* OverlappingActor) override;
	virtual int32 GetCurrentStoredQuantityScaled() const override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Storage")
	TMap<FName, int32> StoredResourceQuantitiesScaled;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Storage")
	TMap<FName, int32> StoredItemCounts;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Storage")
	int32 TotalStoredQuantityScaled = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Storage")
	int32 TotalStoredItemCount = 0;
};
