// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/ShredderVolumeComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Factory/FactoryPayloadActor.h"
#include "Factory/MachineOutputVolumeComponent.h"
#include "Factory/ResourceComponent.h"
#include "Factory/ResourceCompositionAsset.h"
#include "Factory/ResourceDefinitionAsset.h"
#include "Factory/ResourceTypes.h"

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

	UResourceComponent* ResourceData = OverlappingActor->FindComponentByClass<UResourceComponent>();
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

bool UShredderVolumeComponent::TryBuildRecoveredResources(const UResourceComponent* ResourceData, const UPrimitiveComponent* SourcePrimitive, TArray<FResourceAmount>& OutRecoveredResources) const
{
	OutRecoveredResources.Reset();
	if (!ResourceData)
	{
		return false;
	}

	const float RecoveryScalar = FMath::Max(0.0f, BaseRecoveryScalar);
	if (RecoveryScalar <= 0.0f)
	{
		return false;
	}

	if (ResourceData->Composition && ResourceData->Composition->HasDefinedResources())
	{
		const float EffectiveMassKg = ResourceData->ResolveEffectiveMassKg(SourcePrimitive);
		const float TotalMassFraction = ResourceData->Composition->GetTotalMassFraction();
		const float CompositionRecoveryScalar = ResourceData->ResolveRecoveryScalar();
		if (EffectiveMassKg <= 0.0f || TotalMassFraction <= KINDA_SMALL_NUMBER || CompositionRecoveryScalar <= 0.0f)
		{
			return false;
		}

		for (const FResourceCompositionEntry& Entry : ResourceData->Composition->Entries)
		{
			if (!Entry.IsDefined())
			{
				continue;
			}

			const FName ResourceId = Entry.Resource->GetResolvedResourceId();
			if (ResourceId.IsNone() || IsResourceBlocked(ResourceId))
			{
				continue;
			}

			const float NormalizedFraction = FMath::Max(0.0f, Entry.MassFraction) / TotalMassFraction;
			const float QuantityUnits = EffectiveMassKg
				* NormalizedFraction
				* FMath::Max(0.0f, Entry.YieldScalar)
				* CompositionRecoveryScalar
				* RecoveryScalar;
			const int32 QuantityScaled = AgentResource::UnitsToScaled(QuantityUnits);
			if (QuantityScaled <= 0)
			{
				continue;
			}

			FResourceAmount RecoveredAmount;
			RecoveredAmount.ResourceId = ResourceId;
			RecoveredAmount.SetScaledQuantity(QuantityScaled);
			OutRecoveredResources.Add(RecoveredAmount);
		}

		return OutRecoveredResources.Num() > 0;
	}

	if (!ResourceData->HasSimpleResourceDefinition())
	{
		return false;
	}

	const FName ResourceId = ResourceData->GetPrimaryResourceId();
	if (ResourceId.IsNone() || IsResourceBlocked(ResourceId))
	{
		return false;
	}

	const int32 BaseQuantityScaled = ResourceData->ResolveSimpleResourceQuantityScaled(SourcePrimitive);
	const int32 QuantityScaled = FMath::Max(0, FMath::RoundToInt(static_cast<float>(BaseQuantityScaled) * RecoveryScalar));
	if (QuantityScaled <= 0)
	{
		return false;
	}

	FResourceAmount RecoveredAmount;
	RecoveredAmount.ResourceId = ResourceId;
	RecoveredAmount.SetScaledQuantity(QuantityScaled);
	OutRecoveredResources.Add(RecoveredAmount);
	return true;
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
