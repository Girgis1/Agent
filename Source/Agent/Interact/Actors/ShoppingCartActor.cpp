// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interact/Actors/ShoppingCartActor.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Interact/Components/InteractVolumeComponent.h"
#include "Interact/Components/AgentInteractorComponent.h"
#include "Interact/Components/DualHandleGrabComponent.h"
#include "Interact/Components/PhysicsDragFollowerComponent.h"
#include "Interact/Components/TiltReleaseSafetyComponent.h"
#include "Interact/Components/UprightStabilizerComponent.h"
#include "Agent.h"

AShoppingCartActor::AShoppingCartActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CartBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CartBody"));
	CartBody->SetupAttachment(SceneRoot);
	CartBody->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CartBody->SetCollisionObjectType(ECC_PhysicsBody);
	CartBody->SetCollisionResponseToAllChannels(ECR_Block);
	CartBody->SetSimulatePhysics(true);
	CartBody->SetEnableGravity(true);

	HandleAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("HandleAnchor"));
	HandleAnchor->SetupAttachment(CartBody);
	HandleAnchor->SetRelativeLocation(FVector(-80.0f, 0.0f, 82.0f));

	InteractVolume = CreateDefaultSubobject<UInteractVolumeComponent>(TEXT("InteractVolume"));
	InteractVolume->SetupAttachment(SceneRoot);
	InteractVolume->InitBoxExtent(FVector(130.0f, 110.0f, 95.0f));

	DragFollower = CreateDefaultSubobject<UPhysicsDragFollowerComponent>(TEXT("DragFollower"));
	UprightStabilizer = CreateDefaultSubobject<UUprightStabilizerComponent>(TEXT("UprightStabilizer"));
	TiltReleaseSafety = CreateDefaultSubobject<UTiltReleaseSafetyComponent>(TEXT("TiltReleaseSafety"));
}

void AShoppingCartActor::BeginPlay()
{
	Super::BeginPlay();

	RefreshPhysicsBodyBinding();

	if (DragFollower)
	{
		DragFollower->SetPhysicsBody(ActivePhysicsBody);
		DragFollower->SetDrivePoint(HandleAnchor);
	}

	if (UprightStabilizer)
	{
		UprightStabilizer->SetPhysicsBody(ActivePhysicsBody);
		UprightStabilizer->SetStabilizationEnabled(false);
	}

	if (TiltReleaseSafety)
	{
		TiltReleaseSafety->SetPhysicsBody(ActivePhysicsBody);
		TiltReleaseSafety->OnAutoReleaseRequested.AddUObject(this, &AShoppingCartActor::HandleAutoReleaseRequested);
	}
}

void AShoppingCartActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (ActiveInteractor.IsValid())
	{
		AActor* InteractorActor = ActiveInteractor.Get();
		UDualHandleGrabComponent* DualHandleGrab = InteractorActor
			? InteractorActor->FindComponentByClass<UDualHandleGrabComponent>()
			: nullptr;
		if (!DualHandleGrab || !DualHandleGrab->IsGrabbingActor(this))
		{
			StopInteraction(TEXT("DualGrabLost"));
		}
	}
}

bool AShoppingCartActor::CanInteract_Implementation(AActor* Interactor) const
{
	UPrimitiveComponent* PhysicsBody = ActivePhysicsBody ? ActivePhysicsBody.Get() : ResolvePhysicsBody();
	if (!Interactor || !PhysicsBody || !PhysicsBody->IsSimulatingPhysics())
	{
		return false;
	}

	const UDualHandleGrabComponent* DualHandleGrab = Interactor->FindComponentByClass<UDualHandleGrabComponent>();
	if (!DualHandleGrab)
	{
		return false;
	}

	if (DualHandleGrab->IsGrabbing() && !DualHandleGrab->IsGrabbingActor(this))
	{
		return false;
	}

	const FVector InteractionLocation = GetInteractionLocation_Implementation(Interactor);
	const float MaxDistance = FMath::Max(0.0f, MaxInteractDistance);
	if (MaxDistance > 0.0f
		&& FVector::DistSquared(Interactor->GetActorLocation(), InteractionLocation) > FMath::Square(MaxDistance))
	{
		return false;
	}

	return !ActiveInteractor.IsValid() || ActiveInteractor.Get() == Interactor;
}

FVector AShoppingCartActor::GetInteractionLocation_Implementation(AActor* Interactor) const
{
	if (HandleAnchor)
	{
		return HandleAnchor->GetComponentLocation();
	}

	return GetActorLocation();
}

bool AShoppingCartActor::BeginInteract_Implementation(AActor* Interactor)
{
	RefreshPhysicsBodyBinding();

	if (!CanInteract_Implementation(Interactor))
	{
		UE_LOG(
			LogAgent,
			Warning,
			TEXT("[Interact] Cart '%s' rejected interaction. ActivePhysicsBody='%s' sim=%s"),
			*GetNameSafe(this),
			*GetNameSafe(ActivePhysicsBody),
			(ActivePhysicsBody && ActivePhysicsBody->IsSimulatingPhysics()) ? TEXT("true") : TEXT("false"));
		return false;
	}

	if (ActiveInteractor.Get() == Interactor)
	{
		return true;
	}

	UDualHandleGrabComponent* DualHandleGrab = Interactor
		? Interactor->FindComponentByClass<UDualHandleGrabComponent>()
		: nullptr;
	if (!DualHandleGrab || !DualHandleGrab->BeginGrab(this, ActivePhysicsBody))
	{
		return false;
	}

	ActiveInteractor = Interactor;

	if (TiltReleaseSafety)
	{
		TiltReleaseSafety->BeginMonitoring(Interactor, HandleAnchor);
	}

	return true;
}

void AShoppingCartActor::EndInteract_Implementation(AActor* Interactor)
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

bool AShoppingCartActor::IsInteracting_Implementation(AActor* Interactor) const
{
	if (!ActiveInteractor.IsValid())
	{
		return false;
	}

	return Interactor ? ActiveInteractor.Get() == Interactor : true;
}

FText AShoppingCartActor::GetInteractPrompt_Implementation() const
{
	return ActiveInteractor.IsValid()
		? FText::FromString(TEXT("Release Cart"))
		: FText::FromString(TEXT("Grab Cart"));
}

void AShoppingCartActor::StopInteraction(FName Reason)
{
	(void)Reason;

	AActor* PreviousInteractor = ActiveInteractor.Get();

	if (DragFollower)
	{
		DragFollower->StopFollowing();
	}

	if (TiltReleaseSafety)
	{
		TiltReleaseSafety->StopMonitoring();
	}

	ActiveInteractor.Reset();

	if (PreviousInteractor)
	{
		if (UDualHandleGrabComponent* DualHandleGrab = PreviousInteractor->FindComponentByClass<UDualHandleGrabComponent>())
		{
			if (DualHandleGrab->IsGrabbingActor(this))
			{
				DualHandleGrab->EndGrab();
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
}

void AShoppingCartActor::HandleAutoReleaseRequested(FName Reason)
{
	StopInteraction(Reason);
}

void AShoppingCartActor::RefreshPhysicsBodyBinding()
{
	ActivePhysicsBody = ResolvePhysicsBody();
	if (ActivePhysicsBody && ActivePhysicsBody != CartBody)
	{
		UE_LOG(
			LogAgent,
			Log,
			TEXT("[Interact] Cart '%s' using fallback physics body '%s'"),
			*GetNameSafe(this),
			*GetNameSafe(ActivePhysicsBody));
	}
}

UPrimitiveComponent* AShoppingCartActor::ResolvePhysicsBody() const
{
	if (CartBody && CartBody->IsSimulatingPhysics())
	{
		return CartBody;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent)
		{
			continue;
		}

		if (PrimitiveComponent == InteractVolume)
		{
			continue;
		}

		if (!PrimitiveComponent->IsSimulatingPhysics())
		{
			continue;
		}

		return PrimitiveComponent;
	}

	return CartBody;
}
