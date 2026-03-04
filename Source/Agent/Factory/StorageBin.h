// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StorageBin.generated.h"

class UArrowComponent;
class UBoxComponent;
class UStorageVolumeComponent;
class UStaticMeshComponent;
class USceneComponent;

UCLASS()
class AStorageBin : public AActor
{
	GENERATED_BODY()

public:
	AStorageBin();

	UFUNCTION(BlueprintPure, Category="Factory|Storage")
	int32 GetStoredItemCount(FName PayloadType) const;

	UFUNCTION(BlueprintPure, Category="Factory|Storage")
	float GetStoredUnits(FName PayloadType) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Storage")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Storage")
	TObjectPtr<UBoxComponent> SupportCollision = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Storage")
	TObjectPtr<UStaticMeshComponent> BinMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Storage")
	TObjectPtr<UStorageVolumeComponent> IntakeVolume = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Storage")
	TObjectPtr<UArrowComponent> FlowArrow = nullptr;
};
