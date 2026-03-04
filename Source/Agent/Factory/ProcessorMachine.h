// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProcessorMachine.generated.h"

class UBoxComponent;
class UMachineInputVolumeComponent;
class UMachineOutputVolumeComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class AProcessorMachine : public AActor
{
	GENERATED_BODY()

public:
	AProcessorMachine();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Machine")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Machine")
	TObjectPtr<UBoxComponent> SupportCollision = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Machine")
	TObjectPtr<UStaticMeshComponent> MachineMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Machine")
	TObjectPtr<UMachineInputVolumeComponent> InputVolume = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Machine")
	TObjectPtr<UMachineOutputVolumeComponent> OutputVolume = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Machine")
	bool bAutoProcess = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Machine", meta=(ClampMin="0.0", UIMin="0.0"))
	float ProcessInterval = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Machine")
	FName InputResourceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Machine")
	FName OutputResourceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Machine", meta=(ClampMin="1"))
	int32 InputQuantityUnits = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Machine", meta=(ClampMin="1"))
	int32 OutputQuantityUnits = 1;

protected:
	bool TryProcessResources();
	FName ResolveInputResourceId() const;

	float ProcessElapsed = 0.0f;
};
