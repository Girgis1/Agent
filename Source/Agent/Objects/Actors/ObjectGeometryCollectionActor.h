// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ObjectGeometryCollectionActor.generated.h"

class UGeometryCollection;
class UGeometryCollectionComponent;
class UMaterialComponent;
class UObjectFractureComponent;
class UObjectHealthComponent;

UCLASS()
class AObjectGeometryCollectionActor : public AActor
{
	GENERATED_BODY()

public:
	AObjectGeometryCollectionActor();

	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintCallable, Category="Objects|GeometryCollection")
	void SetGeometryCollectionAsset(UGeometryCollection* NewGeometryCollectionAsset);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|GeometryCollection")
	TObjectPtr<UGeometryCollectionComponent> GeometryCollectionComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MaterialActor")
	TObjectPtr<UMaterialComponent> MaterialData = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|MaterialActor")
	TObjectPtr<UObjectHealthComponent> ObjectHealth = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|MaterialActor")
	TObjectPtr<UObjectFractureComponent> ObjectFracture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|GeometryCollection")
	TObjectPtr<UGeometryCollection> GeometryCollectionAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|GeometryCollection|Runtime")
	bool bCrumbleImmediatelyOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|GeometryCollection|Runtime")
	bool bDisableRemovalOnSleep = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|GeometryCollection|Runtime")
	bool bDisableRemovalOnBreak = true;

protected:
	virtual void BeginPlay() override;

	void ApplyConfiguredGeometryCollection();
	void ApplyRuntimeCollectionSettings();
	void HandleDeferredInitialCrumble();
};
