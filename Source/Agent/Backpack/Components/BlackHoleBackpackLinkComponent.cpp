// Copyright Epic Games, Inc. All Rights Reserved.

#include "Backpack/Components/BlackHoleBackpackLinkComponent.h"
#include "Backpack/Actors/BlackHoleBackpackActor.h"
#include "Backpack/BlackHoleEndpointRegistrySubsystem.h"
#include "Backpack/Components/BlackHoleOutputSinkComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Machine/MachineComponent.h"
#include "Material/AgentResourceTypes.h"
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

	if (!ResolveDependencies() || !ShouldTransferForCurrentDeploymentState())
	{
		return;
	}

	TimeUntilNextTransfer = FMath::Max(0.0f, TimeUntilNextTransfer - FMath::Max(0.0f, DeltaTime));
	if (TimeUntilNextTransfer > 0.0f)
	{
		return;
	}

	TransferBufferedResourcesNow();
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
	if (!ResolveDependencies())
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
