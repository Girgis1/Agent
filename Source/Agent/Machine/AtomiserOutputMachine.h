// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AtomiserOutputMachine.generated.h"

class UArrowComponent;
class UAtomiserOutputVolume;
class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class AAtomiserOutputMachine : public AActor
{
	GENERATED_BODY()

public:
	AAtomiserOutputMachine();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Machine|AtomiserOutput")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Machine|AtomiserOutput")
	TObjectPtr<UBoxComponent> SupportCollision = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Machine|AtomiserOutput")
	TObjectPtr<UStaticMeshComponent> MachineMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Machine|AtomiserOutput")
	TObjectPtr<UAtomiserOutputVolume> OutputVolume = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Machine|AtomiserOutput")
	TObjectPtr<UArrowComponent> OutputArrow = nullptr;
};
