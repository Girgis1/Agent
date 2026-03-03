// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "ConveyorBeltStraight.generated.h"

class UArrowComponent;
class UBoxComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;
class USceneComponent;

UCLASS()
class AConveyorBeltStraight : public AActor
{
	GENERATED_BODY()

public:
	AConveyorBeltStraight();

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnPayloadBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnPayloadEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	void UpdateTickState();
	bool IsPayloadValid(const UPrimitiveComponent* PrimitiveComponent) const;
	void ApplyBeltDrive(UPrimitiveComponent* PrimitiveComponent, float DeltaSeconds) const;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Conveyor")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Conveyor")
	TObjectPtr<UBoxComponent> SupportCollision = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Conveyor")
	TObjectPtr<UStaticMeshComponent> ConveyorMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Conveyor")
	TObjectPtr<UBoxComponent> PayloadDriveVolume = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Conveyor")
	TObjectPtr<UArrowComponent> DirectionArrow = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Conveyor")
	float BeltSpeed = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Conveyor")
	float BeltAcceleration = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Conveyor")
	FVector SupportBoxExtent = FVector(50.0f, 50.0f, 10.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Conveyor")
	FVector PayloadDriveExtent = FVector(42.0f, 42.0f, 20.0f);

protected:
	TSet<TWeakObjectPtr<UPrimitiveComponent>> ActivePayloads;
};
