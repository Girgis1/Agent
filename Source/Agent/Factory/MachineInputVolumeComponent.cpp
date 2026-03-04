// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/MachineInputVolumeComponent.h"
#include "Engine/Engine.h"
#include "Factory/FactoryPayloadActor.h"
#include "Factory/ResourceTypes.h"

namespace
{
	static FResourceAmount GetResolvedMachineInputAmount(const AFactoryPayloadActor* PayloadActor)
	{
		if (!PayloadActor)
		{
			return FResourceAmount{};
		}

		FResourceAmount Amount = PayloadActor->GetPayloadResource();
		if (!Amount.HasQuantity())
		{
			Amount.ResourceId = PayloadActor->GetPayloadType();
			Amount.SetWholeUnits(1);
		}

		return Amount;
	}
}

UMachineInputVolumeComponent::UMachineInputVolumeComponent()
{
	DebugName = TEXT("MachineInput");
}

float UMachineInputVolumeComponent::GetBufferedUnits(FName ResourceId) const
{
	if (ResourceId.IsNone())
	{
		return AgentResource::ScaledToUnits(TotalBufferedQuantityScaled);
	}

	if (const int32* FoundQuantity = InputBufferScaled.Find(ResourceId))
	{
		return AgentResource::ScaledToUnits(*FoundQuantity);
	}

	return 0.0f;
}

bool UMachineInputVolumeComponent::HasResourceQuantityUnits(FName ResourceId, int32 QuantityUnits) const
{
	return HasResourceQuantityScaled(ResourceId, AgentResource::WholeUnitsToScaled(QuantityUnits));
}

bool UMachineInputVolumeComponent::HasResourceQuantityScaled(FName ResourceId, int32 QuantityScaled) const
{
	if (ResourceId.IsNone() || QuantityScaled <= 0)
	{
		return false;
	}

	return InputBufferScaled.FindRef(ResourceId) >= QuantityScaled;
}

bool UMachineInputVolumeComponent::ConsumeResourceQuantityScaled(FName ResourceId, int32 QuantityScaled)
{
	if (!HasResourceQuantityScaled(ResourceId, QuantityScaled))
	{
		return false;
	}

	int32& StoredQuantity = InputBufferScaled.FindOrAdd(ResourceId);
	StoredQuantity = FMath::Max(0, StoredQuantity - QuantityScaled);
	if (StoredQuantity == 0)
	{
		InputBufferScaled.Remove(ResourceId);
	}

	TotalBufferedQuantityScaled = FMath::Max(0, TotalBufferedQuantityScaled - QuantityScaled);
	return true;
}

bool UMachineInputVolumeComponent::ConsumeResourceQuantityUnits(FName ResourceId, int32 QuantityUnits)
{
	return ConsumeResourceQuantityScaled(ResourceId, AgentResource::WholeUnitsToScaled(QuantityUnits));
}

bool UMachineInputVolumeComponent::TryProcessOverlappingActor(AActor* OverlappingActor)
{
	AFactoryPayloadActor* PayloadActor = Cast<AFactoryPayloadActor>(OverlappingActor);
	if (!PayloadActor)
	{
		return false;
	}

	const FResourceAmount PayloadAmount = GetResolvedMachineInputAmount(PayloadActor);
	if (!PayloadAmount.HasQuantity() || IsResourceBlocked(PayloadAmount.ResourceId))
	{
		return false;
	}

	const int32 QuantityScaled = PayloadAmount.GetScaledQuantity();
	if (!HasCapacityForAdditionalQuantity(QuantityScaled))
	{
		return false;
	}

	InputBufferScaled.FindOrAdd(PayloadAmount.ResourceId) += QuantityScaled;
	TotalBufferedQuantityScaled += QuantityScaled;

	if (GEngine)
	{
		const FString DebugLabel = DebugName.ToString();
		const FString ResourceLabel = PayloadAmount.ResourceId.ToString();
		GEngine->AddOnScreenDebugMessage(
			static_cast<uint64>(GetUniqueID()) + 21000ULL,
			1.0f,
			FColor::Green,
			FString::Printf(
				TEXT("%s Buffered %s: %.2f"),
				*DebugLabel,
				*ResourceLabel,
				GetBufferedUnits(PayloadAmount.ResourceId)));
	}

	PayloadActor->Destroy();
	return true;
}

int32 UMachineInputVolumeComponent::GetCurrentStoredQuantityScaled() const
{
	return TotalBufferedQuantityScaled;
}
