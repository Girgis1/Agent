// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interact/Components/DualHandleGrabComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"

UDualHandleGrabComponent::UDualHandleGrabComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UDualHandleGrabComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsurePhysicsHandles();
}

void UDualHandleGrabComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsGrabbing)
	{
		return;
	}

	if (!GrabbedBody || !GrabbedBody->IsSimulatingPhysics() || !LeftPhysicsHandle || !RightPhysicsHandle)
	{
		EndGrab();
		return;
	}

	if (LeftPhysicsHandle->GetGrabbedComponent() != GrabbedBody
		|| RightPhysicsHandle->GetGrabbedComponent() != GrabbedBody)
	{
		EndGrab();
		return;
	}

	const float ClampedInterpSpeed = FMath::Max(0.0f, SteeringInputInterpSpeed);
	SmoothedSteeringHorizontal = FMath::FInterpTo(
		SmoothedSteeringHorizontal,
		SteeringHorizontalInput,
		DeltaTime,
		ClampedInterpSpeed);
	SmoothedSteeringVertical = FMath::FInterpTo(
		SmoothedSteeringVertical,
		SteeringVerticalInput,
		DeltaTime,
		ClampedInterpSpeed);

	FVector LeftTarget = FVector::ZeroVector;
	FVector RightTarget = FVector::ZeroVector;
	if (!ComputeHandTargets(LeftTarget, RightTarget))
	{
		EndGrab();
		return;
	}

	if (bPreserveInitialHandAnchorOffset)
	{
		const AActor* OwnerActor = GetOwner();
		const FRotator OwnerYawRotation(0.0f, OwnerActor ? OwnerActor->GetActorRotation().Yaw : 0.0f, 0.0f);
		LeftTarget += OwnerYawRotation.RotateVector(LeftInitialOffsetLocal);
		RightTarget += OwnerYawRotation.RotateVector(RightInitialOffsetLocal);
	}

	const FRotator BodyRotation = GrabbedBody->GetComponentRotation();
	LeftPhysicsHandle->SetTargetLocationAndRotation(LeftTarget, BodyRotation);
	RightPhysicsHandle->SetTargetLocationAndRotation(RightTarget, BodyRotation);

	if (bDrawDebugHandSpheres)
	{
		DrawDebugTargets(LeftTarget, RightTarget);
	}

	const float MaxBreakDistance = FMath::Max(0.0f, BreakDistance);
	if (MaxBreakDistance > 0.0f)
	{
		const FVector TargetMidpoint = (LeftTarget + RightTarget) * 0.5f;
		if (FVector::DistSquared(GrabbedBody->GetComponentLocation(), TargetMidpoint) > FMath::Square(MaxBreakDistance))
		{
			EndGrab();
		}
	}
}

void UDualHandleGrabComponent::SetHandPivots(USceneComponent* InLeftHandPivot, USceneComponent* InRightHandPivot)
{
	LeftHandPivot = InLeftHandPivot;
	RightHandPivot = InRightHandPivot;
}

bool UDualHandleGrabComponent::BeginGrab(AActor* InGrabbedActor, UPrimitiveComponent* InGrabbedBody)
{
	FVector LeftTarget = FVector::ZeroVector;
	FVector RightTarget = FVector::ZeroVector;
	if (!ComputeHandTargets(LeftTarget, RightTarget))
	{
		return false;
	}

	return BeginGrabAtLocations(InGrabbedActor, InGrabbedBody, LeftTarget, RightTarget);
}

bool UDualHandleGrabComponent::BeginGrabAtLocations(
	AActor* InGrabbedActor,
	UPrimitiveComponent* InGrabbedBody,
	const FVector& InLeftGrabLocation,
	const FVector& InRightGrabLocation)
{
	EnsurePhysicsHandles();
	if (!InGrabbedActor || !InGrabbedBody || !InGrabbedBody->IsSimulatingPhysics())
	{
		return false;
	}

	if (!LeftPhysicsHandle || !RightPhysicsHandle || !LeftHandPivot || !RightHandPivot)
	{
		return false;
	}

	EndGrab();

	ApplyHandleSettings(LeftPhysicsHandle);
	ApplyHandleSettings(RightPhysicsHandle);

	FVector LeftTarget = FVector::ZeroVector;
	FVector RightTarget = FVector::ZeroVector;
	if (!ComputeHandTargets(LeftTarget, RightTarget))
	{
		return false;
	}

	const FRotator BodyRotation = InGrabbedBody->GetComponentRotation();
	InGrabbedBody->WakeAllRigidBodies();

	const FVector LeftGrabLocation = InLeftGrabLocation.ContainsNaN() ? LeftTarget : InLeftGrabLocation;
	const FVector RightGrabLocation = InRightGrabLocation.ContainsNaN() ? RightTarget : InRightGrabLocation;
	const float MaxGrabDistance = FMath::Max(0.0f, MaxInitialHandToAnchorDistance);
	if (MaxGrabDistance > 0.0f)
	{
		if (FVector::DistSquared(LeftTarget, LeftGrabLocation) > FMath::Square(MaxGrabDistance)
			|| FVector::DistSquared(RightTarget, RightGrabLocation) > FMath::Square(MaxGrabDistance))
		{
			return false;
		}
	}

	if (bPreserveInitialHandAnchorOffset)
	{
		const AActor* OwnerActor = GetOwner();
		const FRotator OwnerYawRotation(0.0f, OwnerActor ? OwnerActor->GetActorRotation().Yaw : 0.0f, 0.0f);
		LeftInitialOffsetLocal = OwnerYawRotation.UnrotateVector(LeftGrabLocation - LeftTarget);
		RightInitialOffsetLocal = OwnerYawRotation.UnrotateVector(RightGrabLocation - RightTarget);
	}
	else
	{
		LeftInitialOffsetLocal = FVector::ZeroVector;
		RightInitialOffsetLocal = FVector::ZeroVector;
	}

	LeftPhysicsHandle->GrabComponentAtLocationWithRotation(InGrabbedBody, NAME_None, LeftGrabLocation, BodyRotation);
	RightPhysicsHandle->GrabComponentAtLocationWithRotation(InGrabbedBody, NAME_None, RightGrabLocation, BodyRotation);

	if (LeftPhysicsHandle->GetGrabbedComponent() != InGrabbedBody
		|| RightPhysicsHandle->GetGrabbedComponent() != InGrabbedBody)
	{
		EndGrab();
		return false;
	}

	GrabbedActor = InGrabbedActor;
	GrabbedBody = InGrabbedBody;
	SteeringHorizontalInput = 0.0f;
	SteeringVerticalInput = 0.0f;
	SmoothedSteeringHorizontal = 0.0f;
	SmoothedSteeringVertical = 0.0f;
	bIsGrabbing = true;

	LeftPhysicsHandle->SetTargetLocationAndRotation(LeftTarget, BodyRotation);
	RightPhysicsHandle->SetTargetLocationAndRotation(RightTarget, BodyRotation);
	return true;
}

void UDualHandleGrabComponent::EndGrab()
{
	if (LeftPhysicsHandle && LeftPhysicsHandle->GetGrabbedComponent())
	{
		LeftPhysicsHandle->ReleaseComponent();
	}

	if (RightPhysicsHandle && RightPhysicsHandle->GetGrabbedComponent())
	{
		RightPhysicsHandle->ReleaseComponent();
	}

	GrabbedActor.Reset();
	GrabbedBody = nullptr;
	SteeringHorizontalInput = 0.0f;
	SteeringVerticalInput = 0.0f;
	SmoothedSteeringHorizontal = 0.0f;
	SmoothedSteeringVertical = 0.0f;
	LeftInitialOffsetLocal = FVector::ZeroVector;
	RightInitialOffsetLocal = FVector::ZeroVector;
	bIsGrabbing = false;
}

bool UDualHandleGrabComponent::IsGrabbing() const
{
	return bIsGrabbing;
}

bool UDualHandleGrabComponent::IsGrabbingActor(const AActor* InActor) const
{
	return bIsGrabbing && InActor && GrabbedActor.Get() == InActor;
}

AActor* UDualHandleGrabComponent::GetGrabbedActor() const
{
	return GrabbedActor.Get();
}

void UDualHandleGrabComponent::SetSteeringHorizontal(float Value)
{
	SteeringHorizontalInput = FMath::Clamp(Value, -1.0f, 1.0f);
}

void UDualHandleGrabComponent::SetSteeringVertical(float Value)
{
	SteeringVerticalInput = FMath::Clamp(Value, -1.0f, 1.0f);
}

void UDualHandleGrabComponent::EnsurePhysicsHandles()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	auto CreateHandle = [&](TObjectPtr<UPhysicsHandleComponent>& HandleRef, const TCHAR* Name)
	{
		if (HandleRef)
		{
			return;
		}

		UPhysicsHandleComponent* NewHandle = NewObject<UPhysicsHandleComponent>(OwnerActor, UPhysicsHandleComponent::StaticClass(), Name);
		if (!NewHandle)
		{
			return;
		}

		OwnerActor->AddInstanceComponent(NewHandle);
		NewHandle->RegisterComponent();
		ApplyHandleSettings(NewHandle);
		HandleRef = NewHandle;
	};

	CreateHandle(LeftPhysicsHandle, TEXT("DualHandleLeftPhysicsHandle"));
	CreateHandle(RightPhysicsHandle, TEXT("DualHandleRightPhysicsHandle"));
}

void UDualHandleGrabComponent::ApplyHandleSettings(UPhysicsHandleComponent* PhysicsHandle) const
{
	if (!PhysicsHandle)
	{
		return;
	}

	PhysicsHandle->LinearStiffness = FMath::Max(0.0f, HandleLinearStiffness);
	PhysicsHandle->LinearDamping = FMath::Max(0.0f, HandleLinearDamping);
	PhysicsHandle->AngularStiffness = FMath::Max(0.0f, HandleAngularStiffness);
	PhysicsHandle->AngularDamping = FMath::Max(0.0f, HandleAngularDamping);
	PhysicsHandle->InterpolationSpeed = FMath::Max(0.0f, HandleInterpolationSpeed);
	PhysicsHandle->bInterpolateTarget = true;
}

bool UDualHandleGrabComponent::ComputeHandTargets(FVector& OutLeftTarget, FVector& OutRightTarget) const
{
	if (!LeftHandPivot || !RightHandPivot)
	{
		return false;
	}

	const AActor* OwnerActor = GetOwner();
	FVector OwnerForward = OwnerActor ? OwnerActor->GetActorForwardVector() : FVector::ForwardVector;
	OwnerForward.Z = 0.0f;
	if (!OwnerForward.Normalize())
	{
		OwnerForward = FVector::ForwardVector;
	}

	const float HorizontalInput = FMath::Clamp(SmoothedSteeringHorizontal, -1.0f, 1.0f);
	const float VerticalInput = FMath::Clamp(SmoothedSteeringVertical, -1.0f, 1.0f);
	const FVector VerticalOffset = FVector::UpVector * (VerticalInput * FMath::Max(0.0f, MaxVerticalSteeringOffset));
	const FVector DifferentialOffset = OwnerForward * (HorizontalInput * FMath::Max(0.0f, MaxForwardSteeringOffset));

	OutLeftTarget = LeftHandPivot->GetComponentLocation() + VerticalOffset + DifferentialOffset;
	OutRightTarget = RightHandPivot->GetComponentLocation() + VerticalOffset - DifferentialOffset;
	return true;
}

void UDualHandleGrabComponent::DrawDebugTargets(const FVector& LeftTarget, const FVector& RightTarget) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float SphereRadius = FMath::Max(0.1f, DebugSphereRadius);
	DrawDebugSphere(World, LeftTarget, SphereRadius, 16, FColor::Green, false, -1.0f, 0, DebugSphereThickness);
	DrawDebugSphere(World, RightTarget, SphereRadius, 16, FColor::Cyan, false, -1.0f, 0, DebugSphereThickness);
}
