// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShredderMachine.generated.h"

class UBoxComponent;
class UMachineOutputVolumeComponent;
class USceneComponent;
class UShredderBottomVolumeComponent;
class UShredderTopVolumeComponent;
class UStaticMeshComponent;

UCLASS()
class AShredderMachine : public AActor
{
	GENERATED_BODY()

public:
	AShredderMachine();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Machine")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Machine")
	TObjectPtr<UBoxComponent> SupportCollision = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Machine")
	TObjectPtr<UStaticMeshComponent> MachineMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Machine")
	TObjectPtr<UShredderTopVolumeComponent> IntakeVolume = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Machine")
	TObjectPtr<UShredderBottomVolumeComponent> BottomIntakeVolume = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Machine")
	TObjectPtr<UMachineOutputVolumeComponent> OutputVolume = nullptr;
};
