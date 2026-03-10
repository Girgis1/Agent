// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BlackHoleMachineActor.generated.h"

class UArrowComponent;
class UBlackHoleOutputSinkComponent;
class UBoxComponent;
class UOutputVolume;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class ABlackHoleMachineActor : public AActor
{
	GENERATED_BODY()

public:
	ABlackHoleMachineActor();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BlackHole|Machine")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BlackHole|Machine")
	TObjectPtr<UBoxComponent> SupportCollision = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BlackHole|Machine")
	TObjectPtr<UStaticMeshComponent> MachineMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BlackHole|Machine")
	TObjectPtr<UOutputVolume> OutputVolume = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BlackHole|Machine")
	TObjectPtr<UArrowComponent> OutputArrow = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BlackHole|Machine")
	TObjectPtr<UBlackHoleOutputSinkComponent> OutputSink = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Link")
	FName LinkId = TEXT("BlackHole_Default");
};
