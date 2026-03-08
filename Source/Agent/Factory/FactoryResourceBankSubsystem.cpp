// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/FactoryResourceBankSubsystem.h"
#include "Factory/FactoryWorldConfig.h"
#include "Material/AgentResourceTypes.h"
#include "EngineUtils.h"
#include <limits>

void UFactoryResourceBankSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	StoredResourcesScaled.Reset();
	TotalStoredScaled = 0;
	bInfiniteStorage = true;
	CapacityUnits = 0;
	bWorldDefaultsApplied = false;

	ApplyWorldDefaultsIfNeeded();
}

void UFactoryResourceBankSubsystem::Deinitialize()
{
	StoredResourcesScaled.Reset();
	TotalStoredScaled = 0;
	Super::Deinitialize();
}

bool UFactoryResourceBankSubsystem::IsInfiniteStorage() const
{
	const_cast<UFactoryResourceBankSubsystem*>(this)->ApplyWorldDefaultsIfNeeded();
	return bInfiniteStorage;
}

void UFactoryResourceBankSubsystem::SetInfiniteStorage(bool bInInfiniteStorage)
{
	ApplyWorldDefaultsIfNeeded();

	if (bInfiniteStorage == bInInfiniteStorage)
	{
		return;
	}

	bInfiniteStorage = bInInfiniteStorage;
	BroadcastStorageChanged();
}

int32 UFactoryResourceBankSubsystem::GetCapacityUnits() const
{
	const_cast<UFactoryResourceBankSubsystem*>(this)->ApplyWorldDefaultsIfNeeded();
	return CapacityUnits;
}

void UFactoryResourceBankSubsystem::SetCapacityUnits(int32 InCapacityUnits)
{
	ApplyWorldDefaultsIfNeeded();

	const int32 NewCapacityUnits = SanitizeCapacityUnits(InCapacityUnits);
	if (CapacityUnits == NewCapacityUnits)
	{
		return;
	}

	CapacityUnits = NewCapacityUnits;
	BroadcastStorageChanged();
}

float UFactoryResourceBankSubsystem::GetStoredUnits(FName ResourceId) const
{
	return AgentResource::ScaledToUnits(GetStoredScaled(ResourceId));
}

float UFactoryResourceBankSubsystem::GetTotalStoredUnits() const
{
	return AgentResource::ScaledToUnits(GetTotalStoredScaled());
}

int32 UFactoryResourceBankSubsystem::GetStoredScaled(FName ResourceId) const
{
	if (ResourceId.IsNone())
	{
		return GetTotalStoredScaled();
	}

	return FMath::Max(0, StoredResourcesScaled.FindRef(ResourceId));
}

int32 UFactoryResourceBankSubsystem::GetTotalStoredScaled() const
{
	return FMath::Max(0, TotalStoredScaled);
}

float UFactoryResourceBankSubsystem::GetCapacityUnitsAsFloat() const
{
	const_cast<UFactoryResourceBankSubsystem*>(this)->ApplyWorldDefaultsIfNeeded();
	return bInfiniteStorage ? -1.0f : AgentResource::ScaledToUnits(GetCapacityScaled());
}

float UFactoryResourceBankSubsystem::GetRemainingCapacityUnits() const
{
	return IsInfiniteStorage() ? -1.0f : AgentResource::ScaledToUnits(GetRemainingCapacityScaled());
}

int32 UFactoryResourceBankSubsystem::GetRemainingCapacityScaled() const
{
	const_cast<UFactoryResourceBankSubsystem*>(this)->ApplyWorldDefaultsIfNeeded();
	if (bInfiniteStorage)
	{
		return TNumericLimits<int32>::Max();
	}

	return FMath::Max(0, GetCapacityScaled() - GetTotalStoredScaled());
}

bool UFactoryResourceBankSubsystem::HasResourceQuantityUnits(FName ResourceId, int32 QuantityUnits) const
{
	return HasResourceQuantityScaled(ResourceId, AgentResource::WholeUnitsToScaled(QuantityUnits));
}

bool UFactoryResourceBankSubsystem::HasResourceQuantityScaled(FName ResourceId, int32 QuantityScaled) const
{
	if (ResourceId.IsNone() || QuantityScaled <= 0)
	{
		return false;
	}

	return GetStoredScaled(ResourceId) >= QuantityScaled;
}

bool UFactoryResourceBankSubsystem::ConsumeResourceQuantityUnits(FName ResourceId, int32 QuantityUnits)
{
	return ConsumeResourceQuantityScaled(ResourceId, AgentResource::WholeUnitsToScaled(QuantityUnits));
}

bool UFactoryResourceBankSubsystem::ConsumeResourceQuantityScaled(FName ResourceId, int32 QuantityScaled)
{
	if (ResourceId.IsNone() || QuantityScaled <= 0)
	{
		return false;
	}

	TMap<FName, int32> SingleResource;
	SingleResource.Add(ResourceId, QuantityScaled);
	return ConsumeResourcesScaledAtomic(SingleResource);
}

bool UFactoryResourceBankSubsystem::ConsumeResourcesScaledAtomic(const TMap<FName, int32>& ResourceQuantitiesScaled)
{
	ApplyWorldDefaultsIfNeeded();

	if (ResourceQuantitiesScaled.Num() == 0)
	{
		return false;
	}

	int64 TotalToConsumeScaled = 0;
	for (const TPair<FName, int32>& Pair : ResourceQuantitiesScaled)
	{
		const FName ResourceId = Pair.Key;
		const int32 QuantityScaled = FMath::Max(0, Pair.Value);
		if (ResourceId.IsNone() || QuantityScaled <= 0)
		{
			return false;
		}

		const int32 StoredQuantityScaled = StoredResourcesScaled.FindRef(ResourceId);
		if (StoredQuantityScaled < QuantityScaled)
		{
			return false;
		}

		TotalToConsumeScaled += static_cast<int64>(QuantityScaled);
		if (TotalToConsumeScaled > static_cast<int64>(TNumericLimits<int32>::Max()))
		{
			return false;
		}
	}

	const int64 NewTotalStoredScaled = static_cast<int64>(TotalStoredScaled) - TotalToConsumeScaled;
	if (NewTotalStoredScaled < 0 || NewTotalStoredScaled > static_cast<int64>(TNumericLimits<int32>::Max()))
	{
		return false;
	}

	for (const TPair<FName, int32>& Pair : ResourceQuantitiesScaled)
	{
		int32& StoredQuantityScaled = StoredResourcesScaled.FindOrAdd(Pair.Key);
		StoredQuantityScaled = FMath::Max(0, StoredQuantityScaled - FMath::Max(0, Pair.Value));
		if (StoredQuantityScaled == 0)
		{
			StoredResourcesScaled.Remove(Pair.Key);
		}
	}

	TotalStoredScaled = static_cast<int32>(NewTotalStoredScaled);
	BroadcastStorageChanged();
	return true;
}

bool UFactoryResourceBankSubsystem::AddResourcesScaledAtomic(const TMap<FName, int32>& ResourceQuantitiesScaled)
{
	ApplyWorldDefaultsIfNeeded();

	if (!CanAcceptResourcesAtomic(ResourceQuantitiesScaled))
	{
		return false;
	}

	int64 AddedTotalScaled = 0;
	for (const TPair<FName, int32>& Pair : ResourceQuantitiesScaled)
	{
		const int32 QuantityScaled = FMath::Max(0, Pair.Value);
		if (QuantityScaled <= 0)
		{
			continue;
		}

		int32& StoredQuantityScaled = StoredResourcesScaled.FindOrAdd(Pair.Key);
		StoredQuantityScaled += QuantityScaled;
		AddedTotalScaled += static_cast<int64>(QuantityScaled);
	}

	const int64 NewTotalStoredScaled = static_cast<int64>(TotalStoredScaled) + AddedTotalScaled;
	if (NewTotalStoredScaled < 0 || NewTotalStoredScaled > static_cast<int64>(TNumericLimits<int32>::Max()))
	{
		return false;
	}

	TotalStoredScaled = static_cast<int32>(NewTotalStoredScaled);
	BroadcastStorageChanged();
	return true;
}

void UFactoryResourceBankSubsystem::ClearStorage()
{
	if (StoredResourcesScaled.Num() == 0 && TotalStoredScaled == 0)
	{
		return;
	}

	StoredResourcesScaled.Reset();
	TotalStoredScaled = 0;
	BroadcastStorageChanged();
}

void UFactoryResourceBankSubsystem::GetStorageSnapshot(TArray<FResourceStorageEntry>& OutEntries) const
{
	OutEntries.Reset();

	TArray<FName> ResourceIds;
	StoredResourcesScaled.GetKeys(ResourceIds);
	ResourceIds.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});

	for (const FName ResourceId : ResourceIds)
	{
		const int32 QuantityScaled = FMath::Max(0, StoredResourcesScaled.FindRef(ResourceId));
		if (ResourceId.IsNone() || QuantityScaled <= 0)
		{
			continue;
		}

		FResourceStorageEntry Entry;
		Entry.ResourceId = ResourceId;
		Entry.QuantityScaled = QuantityScaled;
		Entry.Units = AgentResource::ScaledToUnits(QuantityScaled);
		OutEntries.Add(Entry);
	}
}

void UFactoryResourceBankSubsystem::ApplyWorldDefaultsIfNeeded()
{
	if (bWorldDefaultsApplied)
	{
		return;
	}

	if (const AFactoryWorldConfig* DefaultConfig = GetDefault<AFactoryWorldConfig>())
	{
		bInfiniteStorage = DefaultConfig->bGlobalStorageInfinite;
		CapacityUnits = SanitizeCapacityUnits(DefaultConfig->GlobalStorageCapacityUnits);
	}

	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AFactoryWorldConfig> It(World); It; ++It)
		{
			if (const AFactoryWorldConfig* WorldConfig = *It)
			{
				bInfiniteStorage = WorldConfig->bGlobalStorageInfinite;
				CapacityUnits = SanitizeCapacityUnits(WorldConfig->GlobalStorageCapacityUnits);
				break;
			}
		}
	}

	bWorldDefaultsApplied = true;
}

void UFactoryResourceBankSubsystem::BroadcastStorageChanged()
{
	OnStorageChanged.Broadcast();
}

int32 UFactoryResourceBankSubsystem::GetCapacityScaled() const
{
	const_cast<UFactoryResourceBankSubsystem*>(this)->ApplyWorldDefaultsIfNeeded();
	return bInfiniteStorage
		? TNumericLimits<int32>::Max()
		: AgentResource::WholeUnitsToScaled(CapacityUnits);
}

int32 UFactoryResourceBankSubsystem::SanitizeCapacityUnits(int32 InCapacityUnits) const
{
	const int32 MaxWholeUnits = TNumericLimits<int32>::Max() / AgentResource::UnitsScale;
	return FMath::Clamp(InCapacityUnits, 0, MaxWholeUnits);
}

bool UFactoryResourceBankSubsystem::CanAcceptResourcesAtomic(const TMap<FName, int32>& ResourceQuantitiesScaled) const
{
	if (ResourceQuantitiesScaled.Num() == 0)
	{
		return false;
	}

	int64 IncomingTotalScaled = 0;
	for (const TPair<FName, int32>& Pair : ResourceQuantitiesScaled)
	{
		const FName ResourceId = Pair.Key;
		const int32 QuantityScaled = FMath::Max(0, Pair.Value);
		if (ResourceId.IsNone() || QuantityScaled <= 0)
		{
			return false;
		}

		const int64 ExistingResourceScaled = static_cast<int64>(StoredResourcesScaled.FindRef(ResourceId));
		if ((ExistingResourceScaled + static_cast<int64>(QuantityScaled)) > static_cast<int64>(TNumericLimits<int32>::Max()))
		{
			return false;
		}

		IncomingTotalScaled += static_cast<int64>(QuantityScaled);
		if (IncomingTotalScaled > static_cast<int64>(TNumericLimits<int32>::Max()))
		{
			return false;
		}
	}

	const int64 NewTotalScaled = static_cast<int64>(TotalStoredScaled) + IncomingTotalScaled;
	if (NewTotalScaled > static_cast<int64>(TNumericLimits<int32>::Max()))
	{
		return false;
	}

	if (!bInfiniteStorage)
	{
		const int64 RemainingCapacityScaled = static_cast<int64>(GetRemainingCapacityScaled());
		if (IncomingTotalScaled > RemainingCapacityScaled)
		{
			return false;
		}
	}

	return true;
}
