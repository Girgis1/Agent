// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interact/Actors/DragCubeActor.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Interact/Components/AgentInteractorComponent.h"
#include "Interact/Components/InteractVolumeComponent.h"
#include "Interact/Components/PhysicsDragFollowerComponent.h"
#include "UObject/ConstructorHelpers.h"

ADragCubeActor::ADragCubeActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	CubeBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CubeBody"));
	CubeBody->SetupAttachment(SceneRoot);
	CubeBody->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CubeBody->SetCollisionObjectType(ECC_PhysicsBody);
	CubeBody->SetCollisionResponseToAllChannels(ECR_Block);
	CubeBody->SetSimulatePhysics(true);
	CubeBody->SetEnableGravity(true);
	CubeBody->SetLinearDamping(0.6f);
	CubeBody->SetAngularDamping(0.4f);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (CubeMesh.Succeeded())
	{
		CubeBody->SetStaticMesh(CubeMesh.Object);
	}

	HandleAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("HandleAnchor"));
	HandleAnchor->SetupAttachment(CubeBody);
	HandleAnchor->SetRelativeLocation(FVector(-60.0f, 0.0f, 48.0f));

	InteractVolume = CreateDefaultSubobject<UInteractVolumeComponent>(TEXT("InteractVolume"));
	InteractVolume->SetupAttachment(SceneRoot);
	InteractVolume->InitBoxExtent(FVector(140.0f, 120.0f, 95.0f));

	DragFollower = CreateDefaultSubobject<UPhysicsDragFollowerComponent>(TEXT("DragFollower"));
	DragFollower->InteractorHandleOffset = FVector(125.0f, 0.0f, 10.0f);
	DragFollower->MaxPositionError = 300.0f;
	DragFollower->MaxLateralError = 125.0f;
	DragFollower->MaxVerticalError = 55.0f;
	DragFollower->HorizontalPositionGain = 32.0f;
	DragFollower->ForwardPositionGainMultiplier = 1.35f;
	DragFollower->LateralPositionGainMultiplier = 0.58f;
	DragFollower->HorizontalVelocityDamping = 11.5f;
	DragFollower->ForwardVelocityDampingMultiplier = 1.25f;
	DragFollower->LateralVelocityDampingMultiplier = 1.0f;
	DragFollower->VerticalPositionGain = 6.0f;
	DragFollower->VerticalVelocityDamping = 10.0f;
	DragFollower->MaxHorizontalAcceleration = 2250.0f;
	DragFollower->MaxForwardPushAccelerationMultiplier = 1.65f;
	DragFollower->MaxForwardPullAccelerationMultiplier = 0.55f;
	DragFollower->MaxLateralAccelerationMultiplier = 0.72f;
	DragFollower->MaxVerticalAcceleration = 900.0f;
	DragFollower->MaxRaiseAccelerationMultiplier = 0.2f;
	DragFollower->MaxDropAccelerationMultiplier = 0.9f;
	DragFollower->TargetLeadTime = 0.08f;
	DragFollower->ExtraLinearDragAcceleration = 1.25f;
	DragFollower->ForwardDragFraction = 0.25f;
	DragFollower->bEnableYawAlignment = true;
	DragFollower->YawAlignmentStrength = 95.0f;
	DragFollower->YawAlignmentDamping = 24.0f;
	DragFollower->MaxYawAlignmentTorque = 120.0f;
}

void ADragCubeActor::BeginPlay()
{
	Super::BeginPlay();

	RefreshPhysicsBodyBinding();
	if (DragFollower)
	{
		DragFollower->SetPhysicsBody(ActivePhysicsBody);
		DragFollower->SetDrivePoint(HandleAnchor ? HandleAnchor : CubeBody);
	}
}

void ADragCubeActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AActor* Interactor = ActiveInteractor.Get();
	if (!Interactor)
	{
		return;
	}

	if (!DragFollower
		|| !DragFollower->IsFollowing()
		|| DragFollower->GetInteractorActor() != Interactor
		|| !ActivePhysicsBody
		|| !ActivePhysicsBody->IsSimulatingPhysics())
	{
		StopInteraction(TEXT("FollowerLost"));
		return;
	}

	const float MaxBreakDistance = FMath::Max(0.0f, BreakDistance);
	if (MaxBreakDistance > 0.0f)
	{
		const float CurrentDistanceSq = FVector::DistSquared(
			Interactor->GetActorLocation(),
			GetInteractionLocation_Implementation(Interactor));
		if (CurrentDistanceSq > FMath::Square(MaxBreakDistance))
		{
			StopInteraction(TEXT("BreakDistanceExceeded"));
		}
	}
}

bool ADragCubeActor::CanInteract_Implementation(AActor* Interactor) const
{
	UPrimitiveComponent* PhysicsBody = ActivePhysicsBody ? ActivePhysicsBody.Get() : ResolvePhysicsBody();
	if (!Interactor || !PhysicsBody || !PhysicsBody->IsSimulatingPhysics())
	{
		return false;
	}

	if (ActiveInteractor.IsValid() && ActiveInteractor.Get() != Interactor)
	{
		return false;
	}

	if (DragFollower && DragFollower->IsFollowing() && DragFollower->GetInteractorActor() != Interactor)
	{
		return false;
	}

	const float MaxDistance = FMath::Max(0.0f, MaxInteractDistance);
	if (MaxDistance > 0.0f)
	{
		const FVector InteractionLocation = GetInteractionLocation_Implementation(Interactor);
		if (FVector::DistSquared(Interactor->GetActorLocation(), InteractionLocation) > FMath::Square(MaxDistance))
		{
			return false;
		}
	}

	return true;
}

FVector ADragCubeActor::GetInteractionLocation_Implementation(AActor* Interactor) const
{
	if (HandleAnchor)
	{
		return HandleAnchor->GetComponentLocation();
	}

	return CubeBody ? CubeBody->GetComponentLocation() : GetActorLocation();
}

bool ADragCubeActor::BeginInteract_Implementation(AActor* Interactor)
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

	if (!DragFollower || !ActivePhysicsBody)
	{
		return false;
	}

	if (!DragFollower->BeginFollowing(Interactor))
	{
		return false;
	}

	ActiveInteractor = Interactor;
	return true;
}

void ADragCubeActor::EndInteract_Implementation(AActor* Interactor)
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

bool ADragCubeActor::IsInteracting_Implementation(AActor* Interactor) const
{
	if (!ActiveInteractor.IsValid())
	{
		return false;
	}

	return Interactor ? ActiveInteractor.Get() == Interactor : true;
}

FText ADragCubeActor::GetInteractPrompt_Implementation() const
{
	return ActiveInteractor.IsValid()
		? FText::FromString(TEXT("Release Cube"))
		: FText::FromString(TEXT("Grab Cube"));
}

UStaticMeshComponent* ADragCubeActor::GetCubeBodyComponent() const
{
	return CubeBody;
}

USceneComponent* ADragCubeActor::GetHandleAnchorComponent() const
{
	return HandleAnchor;
}

UPhysicsDragFollowerComponent* ADragCubeActor::GetDragFollowerComponent() const
{
	return DragFollower;
}

AActor* ADragCubeActor::GetActiveInteractorActor() const
{
	return ActiveInteractor.Get();
}

UPrimitiveComponent* ADragCubeActor::GetActivePhysicsBodyComponent() const
{
	return ActivePhysicsBody;
}

void ADragCubeActor::SetInteractionDistances(float InMaxInteractDistance, float InBreakDistance)
{
	MaxInteractDistance = FMath::Max(0.0f, InMaxInteractDistance);
	BreakDistance = FMath::Max(0.0f, InBreakDistance);
}

void ADragCubeActor::RefreshPhysicsBodyBinding()
{
	ActivePhysicsBody = ResolvePhysicsBody();
}

UPrimitiveComponent* ADragCubeActor::ResolvePhysicsBody() const
{
	if (CubeBody && CubeBody->IsSimulatingPhysics())
	{
		return CubeBody;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent || PrimitiveComponent == InteractVolume)
		{
			continue;
		}

		if (PrimitiveComponent->IsSimulatingPhysics())
		{
			return PrimitiveComponent;
		}
	}

	return CubeBody;
}

void ADragCubeActor::StopInteraction(FName Reason)
{
	(void)Reason;

	AActor* PreviousInteractor = ActiveInteractor.Get();
	ActiveInteractor.Reset();

	if (DragFollower)
	{
		DragFollower->StopFollowing();
	}

	if (!PreviousInteractor)
	{
		return;
	}

	if (UAgentInteractorComponent* InteractorComponent = PreviousInteractor->FindComponentByClass<UAgentInteractorComponent>())
	{
		if (InteractorComponent->GetActiveInteractable() == this)
		{
			InteractorComponent->EndCurrentInteraction();
		}
	}
}
