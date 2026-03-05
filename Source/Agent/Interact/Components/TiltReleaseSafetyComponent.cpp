// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interact/Components/TiltReleaseSafetyComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"

UTiltReleaseSafetyComponent::UTiltReleaseSafetyComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTiltReleaseSafetyComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!PhysicsBody)
	{
		PhysicsBody = Cast<UPrimitiveComponent>(GetOwner() ? GetOwner()->GetRootComponent() : nullptr);
	}
}

void UTiltReleaseSafetyComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bMonitoring)
	{
		return;
	}

	AActor* Interactor = InteractorActor.Get();
	if (!Interactor || !PhysicsBody || !PhysicsBody->IsSimulatingPhysics())
	{
		RequestRelease(TEXT("InteractorLost"));
		return;
	}

	const FVector BodyUp = PhysicsBody->GetUpVector().GetSafeNormal();
	const float UpDot = FMath::Clamp(FVector::DotProduct(BodyUp, FVector::UpVector), -1.0f, 1.0f);
	const float TiltDegrees = FMath::RadiansToDegrees(FMath::Acos(UpDot));

	if (TiltDegrees >= FMath::Max(0.0f, HardTiltReleaseDegrees))
	{
		RequestRelease(TEXT("HardTilt"));
		return;
	}

	if (TiltDegrees >= FMath::Max(0.0f, TiltReleaseDegrees))
	{
		TiltExceededTime += FMath::Max(0.0f, DeltaTime);
		if (TiltExceededTime >= FMath::Max(0.0f, TiltReleaseDuration))
		{
			RequestRelease(TEXT("Tilt"));
			return;
		}
	}
	else
	{
		TiltExceededTime = 0.0f;
	}

	if (!bEnableDistanceRelease)
	{
		DistanceExceededTime = 0.0f;
		return;
	}

	const FVector ReferenceLocation = DistanceReferencePoint
		? DistanceReferencePoint->GetComponentLocation()
		: PhysicsBody->GetComponentLocation();
	const float DistanceToInteractor = FVector::Dist(ReferenceLocation, Interactor->GetActorLocation());
	if (DistanceToInteractor > FMath::Max(0.0f, MaxInteractorDistance))
	{
		DistanceExceededTime += FMath::Max(0.0f, DeltaTime);
		if (DistanceExceededTime >= FMath::Max(0.0f, DistanceReleaseDuration))
		{
			RequestRelease(TEXT("Distance"));
			return;
		}
	}
	else
	{
		DistanceExceededTime = 0.0f;
	}
}

void UTiltReleaseSafetyComponent::SetPhysicsBody(UPrimitiveComponent* InPhysicsBody)
{
	PhysicsBody = InPhysicsBody;
}

void UTiltReleaseSafetyComponent::BeginMonitoring(AActor* InInteractor, USceneComponent* InDistanceReferencePoint)
{
	if (!InInteractor)
	{
		return;
	}

	InteractorActor = InInteractor;
	DistanceReferencePoint = InDistanceReferencePoint;
	TiltExceededTime = 0.0f;
	DistanceExceededTime = 0.0f;
	bMonitoring = true;
}

void UTiltReleaseSafetyComponent::StopMonitoring()
{
	bMonitoring = false;
	InteractorActor.Reset();
	DistanceReferencePoint = nullptr;
	TiltExceededTime = 0.0f;
	DistanceExceededTime = 0.0f;
}

bool UTiltReleaseSafetyComponent::IsMonitoring() const
{
	return bMonitoring;
}

void UTiltReleaseSafetyComponent::RequestRelease(const FName Reason)
{
	if (!bMonitoring)
	{
		return;
	}

	StopMonitoring();
	OnAutoReleaseRequested.Broadcast(Reason);
}
