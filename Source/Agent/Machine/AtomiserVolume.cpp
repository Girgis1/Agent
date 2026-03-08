// Copyright Epic Games, Inc. All Rights Reserved.

#include "Machine/AtomiserVolume.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Factory/FactoryPayloadActor.h"
#include "Material/MaterialComponent.h"
#include "Material/AgentResourceTypes.h"
#include <limits>

UAtomiserVolume::UAtomiserVolume()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	InitBoxExtent(FVector(40.0f, 40.0f, 40.0f));

	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionObjectType(ECC_WorldDynamic);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	SetGenerateOverlapEvents(true);
	SetCanEverAffectNavigation(false);
}

void UAtomiserVolume::BeginPlay()
{
	Super::BeginPlay();
}

void UAtomiserVolume::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEnabled)
	{
		return;
	}

	TimeUntilNextIntake = FMath::Max(0.0f, TimeUntilNextIntake - FMath::Max(0.0f, DeltaTime));
	if (TimeUntilNextIntake > 0.0f)
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	GetOverlappingActors(OverlappingActors);

	for (AActor* OverlappingActor : OverlappingActors)
	{
		if (TryConsumeOverlappingActor(OverlappingActor))
		{
			TimeUntilNextIntake = FMath::Max(0.0f, IntakeInterval);
			break;
		}
	}
}

bool UAtomiserVolume::IsActorAcceptedByWhitelist(AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}

	if (!bUseBlueprintWhitelist)
	{
		return true;
	}

	for (const TSoftClassPtr<AActor>& AllowedClassPtr : AllowedBlueprintClasses)
	{
		if (AllowedClassPtr.IsNull())
		{
			continue;
		}

		UClass* AllowedClass = AllowedClassPtr.Get();
		if (!AllowedClass)
		{
			AllowedClass = AllowedClassPtr.LoadSynchronous();
		}

		if (AllowedClass && Actor->IsA(AllowedClass))
		{
			return true;
		}
	}

	return false;
}

float UAtomiserVolume::GetStoredUnits(FName ResourceId) const
{
	return AgentResource::ScaledToUnits(GetStoredScaled(ResourceId));
}

float UAtomiserVolume::GetTotalStoredUnits() const
{
	return AgentResource::ScaledToUnits(TotalStoredScaled);
}

int32 UAtomiserVolume::GetStoredScaled(FName ResourceId) const
{
	if (ResourceId.IsNone())
	{
		return TotalStoredScaled;
	}

	return FMath::Max(0, StoredResourcesScaled.FindRef(ResourceId));
}

float UAtomiserVolume::GetCapacityUnits() const
{
	return bInfiniteStorage ? -1.0f : AgentResource::ScaledToUnits(GetCapacityScaled());
}

float UAtomiserVolume::GetRemainingCapacityUnits() const
{
	return bInfiniteStorage ? -1.0f : AgentResource::ScaledToUnits(GetRemainingCapacityScaled());
}

int32 UAtomiserVolume::GetRemainingCapacityScaled() const
{
	if (bInfiniteStorage)
	{
		return TNumericLimits<int32>::Max();
	}

	return FMath::Max(0, GetCapacityScaled() - TotalStoredScaled);
}

bool UAtomiserVolume::HasResourceQuantityUnits(FName ResourceId, int32 QuantityUnits) const
{
	return HasResourceQuantityScaled(ResourceId, AgentResource::WholeUnitsToScaled(QuantityUnits));
}

bool UAtomiserVolume::HasResourceQuantityScaled(FName ResourceId, int32 QuantityScaled) const
{
	if (ResourceId.IsNone() || QuantityScaled <= 0)
	{
		return false;
	}

	return StoredResourcesScaled.FindRef(ResourceId) >= QuantityScaled;
}

bool UAtomiserVolume::ConsumeResourceQuantityUnits(FName ResourceId, int32 QuantityUnits)
{
	return ConsumeResourceQuantityScaled(ResourceId, AgentResource::WholeUnitsToScaled(QuantityUnits));
}

bool UAtomiserVolume::ConsumeResourceQuantityScaled(FName ResourceId, int32 QuantityScaled)
{
	if (ResourceId.IsNone() || QuantityScaled <= 0)
	{
		return false;
	}

	TMap<FName, int32> SingleResource;
	SingleResource.Add(ResourceId, QuantityScaled);
	return ConsumeResourcesScaledAtomic(SingleResource);
}

bool UAtomiserVolume::ConsumeResourcesScaledAtomic(const TMap<FName, int32>& ResourceQuantitiesScaled)
{
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
	ShowDebugMessage(FString::Printf(TEXT("Consumed %.2f units"), AgentResource::ScaledToUnits(static_cast<int32>(TotalToConsumeScaled))), FColor::Yellow);
	BroadcastStorageChanged();
	return true;
}

bool UAtomiserVolume::AddResourcesScaledAtomic(const TMap<FName, int32>& ResourceQuantitiesScaled)
{
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
	ShowDebugMessage(FString::Printf(TEXT("Stored %.2f units (Total %.2f)"), AgentResource::ScaledToUnits(static_cast<int32>(AddedTotalScaled)), GetTotalStoredUnits()), FColor::Cyan);
	BroadcastStorageChanged();
	return true;
}

void UAtomiserVolume::ClearStoredResources()
{
	if (StoredResourcesScaled.Num() == 0 && TotalStoredScaled == 0)
	{
		return;
	}

	StoredResourcesScaled.Reset();
	TotalStoredScaled = 0;
	ShowDebugMessage(TEXT("Storage cleared"), FColor::Silver);
	BroadcastStorageChanged();
}

void UAtomiserVolume::GetStorageSnapshot(TArray<FAtomiserStoredResourceEntry>& OutEntries) const
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

		FAtomiserStoredResourceEntry Entry;
		Entry.ResourceId = ResourceId;
		Entry.QuantityScaled = QuantityScaled;
		Entry.Units = AgentResource::ScaledToUnits(QuantityScaled);
		OutEntries.Add(Entry);
	}
}

bool UAtomiserVolume::TryConsumeOverlappingActor(AActor* OverlappingActor)
{
	if (!IsActorEligibleForAtomiser(OverlappingActor))
	{
		return false;
	}

	TMap<FName, int32> ResolvedResourcesScaled;
	if (!TryBuildActorResourceMap(OverlappingActor, ResolvedResourcesScaled))
	{
		ShowDebugMessage(TEXT("Rejected actor with no valid material contents"), FColor::Red);
		return false;
	}

	if (!AddResourcesScaledAtomic(ResolvedResourcesScaled))
	{
		ShowDebugMessage(TEXT("Rejected actor due to capacity or overflow"), FColor::Red);
		return false;
	}

	OverlappingActor->Destroy();
	return true;
}

bool UAtomiserVolume::TryBuildActorResourceMap(const AActor* SourceActor, TMap<FName, int32>& OutResourceQuantitiesScaled) const
{
	OutResourceQuantitiesScaled.Reset();
	if (!SourceActor)
	{
		return false;
	}

	UMaterialComponent* MaterialComponent = SourceActor->FindComponentByClass<UMaterialComponent>();
	if (!MaterialComponent)
	{
		return false;
	}

	UPrimitiveComponent* SourcePrimitive = ResolveSourcePrimitive(SourceActor);
	if (!MaterialComponent->BuildResolvedResourceQuantitiesScaled(SourcePrimitive, OutResourceQuantitiesScaled))
	{
		return false;
	}

	for (auto It = OutResourceQuantitiesScaled.CreateIterator(); It; ++It)
	{
		if (It.Key().IsNone() || It.Value() <= 0)
		{
			It.RemoveCurrent();
		}
	}

	return OutResourceQuantitiesScaled.Num() > 0;
}

bool UAtomiserVolume::IsActorEligibleForAtomiser(const AActor* SourceActor) const
{
	if (!IsValid(SourceActor) || SourceActor == GetOwner())
	{
		return false;
	}

	if (!IsActorAcceptedByWhitelist(const_cast<AActor*>(SourceActor)))
	{
		ShowDebugMessage(TEXT("Rejected actor not in whitelist"), FColor::Red);
		return false;
	}

	if (bRejectFactoryPayloadActors && SourceActor->IsA<AFactoryPayloadActor>())
	{
		ShowDebugMessage(TEXT("Rejected payload actor (pure material actors only)"), FColor::Red);
		return false;
	}

	if (bRequireMaterialComponent && !SourceActor->FindComponentByClass<UMaterialComponent>())
	{
		ShowDebugMessage(TEXT("Rejected actor without MaterialComponent"), FColor::Red);
		return false;
	}

	return true;
}

bool UAtomiserVolume::CanAcceptResourcesAtomic(const TMap<FName, int32>& ResourceQuantitiesScaled) const
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

void UAtomiserVolume::ShowDebugMessage(const FString& Message, FColor Color) const
{
	if (!bShowDebugMessages || !GEngine)
	{
		return;
	}

	const FString Prefix = DebugName.IsNone() ? TEXT("Atomiser") : DebugName.ToString();
	GEngine->AddOnScreenDebugMessage(
		static_cast<uint64>(GetUniqueID()) + 26000ULL,
		1.0f,
		Color,
		FString::Printf(TEXT("%s: %s"), *Prefix, *Message));
}

void UAtomiserVolume::BroadcastStorageChanged()
{
	OnStorageChanged.Broadcast();
}

UPrimitiveComponent* UAtomiserVolume::ResolveSourcePrimitive(const AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	AActor* MutableActor = const_cast<AActor*>(Actor);
	if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(MutableActor->GetRootComponent()))
	{
		return RootPrimitive;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	MutableActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

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

int32 UAtomiserVolume::GetCapacityScaled() const
{
	return bInfiniteStorage
		? TNumericLimits<int32>::Max()
		: AgentResource::WholeUnitsToScaled(CapacityUnits);
}
