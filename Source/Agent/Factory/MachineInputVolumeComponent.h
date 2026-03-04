// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factory/FactoryVolumeComponentBase.h"
#include "MachineInputVolumeComponent.generated.h"

UCLASS(ClassGroup=(Factory), meta=(BlueprintSpawnableComponent))
class UMachineInputVolumeComponent : public UFactoryVolumeComponentBase
{
	GENERATED_BODY()

public:
	UMachineInputVolumeComponent();

	UFUNCTION(BlueprintPure, Category="Factory|Machine")
	float GetBufferedUnits(FName ResourceId) const;

	UFUNCTION(BlueprintPure, Category="Factory|Machine")
	bool HasResourceQuantityUnits(FName ResourceId, int32 QuantityUnits) const;

	bool HasResourceQuantityScaled(FName ResourceId, int32 QuantityScaled) const;
	bool ConsumeResourceQuantityScaled(FName ResourceId, int32 QuantityScaled);

	UFUNCTION(BlueprintCallable, Category="Factory|Machine")
	bool ConsumeResourceQuantityUnits(FName ResourceId, int32 QuantityUnits);

protected:
	virtual bool TryProcessOverlappingActor(AActor* OverlappingActor) override;
	virtual int32 GetCurrentStoredQuantityScaled() const override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Machine")
	TMap<FName, int32> InputBufferScaled;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Machine")
	int32 TotalBufferedQuantityScaled = 0;
};
