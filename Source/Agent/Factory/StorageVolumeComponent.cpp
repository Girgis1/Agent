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

int32 UStorageVolumeComponent::StoreResourceScaledDirect(FName ResourceId, int32 QuantityScaled, int32 ItemCount)
{
	if (ResourceId.IsNone() || QuantityScaled <= 0 || IsResourceBlocked(ResourceId))
	{
		return 0;
	}

	const int32 RequestedScaled = FMath::Max(0, QuantityScaled);
	const int32 MaxQuantityScaled = GetMaxQuantityScaled();
	const int32 RemainingCapacityScaled = MaxQuantityScaled > 0
		? FMath::Max(0, MaxQuantityScaled - GetCurrentStoredQuantityScaled())
		: RequestedScaled;
	const int32 AcceptedScaled = MaxQuantityScaled > 0
		? FMath::Min(RequestedScaled, RemainingCapacityScaled)
		: RequestedScaled;
	if (AcceptedScaled <= 0)
	{
		return 0;
	}

	StoredResourceQuantitiesScaled.FindOrAdd(ResourceId) += AcceptedScaled;
	TotalStoredQuantityScaled += AcceptedScaled;

	const int32 SafeItemCount = FMath::Max(0, ItemCount);
	if (SafeItemCount > 0)
	{
		StoredItemCounts.FindOrAdd(ResourceId) += SafeItemCount;
		TotalStoredItemCount += SafeItemCount;
	}

	return AcceptedScaled;
}

bool UStorageVolumeComponent::StoreResourcesScaledDirect(const TMap<FName, int32>& ResourceQuantitiesScaled, int32 ItemCount)
{
	if (ResourceQuantitiesScaled.Num() == 0)
	{
		return false;
	}

	int32 TotalAdditionalScaled = 0;
	for (const TPair<FName, int32>& Pair : ResourceQuantitiesScaled)
	{
		const FName ResourceId = Pair.Key;
		const int32 QuantityScaled = FMath::Max(0, Pair.Value);
		if (ResourceId.IsNone() || QuantityScaled <= 0 || IsResourceBlocked(ResourceId))
		{
			return false;
		}

		TotalAdditionalScaled += QuantityScaled;
	}

	if (!HasCapacityForAdditionalQuantity(TotalAdditionalScaled))
	{
		return false;
	}

	for (const TPair<FName, int32>& Pair : ResourceQuantitiesScaled)
	{
		const FName ResourceId = Pair.Key;
		const int32 QuantityScaled = FMath::Max(0, Pair.Value);
		if (QuantityScaled <= 0)
		{
			continue;
		}

		StoredResourceQuantitiesScaled.FindOrAdd(ResourceId) += QuantityScaled;
		TotalStoredQuantityScaled += QuantityScaled;
	}

	const int32 SafeItemCount = FMath::Max(0, ItemCount);
	if (SafeItemCount > 0)
	{
		TotalStoredItemCount += SafeItemCount;
	}

	return true;
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

