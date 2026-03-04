// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "FactoryVolumeComponentBase.generated.h"

UCLASS(Abstract, ClassGroup=(Factory), meta=(BlueprintSpawnableComponent))
class UFactoryVolumeComponentBase : public UBoxComponent
{
	GENERATED_BODY()

public:
	UFactoryVolumeComponentBase();

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category="Factory|Volume")
	int32 GetMaxQuantityScaled() const;

	UFUNCTION(BlueprintPure, Category="Factory|Volume")
	int32 GetCurrentQuantityScaled() const;

	UFUNCTION(BlueprintPure, Category="Factory|Volume")
	bool IsResourceBlocked(FName ResourceId) const;

protected:
	virtual bool TryProcessOverlappingActor(AActor* OverlappingActor);
	virtual int32 GetCurrentStoredQuantityScaled() const;

	bool HasCapacityForAdditionalQuantity(int32 AdditionalQuantityScaled) const;

	float TimeUntilNextProcess = 0.0f;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Volume")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Volume", meta=(ClampMin="0"))
	int32 MaxQuantityUnits = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Volume")
	TArray<FName> BlockedResourceIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Volume", meta=(ClampMin="0.0", UIMin="0.0"))
	float ProcessingInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Volume")
	FName DebugName = NAME_None;
};
