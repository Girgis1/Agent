// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldUIHostComponent.h"
#include "WorldUIActor.h"
#include "WorldUIDataProviderInterface.h"
#include "WorldUIRegistrySubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UWorldUIHostComponent::UWorldUIHostComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetMobility(EComponentMobility::Movable);
	bEditableWhenInherited = true;
}

void UWorldUIHostComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		if (UWorldUIRegistrySubsystem* Registry = World->GetSubsystem<UWorldUIRegistrySubsystem>())
		{
			Registry->RegisterHost(this);
		}
	}
}

void UWorldUIHostComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		if (UWorldUIRegistrySubsystem* Registry = World->GetSubsystem<UWorldUIRegistrySubsystem>())
		{
			Registry->UnregisterHost(this);
		}
	}

	Super::EndPlay(EndPlayReason);
}

AActor* UWorldUIHostComponent::GetResolvedProviderActor() const
{
	return ProviderActorOverride ? ProviderActorOverride.Get() : GetOwner();
}

bool UWorldUIHostComponent::BuildWorldUIModel(FWorldUIModel& OutModel) const
{
	OutModel = FWorldUIModel();

	AActor* ProviderActor = GetResolvedProviderActor();
	if (!ProviderActor || !ProviderActor->GetClass()->ImplementsInterface(UWorldUIDataProviderInterface::StaticClass()))
	{
		return false;
	}

	return IWorldUIDataProviderInterface::Execute_BuildWorldUIModel(ProviderActor, OutModel);
}

bool UWorldUIHostComponent::HandleWorldUIAction(const FWorldUIAction& Action) const
{
	AActor* ProviderActor = GetResolvedProviderActor();
	if (!ProviderActor || !ProviderActor->GetClass()->ImplementsInterface(UWorldUIDataProviderInterface::StaticClass()))
	{
		return false;
	}

	return IWorldUIDataProviderInterface::Execute_HandleWorldUIAction(ProviderActor, Action);
}

FVector UWorldUIHostComponent::GetPanelWorldLocation() const
{
	return GetComponentLocation();
}

FRotator UWorldUIHostComponent::GetPanelWorldRotation() const
{
	return GetComponentRotation();
}

FVector UWorldUIHostComponent::GetLeaderAnchorWorldLocation() const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return GetComponentLocation();
	}

	if (!bUseOwnerLocationForLeaderAnchor)
	{
		return GetComponentTransform().TransformPosition(LeaderAnchorLocalOffset);
	}

	const FVector OwnerLocation = OwnerActor->GetActorLocation();
	return OwnerLocation + OwnerActor->GetActorTransform().TransformVectorNoScale(LeaderAnchorLocalOffset);
}
