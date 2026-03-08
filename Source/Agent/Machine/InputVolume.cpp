// Copyright Epic Games, Inc. All Rights Reserved.

#include "Machine/InputVolume.h"
#include "Components/PrimitiveComponent.h"
#include "Factory/FactoryPayloadActor.h"
#include "Material/MaterialComponent.h"
#include "Material/AgentResourceTypes.h"
#include "Machine/MachineComponent.h"

namespace
{
FResourceAmount GetResolvedMachineInputPayloadAmount(const AFactoryPayloadActor* PayloadActor)
{
	if (!PayloadActor)
	{
		return FResourceAmount{};
	}

	FResourceAmount PayloadAmount = PayloadActor->GetPayloadResource();
	if (!PayloadAmount.HasQuantity())
	{
		PayloadAmount.ResourceId = PayloadActor->GetPayloadType();
		PayloadAmount.SetWholeUnits(1);
	}

	return PayloadAmount;
}

UPrimitiveComponent* ResolveResourceSourcePrimitive(AActor* Actor)
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
}

UInputVolume::UInputVolume()
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

void UInputVolume::BeginPlay()
{
	Super::BeginPlay();
	ResolveMachineComponent();
}

void UInputVolume::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEnabled || !ResolveMachineComponent())
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

	bool bConsumedAny = false;
	for (AActor* OverlappingActor : OverlappingActors)
	{
		if (TryConsumeOverlappingActor(OverlappingActor))
		{
			bConsumedAny = true;
		}
	}

	if (bConsumedAny)
	{
		TimeUntilNextIntake = FMath::Max(0.0f, IntakeInterval);
	}
}

void UInputVolume::SetMachineComponent(UMachineComponent* InMachineComponent)
{
	MachineComponent = InMachineComponent;
}

bool UInputVolume::ResolveMachineComponent()
{
	if (MachineComponent)
	{
		return true;
	}

	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return false;
	}

	MachineComponent = OwnerActor->FindComponentByClass<UMachineComponent>();
	return MachineComponent != nullptr;
}

bool UInputVolume::TryConsumeOverlappingActor(AActor* OverlappingActor)
{
	if (!MachineComponent || !OverlappingActor)
	{
		return false;
	}

	if (AFactoryPayloadActor* PayloadActor = Cast<AFactoryPayloadActor>(OverlappingActor))
	{
		FResourceAmount PayloadResource = GetResolvedMachineInputPayloadAmount(PayloadActor);
		if (!PayloadResource.HasQuantity())
		{
			return false;
		}

		const FName ResourceId = PayloadResource.ResourceId;
		const int32 QuantityScaled = PayloadResource.GetScaledQuantity();
		if (ResourceId.IsNone() || QuantityScaled <= 0)
		{
			return false;
		}

		const int32 AcceptedScaled = MachineComponent->AddInputResourceScaled(ResourceId, QuantityScaled);
		if (AcceptedScaled <= 0)
		{
			return false;
		}

		const int32 RemainingScaled = FMath::Max(0, QuantityScaled - AcceptedScaled);
		if (RemainingScaled <= 0 && bDestroyAcceptedPayloads)
		{
			PayloadActor->Destroy();
		}
		else
		{
			PayloadResource.SetScaledQuantity(RemainingScaled);
			PayloadActor->SetPayloadResource(PayloadResource);
		}

		return true;
	}

	UMaterialComponent* ResourceComponent = OverlappingActor->FindComponentByClass<UMaterialComponent>();
	if (!ResourceComponent)
	{
		return false;
	}

	UPrimitiveComponent* SourcePrimitive = ResolveResourceSourcePrimitive(OverlappingActor);

	TMap<FName, int32> ActorResourcesScaled;
	if (!ResourceComponent->BuildResolvedResourceQuantitiesScaled(SourcePrimitive, ActorResourcesScaled))
	{
		return false;
	}

	if (!MachineComponent->AddInputResourcesScaledAtomic(ActorResourcesScaled))
	{
		return false;
	}

	// Resource actors don't currently support per-resource depletion updates, so we destroy on full intake.
	OverlappingActor->Destroy();
	return true;
}

