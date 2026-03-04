// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "ConveyorBeltStraight.generated.h"

class UArrowComponent;
class UBoxComponent;
class AFactoryWorldConfig;
class UConveyorSurfaceVelocityComponent;
class UPhysicalMaterial;
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

	UFUNCTION(BlueprintPure, Category="Conveyor")
	FVector GetSurfaceVelocity() const;

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
	void UpdateSupportPhysicalMaterial();
	bool IsWorldPointSupportedByBelt(const FVector& WorldPoint) const;
	bool IsPayloadValid(const UPrimitiveComponent* PrimitiveComponent) const;
	float GetResolvedBeltSpeed() const;
	UConveyorSurfaceVelocityComponent* GetSurfaceVelocityConsumer(AActor* OtherActor) const;
	AFactoryWorldConfig* GetFactoryWorldConfig() const;
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
	float BeltSpeed = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Conveyor")
	bool bUseMasterSpeedSettings = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Conveyor|Feel")
	float BeltSurfaceFriction = 1.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Conveyor|Feel")
	float BeltSurfaceRestitution = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Conveyor")
	FVector SupportBoxExtent = FVector(50.0f, 50.0f, 10.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Conveyor")
	FVector PayloadDriveExtent = FVector(42.0f, 42.0f, 20.0f);

protected:
	UPROPERTY(Transient)
	TObjectPtr<UPhysicalMaterial> RuntimeSupportPhysicalMaterial = nullptr;

	mutable TWeakObjectPtr<AFactoryWorldConfig> CachedWorldConfig;

	TSet<TWeakObjectPtr<UPrimitiveComponent>> ActivePayloads;
	TSet<TWeakObjectPtr<UConveyorSurfaceVelocityComponent>> ActiveSurfaceVelocityConsumers;
};
