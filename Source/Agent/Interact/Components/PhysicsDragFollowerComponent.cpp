// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interact/Components/PhysicsDragFollowerComponent.h"
#include "Components/PrimitiveComponent.h"
#include "DrawDebugHelpers.h"
#include "GameFramework/Actor.h"

UPhysicsDragFollowerComponent::UPhysicsDragFollowerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UPhysicsDragFollowerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!PhysicsBody)
	{
		PhysicsBody = Cast<UPrimitiveComponent>(GetOwner() ? GetOwner()->GetRootComponent() : nullptr);
	}

	if (!DrivePoint && PhysicsBody)
	{
		DrivePoint = PhysicsBody;
	}
}

void UPhysicsDragFollowerComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsFollowing || !PhysicsBody || !PhysicsBody->IsSimulatingPhysics())
	{
		return;
	}

	AActor* Interactor = InteractorActor.Get();
	if (!Interactor)
	{
		StopFollowing();
		return;
	}

	const float ClampedDeltaTime = FMath::Max(0.0f, DeltaTime);
	if (bAllowSteerInput)
	{
		DesiredSteerYawOffset = FRotator::NormalizeAxis(
			DesiredSteerYawOffset + (SteeringInput * FMath::Max(0.0f, SteerYawRate) * ClampedDeltaTime));

		const float MaxOffset = FMath::Max(0.0f, MaxSteerYawOffset);
		if (MaxOffset > 0.0f)
		{
			DesiredSteerYawOffset = FMath::Clamp(DesiredSteerYawOffset, -MaxOffset, MaxOffset);
		}

		if (FMath::Abs(SteeringInput) <= KINDA_SMALL_NUMBER && SteerCenteringSpeed > 0.0f)
		{
			DesiredSteerYawOffset = FMath::FInterpTo(
				DesiredSteerYawOffset,
				0.0f,
				ClampedDeltaTime,
				SteerCenteringSpeed);
		}
	}
	else
	{
		DesiredSteerYawOffset = 0.0f;
	}

	const float LiftTarget = bAllowLiftInput
		? (FMath::Clamp(LiftInput, -1.0f, 1.0f) * FMath::Max(0.0f, MaxLiftOffset))
		: 0.0f;
	SmoothedLiftOffset = FMath::FInterpTo(
		SmoothedLiftOffset,
		LiftTarget,
		ClampedDeltaTime,
		FMath::Max(0.0f, LiftInputInterpSpeed));

	CurrentTargetLocation = ComputeTargetLocation();

	const FVector DriveLocation = DrivePoint ? DrivePoint->GetComponentLocation() : PhysicsBody->GetCenterOfMass(NAME_None);
	const FVector DriveVelocity = PhysicsBody->GetPhysicsLinearVelocityAtPoint(DriveLocation, NAME_None);
	const float DesiredYawDegrees = Interactor->GetActorRotation().Yaw
		+ (bAllowSteerInput ? DesiredSteerYawOffset : 0.0f);
	FVector InteractorForward = FRotator(0.0f, DesiredYawDegrees, 0.0f).Vector().GetSafeNormal2D();
	if (InteractorForward.IsNearlyZero())
	{
		InteractorForward = Interactor->GetActorForwardVector().GetSafeNormal2D();
	}

	FVector InteractorRight = FRotationMatrix(FRotator(0.0f, DesiredYawDegrees, 0.0f)).GetUnitAxis(EAxis::Y).GetSafeNormal2D();
	if (InteractorRight.IsNearlyZero())
	{
		InteractorRight = Interactor->GetActorRightVector().GetSafeNormal2D();
	}

	const FVector WorldUp = FVector::UpVector;
	FVector PositionError = CurrentTargetLocation - DriveLocation;
	if (MaxPositionError > 0.0f)
	{
		PositionError = PositionError.GetClampedToMaxSize(MaxPositionError);
	}

	float ForwardError = FVector::DotProduct(PositionError, InteractorForward);
	float LateralError = FVector::DotProduct(PositionError, InteractorRight);
	float VerticalError = PositionError.Z;

	const float MaxError = FMath::Max(0.0f, MaxPositionError);
	ForwardError = FMath::Clamp(ForwardError, -MaxError, MaxError);
	LateralError = FMath::Clamp(LateralError, -FMath::Max(0.0f, MaxLateralError), FMath::Max(0.0f, MaxLateralError));
	VerticalError = FMath::Clamp(VerticalError, -FMath::Max(0.0f, MaxVerticalError), FMath::Max(0.0f, MaxVerticalError));

	const float ForwardVelocity = FVector::DotProduct(DriveVelocity, InteractorForward);
	const float LateralVelocity = FVector::DotProduct(DriveVelocity, InteractorRight);
	const float VerticalVelocity = DriveVelocity.Z;

	const float ForwardGain = FMath::Max(0.0f, HorizontalPositionGain * ForwardPositionGainMultiplier);
	const float LateralGain = FMath::Max(0.0f, HorizontalPositionGain * LateralPositionGainMultiplier);
	const float ForwardDamping = FMath::Max(0.0f, HorizontalVelocityDamping * ForwardVelocityDampingMultiplier);
	const float LateralDamping = FMath::Max(0.0f, HorizontalVelocityDamping * LateralVelocityDampingMultiplier);

	float ForwardAcceleration = (ForwardError * ForwardGain) - (ForwardVelocity * ForwardDamping);
	float LateralAcceleration = (LateralError * LateralGain) - (LateralVelocity * LateralDamping);
	float VerticalAcceleration = (VerticalError * VerticalPositionGain) - (VerticalVelocity * VerticalVelocityDamping);

	const float MaxHorizontalAccel = FMath::Max(0.0f, MaxHorizontalAcceleration);
	ForwardAcceleration = FMath::Clamp(
		ForwardAcceleration,
		-MaxHorizontalAccel * FMath::Max(0.0f, MaxForwardPullAccelerationMultiplier),
		MaxHorizontalAccel * FMath::Max(0.0f, MaxForwardPushAccelerationMultiplier));
	LateralAcceleration = FMath::Clamp(
		LateralAcceleration,
		-MaxHorizontalAccel * FMath::Max(0.0f, MaxLateralAccelerationMultiplier),
		MaxHorizontalAccel * FMath::Max(0.0f, MaxLateralAccelerationMultiplier));

	const float MaxVerticalAccel = FMath::Max(0.0f, MaxVerticalAcceleration);
	VerticalAcceleration = FMath::Clamp(
		VerticalAcceleration,
		-MaxVerticalAccel * FMath::Max(0.0f, MaxDropAccelerationMultiplier),
		MaxVerticalAccel * FMath::Max(0.0f, MaxRaiseAccelerationMultiplier));

	const FVector DriveAcceleration = (InteractorForward * ForwardAcceleration)
		+ (InteractorRight * LateralAcceleration)
		+ (WorldUp * VerticalAcceleration);
	const float BodyMassKg = FMath::Max(0.01f, PhysicsBody->GetMass());
	PhysicsBody->AddForceAtLocation(DriveAcceleration * BodyMassKg, DriveLocation, NAME_None);

	if (bDrawDebug)
	{
		if (UWorld* World = GetWorld())
		{
			const float SphereRadius = FMath::Max(0.1f, DebugSphereRadius);
			const float LineThickness = FMath::Max(0.1f, DebugLineThickness);
			DrawDebugSphere(World, CurrentTargetLocation, SphereRadius, 16, FColor::Yellow, false, -1.0f, 0, LineThickness);
			DrawDebugSphere(World, DriveLocation, SphereRadius * 0.7f, 12, FColor::Cyan, false, -1.0f, 0, LineThickness);
			DrawDebugLine(World, DriveLocation, CurrentTargetLocation, FColor::Orange, false, -1.0f, 0, LineThickness);
		}
	}

	if (ExtraLinearDragAcceleration > 0.0f)
	{
		const FVector BodyVelocity = PhysicsBody->GetPhysicsLinearVelocity(NAME_None);
		const float BodyForwardVelocity = FVector::DotProduct(BodyVelocity, InteractorForward);
		const FVector ForwardVelocityVector = InteractorForward * BodyForwardVelocity;
		const FVector LateralVelocityVector = BodyVelocity - ForwardVelocityVector;
		const FVector DragForce = (-LateralVelocityVector * (BodyMassKg * ExtraLinearDragAcceleration))
			+ (-ForwardVelocityVector * (BodyMassKg * ExtraLinearDragAcceleration * FMath::Max(0.0f, ForwardDragFraction)));
		PhysicsBody->AddForce(DragForce, NAME_None, false);
	}

	if (bEnableYawAlignment)
	{
		FVector BodyForward = PhysicsBody->GetForwardVector();
		BodyForward.Z = 0.0f;
		BodyForward.Normalize();

		FVector DesiredForward = InteractorForward;
		DesiredForward.Z = 0.0f;
		DesiredForward.Normalize();

		if (!BodyForward.IsNearlyZero() && !DesiredForward.IsNearlyZero())
		{
			const float CrossZ = FVector::CrossProduct(BodyForward, DesiredForward).Z;
			const float YawErrorRadians = FMath::Asin(FMath::Clamp(CrossZ, -1.0f, 1.0f));
			const float YawRate = FVector::DotProduct(PhysicsBody->GetPhysicsAngularVelocityInRadians(NAME_None), WorldUp);
			float YawTorque = (YawErrorRadians * YawAlignmentStrength) - (YawRate * YawAlignmentDamping);
			YawTorque = FMath::Clamp(
				YawTorque,
				-FMath::Max(0.0f, MaxYawAlignmentTorque),
				FMath::Max(0.0f, MaxYawAlignmentTorque));

			PhysicsBody->AddTorqueInRadians(WorldUp * (YawTorque * BodyMassKg), NAME_None, false);
		}
	}
}

void UPhysicsDragFollowerComponent::SetPhysicsBody(UPrimitiveComponent* InPhysicsBody)
{
	PhysicsBody = InPhysicsBody;
	if (!DrivePoint)
	{
		DrivePoint = PhysicsBody;
	}
}

void UPhysicsDragFollowerComponent::SetDrivePoint(USceneComponent* InDrivePoint)
{
	DrivePoint = InDrivePoint;
}

bool UPhysicsDragFollowerComponent::BeginFollowing(AActor* InInteractor)
{
	if (!InInteractor || !PhysicsBody || !PhysicsBody->IsSimulatingPhysics())
	{
		return false;
	}

	InteractorActor = InInteractor;
	bIsFollowing = true;
	ResetControlInputs();
	CurrentTargetLocation = ComputeTargetLocation();
	PhysicsBody->WakeAllRigidBodies();
	return true;
}

void UPhysicsDragFollowerComponent::StopFollowing()
{
	bIsFollowing = false;
	InteractorActor.Reset();
	CurrentTargetLocation = FVector::ZeroVector;
	ResetControlInputs();
}

void UPhysicsDragFollowerComponent::SetSteerInput(float Value)
{
	SteeringInput = FMath::Clamp(Value, -1.0f, 1.0f);
}

void UPhysicsDragFollowerComponent::SetLiftInput(float Value)
{
	LiftInput = FMath::Clamp(Value, -1.0f, 1.0f);
}

void UPhysicsDragFollowerComponent::ResetControlInputs()
{
	SteeringInput = 0.0f;
	LiftInput = 0.0f;
	DesiredSteerYawOffset = 0.0f;
	SmoothedLiftOffset = 0.0f;
}

bool UPhysicsDragFollowerComponent::IsFollowing() const
{
	return bIsFollowing;
}

AActor* UPhysicsDragFollowerComponent::GetInteractorActor() const
{
	return InteractorActor.Get();
}

FVector UPhysicsDragFollowerComponent::GetCurrentTargetLocation() const
{
	return CurrentTargetLocation;
}

FVector UPhysicsDragFollowerComponent::ComputeTargetLocation() const
{
	const AActor* Interactor = InteractorActor.Get();
	if (!Interactor)
	{
		return FVector::ZeroVector;
	}

	const FRotator YawOnlyRotation(0.0f, Interactor->GetActorRotation().Yaw, 0.0f);
	FVector TargetOffset = InteractorHandleOffset;
	TargetOffset.Z += SmoothedLiftOffset;
	const FVector Offset = YawOnlyRotation.RotateVector(TargetOffset);
	const FVector HorizontalInteractorVelocity = FVector(Interactor->GetVelocity().X, Interactor->GetVelocity().Y, 0.0f);
	return Interactor->GetActorLocation() + Offset + (HorizontalInteractorVelocity * FMath::Max(0.0f, TargetLeadTime));
}
