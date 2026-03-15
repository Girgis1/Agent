// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/ShredderVolumeComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Factory/FactoryPayloadActor.h"
#include "Factory/MachineOutputVolumeComponent.h"
#include "GameFramework/Pawn.h"
#include "Material/MaterialComponent.h"
#include "Material/AgentResourceTypes.h"
#include "Objects/Components/ObjectHealthComponent.h"

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

void UShredderVolumeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	CleanupTrackedActors();
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
	UPrimitiveComponent* SourcePrimitive = FindBestShredSourcePrimitive(OverlappingActor);
	UMaterialComponent* ResourceData = OverlappingActor ? OverlappingActor->FindComponentByClass<UMaterialComponent>() : nullptr;
	const TWeakObjectPtr<AActor> ActorKey(OverlappingActor);
	if (!IsActorEligibleForShredding(OverlappingActor, SourcePrimitive))
	{
		ClearTrackedActorState(ActorKey);
		return false;
	}

	const bool bShouldForceCleanup = ShouldForceCleanupActor(OverlappingActor);
	const bool bShouldConsumeImmediately = bConsumeOverlapsImmediately && (ResourceData != nullptr || bDestroyWithoutResourceComponent);
	if (bRequireHealthDepletionBeforeShredding)
	{
		if (!bShouldConsumeImmediately)
		{
			if (UObjectHealthComponent* ObjectHealth = UObjectHealthComponent::FindObjectHealthComponent(OverlappingActor))
			{
				if (!ObjectHealth->IsDepleted() && !bShouldForceCleanup)
				{
					return false;
				}
			}
		}
	}

	if (!bShouldForceCleanup && !bShouldConsumeImmediately && !IsActorReadyForShredding(OverlappingActor))
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
		ClearTrackedActorState(ActorKey);
		return true;
	}

	if (!SourcePrimitive)
	{
		if (bShouldForceCleanup)
		{
			OverlappingActor->Destroy();
			ClearTrackedActorState(ActorKey);
			return true;
		}

		return false;
	}

	if (!ResourceData)
	{
		if (bDestroyWithoutResourceComponent || bShouldForceCleanup)
		{
			OverlappingActor->Destroy();
			ClearTrackedActorState(ActorKey);
			return true;
		}

		return false;
	}

	TArray<FResourceAmount> RecoveredResources;
	if (!TryBuildRecoveredResources(OverlappingActor, ResourceData, SourcePrimitive, RecoveredResources))
	{
		if (bDestroyIfNoRecoverableResources || bShouldForceCleanup)
		{
			OverlappingActor->Destroy();
			ClearTrackedActorState(ActorKey);
			return true;
		}

		return false;
	}

	int32 TotalRecoveredScaled = 0;
	for (const FResourceAmount& RecoveredAmount : RecoveredResources)
	{
		TotalRecoveredScaled += RecoveredAmount.GetScaledQuantity();
	}

	if (TotalRecoveredScaled <= 0)
	{
		const float MaterialRecoveryScalar = UObjectHealthComponent::GetActorMaterialRecoveryScalar(OverlappingActor);
		if (MaterialRecoveryScalar <= KINDA_SMALL_NUMBER || bDestroyIfNoRecoverableResources || bShouldForceCleanup)
		{
			OverlappingActor->Destroy();
			ClearTrackedActorState(ActorKey);
			return true;
		}

		return false;
	}

	if (!HasCapacityForAdditionalQuantity(TotalRecoveredScaled))
	{
		return false;
	}

	CommitRecoveredResources(RecoveredResources);
	FlushBufferedResourcesToOutput();
	OverlappingActor->Destroy();
	ClearTrackedActorState(ActorKey);
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

bool UShredderVolumeComponent::TryBuildRecoveredResources(
	const AActor* SourceActor,
	UMaterialComponent* ResourceData,
	UPrimitiveComponent* SourcePrimitive,
	TArray<FResourceAmount>& OutRecoveredResources) const
{
	OutRecoveredResources.Reset();
	if (!ResourceData || !SourcePrimitive)
	{
		return false;
	}

	const float BaseScalar = FMath::Max(0.0f, BaseRecoveryScalar);
	if (BaseScalar <= 0.0f)
	{
		return false;
	}

	const float DamagedPenaltyRecoveryScalar = FMath::Clamp(
		UObjectHealthComponent::GetActorMaterialRecoveryScalar(SourceActor),
		0.0f,
		1.0f);
	const float EffectiveRecoveryScalar = BaseScalar * DamagedPenaltyRecoveryScalar;

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
		RecoveredAmount.SetScaledQuantity(FMath::Max(0, FMath::RoundToInt(static_cast<float>(QuantityScaled) * EffectiveRecoveryScalar)));
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

bool UShredderVolumeComponent::IsActorEligibleForShredding(const AActor* OverlappingActor, const UPrimitiveComponent* SourcePrimitive) const
{
	if (!IsValid(OverlappingActor) || OverlappingActor == GetOwner())
	{
		return false;
	}

	if (bIgnorePawns && OverlappingActor->IsA<APawn>())
	{
		return false;
	}

	if (!bOnlyShredMovableActors)
	{
		return true;
	}

	return SourcePrimitive && SourcePrimitive->Mobility == EComponentMobility::Movable;
}

bool UShredderVolumeComponent::IsActorReadyForShredding(AActor* OverlappingActor)
{
	if (!IsValid(OverlappingActor))
	{
		return false;
	}

	const TWeakObjectPtr<AActor> ActorKey(OverlappingActor);
	if (!bUseShredDelay)
	{
		PendingReadyTimeByActor.Remove(ActorKey);
		return true;
	}

	const float MinDelay = FMath::Max(0.0f, ShredDelayMinSeconds);
	const float MaxDelay = FMath::Max(MinDelay, ShredDelayMaxSeconds);
	if (MaxDelay <= KINDA_SMALL_NUMBER)
	{
		PendingReadyTimeByActor.Remove(ActorKey);
		return true;
	}

	const float NowSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (const float* FoundReadyTime = PendingReadyTimeByActor.Find(ActorKey))
	{
		return NowSeconds >= *FoundReadyTime;
	}

	const float DelaySeconds = FMath::IsNearlyEqual(MinDelay, MaxDelay)
		? MinDelay
		: FMath::FRandRange(MinDelay, MaxDelay);
	PendingReadyTimeByActor.Add(ActorKey, NowSeconds + DelaySeconds);
	return false;
}

bool UShredderVolumeComponent::ShouldForceCleanupActor(AActor* OverlappingActor)
{
	if (!IsValid(OverlappingActor))
	{
		return false;
	}

	const TWeakObjectPtr<AActor> ActorKey(OverlappingActor);
	if (!bForceCleanupActorsStuckInVolume)
	{
		ForceCleanupReadyTimeByActor.Remove(ActorKey);
		return false;
	}

	const float SafeDelaySeconds = FMath::Max(0.0f, ForceCleanupDelaySeconds);
	if (SafeDelaySeconds <= KINDA_SMALL_NUMBER)
	{
		return true;
	}

	const float NowSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if (const float* FoundReadyTime = ForceCleanupReadyTimeByActor.Find(ActorKey))
	{
		return NowSeconds >= *FoundReadyTime;
	}

	ForceCleanupReadyTimeByActor.Add(ActorKey, NowSeconds + SafeDelaySeconds);
	return false;
}

void UShredderVolumeComponent::ClearTrackedActorState(const TWeakObjectPtr<AActor>& ActorKey)
{
	PendingReadyTimeByActor.Remove(ActorKey);
	ForceCleanupReadyTimeByActor.Remove(ActorKey);
}

void UShredderVolumeComponent::CleanupTrackedActors()
{
	if (PendingReadyTimeByActor.Num() == 0 && ForceCleanupReadyTimeByActor.Num() == 0)
	{
		return;
	}

	for (auto It = PendingReadyTimeByActor.CreateIterator(); It; ++It)
	{
		AActor* TrackedActor = It.Key().Get();
		if (!IsValid(TrackedActor) || !IsOverlappingActor(TrackedActor))
		{
			It.RemoveCurrent();
		}
	}

	for (auto It = ForceCleanupReadyTimeByActor.CreateIterator(); It; ++It)
	{
		AActor* TrackedActor = It.Key().Get();
		if (!IsValid(TrackedActor) || !IsOverlappingActor(TrackedActor))
		{
			It.RemoveCurrent();
		}
	}
}

