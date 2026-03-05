// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interact/Components/GrabVehiclePushComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"

UGrabVehiclePushComponent::UGrabVehiclePushComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UGrabVehiclePushComponent::BeginPlay()
{
	Super::BeginPlay();
	EnsurePhysicsHandle();
}

void UGrabVehiclePushComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsPushing)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !GrabbedBody || !GrabbedBody->IsSimulatingPhysics())
	{
		EndPush();
		return;
	}

	if (!PushPhysicsHandle || PushPhysicsHandle->GetGrabbedComponent() != GrabbedBody)
	{
		EndPush();
		return;
	}

	DesiredYawOffset = FRotator::NormalizeAxis(
		DesiredYawOffset + (SteerInput * FMath::Max(0.0f, SteerYawRate) * DeltaTime));

	const float LiftTarget = bAllowLiftInput
		? (FMath::Clamp(LiftInput, -1.0f, 1.0f) * FMath::Max(0.0f, MaxLiftOffset))
		: 0.0f;
	SmoothedLiftOffset = FMath::FInterpTo(
		SmoothedLiftOffset,
		LiftTarget,
		DeltaTime,
		FMath::Max(0.0f, LiftInputInterpSpeed));

	FVector TargetLocation = FVector::ZeroVector;
	FRotator TargetRotation = FRotator::ZeroRotator;
	if (!ComputeHoldTarget(TargetLocation, TargetRotation))
	{
		EndPush();
		return;
	}

	PushPhysicsHandle->SetTargetLocationAndRotation(TargetLocation, TargetRotation);

	const FVector BodyForward = GrabbedBody->GetForwardVector().GetSafeNormal2D();
	const FVector DesiredForward = TargetRotation.Vector().GetSafeNormal2D();
	if (!BodyForward.IsNearlyZero() && !DesiredForward.IsNearlyZero())
	{
		const float CrossZ = FVector::CrossProduct(BodyForward, DesiredForward).Z;
		const float YawErrorRadians = FMath::Asin(FMath::Clamp(CrossZ, -1.0f, 1.0f));
		const float CurrentYawRate = FVector::DotProduct(
			GrabbedBody->GetPhysicsAngularVelocityInRadians(NAME_None),
			FVector::UpVector);
		float YawTorque = (YawErrorRadians * FMath::Max(0.0f, YawTorqueStrength))
			- (CurrentYawRate * FMath::Max(0.0f, YawTorqueDamping));
		YawTorque = FMath::Clamp(
			YawTorque,
			-FMath::Max(0.0f, MaxYawTorque),
			FMath::Max(0.0f, MaxYawTorque));

		const float BodyMassKg = FMath::Max(0.01f, GrabbedBody->GetMass());
		GrabbedBody->AddTorqueInRadians(FVector::UpVector * (YawTorque * BodyMassKg), NAME_None, false);
	}

	const float MaxAllowedDistance = FMath::Max(0.0f, BreakDistance);
	if (MaxAllowedDistance > 0.0f)
	{
		const float DistanceSq = FVector::DistSquared(OwnerActor->GetActorLocation(), GrabbedBody->GetComponentLocation());
		if (DistanceSq > FMath::Square(MaxAllowedDistance))
		{
			EndPush();
			return;
		}
	}

	if (bDrawDebug)
	{
		const UWorld* World = GetWorld();
		if (World)
		{
			DrawDebugSphere(World, TargetLocation, FMath::Max(0.1f, DebugSphereRadius), 16, FColor::Yellow, false, -1.0f, 0, DebugLineThickness);
			DrawDebugLine(World, GrabbedBody->GetComponentLocation(), TargetLocation, FColor::Orange, false, -1.0f, 0, DebugLineThickness);
		}
	}
}

bool UGrabVehiclePushComponent::BeginPush(AActor* InVehicleActor, UPrimitiveComponent* InPhysicsBody, const FVector& InGrabPointWorld)
{
	EnsurePhysicsHandle();
	if (!InVehicleActor || !InPhysicsBody || !InPhysicsBody->IsSimulatingPhysics() || !PushPhysicsHandle)
	{
		return false;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return false;
	}

	const float MaxStartDistance = FMath::Max(0.0f, MaxStartGrabDistance);
	if (MaxStartDistance > 0.0f
		&& FVector::DistSquared(OwnerActor->GetActorLocation(), InGrabPointWorld) > FMath::Square(MaxStartDistance))
	{
		return false;
	}

	EndPush();
	ApplyHandleSettings();

	const FRotator OwnerYawRotation(0.0f, OwnerActor->GetActorRotation().Yaw, 0.0f);
	const FVector LocalGrab = OwnerYawRotation.UnrotateVector(InGrabPointWorld - OwnerActor->GetActorLocation());
	InitialVerticalLocalOffset = LocalGrab.Z;
	DesiredYawOffset = FRotator::NormalizeAxis(InPhysicsBody->GetComponentRotation().Yaw - OwnerYawRotation.Yaw);
	SteerInput = 0.0f;
	LiftInput = 0.0f;
	SmoothedLiftOffset = 0.0f;

	InPhysicsBody->WakeAllRigidBodies();
	PushPhysicsHandle->GrabComponentAtLocationWithRotation(
		InPhysicsBody,
		NAME_None,
		InGrabPointWorld,
		InPhysicsBody->GetComponentRotation());
	if (PushPhysicsHandle->GetGrabbedComponent() != InPhysicsBody)
	{
		EndPush();
		return false;
	}

	GrabbedBody = InPhysicsBody;
	PushedActor = InVehicleActor;
	bIsPushing = true;

	FVector TargetLocation = FVector::ZeroVector;
	FRotator TargetRotation = FRotator::ZeroRotator;
	if (ComputeHoldTarget(TargetLocation, TargetRotation))
	{
		PushPhysicsHandle->SetTargetLocationAndRotation(TargetLocation, TargetRotation);
	}

	return true;
}

void UGrabVehiclePushComponent::EndPush()
{
	if (PushPhysicsHandle && PushPhysicsHandle->GetGrabbedComponent())
	{
		PushPhysicsHandle->ReleaseComponent();
	}

	GrabbedBody = nullptr;
	PushedActor.Reset();
	InitialVerticalLocalOffset = 0.0f;
	DesiredYawOffset = 0.0f;
	SteerInput = 0.0f;
	LiftInput = 0.0f;
	SmoothedLiftOffset = 0.0f;
	bIsPushing = false;
}

bool UGrabVehiclePushComponent::IsPushing() const
{
	return bIsPushing;
}

bool UGrabVehiclePushComponent::IsPushingActor(const AActor* InActor) const
{
	return bIsPushing && InActor && PushedActor.Get() == InActor;
}

AActor* UGrabVehiclePushComponent::GetPushedActor() const
{
	return PushedActor.Get();
}

void UGrabVehiclePushComponent::SetSteerInput(float Value)
{
	SteerInput = FMath::Clamp(Value, -1.0f, 1.0f);
}

void UGrabVehiclePushComponent::SetLiftInput(float Value)
{
	LiftInput = FMath::Clamp(Value, -1.0f, 1.0f);
}

void UGrabVehiclePushComponent::EnsurePhysicsHandle()
{
	if (PushPhysicsHandle)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	PushPhysicsHandle = NewObject<UPhysicsHandleComponent>(
		OwnerActor,
		UPhysicsHandleComponent::StaticClass(),
		TEXT("GrabVehiclePushHandle"));
	if (!PushPhysicsHandle)
	{
		return;
	}

	OwnerActor->AddInstanceComponent(PushPhysicsHandle);
	PushPhysicsHandle->RegisterComponent();
	ApplyHandleSettings();
}

void UGrabVehiclePushComponent::ApplyHandleSettings() const
{
	if (!PushPhysicsHandle)
	{
		return;
	}

	PushPhysicsHandle->LinearStiffness = FMath::Max(0.0f, HandleLinearStiffness);
	PushPhysicsHandle->LinearDamping = FMath::Max(0.0f, HandleLinearDamping);
	PushPhysicsHandle->AngularStiffness = FMath::Max(0.0f, HandleAngularStiffness);
	PushPhysicsHandle->AngularDamping = FMath::Max(0.0f, HandleAngularDamping);
	PushPhysicsHandle->InterpolationSpeed = FMath::Max(0.0f, HandleInterpolationSpeed);
	PushPhysicsHandle->bInterpolateTarget = true;
}

bool UGrabVehiclePushComponent::ComputeHoldTarget(FVector& OutTargetLocation, FRotator& OutTargetRotation) const
{
	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return false;
	}

	const FRotator OwnerYawRotation(0.0f, OwnerActor->GetActorRotation().Yaw, 0.0f);
	const float VerticalBase = HoldVerticalOffset + (bUseInitialGrabVerticalOffset ? InitialVerticalLocalOffset : 0.0f);
	const FVector LocalTargetOffset(
		FMath::Max(0.0f, HoldForwardOffset),
		HoldLateralOffset,
		VerticalBase + SmoothedLiftOffset);
	OutTargetLocation = OwnerActor->GetActorLocation() + OwnerYawRotation.RotateVector(LocalTargetOffset);
	OutTargetRotation = FRotator(0.0f, FRotator::NormalizeAxis(OwnerYawRotation.Yaw + DesiredYawOffset), 0.0f);
	return true;
}
