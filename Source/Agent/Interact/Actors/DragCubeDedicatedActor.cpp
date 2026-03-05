// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interact/Actors/DragCubeDedicatedActor.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Interact/Components/PhysicsDragFollowerComponent.h"
#include "Interact/Components/UprightStabilizerComponent.h"
#include "GameFramework/Character.h"

ADragCubeDedicatedActor::ADragCubeDedicatedActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SetInteractionDistances(330.0f, 500.0f);

	UprightStabilizer = CreateDefaultSubobject<UUprightStabilizerComponent>(TEXT("UprightStabilizer"));

	if (USceneComponent* HandleAnchorComponent = GetHandleAnchorComponent())
	{
		HandleAnchorComponent->SetRelativeLocation(FVector(-72.0f, 0.0f, 56.0f));
	}

	if (UStaticMeshComponent* CubeBodyComponent = GetCubeBodyComponent())
	{
		CubeBodyComponent->SetLinearDamping(0.7f);
		CubeBodyComponent->SetAngularDamping(0.55f);
	}

	if (UPhysicsDragFollowerComponent* DragFollowerComponent = GetDragFollowerComponent())
	{
		DragFollowerComponent->InteractorHandleOffset = FVector(132.0f, 0.0f, 12.0f);
		DragFollowerComponent->MaxPositionError = 320.0f;
		DragFollowerComponent->MaxLateralError = 135.0f;
		DragFollowerComponent->MaxVerticalError = 60.0f;
		DragFollowerComponent->HorizontalPositionGain = 34.0f;
		DragFollowerComponent->ForwardPositionGainMultiplier = 1.42f;
		DragFollowerComponent->LateralPositionGainMultiplier = 0.5f;
		DragFollowerComponent->HorizontalVelocityDamping = 12.5f;
		DragFollowerComponent->ForwardVelocityDampingMultiplier = 1.35f;
		DragFollowerComponent->LateralVelocityDampingMultiplier = 1.05f;
		DragFollowerComponent->VerticalPositionGain = 6.0f;
		DragFollowerComponent->VerticalVelocityDamping = 10.5f;
		DragFollowerComponent->MaxHorizontalAcceleration = 2150.0f;
		DragFollowerComponent->MaxForwardPushAccelerationMultiplier = 1.75f;
		DragFollowerComponent->MaxForwardPullAccelerationMultiplier = 0.5f;
		DragFollowerComponent->MaxLateralAccelerationMultiplier = 0.62f;
		DragFollowerComponent->MaxVerticalAcceleration = 850.0f;
		DragFollowerComponent->MaxRaiseAccelerationMultiplier = 0.2f;
		DragFollowerComponent->MaxDropAccelerationMultiplier = 0.9f;
		DragFollowerComponent->TargetLeadTime = 0.09f;
		DragFollowerComponent->ExtraLinearDragAcceleration = 1.5f;
		DragFollowerComponent->ForwardDragFraction = 0.28f;
		DragFollowerComponent->bEnableYawAlignment = true;
		DragFollowerComponent->YawAlignmentStrength = 80.0f;
		DragFollowerComponent->YawAlignmentDamping = 25.0f;
		DragFollowerComponent->MaxYawAlignmentTorque = 100.0f;
		DragFollowerComponent->bAllowSteerInput = true;
		DragFollowerComponent->SteerYawRate = 92.0f;
		DragFollowerComponent->MaxSteerYawOffset = 70.0f;
		DragFollowerComponent->SteerCenteringSpeed = 0.0f;
		DragFollowerComponent->bAllowLiftInput = true;
		DragFollowerComponent->MaxLiftOffset = 38.0f;
		DragFollowerComponent->LiftInputInterpSpeed = 11.0f;
		DragFollowerComponent->bDrawDebug = true;
		DragFollowerComponent->DebugSphereRadius = 5.5f;
		DragFollowerComponent->DebugLineThickness = 1.8f;
	}

	bUseMoveRightAsSteerFallback = false;
}

void ADragCubeDedicatedActor::BeginPlay()
{
	Super::BeginPlay();

	if (UStaticMeshComponent* CubeBodyComponent = GetCubeBodyComponent())
	{
		if (bOverrideCubeMass)
		{
			CubeBodyComponent->SetMassOverrideInKg(NAME_None, FMath::Max(1.0f, CubeMassKg), true);
		}
	}

	if (UPhysicsDragFollowerComponent* DragFollowerComponent = GetDragFollowerComponent())
	{
		DragFollowerComponent->bDrawDebug = bEnableFollowerDebug;
	}

	if (UprightStabilizer)
	{
		UprightStabilizer->SetPhysicsBody(GetActivePhysicsBodyComponent());
		UprightStabilizer->UprightTorqueStrength = FMath::Max(0.0f, UprightAssistTorqueStrength);
		UprightStabilizer->UprightAngularDamping = FMath::Max(0.0f, UprightAssistAngularDamping);
		UprightStabilizer->MaxUprightTorque = FMath::Max(0.0f, UprightAssistMaxTorque);
		UprightStabilizer->SetStabilizationEnabled(false);
	}
}

void ADragCubeDedicatedActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AActor* CurrentInteractor = GetActiveInteractorActor();
	UPrimitiveComponent* ActiveBody = GetActivePhysicsBodyComponent();
	const bool bIsActivelyDriven = CurrentInteractor && ActiveBody && ActiveBody->IsSimulatingPhysics();

	if (!bIsActivelyDriven)
	{
		ResetControlInput();
		if (UprightStabilizer)
		{
			UprightStabilizer->SetStabilizationEnabled(false);
		}
		return;
	}

	if (UprightStabilizer)
	{
		UprightStabilizer->SetStabilizationEnabled(bEnableHandUprightAssist);
	}

	ApplyFollowerControlInput();
	ApplyDriveAssist();
	ApplyInteractorFollowAssist(DeltaSeconds);
}

bool ADragCubeDedicatedActor::BeginInteract_Implementation(AActor* Interactor)
{
	const bool bDidBegin = Super::BeginInteract_Implementation(Interactor);
	if (!bDidBegin)
	{
		return false;
	}

	ResetControlInput();
	if (UprightStabilizer)
	{
		UprightStabilizer->SetStabilizationEnabled(bEnableHandUprightAssist);
	}

	return true;
}

void ADragCubeDedicatedActor::EndInteract_Implementation(AActor* Interactor)
{
	Super::EndInteract_Implementation(Interactor);
	ResetControlInput();
	if (UprightStabilizer)
	{
		UprightStabilizer->SetStabilizationEnabled(false);
	}
}

void ADragCubeDedicatedActor::SetDriveInput(float ForwardValue, float RightValue)
{
	DriveForwardInput = FMath::Clamp(ForwardValue, -1.0f, 1.0f);
	MoveSteerInput = FMath::Clamp(RightValue, -1.0f, 1.0f);
	ApplyFollowerControlInput();
}

void ADragCubeDedicatedActor::SetSteerInput(float Value)
{
	StickSteerInput = FMath::Clamp(Value, -1.0f, 1.0f);
	ApplyFollowerControlInput();
}

void ADragCubeDedicatedActor::SetLiftInput(float Value)
{
	LiftControlInput = FMath::Clamp(Value, -1.0f, 1.0f);
	ApplyFollowerControlInput();
}

bool ADragCubeDedicatedActor::IsDrivenBy(const AActor* Interactor) const
{
	return Interactor && GetActiveInteractorActor() == Interactor;
}

void ADragCubeDedicatedActor::ApplyDriveAssist()
{
	UPrimitiveComponent* ActiveBody = GetActivePhysicsBodyComponent();
	if (!ActiveBody || !ActiveBody->IsSimulatingPhysics())
	{
		return;
	}

	FVector BodyForward = ActiveBody->GetForwardVector();
	BodyForward.Z = 0.0f;
	if (!BodyForward.Normalize())
	{
		BodyForward = FVector::ForwardVector;
	}

	FVector BodyRight = ActiveBody->GetRightVector();
	BodyRight.Z = 0.0f;
	if (!BodyRight.Normalize())
	{
		BodyRight = FVector::RightVector;
	}

	const FVector BodyVelocity = ActiveBody->GetPhysicsLinearVelocity(NAME_None);
	const float ForwardSpeed = FVector::DotProduct(BodyVelocity, BodyForward);
	const float LateralSpeed = FVector::DotProduct(BodyVelocity, BodyRight);
	const float TargetForwardSpeed = DriveForwardInput * FMath::Max(0.0f, MaxDriveSpeed);
	float ForwardAcceleration = (TargetForwardSpeed - ForwardSpeed) * FMath::Max(0.0f, DriveResponse);
	ForwardAcceleration -= ForwardSpeed * FMath::Max(0.0f, ForwardSpeedDamping);
	ForwardAcceleration = FMath::Clamp(
		ForwardAcceleration,
		-FMath::Max(0.0f, DriveBrakeAcceleration),
		FMath::Max(0.0f, DriveAcceleration));

	const float LateralAcceleration = -LateralSpeed * FMath::Max(0.0f, LateralSpeedDamping);
	const FVector DriveAccelerationVector = (BodyForward * ForwardAcceleration) + (BodyRight * LateralAcceleration);
	const float BodyMassKg = FMath::Max(0.01f, ActiveBody->GetMass());
	ActiveBody->AddForce(DriveAccelerationVector * BodyMassKg, NAME_None, false);
}

void ADragCubeDedicatedActor::ApplyInteractorFollowAssist(float DeltaSeconds)
{
	if (!bEnableInteractorFollowAssist)
	{
		return;
	}

	AActor* CurrentInteractor = GetActiveInteractorActor();
	ACharacter* InteractorCharacter = Cast<ACharacter>(CurrentInteractor);
	UPrimitiveComponent* ActiveBody = GetActivePhysicsBodyComponent();
	USceneComponent* HandleAnchorComponent = GetHandleAnchorComponent();
	if (!InteractorCharacter || !ActiveBody || !HandleAnchorComponent)
	{
		return;
	}

	FVector BodyForward = ActiveBody->GetForwardVector();
	BodyForward.Z = 0.0f;
	if (!BodyForward.Normalize())
	{
		BodyForward = FVector::ForwardVector;
	}

	const FVector InteractorLocation = InteractorCharacter->GetActorLocation();
	FVector TargetInteractorLocation = HandleAnchorComponent->GetComponentLocation()
		- (BodyForward * FMath::Max(0.0f, InteractorFollowDistance));
	TargetInteractorLocation.Z = InteractorLocation.Z;

	FVector ToTarget = TargetInteractorLocation - InteractorLocation;
	ToTarget.Z = 0.0f;
	const float DistanceToTarget = ToTarget.Size();
	if (DistanceToTarget > KINDA_SMALL_NUMBER)
	{
		FVector BodyRight = ActiveBody->GetRightVector();
		BodyRight.Z = 0.0f;
		if (!BodyRight.Normalize())
		{
			BodyRight = FVector::RightVector;
		}

		const float FollowScale = (DistanceToTarget / FMath::Max(1.0f, InteractorFollowDistance))
			* FMath::Max(0.0f, InteractorFollowInputScale);
		const float ForwardError = FVector::DotProduct(ToTarget, BodyForward);
		const float LateralError = FVector::DotProduct(ToTarget, BodyRight);
		const float ForwardInput = FMath::Clamp(
			(ForwardError / FMath::Max(1.0f, InteractorFollowDistance)) * FollowScale,
			-1.5f,
			1.5f);
		const float LateralInput = FMath::Clamp(
			(LateralError / FMath::Max(1.0f, InteractorFollowDistance))
				* FollowScale
				* FMath::Max(0.0f, InteractorFollowLateralInputScale),
			-0.8f,
			0.8f);
		InteractorCharacter->AddMovementInput(BodyForward, ForwardInput);
		InteractorCharacter->AddMovementInput(BodyRight, LateralInput);
	}

	if (InteractorYawAlignSpeed > 0.0f)
	{
		const FRotator CurrentRotation = InteractorCharacter->GetActorRotation();
		const FRotator TargetRotation(0.0f, BodyForward.Rotation().Yaw, 0.0f);
		InteractorCharacter->SetActorRotation(
			FMath::RInterpTo(CurrentRotation, TargetRotation, FMath::Max(0.0f, DeltaSeconds), InteractorYawAlignSpeed));
	}
}

void ADragCubeDedicatedActor::ApplyFollowerControlInput()
{
	UPhysicsDragFollowerComponent* DragFollowerComponent = GetDragFollowerComponent();
	if (!DragFollowerComponent)
	{
		return;
	}

	const float EffectiveSteerInput = (FMath::Abs(StickSteerInput) > 0.05f)
		? StickSteerInput
		: (bUseMoveRightAsSteerFallback ? MoveSteerInput : 0.0f);
	DragFollowerComponent->SetSteerInput(EffectiveSteerInput);
	DragFollowerComponent->SetLiftInput(LiftControlInput);
}

void ADragCubeDedicatedActor::ResetControlInput()
{
	DriveForwardInput = 0.0f;
	MoveSteerInput = 0.0f;
	StickSteerInput = 0.0f;
	LiftControlInput = 0.0f;
	if (UPhysicsDragFollowerComponent* DragFollowerComponent = GetDragFollowerComponent())
	{
		DragFollowerComponent->ResetControlInputs();
	}
}
