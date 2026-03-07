// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "OutputVolume.generated.h"

class AFactoryPayloadActor;
class AActor;
class UArrowComponent;

UCLASS(ClassGroup=(Machine), meta=(BlueprintSpawnableComponent))
class UOutputVolume : public UBoxComponent
{
	GENERATED_BODY()

public:
	UOutputVolume();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category="Machine|Output")
	void SetOutputArrow(UArrowComponent* InOutputArrow);

	int32 QueueResourceScaled(FName ResourceId, int32 QuantityScaled, TSubclassOf<AActor> OutputActorClassOverride);
	int32 QueueResourceScaled(FName ResourceId, int32 QuantityScaled);

	UFUNCTION(BlueprintCallable, Category="Machine|Output")
	void QueueResourceUnits(FName ResourceId, int32 QuantityUnits);

	UFUNCTION(BlueprintPure, Category="Machine|Output")
	float GetPendingUnits(FName ResourceId) const;

protected:
	bool ResolveOutputArrow();
	FVector GetSpawnLocation() const;
	FRotator GetSpawnRotation() const;
	TSubclassOf<AActor> ResolveSpawnClassForResource(FName ResourceId);
	void RebuildResourceOutputClassLookup();
	bool TrySpawnOnePayload();

	float TimeUntilNextSpawn = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UArrowComponent> OutputArrow = nullptr;

	UPROPERTY(Transient)
	TMap<FName, TSubclassOf<AActor>> ResourceOutputActorClassById;

	UPROPERTY(Transient)
	TMap<FName, TSubclassOf<AActor>> PendingOutputActorClassOverrideById;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Output")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Output")
	bool bAutoOutput = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Output", meta=(ClampMin="0.0", UIMin="0.0"))
	float SpawnInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Output", meta=(ClampMin="1", UIMin="1"))
	int32 OutputChunkUnits = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Output")
	TSubclassOf<AFactoryPayloadActor> PayloadActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Output")
	FVector LocalSpawnOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Output")
	float SpawnImpulse = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Output")
	FName OutputArrowComponentName = TEXT("OutputArrow");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Machine|Output")
	TMap<FName, int32> PendingOutputScaled;
};
