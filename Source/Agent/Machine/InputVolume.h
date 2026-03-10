// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "InputVolume.generated.h"

class UMachineComponent;

UCLASS(ClassGroup=(Machine), meta=(BlueprintSpawnableComponent))
class UInputVolume : public UBoxComponent
{
	GENERATED_BODY()

public:
	UInputVolume();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category="Machine|Input")
	void SetMachineComponent(UMachineComponent* InMachineComponent);

protected:
	bool ResolveMachineComponent();
	bool TryConsumeOverlappingActor(AActor* OverlappingActor);

	float TimeUntilNextIntake = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMachineComponent> MachineComponent = nullptr;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Input")
	bool bEnabled = true;

	/** When false, this volume will not auto-discover a machine component from its owner. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Input")
	bool bAutoResolveMachineComponent = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Input", meta=(ClampMin="0.0", UIMin="0.0"))
	float IntakeInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Input")
	bool bDestroyAcceptedPayloads = true;
};
