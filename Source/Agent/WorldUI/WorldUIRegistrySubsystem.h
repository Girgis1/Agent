// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WorldUIRegistrySubsystem.generated.h"

class UWorldUIHostComponent;

UCLASS()
class UWorldUIRegistrySubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void RegisterHost(UWorldUIHostComponent* HostComponent);
	void UnregisterHost(UWorldUIHostComponent* HostComponent);
	void GetRegisteredHosts(TArray<UWorldUIHostComponent*>& OutHosts) const;

private:
	void RemoveInvalidHosts();

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<UWorldUIHostComponent>> RegisteredHosts;
};
