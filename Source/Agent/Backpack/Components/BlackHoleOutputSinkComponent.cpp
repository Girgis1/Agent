// Copyright Epic Games, Inc. All Rights Reserved.

#include "Backpack/Components/BlackHoleOutputSinkComponent.h"
#include "Backpack/BlackHoleEndpointRegistrySubsystem.h"
#include "Engine/World.h"
#include "Machine/OutputVolume.h"

UBlackHoleOutputSinkComponent::UBlackHoleOutputSinkComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UBlackHoleOutputSinkComponent::BeginPlay()
{
	Super::BeginPlay();

	ResolveOutputVolume();
	RegisterWithEndpointRegistry();
}

void UBlackHoleOutputSinkComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromEndpointRegistry();
	Super::EndPlay(EndPlayReason);
}

void UBlackHoleOutputSinkComponent::SetOutputVolume(UOutputVolume* InOutputVolume)
{
	OutputVolume = InOutputVolume;
}

int32 UBlackHoleOutputSinkComponent::QueueResourceScaled(FName ResourceId, int32 QuantityScaled, TSubclassOf<AActor> OutputActorClassOverride)
{
	if (!ResolveOutputVolume() || ResourceId.IsNone() || QuantityScaled <= 0)
	{
		return 0;
	}

	return OutputVolume->QueueResourceScaled(ResourceId, QuantityScaled, OutputActorClassOverride);
}

int32 UBlackHoleOutputSinkComponent::QueueResourceScaled(FName ResourceId, int32 QuantityScaled)
{
	return QueueResourceScaled(ResourceId, QuantityScaled, nullptr);
}

bool UBlackHoleOutputSinkComponent::IsSinkReady() const
{
	return IsValid(OutputVolume) && !LinkId.IsNone();
}

void UBlackHoleOutputSinkComponent::RefreshSinkRegistration()
{
	UnregisterFromEndpointRegistry();
	RegisterWithEndpointRegistry();
}

bool UBlackHoleOutputSinkComponent::ResolveOutputVolume()
{
	if (OutputVolume)
	{
		return true;
	}

	if (const AActor* OwnerActor = GetOwner())
	{
		OutputVolume = OwnerActor->FindComponentByClass<UOutputVolume>();
	}

	return OutputVolume != nullptr;
}

void UBlackHoleOutputSinkComponent::RegisterWithEndpointRegistry()
{
	if (bIsRegistered || LinkId.IsNone())
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (UBlackHoleEndpointRegistrySubsystem* EndpointRegistry = World->GetSubsystem<UBlackHoleEndpointRegistrySubsystem>())
		{
			EndpointRegistry->RegisterSink(this, LinkId);
			bIsRegistered = true;
		}
	}
}

void UBlackHoleOutputSinkComponent::UnregisterFromEndpointRegistry()
{
	if (!bIsRegistered)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		if (UBlackHoleEndpointRegistrySubsystem* EndpointRegistry = World->GetSubsystem<UBlackHoleEndpointRegistrySubsystem>())
		{
			EndpointRegistry->UnregisterSink(this, LinkId);
		}
	}

	bIsRegistered = false;
}

