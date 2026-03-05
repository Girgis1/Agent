// Copyright Epic Games, Inc. All Rights Reserved.

#include "Vehicle/Components/TrolleyMovementComponent.h"
#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"

UTrolleyMovementComponent::UTrolleyMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UTrolleyMovementComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ShouldSkipUpdate(DeltaTime) || !UpdatedComponent)
	{
		return;
	}

	const float MaxForward = FMath::Max(0.0f, MaxForwardSpeed);
	const float MaxReverse = FMath::Max(0.0f, MaxReverseSpeed);
	const float Input = FMath::Clamp(ThrottleInput, -1.0f, 1.0f);
	const float ClampedSteering = FMath::Clamp(SteeringInput, -1.0f, 1.0f);

	UPrimitiveComponent* SimulatedPrimitive = Cast<UPrimitiveComponent>(UpdatedComponent);
	if (SimulatedPrimitive && SimulatedPrimitive->IsSimulatingPhysics())
	{
		const FVector Forward = UpdatedComponent->GetForwardVector();
		const FVector Right = UpdatedComponent->GetRightVector();
		const FVector LinearVelocity = SimulatedPrimitive->GetPhysicsLinearVelocity();
		const float ForwardSpeed = FVector::DotProduct(LinearVelocity, Forward);
		const float SideSpeed = FVector::DotProduct(LinearVelocity, Right);

		CurrentForwardSpeed = ForwardSpeed;
		Velocity = LinearVelocity;

		const float SpeedAlpha = MaxForward > KINDA_SMALL_NUMBER
			? FMath::Clamp(FMath::Abs(ForwardSpeed) / MaxForward, 0.0f, 1.0f)
			: 0.0f;
		const float DriveLimitScale = FMath::Max(0.05f, 1.0f - SpeedAlpha);
		const FVector RearDriveLocation = SimulatedPrimitive->GetCenterOfMass() + (Forward * RearDriveOffset);

		if (Input > KINDA_SMALL_NUMBER && ForwardSpeed < MaxForward)
		{
			const FVector Force = Forward * (Input * FMath::Max(0.0f, DriveForce) * DriveLimitScale);
			SimulatedPrimitive->AddForceAtLocation(Force, RearDriveLocation);
		}
		else if (Input < -KINDA_SMALL_NUMBER && ForwardSpeed > -MaxReverse)
		{
			const FVector Force = Forward * (Input * FMath::Max(0.0f, ReverseDriveForce) * DriveLimitScale);
			SimulatedPrimitive->AddForceAtLocation(Force, RearDriveLocation);
		}

		const float Mass = FMath::Max(1.0f, SimulatedPrimitive->GetMass());
		SimulatedPrimitive->AddForce(-LinearVelocity * FMath::Max(0.0f, RollingResistance) * Mass);

		const float LateralGripStrength = FMath::Max(0.0f, bHandbrakeActive ? HandbrakeLateralGrip : LateralGrip);
		SimulatedPrimitive->AddForce(-(Right * SideSpeed) * LateralGripStrength * Mass);

		if (bHandbrakeActive)
		{
			SimulatedPrimitive->AddForce(-(Forward * ForwardSpeed) * FMath::Max(0.0f, HandbrakeDrag) * Mass);
		}

		if (!FMath::IsNearlyZero(ClampedSteering, 0.01f) && !FMath::IsNearlyZero(ForwardSpeed, 4.0f))
		{
			const float SteeringTorque = FMath::Lerp(
				FMath::Max(0.0f, SteeringTorqueAtLowSpeed),
				FMath::Max(0.0f, SteeringTorqueAtHighSpeed),
				SpeedAlpha);
			const float DirectionScale = ForwardSpeed >= 0.0f ? 1.0f : -1.0f;
			SimulatedPrimitive->AddTorqueInRadians(
				FVector::UpVector * (ClampedSteering * SteeringTorque * DirectionScale),
				NAME_None,
				true);
		}
		return;
	}

	const bool bHasInput = !FMath::IsNearlyZero(Input, KINDA_SMALL_NUMBER);
	if (bHasInput)
	{
		const float Direction = FMath::Sign(Input);
		CurrentForwardSpeed += Direction * FMath::Max(0.0f, KinematicAcceleration) * DeltaTime;
	}
	else
	{
		const float Slowdown = FMath::Max(0.0f, bHandbrakeActive ? KinematicBrakeDeceleration : KinematicDeceleration);
		CurrentForwardSpeed = FMath::FInterpConstantTo(CurrentForwardSpeed, 0.0f, DeltaTime, Slowdown);
	}

	CurrentForwardSpeed = FMath::Clamp(CurrentForwardSpeed, -MaxReverse, MaxForward);
	Velocity = UpdatedComponent->GetForwardVector() * CurrentForwardSpeed;

	const FVector DeltaLocation = Velocity * DeltaTime;
	if (!DeltaLocation.IsNearlyZero())
	{
		FHitResult Hit;
		SafeMoveUpdatedComponent(DeltaLocation, UpdatedComponent->GetComponentQuat(), true, Hit);
		if (Hit.IsValidBlockingHit())
		{
			SlideAlongSurface(DeltaLocation, 1.0f - Hit.Time, Hit.Normal, Hit, true);
		}
	}

	if (!FMath::IsNearlyZero(ClampedSteering, 0.001f))
	{
		const float SpeedAlpha = MaxForward > KINDA_SMALL_NUMBER
			? FMath::Clamp(FMath::Abs(CurrentForwardSpeed) / MaxForward, 0.0f, 1.0f)
			: 0.0f;
		const float TurnRate = FMath::Lerp(KinematicTurnRateAtLowSpeed, KinematicTurnRateAtHighSpeed, SpeedAlpha);
		const float ReverseSteerScale = CurrentForwardSpeed >= 0.0f ? 1.0f : -1.0f;
		const float YawDelta = ClampedSteering * TurnRate * ReverseSteerScale * DeltaTime;
		if (!FMath::IsNearlyZero(YawDelta))
		{
			const FRotator NewRotation = UpdatedComponent->GetComponentRotation() + FRotator(0.0f, YawDelta, 0.0f);
			MoveUpdatedComponent(FVector::ZeroVector, NewRotation, true);
		}
	}
}

float UTrolleyMovementComponent::GetMaxSpeed() const
{
	return CurrentForwardSpeed >= 0.0f
		? FMath::Max(0.0f, MaxForwardSpeed)
		: FMath::Max(0.0f, MaxReverseSpeed);
}

void UTrolleyMovementComponent::StopMovementImmediately()
{
	Super::StopMovementImmediately();
	if (UPrimitiveComponent* SimulatedPrimitive = Cast<UPrimitiveComponent>(UpdatedComponent))
	{
		if (SimulatedPrimitive->IsSimulatingPhysics())
		{
			SimulatedPrimitive->SetPhysicsLinearVelocity(FVector::ZeroVector);
			SimulatedPrimitive->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		}
	}

	CurrentForwardSpeed = 0.0f;
	Velocity = FVector::ZeroVector;
}

void UTrolleyMovementComponent::SetThrottleInput(float Value)
{
	ThrottleInput = FMath::Clamp(Value, -1.0f, 1.0f);
}

void UTrolleyMovementComponent::SetSteeringInput(float Value)
{
	SteeringInput = FMath::Clamp(Value, -1.0f, 1.0f);
}

void UTrolleyMovementComponent::SetHandbrakeInput(bool bNewHandbrakeActive)
{
	bHandbrakeActive = bNewHandbrakeActive;
}

float UTrolleyMovementComponent::GetForwardSpeed() const
{
	return CurrentForwardSpeed;
}
