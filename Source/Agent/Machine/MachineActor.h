// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MachineActor.generated.h"

class UArrowComponent;
class UBoxComponent;
class UInputVolume;
class UMachineComponent;
class UOutputVolume;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class AMachineActor : public AActor
{
	GENERATED_BODY()

public:
	AMachineActor();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Machine")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Machine")
	TObjectPtr<UBoxComponent> SupportCollision = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Machine")
	TObjectPtr<UStaticMeshComponent> MachineMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Machine")
	TObjectPtr<UInputVolume> InputVolume = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Machine")
	TObjectPtr<UOutputVolume> OutputVolume = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Machine")
	TObjectPtr<UArrowComponent> OutputArrow = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Machine")
	TObjectPtr<UMachineComponent> MachineComponent = nullptr;
};
