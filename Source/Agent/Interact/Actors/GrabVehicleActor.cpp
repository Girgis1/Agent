// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interact/Actors/GrabVehicleActor.h"
#include "Components/ArrowComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Interact/Components/AgentInteractorComponent.h"
#include "Interact/Components/GrabVehicleComponent.h"
#include "Interact/Components/GrabVehiclePushComponent.h"
#include "Interact/Components/InteractVolumeComponent.h"

AGrabVehicleActor::AGrabVehicleActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	VehicleBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("VehicleBody"));
	VehicleBody->SetupAttachment(SceneRoot);
	VehicleBody->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	VehicleBody->SetCollisionObjectType(ECC_PhysicsBody);
	VehicleBody->SetCollisionResponseToAllChannels(ECR_Block);
	VehicleBody->SetSimulatePhysics(true);
	VehicleBody->SetEnableGravity(true);

	InteractVolume = CreateDefaultSubobject<UInteractVolumeComponent>(TEXT("InteractVolume"));
	InteractVolume->SetupAttachment(SceneRoot);
	InteractVolume->InitBoxExtent(FVector(150.0f, 120.0f, 100.0f));

	GrabVehicleComponent = CreateDefaultSubobject<UGrabVehicleComponent>(TEXT("GrabVehicleComponent"));

	GrabPoint = CreateDefaultSubobject<USceneComponent>(TEXT("GrabPoint"));
	GrabPoint->SetupAttachment(VehicleBody);
	GrabPoint->SetRelativeLocation(FVector(-65.0f, 0.0f, 62.0f));

	ForwardArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("ForwardArrow"));
	ForwardArrow->SetupAttachment(VehicleBody);
	ForwardArrow->SetRelativeLocation(FVector::ZeroVector);
	ForwardArrow->SetArrowColor(FColor::Green);
	ForwardArrow->ArrowSize = 1.5f;
	ForwardArrow->bTreatAsASprite = false;
}

void AGrabVehicleActor::BeginPlay()
{
	Super::BeginPlay();
	RefreshPhysicsBodyBinding();
	if (GrabVehicleComponent)
	{
		GrabVehicleComponent->ApplyInteractionVolume(InteractVolume);
	}
}

void AGrabVehicleActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	if (GrabVehicleComponent)
	{
		GrabVehicleComponent->ApplyInteractionVolume(InteractVolume);
	}
}

void AGrabVehicleActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!ActiveInteractor.IsValid())
	{
		return;
	}

	AActor* InteractorActor = ActiveInteractor.Get();
	UGrabVehiclePushComponent* PushComponent = InteractorActor
		? InteractorActor->FindComponentByClass<UGrabVehiclePushComponent>()
		: nullptr;
	if (!PushComponent || !PushComponent->IsPushingActor(this))
	{
		StopInteraction(TEXT("PushGrabLost"));
	}
}

bool AGrabVehicleActor::CanInteract_Implementation(AActor* Interactor) const
{
	UPrimitiveComponent* PhysicsBody = ActivePhysicsBody ? ActivePhysicsBody.Get() : ResolvePhysicsBody();
	if (!Interactor || !PhysicsBody || !PhysicsBody->IsSimulatingPhysics())
	{
		return false;
	}

	const UGrabVehiclePushComponent* PushComponent = Interactor->FindComponentByClass<UGrabVehiclePushComponent>();
	if (!PushComponent)
	{
		return false;
	}

	if (PushComponent->IsPushing() && !PushComponent->IsPushingActor(this))
	{
		return false;
	}

	if (ActiveInteractor.Get() == Interactor)
	{
		return true;
	}

	const float AllowedDistance = GrabVehicleComponent
		? GrabVehicleComponent->GetMaxInteractDistance()
		: 280.0f;
	if (AllowedDistance > 0.0f)
	{
		const FVector InteractionLocation = GetInteractionLocation_Implementation(Interactor);
		if (FVector::DistSquared(Interactor->GetActorLocation(), InteractionLocation) > FMath::Square(AllowedDistance))
		{
			return false;
		}
	}

	return !ActiveInteractor.IsValid() || ActiveInteractor.Get() == Interactor;
}

FVector AGrabVehicleActor::GetInteractionLocation_Implementation(AActor* Interactor) const
{
	if (GrabPoint)
	{
		return GrabPoint->GetComponentLocation();
	}

	if (ForwardArrow)
	{
		return ForwardArrow->GetComponentLocation();
	}

	return GetActorLocation();
}

bool AGrabVehicleActor::BeginInteract_Implementation(AActor* Interactor)
{
	RefreshPhysicsBodyBinding();
	if (!CanInteract_Implementation(Interactor))
	{
		return false;
	}

	if (ActiveInteractor.Get() == Interactor)
	{
		return true;
	}

	UGrabVehiclePushComponent* PushComponent = Interactor
		? Interactor->FindComponentByClass<UGrabVehiclePushComponent>()
		: nullptr;
	if (!PushComponent || !ActivePhysicsBody)
	{
		return false;
	}

	if (GrabVehicleComponent)
	{
		GrabVehicleComponent->ApplyPushTuning(PushComponent, PushComponent->DebugSphereRadius);
	}

	const FVector GrabLocation = GetInteractionLocation_Implementation(Interactor);
	if (!PushComponent->BeginPush(this, ActivePhysicsBody, GrabLocation))
	{
		return false;
	}

	ActiveInteractor = Interactor;
	return true;
}

void AGrabVehicleActor::EndInteract_Implementation(AActor* Interactor)
{
	if (!ActiveInteractor.IsValid())
	{
		return;
	}

	if (Interactor && ActiveInteractor.Get() != Interactor)
	{
		return;
	}

	StopInteraction(TEXT("ManualDetach"));
}

bool AGrabVehicleActor::IsInteracting_Implementation(AActor* Interactor) const
{
	if (!ActiveInteractor.IsValid())
	{
		return false;
	}

	return Interactor ? ActiveInteractor.Get() == Interactor : true;
}

FText AGrabVehicleActor::GetInteractPrompt_Implementation() const
{
	return ActiveInteractor.IsValid()
		? FText::FromString(TEXT("Release Vehicle"))
		: FText::FromString(TEXT("Grab Vehicle"));
}

void AGrabVehicleActor::RefreshPhysicsBodyBinding()
{
	ActivePhysicsBody = ResolvePhysicsBody();
}

UPrimitiveComponent* AGrabVehicleActor::ResolvePhysicsBody() const
{
	if (VehicleBody && VehicleBody->IsSimulatingPhysics())
	{
		return VehicleBody;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent || PrimitiveComponent == InteractVolume)
		{
			continue;
		}

		if (!PrimitiveComponent->IsSimulatingPhysics())
		{
			continue;
		}

		return PrimitiveComponent;
	}

	return VehicleBody;
}

void AGrabVehicleActor::StopInteraction(FName Reason)
{
	(void)Reason;

	AActor* PreviousInteractor = ActiveInteractor.Get();
	ActiveInteractor.Reset();

	if (!PreviousInteractor)
	{
		return;
	}

	if (UGrabVehiclePushComponent* PushComponent = PreviousInteractor->FindComponentByClass<UGrabVehiclePushComponent>())
	{
		if (PushComponent->IsPushingActor(this))
		{
			PushComponent->EndPush();
		}
	}

	if (UAgentInteractorComponent* InteractorComponent = PreviousInteractor->FindComponentByClass<UAgentInteractorComponent>())
	{
		if (InteractorComponent->GetActiveInteractable() == this)
		{
			InteractorComponent->EndCurrentInteraction();
		}
	}
}
