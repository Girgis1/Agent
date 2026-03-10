// Copyright Epic Games, Inc. All Rights Reserved.

#include "Backpack/Components/BlackHoleBackpackLinkComponent.h"
#include "Backpack/Actors/BlackHoleBackpackActor.h"
#include "Backpack/BlackHoleEndpointRegistrySubsystem.h"
#include "Backpack/Components/BlackHoleOutputSinkComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Factory/FactoryPayloadActor.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Machine/InputVolume.h"
#include "Machine/MachineComponent.h"
#include "Material/AgentResourceTypes.h"
#include "Material/MaterialComponent.h"
#include "Material/MaterialDefinitionAsset.h"
#include "PhysicsEngine/BodyInstance.h"
#include "UObject/UObjectIterator.h"
#include <limits>

namespace
{
const TCHAR* BackpackRecipeClassOutputPrefix = TEXT("__RecipeClassOutput__:");

TSubclassOf<AActor> ResolveRecipeOutputClassOverride(FName ResourceId)
{
	if (ResourceId.IsNone())
	{
		return nullptr;
	}

	FString ResourceKey = ResourceId.ToString();
	if (!ResourceKey.RemoveFromStart(BackpackRecipeClassOutputPrefix) || ResourceKey.IsEmpty())
	{
		return nullptr;
	}

	UClass* LoadedClass = FindObject<UClass>(nullptr, *ResourceKey);
	if (!LoadedClass)
	{
		LoadedClass = LoadObject<UClass>(nullptr, *ResourceKey);
	}

	if (!LoadedClass || !LoadedClass->IsChildOf(AActor::StaticClass()))
	{
		return nullptr;
	}

	return LoadedClass;
}

FResourceAmount GetResolvedPayloadAmount(const AFactoryPayloadActor* PayloadActor)
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

float ResolvePrimitiveMassKgWithoutPhysicsWarning(const UPrimitiveComponent* PrimitiveComponent)
{
	if (!PrimitiveComponent)
	{
		return 0.0f;
	}

	if (const FBodyInstance* BodyInstance = PrimitiveComponent->GetBodyInstance())
	{
		const float BodyMassKg = BodyInstance->GetBodyMass();
		if (BodyMassKg > KINDA_SMALL_NUMBER)
		{
			return FMath::Max(0.0f, BodyMassKg);
		}
	}

	if (PrimitiveComponent->IsSimulatingPhysics())
	{
		return FMath::Max(0.0f, PrimitiveComponent->GetMass());
	}

	return 0.0f;
}
}

UBlackHoleBackpackLinkComponent::UBlackHoleBackpackLinkComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UBlackHoleBackpackLinkComponent::BeginPlay()
{
	Super::BeginPlay();
	ResolveDependencies();
}

void UBlackHoleBackpackLinkComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!ResolveDependencies())
	{
		return;
	}

	UBlackHoleOutputSinkComponent* SinkComponent = ResolveLinkedSink();
	const bool bSinkActivated = SinkComponent ? SinkComponent->IsMachineActivated() : false;
	const bool bShouldOperateNow = ShouldTransferForCurrentDeploymentState();

	if (bUseTeleportQueueFlow)
	{
		if (!bBlackHoleEnabled)
		{
			UpdateBlackHoleIntakeState(true);
			return;
		}

		UpdateBlackHoleIntakeState(!bShouldOperateNow || (bDeactivateBackpackWhileSinkActivated && bSinkActivated));
	}

	if (!bShouldOperateNow)
	{
		return;
	}

	if (bUseTeleportQueueFlow)
	{
		ProcessTeleportIntake();
	}

	TimeUntilNextTransfer = FMath::Max(0.0f, TimeUntilNextTransfer - FMath::Max(0.0f, DeltaTime));
	if (TimeUntilNextTransfer > 0.0f)
	{
		return;
	}

	if (bUseTeleportQueueFlow)
	{
		TransferTeleportItemsNow();
	}
	else
	{
		TransferBufferedResourcesNow();
	}

	TimeUntilNextTransfer = FMath::Max(0.0f, TransferInterval);
}

void UBlackHoleBackpackLinkComponent::SetSourceMachineComponent(UMachineComponent* InMachineComponent)
{
	SourceMachineComponent = InMachineComponent;
}

void UBlackHoleBackpackLinkComponent::SetLinkId(FName InLinkId)
{
	LinkId = InLinkId;
}

bool UBlackHoleBackpackLinkComponent::TransferBufferedResourcesNow()
{
	if (!ResolveDependencies() || !SourceMachineComponent)
	{
		return false;
	}

	UBlackHoleOutputSinkComponent* SinkComponent = ResolveLinkedSink();
	if (!SinkComponent || !SinkComponent->IsSinkReady())
	{
		return false;
	}

	if (SourceMachineComponent->StoredResourcesScaled.Num() == 0)
	{
		return false;
	}

	int32 RemainingBudgetScaled = TNumericLimits<int32>::Max();
	if (MaxTransferUnitsPerTick > 0)
	{
		RemainingBudgetScaled = AgentResource::WholeUnitsToScaled(MaxTransferUnitsPerTick);
	}

	TArray<FName> ResourceIds;
	SourceMachineComponent->StoredResourcesScaled.GetKeys(ResourceIds);
	ResourceIds.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});

	TMap<FName, int32> ToConsumeScaled;
	for (const FName& ResourceId : ResourceIds)
	{
		if (ResourceId.IsNone())
		{
			continue;
		}

		const int32 StoredScaled = FMath::Max(0, SourceMachineComponent->StoredResourcesScaled.FindRef(ResourceId));
		if (StoredScaled <= 0)
		{
			continue;
		}

		const int32 DesiredTransferScaled = RemainingBudgetScaled == TNumericLimits<int32>::Max()
			? StoredScaled
			: FMath::Min(StoredScaled, RemainingBudgetScaled);
		if (DesiredTransferScaled <= 0)
		{
			continue;
		}

		const TSubclassOf<AActor> OutputActorClassOverride = ResolveRecipeOutputClassOverride(ResourceId);
		const int32 AcceptedScaled = SinkComponent->QueueResourceScaled(ResourceId, DesiredTransferScaled, OutputActorClassOverride);
		if (AcceptedScaled <= 0)
		{
			continue;
		}

		ToConsumeScaled.Add(ResourceId, AcceptedScaled);
		if (RemainingBudgetScaled != TNumericLimits<int32>::Max())
		{
			RemainingBudgetScaled = FMath::Max(0, RemainingBudgetScaled - AcceptedScaled);
			if (RemainingBudgetScaled <= 0)
			{
				break;
			}
		}
	}

	if (ToConsumeScaled.Num() == 0)
	{
		return false;
	}

	return SourceMachineComponent->ConsumeStoredResourcesScaledAtomic(ToConsumeScaled);
}

bool UBlackHoleBackpackLinkComponent::TransferTeleportItemsNow()
{
	if (!bUseTeleportQueueFlow || QueuedTeleportItems.Num() == 0)
	{
		return false;
	}

	UBlackHoleOutputSinkComponent* SinkComponent = ResolveLinkedSink();
	if (!SinkComponent || !SinkComponent->IsSinkReady())
	{
		return false;
	}

	if (!SinkComponent->CanAcceptTeleportInput())
	{
		return false;
	}

	int32 RemainingItemBudget = MaxTransferUnitsPerTick > 0 ? MaxTransferUnitsPerTick : TNumericLimits<int32>::Max();
	bool bTransferredAny = false;
	while (QueuedTeleportItems.Num() > 0 && RemainingItemBudget > 0)
	{
		FBlackHoleTeleportQueuedItem& ItemToTransfer = QueuedTeleportItems[0];
		if (!ItemToTransfer.ActorClass.Get())
		{
			StoredTeleportMassKg = FMath::Max(0.0f, StoredTeleportMassKg - FMath::Max(0.0f, ItemToTransfer.TotalMassKg));
			QueuedTeleportItems.RemoveAt(0);
			continue;
		}

		if (!SinkComponent->QueueTeleportActor(ItemToTransfer.ActorClass, ItemToTransfer.ResourceQuantitiesScaled, ItemToTransfer.TotalMassKg))
		{
			break;
		}

		StoredTeleportMassKg = FMath::Max(0.0f, StoredTeleportMassKg - FMath::Max(0.0f, ItemToTransfer.TotalMassKg));
		QueuedTeleportItems.RemoveAt(0);
		bTransferredAny = true;
		if (RemainingItemBudget != TNumericLimits<int32>::Max())
		{
			--RemainingItemBudget;
		}
	}

	return bTransferredAny;
}

void UBlackHoleBackpackLinkComponent::SetBlackHoleEnabled(bool bInEnabled)
{
	bBlackHoleEnabled = bInEnabled;
	UpdateBlackHoleIntakeState(!bInEnabled);
}

bool UBlackHoleBackpackLinkComponent::ResolveDependencies()
{
	if (!OwningBackItem)
	{
		OwningBackItem = Cast<ABlackHoleBackpackActor>(GetOwner());
	}

	if (!SourceMachineComponent)
	{
		if (AActor* OwnerActor = GetOwner())
		{
			SourceMachineComponent = OwnerActor->FindComponentByClass<UMachineComponent>();
		}
	}

	if (bUseTeleportQueueFlow)
	{
		return OwningBackItem != nullptr;
	}

	return SourceMachineComponent != nullptr;
}

UBlackHoleOutputSinkComponent* UBlackHoleBackpackLinkComponent::ResolveLinkedSink() const
{
	if (LinkId.IsNone())
	{
		return nullptr;
	}

	if (UWorld* World = GetWorld())
	{
		if (const UBlackHoleEndpointRegistrySubsystem* Registry = World->GetSubsystem<UBlackHoleEndpointRegistrySubsystem>())
		{
			return Registry->ResolveSink(LinkId);
		}
	}

	return nullptr;
}

bool UBlackHoleBackpackLinkComponent::ShouldTransferForCurrentDeploymentState() const
{
	const bool bIsDeployed = OwningBackItem ? OwningBackItem->IsItemDeployed() : true;
	return bIsDeployed ? bTransferWhenDeployed : bTransferWhenAttached;
}

bool UBlackHoleBackpackLinkComponent::TryQueueTeleportActorFromOverlap(AActor* OverlappingActor)
{
	if (!OverlappingActor || OverlappingActor == GetOwner() || OverlappingActor == OwningBackItem.Get())
	{
		return false;
	}

	if (OverlappingActor->IsPendingKillPending())
	{
		return false;
	}

	FBlackHoleTeleportQueuedItem QueuedItem;
	if (!BuildTeleportItemFromActor(OverlappingActor, QueuedItem))
	{
		return false;
	}

	const float NextStoredMassKg = StoredTeleportMassKg + FMath::Max(0.0f, QueuedItem.TotalMassKg);
	if (MaxStoredTeleportMassKg > 0.0f && NextStoredMassKg > MaxStoredTeleportMassKg)
	{
		return false;
	}

	QueuedTeleportItems.Add(MoveTemp(QueuedItem));
	StoredTeleportMassKg = NextStoredMassKg;
	OverlappingActor->Destroy();
	return true;
}

void UBlackHoleBackpackLinkComponent::ProcessTeleportIntake()
{
	if (!bUseTeleportQueueFlow || !bBlackHoleEnabled)
	{
		return;
	}

	UInputVolume* BackpackInput = ResolveBackpackInputVolume();
	if (!BackpackInput)
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	BackpackInput->GetOverlappingActors(OverlappingActors);
	for (AActor* OverlappingActor : OverlappingActors)
	{
		TryQueueTeleportActorFromOverlap(OverlappingActor);
	}
}

bool UBlackHoleBackpackLinkComponent::BuildTeleportItemFromActor(AActor* SourceActor, FBlackHoleTeleportQueuedItem& OutQueuedItem)
{
	OutQueuedItem = {};
	if (!SourceActor || !SourceActor->GetClass())
	{
		return false;
	}

	if (SourceActor->IsA<APawn>())
	{
		return false;
	}

	OutQueuedItem.ActorClass = SourceActor->GetClass();

	bool bHasMaterialOrPayloadData = false;
	if (const AFactoryPayloadActor* PayloadActor = Cast<AFactoryPayloadActor>(SourceActor))
	{
		const FResourceAmount PayloadAmount = GetResolvedPayloadAmount(PayloadActor);
		if (PayloadAmount.HasQuantity())
		{
			OutQueuedItem.ResourceQuantitiesScaled.FindOrAdd(PayloadAmount.ResourceId) += PayloadAmount.GetScaledQuantity();
			bHasMaterialOrPayloadData = true;
		}
	}
	else if (UMaterialComponent* MaterialComponent = SourceActor->FindComponentByClass<UMaterialComponent>())
	{
		TMap<FName, int32> ResourceQuantitiesScaled;
		MaterialComponent->BuildResolvedResourceQuantitiesScaled(ResolveResourceSourcePrimitive(SourceActor), ResourceQuantitiesScaled);
		for (const TPair<FName, int32>& Pair : ResourceQuantitiesScaled)
		{
			const FName ResourceId = Pair.Key;
			const int32 QuantityScaled = FMath::Max(0, Pair.Value);
			if (ResourceId.IsNone() || QuantityScaled <= 0)
			{
				continue;
			}

			OutQueuedItem.ResourceQuantitiesScaled.FindOrAdd(ResourceId) += QuantityScaled;
		}

		bHasMaterialOrPayloadData = OutQueuedItem.ResourceQuantitiesScaled.Num() > 0;
	}

	UPrimitiveComponent* SourcePrimitive = ResolveResourceSourcePrimitive(SourceActor);
	if (!bHasMaterialOrPayloadData)
	{
		if (!bAllowActorsWithoutMaterialData)
		{
			return false;
		}

		// Only allow "no-material" teleports for loose physics objects.
		if (!SourcePrimitive || !SourcePrimitive->IsSimulatingPhysics())
		{
			return false;
		}
	}

	OutQueuedItem.TotalMassKg = ComputeResourceMassKg(OutQueuedItem.ResourceQuantitiesScaled);
	if (OutQueuedItem.TotalMassKg <= KINDA_SMALL_NUMBER)
	{
		OutQueuedItem.TotalMassKg = ResolvePrimitiveMassKgWithoutPhysicsWarning(SourcePrimitive);
	}

	return OutQueuedItem.ActorClass.Get() != nullptr;
}

float UBlackHoleBackpackLinkComponent::ResolveResourceMassPerUnitKg(FName ResourceId) const
{
	if (ResourceId.IsNone())
	{
		return 0.0f;
	}

	if (const float* FoundMassPerUnit = ResourceMassPerUnitKgById.Find(ResourceId))
	{
		return FMath::Max(0.0f, *FoundMassPerUnit);
	}

	for (TObjectIterator<UMaterialDefinitionAsset> It; It; ++It)
	{
		const UMaterialDefinitionAsset* MaterialDefinition = *It;
		if (!MaterialDefinition || MaterialDefinition->GetResolvedMaterialId() != ResourceId)
		{
			continue;
		}

		const float MassPerUnitKg = FMath::Max(0.0f, MaterialDefinition->GetMassPerUnitKg());
		ResourceMassPerUnitKgById.FindOrAdd(ResourceId) = MassPerUnitKg;
		return MassPerUnitKg;
	}

	ResourceMassPerUnitKgById.FindOrAdd(ResourceId) = 0.0f;
	return 0.0f;
}

float UBlackHoleBackpackLinkComponent::ComputeResourceMassKg(const TMap<FName, int32>& ResourceQuantitiesScaled) const
{
	float TotalMassKg = 0.0f;
	for (const TPair<FName, int32>& Pair : ResourceQuantitiesScaled)
	{
		const FName ResourceId = Pair.Key;
		const int32 QuantityScaled = FMath::Max(0, Pair.Value);
		if (ResourceId.IsNone() || QuantityScaled <= 0)
		{
			continue;
		}

		const float Units = AgentResource::ScaledToUnits(QuantityScaled);
		TotalMassKg += Units * ResolveResourceMassPerUnitKg(ResourceId);
	}

	return FMath::Max(0.0f, TotalMassKg);
}

void UBlackHoleBackpackLinkComponent::UpdateBlackHoleIntakeState(bool bForceDisable)
{
	if (!bUseTeleportQueueFlow)
	{
		return;
	}

	UInputVolume* BackpackInput = ResolveBackpackInputVolume();
	if (!BackpackInput)
	{
		return;
	}

	const bool bShouldEnable = bBlackHoleEnabled && !bForceDisable;
	BackpackInput->bEnabled = bShouldEnable;
	BackpackInput->SetComponentTickEnabled(bShouldEnable);
	BackpackInput->SetGenerateOverlapEvents(bShouldEnable);
	BackpackInput->SetCollisionEnabled(bShouldEnable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

UInputVolume* UBlackHoleBackpackLinkComponent::ResolveBackpackInputVolume() const
{
	return OwningBackItem ? OwningBackItem->InputVolume.Get() : nullptr;
}

UPrimitiveComponent* UBlackHoleBackpackLinkComponent::ResolveResourceSourcePrimitive(AActor* Actor) const
{
	if (!Actor)
	{
		return nullptr;
	}

	if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
	{
		return RootPrimitive;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	UPrimitiveComponent* FirstUsablePrimitive = nullptr;
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent || PrimitiveComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
		{
			continue;
		}

		if (!FirstUsablePrimitive)
		{
			FirstUsablePrimitive = PrimitiveComponent;
		}

		if (PrimitiveComponent->IsSimulatingPhysics())
		{
			return PrimitiveComponent;
		}
	}

	return FirstUsablePrimitive;
}
