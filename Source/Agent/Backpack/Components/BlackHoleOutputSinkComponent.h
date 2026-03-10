// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BlackHoleOutputSinkComponent.generated.h"

class AActor;
class UOutputVolume;

UCLASS(ClassGroup=(BlackHole), meta=(BlueprintSpawnableComponent))
class UBlackHoleOutputSinkComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBlackHoleOutputSinkComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category="BlackHole|Sink")
	void SetOutputVolume(UOutputVolume* InOutputVolume);

	UFUNCTION(BlueprintPure, Category="BlackHole|Sink")
	UOutputVolume* GetOutputVolume() const { return OutputVolume; }

	UFUNCTION(BlueprintCallable, Category="BlackHole|Sink")
	int32 QueueResourceScaled(FName ResourceId, int32 QuantityScaled, TSubclassOf<AActor> OutputActorClassOverride);

	// C++ convenience overload that uses no output class override.
	int32 QueueResourceScaled(FName ResourceId, int32 QuantityScaled);

	UFUNCTION(BlueprintPure, Category="BlackHole|Sink")
	bool IsSinkReady() const;

	UFUNCTION(BlueprintCallable, Category="BlackHole|Sink")
	void RefreshSinkRegistration();

protected:
	bool ResolveOutputVolume();
	void RegisterWithEndpointRegistry();
	void UnregisterFromEndpointRegistry();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Link")
	FName LinkId = TEXT("BlackHole_Default");

protected:
	UPROPERTY(Transient)
	TObjectPtr<UOutputVolume> OutputVolume = nullptr;

	bool bIsRegistered = false;
};
