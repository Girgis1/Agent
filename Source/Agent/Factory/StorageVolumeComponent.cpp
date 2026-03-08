// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/StorageVolumeComponent.h"
#include "Engine/Engine.h"
#include "Factory/FactoryPayloadActor.h"
#include "Material/AgentResourceTypes.h"

namespace
{
	static FResourceAmount GetResolvedPayloadAmount(const AFactoryPayloadActor* PayloadActor)
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

UStorageVolumeComponent::UStorageVolumeComponent()
{
	DebugName = TEXT("Storage");
}

int32 UStorageVolumeComponent::GetStoredItemCount(FName ResourceId) const
{
	if (ResourceId.IsNone())
	{
		return TotalStoredItemCount;
	}

	if (const int32* FoundCount = StoredItemCounts.Find(ResourceId))
	{
		return *FoundCount;
	}

	return 0;
}

float UStorageVolumeComponent::GetStoredUnits(FName ResourceId) const
{
	if (ResourceId.IsNone())
	{
		return GetTotalStoredUnits();
	}

	if (const int32* FoundQuantity = StoredResourceQuantitiesScaled.Find(ResourceId))
	{
		return AgentResource::ScaledToUnits(*FoundQuantity);
	}

	return 0.0f;
}

float UStorageVolumeComponent::GetTotalStoredUnits() const
{
	return AgentResource::ScaledToUnits(TotalStoredQuantityScaled);
}

bool UStorageVolumeComponent::TryProcessOverlappingActor(AActor* OverlappingActor)
{
	AFactoryPayloadActor* PayloadActor = Cast<AFactoryPayloadActor>(OverlappingActor);
	if (!PayloadActor)
	{
		return false;
	}

	const FResourceAmount PayloadAmount = GetResolvedPayloadAmount(PayloadActor);
	if (!PayloadAmount.HasQuantity() || IsResourceBlocked(PayloadAmount.ResourceId))
	{
		return false;
	}

	const int32 QuantityScaled = PayloadAmount.GetScaledQuantity();
	if (!HasCapacityForAdditionalQuantity(QuantityScaled))
	{
		return false;
	}

	StoredResourceQuantitiesScaled.FindOrAdd(PayloadAmount.ResourceId) += QuantityScaled;
	StoredItemCounts.FindOrAdd(PayloadAmount.ResourceId) += 1;
	TotalStoredQuantityScaled += QuantityScaled;
	++TotalStoredItemCount;

	if (GEngine)
	{
		const FString DebugLabel = DebugName.ToString();
		const FString ResourceLabel = PayloadAmount.ResourceId.ToString();
		GEngine->AddOnScreenDebugMessage(
			static_cast<uint64>(GetUniqueID()) + 19000ULL,
			1.0f,
			FColor::Cyan,
			FString::Printf(
				TEXT("%s Stored %s: %.2f (Total %.2f)"),
				*DebugLabel,
				*ResourceLabel,
				AgentResource::ScaledToUnits(StoredResourceQuantitiesScaled.FindRef(PayloadAmount.ResourceId)),
				GetTotalStoredUnits()));
	}

	PayloadActor->Destroy();
	return true;
}

int32 UStorageVolumeComponent::GetCurrentStoredQuantityScaled() const
{
	return TotalStoredQuantityScaled;
}

