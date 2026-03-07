// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/FactoryVolumeComponentBase.h"
#include "Material/MaterialTypes.h"

UFactoryVolumeComponentBase::UFactoryVolumeComponentBase()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	InitBoxExtent(FVector(40.0f, 40.0f, 40.0f));
}

void UFactoryVolumeComponentBase::OnRegister()
{
	Super::OnRegister();

	SetCollisionEnabled(GetConfiguredCollisionEnabled());
	SetCollisionObjectType(ECC_WorldDynamic);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	SetGenerateOverlapEvents(GetConfiguredGenerateOverlapEvents());
	SetCanEverAffectNavigation(false);
}

void UFactoryVolumeComponentBase::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEnabled)
	{
		return;
	}

	TimeUntilNextProcess = FMath::Max(0.0f, TimeUntilNextProcess - FMath::Max(0.0f, DeltaTime));
	if (TimeUntilNextProcess > 0.0f)
	{
		return;
	}

	TArray<AActor*> OverlappingActors;
	GetOverlappingActors(OverlappingActors);

	for (AActor* OverlappingActor : OverlappingActors)
	{
		if (TryProcessOverlappingActor(OverlappingActor))
		{
			TimeUntilNextProcess = FMath::Max(0.0f, ProcessingInterval);
			break;
		}
	}
}

int32 UFactoryVolumeComponentBase::GetMaxQuantityScaled() const
{
	return AgentResource::WholeUnitsToScaled(MaxQuantityUnits);
}

int32 UFactoryVolumeComponentBase::GetCurrentQuantityScaled() const
{
	return GetCurrentStoredQuantityScaled();
}

bool UFactoryVolumeComponentBase::IsResourceBlocked(FName ResourceId) const
{
	return !ResourceId.IsNone() && BlockedResourceIds.Contains(ResourceId);
}

bool UFactoryVolumeComponentBase::TryProcessOverlappingActor(AActor* OverlappingActor)
{
	(void)OverlappingActor;
	return false;
}

int32 UFactoryVolumeComponentBase::GetCurrentStoredQuantityScaled() const
{
	return 0;
}

ECollisionEnabled::Type UFactoryVolumeComponentBase::GetConfiguredCollisionEnabled() const
{
	return ECollisionEnabled::QueryOnly;
}

bool UFactoryVolumeComponentBase::GetConfiguredGenerateOverlapEvents() const
{
	return true;
}

bool UFactoryVolumeComponentBase::HasCapacityForAdditionalQuantity(int32 AdditionalQuantityScaled) const
{
	if (AdditionalQuantityScaled <= 0)
	{
		return true;
	}

	const int32 MaxQuantityScaled = GetMaxQuantityScaled();
	if (MaxQuantityScaled <= 0)
	{
		return true;
	}

	return (GetCurrentStoredQuantityScaled() + AdditionalQuantityScaled) <= MaxQuantityScaled;
}

