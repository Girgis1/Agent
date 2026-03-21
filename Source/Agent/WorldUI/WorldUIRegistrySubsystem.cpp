// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldUIRegistrySubsystem.h"
#include "WorldUIHostComponent.h"

void UWorldUIRegistrySubsystem::RegisterHost(UWorldUIHostComponent* HostComponent)
{
	if (!HostComponent)
	{
		return;
	}

	RemoveInvalidHosts();
	RegisteredHosts.AddUnique(HostComponent);
}

void UWorldUIRegistrySubsystem::UnregisterHost(UWorldUIHostComponent* HostComponent)
{
	if (!HostComponent)
	{
		return;
	}

	RegisteredHosts.RemoveSingleSwap(HostComponent);
	RemoveInvalidHosts();
}

void UWorldUIRegistrySubsystem::GetRegisteredHosts(TArray<UWorldUIHostComponent*>& OutHosts) const
{
	OutHosts.Reset();
	for (const TWeakObjectPtr<UWorldUIHostComponent>& HostPtr : RegisteredHosts)
	{
		if (UWorldUIHostComponent* HostComponent = HostPtr.Get())
		{
			OutHosts.Add(HostComponent);
		}
	}
}

void UWorldUIRegistrySubsystem::RemoveInvalidHosts()
{
	RegisteredHosts.RemoveAll([](const TWeakObjectPtr<UWorldUIHostComponent>& HostPtr)
	{
		return !HostPtr.IsValid();
	});
}
