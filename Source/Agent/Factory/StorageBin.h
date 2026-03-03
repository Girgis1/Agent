// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StorageBin.generated.h"

class AFactoryPayloadActor;
class UArrowComponent;
class UBoxComponent;
class UPrimitiveComponent;
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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Storage")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Storage")
	TObjectPtr<UBoxComponent> SupportCollision = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Storage")
	TObjectPtr<UStaticMeshComponent> BinMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Storage")
	TObjectPtr<UBoxComponent> IntakeVolume = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Storage")
	TObjectPtr<UArrowComponent> FlowArrow = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Storage")
	FName AcceptedPayloadType = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Storage")
	int32 TotalStoredItemCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Storage")
	TMap<FName, int32> StoredItemCounts;

protected:
	UFUNCTION()
	void OnIntakeBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
};
