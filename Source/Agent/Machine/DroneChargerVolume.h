// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "DroneChargerVolume.generated.h"

class ADroneCompanion;
class UPrimitiveComponent;

UCLASS(ClassGroup=(Machine), meta=(BlueprintSpawnableComponent))
class UDroneChargerVolume : public UBoxComponent
{
	GENERATED_BODY()

public:
	UDroneChargerVolume();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	UFUNCTION()
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	void TrackDroneOverlap(ADroneCompanion* DroneCompanion, bool bIsEntering);

	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<ADroneCompanion>> OverlappingDrones;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|DroneCharger")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|DroneCharger", meta=(ClampMin="0.0", UIMin="0.0"))
	float ChargeRatePercentPerSecond = 20.0f;
};
