// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interact/Components/GrabVehicleComponent.h"
#include "Interact/Components/GrabVehiclePushComponent.h"
#include "Interact/Components/InteractVolumeComponent.h"

UGrabVehicleComponent::UGrabVehicleComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGrabVehicleComponent::ApplyInteractionVolume(UInteractVolumeComponent* InInteractVolume) const
{
	if (!InInteractVolume)
	{
		return;
	}

	const FVector SafeExtent(
		FMath::Max(1.0f, InteractVolumeExtent.X),
		FMath::Max(1.0f, InteractVolumeExtent.Y),
		FMath::Max(1.0f, InteractVolumeExtent.Z));
	InInteractVolume->InitBoxExtent(SafeExtent);
}

void UGrabVehicleComponent::ApplyPushTuning(
	UGrabVehiclePushComponent* InPushComponent,
	float FallbackDebugSphereRadius) const
{
	if (!InPushComponent || !bOverridePushTuning)
	{
		return;
	}

	InPushComponent->HoldForwardOffset = FMath::Max(0.0f, HoldForwardOffset);
	InPushComponent->HoldLateralOffset = HoldLateralOffset;
	InPushComponent->HoldVerticalOffset = HoldVerticalOffset;
	InPushComponent->bUseInitialGrabVerticalOffset = bUseInitialGrabVerticalOffset;
	InPushComponent->bAllowLiftInput = bAllowLiftInput;
	InPushComponent->MaxLiftOffset = FMath::Max(0.0f, MaxLiftOffset);
	InPushComponent->LiftInputInterpSpeed = FMath::Max(0.0f, LiftInputInterpSpeed);
	InPushComponent->BreakDistance = FMath::Max(0.0f, BreakDistance);
	InPushComponent->MaxStartGrabDistance = FMath::Max(0.0f, MaxStartGrabDistance);
	InPushComponent->HandleLinearStiffness = FMath::Max(0.0f, HandleLinearStiffness);
	InPushComponent->HandleLinearDamping = FMath::Max(0.0f, HandleLinearDamping);
	InPushComponent->HandleAngularStiffness = FMath::Max(0.0f, HandleAngularStiffness);
	InPushComponent->HandleAngularDamping = FMath::Max(0.0f, HandleAngularDamping);
	InPushComponent->HandleInterpolationSpeed = FMath::Max(0.0f, HandleInterpolationSpeed);
	InPushComponent->SteerYawRate = FMath::Max(0.0f, SteerYawRate);
	InPushComponent->YawTorqueStrength = FMath::Max(0.0f, YawTorqueStrength);
	InPushComponent->YawTorqueDamping = FMath::Max(0.0f, YawTorqueDamping);
	InPushComponent->MaxYawTorque = FMath::Max(0.0f, MaxYawTorque);
	InPushComponent->bDrawDebug = bDrawDebug;
	InPushComponent->DebugSphereRadius = FMath::Max(
		0.1f,
		bOverrideDebugSphereRadius ? DebugSphereRadius : FallbackDebugSphereRadius);
	InPushComponent->DebugLineThickness = FMath::Max(0.1f, DebugLineThickness);
}

float UGrabVehicleComponent::GetMaxInteractDistance() const
{
	return FMath::Max(0.0f, MaxInteractDistance);
}
