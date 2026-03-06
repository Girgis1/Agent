// Copyright Epic Games, Inc. All Rights Reserved.

#include "Vehicle/Components/TrolleyMovementComponent.h"
#include "Vehicle/Components/VehicleSeatComponent.h"
#include "Factory/ConveyorSurfaceVelocityComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
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

	const float StrengthMultiplier = FMath::Max(0.0f, this->StrengthSpeedMultiplier);
	const float BaseMaxForward = FMath::Max(0.0f, MaxForwardSpeed);
	const float MaxForward = BaseMaxForward * StrengthMultiplier;
	const float MaxReverse = FMath::Max(0.0f, MaxReverseSpeed) * StrengthMultiplier;
	const float Input = FMath::Clamp(ThrottleInput, -1.0f, 1.0f);
	const float ClampedSteering = FMath::Clamp(SteeringInput, -1.0f, 1.0f);
	const float EffectivePayloadMassKg = FMath::Max(0.0f, PayloadMassKg) * FMath::Max(0.0f, PayloadMassInfluence);
	FVector ConveyorVelocity = FVector::ZeroVector;
	if (const AActor* OwnerActor = GetOwner())
	{
		if (const UConveyorSurfaceVelocityComponent* SurfaceVelocityComponent = OwnerActor->FindComponentByClass<UConveyorSurfaceVelocityComponent>())
		{
			ConveyorVelocity = SurfaceVelocityComponent->GetConveyorSurfaceVelocity();
		}
	}

	UPrimitiveComponent* SimulatedPrimitive = Cast<UPrimitiveComponent>(UpdatedComponent);
	if (SimulatedPrimitive && SimulatedPrimitive->IsSimulatingPhysics())
	{
		const float BaseBodyMassKg = FMath::Max(1.0f, SimulatedPrimitive->GetMass());
		const float LoadRatio = 1.0f + (EffectivePayloadMassKg / BaseBodyMassKg);
		const float LoadDriveScale = FMath::Clamp(
			FMath::Pow(LoadRatio, -FMath::Max(0.0f, LoadDriveExponent)),
			FMath::Clamp(MinLoadDriveScale, 0.0f, 1.0f),
			1.0f);
		const float StrengthCargoScale = FMath::Clamp(StrengthToCargoScale, 0.0f, 1.0f);
		const float StrengthLoadFactor = FMath::Pow(FMath::Max(0.01f, StrengthMultiplier), StrengthCargoScale);
		const float DriveScale = FMath::Max(0.0f, LoadDriveScale * StrengthLoadFactor);
		const float SteeringScale = FMath::Lerp(
			1.0f,
			DriveScale,
			FMath::Clamp(LoadAffectsSteering, 0.0f, 1.0f));

		const FVector Forward = UpdatedComponent->GetForwardVector();
		const FVector Right = UpdatedComponent->GetRightVector();
		const FVector LinearVelocity = SimulatedPrimitive->GetPhysicsLinearVelocity();
		const float ForwardSpeed = FVector::DotProduct(LinearVelocity, Forward);
		const float SideSpeed = FVector::DotProduct(LinearVelocity, Right);
		const float LowSpeedStrafeBlend = ComputeLowSpeedStrafeBlend(FMath::Abs(ForwardSpeed));

		CurrentForwardSpeed = ForwardSpeed;
		CurrentLateralSpeed = SideSpeed;
		Velocity = LinearVelocity;

		const float SpeedAlpha = MaxForward > KINDA_SMALL_NUMBER
			? FMath::Clamp(FMath::Abs(ForwardSpeed) / MaxForward, 0.0f, 1.0f)
			: 0.0f;
		const float DriveLimitScale = FMath::Max(0.05f, 1.0f - SpeedAlpha) * DriveScale;
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

			const float MaxStrafeSpeed = BaseMaxForward
				* FMath::Max(0.0f, LowSpeedStrafeSlowFactor)
				* LowSpeedStrafeBlend;
			const float StrafeSpeedAlpha = MaxStrafeSpeed > KINDA_SMALL_NUMBER
				? FMath::Clamp(FMath::Abs(SideSpeed) / MaxStrafeSpeed, 0.0f, 1.0f)
				: 1.0f;
			const float StrafeDriveLimitScale = FMath::Max(0.05f, 1.0f - StrafeSpeedAlpha) * FMath::Max(0.0f, LoadDriveScale);
		if (MaxStrafeSpeed > KINDA_SMALL_NUMBER && !FMath::IsNearlyZero(ClampedSteering, 0.01f))
		{
			const float StrafeForce = FMath::Max(0.0f, LowSpeedStrafeForce)
				* StrafeDriveLimitScale;
			FVector StrafeForceLocation = RearDriveLocation;
			if (const AActor* OwnerActor = GetOwner())
			{
				if (const UVehicleSeatComponent* SeatComponent = OwnerActor->FindComponentByClass<UVehicleSeatComponent>())
				{
					if (const USceneComponent* AttachPoint = SeatComponent->DriverAttachPoint.Get())
					{
						const float ForwardOffset = LowSpeedStrafeBlend * LowSpeedStrafeForcePointForwardOffset;
						StrafeForceLocation = AttachPoint->GetComponentLocation()
							+ (AttachPoint->GetForwardVector() * ForwardOffset);
					}
				}
			}
			SimulatedPrimitive->AddForceAtLocation(
				Right * (ClampedSteering * StrafeForce),
				StrafeForceLocation);
		}

		if (bHandbrakeActive)
		{
			SimulatedPrimitive->AddForce(-(Forward * ForwardSpeed) * FMath::Max(0.0f, HandbrakeDrag) * Mass);
		}

		if (!ConveyorVelocity.IsNearlyZero())
		{
			const FVector ConveyorPlanarVelocity(ConveyorVelocity.X, ConveyorVelocity.Y, 0.0f);
			const FVector CurrentPlanarVelocity(LinearVelocity.X, LinearVelocity.Y, 0.0f);
			const FVector ConveyorVelocityDelta = ConveyorPlanarVelocity - CurrentPlanarVelocity;
			SimulatedPrimitive->AddForce(ConveyorVelocityDelta * Mass * FMath::Max(0.0f, ConveyorCarryForceScale));

			if (ConveyorVelocityAssist > 0.0f)
			{
				FVector AssistedVelocity = LinearVelocity;
				AssistedVelocity += ConveyorPlanarVelocity * (ConveyorVelocityAssist * DeltaTime);
				SimulatedPrimitive->SetPhysicsLinearVelocity(AssistedVelocity, false);
			}
		}

		if (!FMath::IsNearlyZero(ClampedSteering, 0.01f)
			&& FMath::Abs(ForwardSpeed) >= FMath::Max(0.0f, MinSteerSpeed))
		{
			const float SteeringForce = FMath::Lerp(
				FMath::Max(0.0f, SteeringForceAtLowSpeed),
				FMath::Max(0.0f, SteeringForceAtHighSpeed),
				SpeedAlpha) * SteeringScale;
			const float DirectionScale = ForwardSpeed >= 0.0f ? 1.0f : -1.0f;
			const FVector FrontSteerLocation = SimulatedPrimitive->GetCenterOfMass() + (Forward * FrontSteerOffset);
			const FVector SteeringVector = Right * (ClampedSteering * SteeringForce * DirectionScale);
			SimulatedPrimitive->AddForceAtLocation(SteeringVector, FrontSteerLocation);
		}

		{
			FVector AngularVelocityDeg = SimulatedPrimitive->GetPhysicsAngularVelocityInDegrees();
			const FVector OriginalAngularVelocityDeg = AngularVelocityDeg;
			AngularVelocityDeg.X = FMath::Clamp(
				AngularVelocityDeg.X,
				-MaxPitchRollAngularSpeedDeg,
				MaxPitchRollAngularSpeedDeg);
			AngularVelocityDeg.Y = FMath::Clamp(
				AngularVelocityDeg.Y,
				-MaxPitchRollAngularSpeedDeg,
				MaxPitchRollAngularSpeedDeg);
			AngularVelocityDeg.Z = FMath::Clamp(
				AngularVelocityDeg.Z,
				-MaxYawAngularSpeedDeg,
				MaxYawAngularSpeedDeg);
			if (!AngularVelocityDeg.Equals(OriginalAngularVelocityDeg, KINDA_SMALL_NUMBER))
			{
				SimulatedPrimitive->SetPhysicsAngularVelocityInDegrees(AngularVelocityDeg, false);
			}
		}
		return;
	}

	const bool bHasInput = !FMath::IsNearlyZero(Input, KINDA_SMALL_NUMBER);
	const float KinematicBaseMassKg = 250.0f;
	const float KinematicLoadRatio = 1.0f + (EffectivePayloadMassKg / KinematicBaseMassKg);
	const float KinematicLoadScale = FMath::Clamp(
		FMath::Pow(KinematicLoadRatio, -FMath::Max(0.0f, LoadDriveExponent)),
		FMath::Clamp(MinLoadDriveScale, 0.0f, 1.0f),
		1.0f);
	const float StrengthCargoScale = FMath::Clamp(StrengthToCargoScale, 0.0f, 1.0f);
	const float KinematicStrengthLoadFactor = FMath::Pow(FMath::Max(0.01f, StrengthMultiplier), StrengthCargoScale);
	const float KinematicDriveScale = FMath::Max(0.0f, KinematicLoadScale * KinematicStrengthLoadFactor);
	const float KinematicStrafeScale = FMath::Max(0.0f, KinematicLoadScale);
	if (bHasInput)
	{
		const float Direction = FMath::Sign(Input);
		CurrentForwardSpeed += Direction * FMath::Max(0.0f, KinematicAcceleration) * KinematicDriveScale * DeltaTime;
	}
	else
	{
		const float Slowdown = FMath::Max(0.0f, bHandbrakeActive ? KinematicBrakeDeceleration : KinematicDeceleration);
		CurrentForwardSpeed = FMath::FInterpConstantTo(CurrentForwardSpeed, 0.0f, DeltaTime, Slowdown);
	}

	CurrentForwardSpeed = FMath::Clamp(CurrentForwardSpeed, -MaxReverse, MaxForward);
	const float LowSpeedStrafeBlend = ComputeLowSpeedStrafeBlend(FMath::Abs(CurrentForwardSpeed));
	const float KinematicMaxStrafeSpeed = BaseMaxForward
		* FMath::Max(0.0f, LowSpeedStrafeSlowFactor)
		* LowSpeedStrafeBlend;
	const bool bHasStrafeInput = !FMath::IsNearlyZero(ClampedSteering, KINDA_SMALL_NUMBER);
	if (bHasStrafeInput && KinematicMaxStrafeSpeed > KINDA_SMALL_NUMBER)
	{
		const float Direction = FMath::Sign(ClampedSteering);
		CurrentLateralSpeed += Direction * FMath::Max(0.0f, KinematicAcceleration) * KinematicStrafeScale * DeltaTime;
	}
	else
	{
		const float LateralSlowdown = FMath::Max(0.0f, bHandbrakeActive ? KinematicBrakeDeceleration : KinematicDeceleration);
		CurrentLateralSpeed = FMath::FInterpConstantTo(CurrentLateralSpeed, 0.0f, DeltaTime, LateralSlowdown);
	}
	CurrentLateralSpeed = FMath::Clamp(CurrentLateralSpeed, -KinematicMaxStrafeSpeed, KinematicMaxStrafeSpeed);
	Velocity = (UpdatedComponent->GetForwardVector() * CurrentForwardSpeed)
		+ (UpdatedComponent->GetRightVector() * CurrentLateralSpeed);

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
		const float TurnRate = FMath::Lerp(KinematicTurnRateAtLowSpeed, KinematicTurnRateAtHighSpeed, SpeedAlpha)
			* FMath::Lerp(1.0f, KinematicDriveScale, FMath::Clamp(LoadAffectsSteering, 0.0f, 1.0f));
		const float ReverseSteerScale = CurrentForwardSpeed >= 0.0f ? 1.0f : -1.0f;
		const float YawDelta = ClampedSteering * TurnRate * ReverseSteerScale * DeltaTime;
		if (!FMath::IsNearlyZero(YawDelta))
		{
			const FRotator NewRotation = UpdatedComponent->GetComponentRotation() + FRotator(0.0f, YawDelta, 0.0f);
			MoveUpdatedComponent(FVector::ZeroVector, NewRotation, true);
		}
	}

	if (!ConveyorVelocity.IsNearlyZero())
	{
		const FVector ConveyorDelta = ConveyorVelocity * DeltaTime * FMath::Max(0.0f, ConveyorVelocityAssist);
		if (!ConveyorDelta.IsNearlyZero())
		{
			FHitResult Hit;
			SafeMoveUpdatedComponent(ConveyorDelta, UpdatedComponent->GetComponentQuat(), true, Hit);
			if (Hit.IsValidBlockingHit())
			{
				SlideAlongSurface(ConveyorDelta, 1.0f - Hit.Time, Hit.Normal, Hit, true);
			}
		}
	}
}

float UTrolleyMovementComponent::GetMaxSpeed() const
{
	const float StrengthMultiplier = FMath::Max(0.0f, this->StrengthSpeedMultiplier);
	return CurrentForwardSpeed >= 0.0f
		? FMath::Max(0.0f, MaxForwardSpeed) * StrengthMultiplier
		: FMath::Max(0.0f, MaxReverseSpeed) * StrengthMultiplier;
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
	CurrentLateralSpeed = 0.0f;
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

void UTrolleyMovementComponent::SetPayloadMassKg(float NewPayloadMassKg)
{
	PayloadMassKg = FMath::Max(0.0f, NewPayloadMassKg);
}

void UTrolleyMovementComponent::SetDriverStrengthSpeedMultiplier(float NewDriverStrengthSpeedMultiplier)
{
	StrengthSpeedMultiplier = FMath::Max(0.0f, NewDriverStrengthSpeedMultiplier);
}

float UTrolleyMovementComponent::GetForwardSpeed() const
{
	return CurrentForwardSpeed;
}

float UTrolleyMovementComponent::ComputeLowSpeedStrafeBlend(float AbsForwardSpeed) const
{
	const float Threshold = FMath::Max(0.0f, LowSpeedStrafeThreshold);
	if (Threshold <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const float NormalizedSpeed = FMath::Clamp(AbsForwardSpeed / Threshold, 0.0f, 1.0f);
	const float Smoothed = NormalizedSpeed * NormalizedSpeed * (3.0f - (2.0f * NormalizedSpeed));
	const float Blend = 1.0f - Smoothed;
	return FMath::Pow(
		FMath::Clamp(Blend, 0.0f, 1.0f),
		FMath::Max(0.01f, LowSpeedStrafeBlendExponent));
}
