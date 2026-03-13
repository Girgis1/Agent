// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ConveyorPlacementPreview.generated.h"

class UArrowComponent;
class UChildActorComponent;
class UMaterialInterface;
class UStaticMeshComponent;
class USceneComponent;
class AActor;

UCLASS()
class AConveyorPlacementPreview : public AActor
{
	GENERATED_BODY()

public:
	AConveyorPlacementPreview();

	UFUNCTION(BlueprintCallable, Category="Conveyor")
	void SetPlacementState(bool bIsValid);

	UFUNCTION(BlueprintCallable, Category="Conveyor")
	void SetPreviewActorClass(TSubclassOf<AActor> InPreviewActorClass);

	UFUNCTION(BlueprintCallable, Category="Conveyor")
	void SetGhostMaterials(UMaterialInterface* InValidPlacementMaterial, UMaterialInterface* InInvalidPlacementMaterial);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Conveyor")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Conveyor")
	TObjectPtr<UStaticMeshComponent> PreviewMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Conveyor")
	TObjectPtr<UChildActorComponent> PreviewBuildable = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Conveyor")
	TObjectPtr<UArrowComponent> DirectionArrow = nullptr;

protected:
	void RefreshPreviewVisuals();
	void ConfigurePreviewBuildableActor(AActor* PreviewActor) const;
	void ApplyGhostMaterialToActor(AActor* PreviewActor, UMaterialInterface* GhostMaterial) const;

	UPROPERTY(Transient)
	TSubclassOf<AActor> PreviewActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Conveyor", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UMaterialInterface> ValidPlacementMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Conveyor", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UMaterialInterface> InvalidPlacementMaterial = nullptr;

	bool bPlacementValid = true;
};
