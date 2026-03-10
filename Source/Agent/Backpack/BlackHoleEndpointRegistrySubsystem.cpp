// Copyright Epic Games, Inc. All Rights Reserved.

#include "Backpack/BlackHoleEndpointRegistrySubsystem.h"
#include "Backpack/Components/BlackHoleOutputSinkComponent.h"

void UBlackHoleEndpointRegistrySubsystem::RegisterSink(UBlackHoleOutputSinkComponent* SinkComponent, FName LinkId)
{
	if (!IsValid(SinkComponent) || LinkId.IsNone())
	{
		return;
	}

	CleanupInvalidEntries();
	SinkByLinkId.FindOrAdd(LinkId) = SinkComponent;
}

void UBlackHoleEndpointRegistrySubsystem::UnregisterSink(UBlackHoleOutputSinkComponent* SinkComponent, FName LinkId)
{
	if (LinkId.IsNone())
	{
		return;
	}

	CleanupInvalidEntries();

	if (const TWeakObjectPtr<UBlackHoleOutputSinkComponent>* ExistingSinkPtr = SinkByLinkId.Find(LinkId))
	{
		if (!ExistingSinkPtr->IsValid() || ExistingSinkPtr->Get() == SinkComponent)
		{
			SinkByLinkId.Remove(LinkId);
		}
	}
}

UBlackHoleOutputSinkComponent* UBlackHoleEndpointRegistrySubsystem::ResolveSink(FName LinkId) const
{
	if (LinkId.IsNone())
	{
		return nullptr;
	}

	CleanupInvalidEntries();

	if (const TWeakObjectPtr<UBlackHoleOutputSinkComponent>* FoundSinkPtr = SinkByLinkId.Find(LinkId))
	{
		return FoundSinkPtr->Get();
	}

	return nullptr;
}

void UBlackHoleEndpointRegistrySubsystem::CleanupInvalidEntries() const
{
	for (auto It = SinkByLinkId.CreateIterator(); It; ++It)
	{
		if (It.Key().IsNone() || !It.Value().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

