// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factory/FactoryVolumeComponentBase.h"
#include "MachineOutputVolumeComponent.generated.h"

class AFactoryPayloadActor;

UCLASS(ClassGroup=(Factory), meta=(BlueprintSpawnableComponent))
class UMachineOutputVolumeComponent : public UFactoryVolumeComponentBase
{
	GENERATED_BODY()

public:
	UMachineOutputVolumeComponent();

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category="Factory|Machine")
	void QueueResourceAmount(const FResourceAmount& ResourceAmount);

	UFUNCTION(BlueprintCallable, Category="Factory|Machine")
	void QueueWholeUnits(FName ResourceId, int32 QuantityUnits);

	int32 QueueResourceScaled(FName ResourceId, int32 QuantityScaled);

	UFUNCTION(BlueprintPure, Category="Factory|Machine")
	float GetBufferedUnits(FName ResourceId) const;

protected:
	virtual bool TryProcessOverlappingActor(AActor* OverlappingActor) override;
	virtual int32 GetCurrentStoredQuantityScaled() const override;

	bool CanSpawnPayload() const;
	bool TryEmitOnePayload();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Machine")
	TSubclassOf<AFactoryPayloadActor> PayloadActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Machine", meta=(ClampMin="1"))
	int32 OutputChunkSizeUnits = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Machine", meta=(ClampMin="0.0", UIMin="0.0"))
	float OutputImpulse = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Machine", meta=(ClampMin="1.0", UIMin="1.0"))
	float OutputClearanceRadius = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Machine")
	bool bAutoEject = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Machine")
	TMap<FName, int32> PendingOutputQuantitiesScaled;
};
