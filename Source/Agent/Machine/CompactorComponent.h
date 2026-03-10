// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CompactorComponent.generated.h"

class AActor;
class UMachineComponent;
class UOutputVolume;

UCLASS(ClassGroup=(Machine), meta=(BlueprintSpawnableComponent))
class UCompactorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCompactorComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category="Machine|Compactor")
	void SetMachineComponent(UMachineComponent* InMachineComponent);

	UFUNCTION(BlueprintCallable, Category="Machine|Compactor")
	void SetOutputVolume(UOutputVolume* InOutputVolume);

	UFUNCTION(BlueprintPure, Category="Machine|Compactor")
	float GetCycleProgress01() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Compactor")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Compactor", meta=(ClampMin="0.01", UIMin="0.01"))
	float CycleTimeSeconds = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Compactor", meta=(ClampMin="0.0", UIMin="0.0"))
	float CompactionSpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Compactor", meta=(ClampMin="0.01", UIMin="0.01"))
	float MinOutputUnits = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Compactor", meta=(ClampMin="0.01", UIMin="0.01"))
	float MaxOutputUnits = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Compactor", meta=(ClampMin="0.05", UIMin="0.05"))
	float MinOutputVisualScale = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Compactor", meta=(ClampMin="0.05", UIMin="0.05"))
	float MaxOutputVisualScale = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Compactor")
	TSubclassOf<AActor> MixedMaterialActorClass;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Machine|Compactor")
	FText RuntimeStatus;

protected:
	bool ResolveDependencies();
	float ResolveGlobalFactorySpeed() const;
	float ResolveEffectiveCycleSpeed() const;
	bool TryRunCompactionCycle();
	bool BuildCompactionConsumptionMap(const TMap<FName, int32>& StoredResourcesScaled, TMap<FName, int32>& OutConsumptionScaled, int32& OutConsumedTotalScaled) const;
	void SetRuntimeStatus(const TCHAR* Message);

	UPROPERTY(Transient)
	TObjectPtr<UMachineComponent> MachineComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UOutputVolume> OutputVolume = nullptr;

	UPROPERTY(Transient)
	float CurrentCycleProgressSeconds = 0.0f;
};

