// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/ShredderVolumeComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Factory/FactoryPayloadActor.h"
#include "Factory/MachineOutputVolumeComponent.h"
#include "Material/MaterialComponent.h"
#include "Material/MaterialTypes.h"

namespace
{
	static UPrimitiveComponent* FindBestShredSourcePrimitive(AActor* OverlappingActor)
	{
		if (!IsValid(OverlappingActor))
		{
			return nullptr;
		}

		if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(OverlappingActor->GetRootComponent()))
		{
			return RootPrimitive;
		}

		TArray<UPrimitiveComponent*> PrimitiveComponents;
		OverlappingActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (IsValid(PrimitiveComponent))
			{
				return PrimitiveComponent;
			}
		}

		return nullptr;
	}

	static FResourceAmount GetResolvedShredderPayloadAmount(const AFactoryPayloadActor* PayloadActor)
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

UShredderVolumeComponent::UShredderVolumeComponent()
{
	DebugName = TEXT("Shredder");
}

float UShredderVolumeComponent::GetBufferedUnits(FName ResourceId) const
{
	if (ResourceId.IsNone())
	{
		return AgentResource::ScaledToUnits(GetCurrentStoredQuantityScaled());
	}

	if (const int32* FoundQuantity = InternalResourceBufferScaled.Find(ResourceId))
	{
		return AgentResource::ScaledToUnits(*FoundQuantity);
	}

	return 0.0f;
}

bool UShredderVolumeComponent::TryProcessOverlappingActor(AActor* OverlappingActor)
{
	if (!IsValid(OverlappingActor) || OverlappingActor == GetOwner())
	{
		return false;
	}

	if (AFactoryPayloadActor* PayloadActor = Cast<AFactoryPayloadActor>(OverlappingActor))
	{
		const FResourceAmount PayloadAmount = GetResolvedShredderPayloadAmount(PayloadActor);
		if (!PayloadAmount.HasQuantity() || IsResourceBlocked(PayloadAmount.ResourceId))
		{
			return false;
		}

		if (!HasCapacityForAdditionalQuantity(PayloadAmount.GetScaledQuantity()))
		{
			return false;
		}

		InternalResourceBufferScaled.FindOrAdd(PayloadAmount.ResourceId) += PayloadAmount.GetScaledQuantity();
		FlushBufferedResourcesToOutput();
		PayloadActor->Destroy();
		return true;
	}

	UPrimitiveComponent* SourcePrimitive = FindBestShredSourcePrimitive(OverlappingActor);
	if (!SourcePrimitive)
	{
		return false;
	}

	UMaterialComponent* ResourceData = OverlappingActor->FindComponentByClass<UMaterialComponent>();
	if (!ResourceData)
	{
		if (bDestroyWithoutResourceComponent)
		{
			OverlappingActor->Destroy();
			return true;
		}

		return false;
	}

	TArray<FResourceAmount> RecoveredResources;
	if (!TryBuildRecoveredResources(ResourceData, SourcePrimitive, RecoveredResources))
	{
		return false;
	}

	int32 TotalRecoveredScaled = 0;
	for (const FResourceAmount& RecoveredAmount : RecoveredResources)
	{
		TotalRecoveredScaled += RecoveredAmount.GetScaledQuantity();
	}

	if (TotalRecoveredScaled <= 0 || !HasCapacityForAdditionalQuantity(TotalRecoveredScaled))
	{
		return false;
	}

	CommitRecoveredResources(RecoveredResources);
	FlushBufferedResourcesToOutput();
	OverlappingActor->Destroy();
	return true;
}

int32 UShredderVolumeComponent::GetCurrentStoredQuantityScaled() const
{
	int32 TotalQuantityScaled = 0;
	for (const TPair<FName, int32>& Pair : InternalResourceBufferScaled)
	{
		TotalQuantityScaled += FMath::Max(0, Pair.Value);
	}

	return TotalQuantityScaled;
}

bool UShredderVolumeComponent::TryBuildRecoveredResources(UMaterialComponent* ResourceData, UPrimitiveComponent* SourcePrimitive, TArray<FResourceAmount>& OutRecoveredResources) const
{
	OutRecoveredResources.Reset();
	if (!ResourceData || !SourcePrimitive)
	{
		return false;
	}

	const float RecoveryScalar = FMath::Max(0.0f, BaseRecoveryScalar);
	if (RecoveryScalar <= 0.0f)
	{
		return false;
	}

	TMap<FName, int32> ResolvedQuantitiesScaled;
	if (!ResourceData->BuildResolvedResourceQuantitiesScaled(SourcePrimitive, ResolvedQuantitiesScaled))
	{
		return false;
	}

	for (const TPair<FName, int32>& Pair : ResolvedQuantitiesScaled)
	{
		const FName ResourceId = Pair.Key;
		const int32 QuantityScaled = FMath::Max(0, Pair.Value);
		if (ResourceId.IsNone() || QuantityScaled <= 0 || IsResourceBlocked(ResourceId))
		{
			continue;
		}

		FResourceAmount RecoveredAmount;
		RecoveredAmount.ResourceId = ResourceId;
		RecoveredAmount.SetScaledQuantity(FMath::Max(0, FMath::RoundToInt(static_cast<float>(QuantityScaled) * RecoveryScalar)));
		if (RecoveredAmount.HasQuantity())
		{
			OutRecoveredResources.Add(RecoveredAmount);
		}
	}

	return OutRecoveredResources.Num() > 0;
}

void UShredderVolumeComponent::CommitRecoveredResources(const TArray<FResourceAmount>& RecoveredResources)
{
	for (const FResourceAmount& RecoveredAmount : RecoveredResources)
	{
		if (!RecoveredAmount.HasQuantity())
		{
			continue;
		}

		InternalResourceBufferScaled.FindOrAdd(RecoveredAmount.ResourceId) += RecoveredAmount.GetScaledQuantity();

		if (GEngine)
		{
			const FString DebugLabel = DebugName.ToString();
			const FString ResourceLabel = RecoveredAmount.ResourceId.ToString();
			GEngine->AddOnScreenDebugMessage(
				static_cast<uint64>(GetUniqueID()) + 20000ULL,
				1.0f,
				FColor::Orange,
				FString::Printf(TEXT("%s Buffered %s: %.2f"), *DebugLabel, *ResourceLabel, RecoveredAmount.GetUnits()));
		}
	}
}

void UShredderVolumeComponent::FlushBufferedResourcesToOutput()
{
	UMachineOutputVolumeComponent* OutputVolume = FindOutputVolume();
	if (!OutputVolume)
	{
		return;
	}

	TArray<FName> ResourceIds;
	InternalResourceBufferScaled.GetKeys(ResourceIds);
	for (const FName& ResourceId : ResourceIds)
	{
		int32* FoundQuantity = InternalResourceBufferScaled.Find(ResourceId);
		if (!FoundQuantity)
		{
			continue;
		}

		const int32 QuantityScaled = FMath::Max(0, *FoundQuantity);
		if (QuantityScaled <= 0)
		{
			InternalResourceBufferScaled.Remove(ResourceId);
			continue;
		}

		const int32 AcceptedQuantityScaled = OutputVolume->QueueResourceScaled(ResourceId, QuantityScaled);
		*FoundQuantity = FMath::Max(0, QuantityScaled - AcceptedQuantityScaled);
		if (*FoundQuantity == 0)
		{
			InternalResourceBufferScaled.Remove(ResourceId);
		}
	}
}

UMachineOutputVolumeComponent* UShredderVolumeComponent::FindOutputVolume() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UMachineOutputVolumeComponent>() : nullptr;
}

