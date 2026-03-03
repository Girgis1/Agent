// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ConveyorPlacementPreview.generated.h"

class UArrowComponent;
class UStaticMeshComponent;
class USceneComponent;

UCLASS()
class AConveyorPlacementPreview : public AActor
{
	GENERATED_BODY()

public:
	AConveyorPlacementPreview();

	UFUNCTION(BlueprintCallable, Category="Conveyor")
	void SetPlacementState(bool bIsValid);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Conveyor")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Conveyor")
	TObjectPtr<UStaticMeshComponent> PreviewMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Conveyor")
	TObjectPtr<UArrowComponent> DirectionArrow = nullptr;

protected:
	bool bPlacementValid = true;
};
