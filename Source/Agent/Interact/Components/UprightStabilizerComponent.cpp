// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interact/Components/UprightStabilizerComponent.h"
#include "Components/PrimitiveComponent.h"

UUprightStabilizerComponent::UUprightStabilizerComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UUprightStabilizerComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!PhysicsBody)
	{
		PhysicsBody = Cast<UPrimitiveComponent>(GetOwner() ? GetOwner()->GetRootComponent() : nullptr);
	}
}

void UUprightStabilizerComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bStabilizationEnabled || !PhysicsBody || !PhysicsBody->IsSimulatingPhysics())
	{
		return;
	}

	const FVector BodyUp = PhysicsBody->GetUpVector().GetSafeNormal();
	const FVector WorldUp = FVector::UpVector;
	const float UpDot = FMath::Clamp(FVector::DotProduct(BodyUp, WorldUp), -1.0f, 1.0f);
	const float TiltRadians = FMath::Acos(UpDot);
	const float TiltDegrees = FMath::RadiansToDegrees(TiltRadians);
	if (TiltDegrees <= FMath::Max(0.0f, UprightDeadZoneDegrees))
	{
		return;
	}

	FVector CorrectionAxis = FVector::CrossProduct(BodyUp, WorldUp);
	const float AxisMagnitude = CorrectionAxis.Size();
	if (AxisMagnitude <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	CorrectionAxis /= AxisMagnitude;
	const float BodyMassKg = FMath::Max(0.01f, PhysicsBody->GetMass());
	FVector CorrectionTorque = CorrectionAxis * (TiltRadians * UprightTorqueStrength * BodyMassKg);

	FVector AngularVelocity = PhysicsBody->GetPhysicsAngularVelocityInRadians(NAME_None);
	AngularVelocity -= FVector::DotProduct(AngularVelocity, WorldUp) * WorldUp;
	const FVector DampingTorque = -AngularVelocity * (UprightAngularDamping * BodyMassKg);

	FVector TotalTorque = CorrectionTorque + DampingTorque;
	const float MaxTorqueMagnitude = FMath::Max(0.0f, MaxUprightTorque) * BodyMassKg;
	if (MaxTorqueMagnitude > 0.0f)
	{
		TotalTorque = TotalTorque.GetClampedToMaxSize(MaxTorqueMagnitude);
	}

	PhysicsBody->AddTorqueInRadians(TotalTorque, NAME_None, false);
}

void UUprightStabilizerComponent::SetPhysicsBody(UPrimitiveComponent* InPhysicsBody)
{
	PhysicsBody = InPhysicsBody;
}

void UUprightStabilizerComponent::SetStabilizationEnabled(bool bEnabled)
{
	bStabilizationEnabled = bEnabled;
	if (bStabilizationEnabled && PhysicsBody)
	{
		PhysicsBody->WakeAllRigidBodies();
	}
}

bool UUprightStabilizerComponent::IsStabilizationEnabled() const
{
	return bStabilizationEnabled;
}

