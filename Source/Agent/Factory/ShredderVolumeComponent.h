// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factory/FactoryVolumeComponentBase.h"
#include "Factory/ResourceTypes.h"
#include "ShredderVolumeComponent.generated.h"

class UMachineOutputVolumeComponent;
class UPrimitiveComponent;
class UResourceComponent;

UCLASS(ClassGroup=(Factory), meta=(BlueprintSpawnableComponent))
class UShredderVolumeComponent : public UFactoryVolumeComponentBase
{
	GENERATED_BODY()

public:
	UShredderVolumeComponent();

	UFUNCTION(BlueprintPure, Category="Factory|Shredder")
	float GetBufferedUnits(FName ResourceId) const;

protected:
	virtual bool TryProcessOverlappingActor(AActor* OverlappingActor) override;
	virtual int32 GetCurrentStoredQuantityScaled() const override;

	bool TryBuildRecoveredResources(const UResourceComponent* ResourceData, const UPrimitiveComponent* SourcePrimitive, TArray<FResourceAmount>& OutRecoveredResources) const;
	void CommitRecoveredResources(const TArray<FResourceAmount>& RecoveredResources);
	void FlushBufferedResourcesToOutput();
	UMachineOutputVolumeComponent* FindOutputVolume() const;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Shredder", meta=(ClampMin="0.0", UIMin="0.0"))
	float BaseRecoveryScalar = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Shredder")
	bool bDestroyWithoutResourceComponent = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Shredder")
	TMap<FName, int32> InternalResourceBufferScaled;
};
