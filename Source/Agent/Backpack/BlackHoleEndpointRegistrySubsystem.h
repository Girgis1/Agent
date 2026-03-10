// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BlackHoleEndpointRegistrySubsystem.generated.h"

class UBlackHoleOutputSinkComponent;

UCLASS()
class UBlackHoleEndpointRegistrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category="BlackHole|Link")
	void RegisterSink(UBlackHoleOutputSinkComponent* SinkComponent, FName LinkId);

	UFUNCTION(BlueprintCallable, Category="BlackHole|Link")
	void UnregisterSink(UBlackHoleOutputSinkComponent* SinkComponent, FName LinkId);

	UFUNCTION(BlueprintPure, Category="BlackHole|Link")
	UBlackHoleOutputSinkComponent* ResolveSink(FName LinkId) const;

protected:
	void CleanupInvalidEntries() const;

protected:
	UPROPERTY(Transient)
	mutable TMap<FName, TWeakObjectPtr<UBlackHoleOutputSinkComponent>> SinkByLinkId;
};

