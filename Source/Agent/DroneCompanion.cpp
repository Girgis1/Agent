// Copyright Epic Games, Inc. All Rights Reserved.

#include "DroneCompanion.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Engine.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/ConstructorHelpers.h"

ADroneCompanion::ADroneCompanion()
{
	PrimaryActorTick.bCanEverTick = true;

	DroneBody = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DroneBody"));
	SetRootComponent(DroneBody);
	DroneBody->SetCollisionObjectType(ECC_PhysicsBody);
	DroneBody->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	DroneBody->SetCollisionResponseToAllChannels(ECR_Block);
	DroneBody->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	DroneBody->SetSimulatePhysics(true);
	DroneBody->SetEnableGravity(true);
	DroneBody->SetNotifyRigidBodyCollision(true);
	DroneBody->BodyInstance.bUseCCD = true;
	DroneBody->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DroneSphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (DroneSphereMesh.Succeeded())
	{
		DroneBody->SetStaticMesh(DroneSphereMesh.Object);
	}

	DroneBody->SetRelativeScale3D(FVector(0.5f));

	CameraMount = CreateDefaultSubobject<USceneComponent>(TEXT("CameraMount"));
	CameraMount->SetupAttachment(DroneBody);

	DroneCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("DroneCamera"));
	DroneCamera->SetupAttachment(CameraMount);
	DroneCamera->bUsePawnControlRotation = false;
	DroneCamera->FieldOfView = PilotCameraFieldOfView;
}

void ADroneCompanion::BeginPlay()
{
	Super::BeginPlay();

	if (DroneBody)
	{
		DroneBody->SetRelativeScale3D(FVector(FMath::Max(DroneBodyVisualScale, 0.01f)));
		DroneBody->SetMassOverrideInKg(NAME_None, FMath::Max(0.1f, DroneMassKg), true);
		DroneBody->SetLinearDamping(DroneBodyLinearDamping);
		DroneBody->SetAngularDamping(DroneBodyAngularDamping);
		DroneBody->OnComponentHit.AddDynamic(this, &ADroneCompanion::OnDroneBodyHit);
	}

	ApplyRuntimePhysicalMaterial();
	AdjustCameraTilt(0.0f);
	UpdateBuddyDrift(BuddyDriftUpdateInterval);
}

void ADroneCompanion::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!DroneBody)
	{
		return;
	}

	PreviousLinearVelocity = DroneBody->GetPhysicsLinearVelocity();

	if (ImpactDebugTimeRemaining > 0.0f)
	{
		ImpactDebugTimeRemaining = FMath::Max(0.0f, ImpactDebugTimeRemaining - DeltaSeconds);
		if (ImpactDebugTimeRemaining <= 0.0f)
		{
			bImpactDetected = false;
			bImpactWouldCrash = false;
		}
	}

	UpdateBuddyDrift(DeltaSeconds);
	UpdateCrashRecovery(DeltaSeconds);

	if (!bCrashed)
	{
		if (CompanionMode == EDroneCompanionMode::PilotControlled)
		{
			if (bUseSimplePilotControls)
			{
				UpdateSimpleFlight(DeltaSeconds);
			}
			else
			{
				UpdateComplexFlight(DeltaSeconds);
			}
		}
		else if (CompanionMode == EDroneCompanionMode::MapMode)
		{
			UpdateMapFlight(DeltaSeconds);
		}
		else
		{
			UpdateAutonomousFlight(DeltaSeconds);
		}
	}

	ClampVelocity();
	UpdateDebugOutput();
}

void ADroneCompanion::SetFollowTarget(AActor* NewFollowTarget)
{
	FollowTarget = NewFollowTarget;
}

void ADroneCompanion::SetCompanionMode(EDroneCompanionMode NewMode)
{
	const EDroneCompanionMode PreviousMode = CompanionMode;
	CompanionMode = NewMode;
	if (CompanionMode != EDroneCompanionMode::PilotControlled)
	{
		bCrashed = false;
		CrashRecoveryTimeRemaining = 0.0f;
	}

	if (CompanionMode != EDroneCompanionMode::PilotControlled)
	{
		ResetPilotInputs();
	}

	if (CompanionMode == EDroneCompanionMode::MapMode && PreviousMode != EDroneCompanionMode::MapMode)
	{
		MapTargetLocation = DroneBody ? DroneBody->GetComponentLocation() : GetActorLocation();

		if (FollowTarget)
		{
			const float BaseHeight = FollowTarget->GetActorLocation().Z;
			const float MinimumHeight = BaseHeight + MapMinHeightAboveTarget;
			const float MaximumHeight = BaseHeight + MapMaxHeightAboveTarget;
			const float RaisedHeight = FMath::Max(MapTargetLocation.Z + MapEntryRiseHeight, MinimumHeight);
			MapTargetLocation.Z = FMath::Clamp(RaisedHeight, MinimumHeight, MaximumHeight);
		}
		else
		{
			MapTargetLocation.Z += MapEntryRiseHeight;
		}
	}

	RefreshCameraMountRotation();

	if (DroneCamera)
	{
		DroneCamera->SetFieldOfView(
			CompanionMode == EDroneCompanionMode::ThirdPersonFollow
				? ThirdPersonCameraFieldOfView
				: (CompanionMode == EDroneCompanionMode::MapMode
					? MapCameraFieldOfView
					: PilotCameraFieldOfView));
	}
}

void ADroneCompanion::SetViewReferenceRotation(const FRotator& NewRotation)
{
	ViewReferenceRotation = NewRotation;
}

void ADroneCompanion::SetThirdPersonCameraTarget(const FVector& NewLocation, const FRotator& NewRotation)
{
	ThirdPersonCameraTargetLocation = NewLocation;
	ThirdPersonCameraTargetRotation = NewRotation;
	bHasThirdPersonCameraTarget = true;
}

void ADroneCompanion::SetHoldTransform(const FVector& NewLocation, const FRotator& NewRotation)
{
	HoldTargetLocation = NewLocation;
	HoldTargetRotation = NewRotation;
}

void ADroneCompanion::SetPilotInputs(float InThrottleInput, float InYawInput, float InRollInput, float InPitchInput)
{
	auto ApplyDeadzone = [this](float Value)
	{
		const float ClampedDeadzone = FMath::Clamp(PilotInputDeadzone, 0.0f, 0.95f);
		const float AbsValue = FMath::Abs(Value);
		if (AbsValue <= ClampedDeadzone)
		{
			return 0.0f;
		}

		const float ScaledMagnitude = (AbsValue - ClampedDeadzone) / FMath::Max(KINDA_SMALL_NUMBER, 1.0f - ClampedDeadzone);
		return FMath::Sign(Value) * FMath::Clamp(ScaledMagnitude, 0.0f, 1.0f);
	};

	PilotThrottleInput = FMath::Clamp(ApplyDeadzone(InThrottleInput), -1.0f, 1.0f);
	PilotYawInput = FMath::Clamp(ApplyDeadzone(InYawInput), -1.0f, 1.0f);
	PilotRollInput = FMath::Clamp(ApplyDeadzone(InRollInput), -1.0f, 1.0f);
	PilotPitchInput = FMath::Clamp(ApplyDeadzone(InPitchInput), -1.0f, 1.0f);
}

void ADroneCompanion::ResetPilotInputs()
{
	PilotThrottleInput = 0.0f;
	AppliedThrottleInput = 0.0f;
	PilotYawInput = 0.0f;
	PilotRollInput = 0.0f;
	PilotPitchInput = 0.0f;
	CurrentHoverBaseAcceleration = 0.0f;
	CurrentHoverCommandInput = 0.0f;
	CurrentHoverVerticalAcceleration = 0.0f;
	CurrentHoverLiftDot = 1.0f;
}

void ADroneCompanion::SetUseSimplePilotControls(bool bEnable)
{
	bUseSimplePilotControls = bEnable;
}

void ADroneCompanion::SetPilotStabilizerEnabled(bool bEnable)
{
	bPilotStabilizerEnabled = bEnable;
}

void ADroneCompanion::TeleportDroneToTransform(const FVector& NewLocation, const FRotator& NewRotation)
{
	if (!DroneBody)
	{
		return;
	}

	DroneBody->SetWorldLocationAndRotation(
		NewLocation,
		NewRotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	DroneBody->SetPhysicsLinearVelocity(FVector::ZeroVector);
	DroneBody->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	PreviousLinearVelocity = FVector::ZeroVector;
}

void ADroneCompanion::SetPilotHoverModeEnabled(bool bEnable)
{
	if (bPilotHoverModeEnabled == bEnable)
	{
		return;
	}

	bPilotHoverModeEnabled = bEnable;
	CurrentHoverBaseAcceleration = 0.0f;
	CurrentHoverCommandInput = 0.0f;
	CurrentHoverVerticalAcceleration = 0.0f;
	CurrentHoverLiftDot = 1.0f;
}

void ADroneCompanion::AdjustCameraTilt(float DeltaDegrees)
{
	CameraTiltDegrees = FMath::Clamp(CameraTiltDegrees + DeltaDegrees, DroneMinCameraTilt, DroneMaxCameraTilt);
	RefreshCameraMountRotation();
}

void ADroneCompanion::OnDroneBodyHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (!DroneBody || !Hit.bBlockingHit)
	{
		return;
	}

	const FVector RawImpactNormal = Hit.ImpactNormal.IsNearlyZero() ? Hit.Normal : Hit.ImpactNormal;
	const FVector ImpactNormal = RawImpactNormal.IsNearlyZero() ? FVector::UpVector : RawImpactNormal.GetSafeNormal();
	const FVector CurrentVelocity = DroneBody->GetPhysicsLinearVelocity();
	const FVector SampleVelocity = PreviousLinearVelocity.IsNearlyZero() ? CurrentVelocity : PreviousLinearVelocity;
	const float VelocityImpactSpeed = FMath::Abs(FVector::DotProduct(SampleVelocity, ImpactNormal));
	const float ImpulseImpactSpeed = NormalImpulse.Size() / FMath::Max(DroneMassKg, 0.1f);

	LastImpactSpeed = FMath::Max(VelocityImpactSpeed, ImpulseImpactSpeed);
	bImpactDetected = true;
	bImpactWouldCrash = LastImpactSpeed >= DroneCrashSpeed;
	ImpactDebugTimeRemaining = DroneImpactDebugHoldTime;

	if (CompanionMode == EDroneCompanionMode::PilotControlled && bImpactWouldCrash)
	{
		bCrashed = true;
		CrashRecoveryTimeRemaining = FMath::Clamp(
			LastImpactSpeed * DroneCrashRecoveryTimePerSpeed,
			DroneCrashMinRecoveryTime,
			DroneCrashMaxRecoveryTime);
		ResetPilotInputs();
	}
}

void ADroneCompanion::UpdateCrashRecovery(float DeltaSeconds)
{
	if (!bCrashed || !DroneBody)
	{
		return;
	}

	CurrentHoverCommandInput = 0.0f;
	CurrentHoverVerticalAcceleration = 0.0f;
	CurrentHoverBaseAcceleration = 0.0f;
	CurrentHoverLiftDot = 1.0f;

	const float CurrentCrashSpeed = DroneBody->GetPhysicsLinearVelocity().Size();
	if (CurrentCrashSpeed <= FMath::Max(0.0f, CrashSelfRightActivationSpeed))
	{
		const FVector CrashSelfRightVelocity = BuildUprightAssistAngularVelocity(
			CrashSelfRightAssist,
			CrashSelfRightMaxCorrectionRate);
		ApplyDesiredAngularVelocity(
			CrashSelfRightVelocity,
			DeltaSeconds,
			FMath::Max(PilotAngularResponse, AutopilotAngularResponse));
	}

	CrashRecoveryTimeRemaining = FMath::Max(0.0f, CrashRecoveryTimeRemaining - DeltaSeconds);

	if (CrashRecoveryTimeRemaining > 0.0f)
	{
		return;
	}

	const bool bLinearSettled = DroneBody->GetPhysicsLinearVelocity().Size() <= DroneCrashSettleLinearSpeed;
	const bool bAngularSettled = DroneBody->GetPhysicsAngularVelocityInDegrees().Size() <= DroneCrashSettleAngularSpeed;

	if (bLinearSettled && bAngularSettled)
	{
		bCrashed = false;
	}
}

void ADroneCompanion::UpdateComplexFlight(float DeltaSeconds)
{
	if (!DroneBody)
	{
		return;
	}

	AppliedThrottleInput = FMath::FInterpTo(
		AppliedThrottleInput,
		PilotThrottleInput,
		DeltaSeconds,
		PilotThrottleResponse);

	if (bPilotHoverModeEnabled)
	{
		const float GravityAcceleration = GetWorld() ? FMath::Abs(GetWorld()->GetGravityZ()) : 980.0f;
		const float LiftDot = FVector::DotProduct(DroneBody->GetUpVector().GetSafeNormal(), FVector::UpVector);
		const float EffectiveLiftDot = LiftDot > 0.0f
			? FMath::Max(LiftDot, FMath::Max(KINDA_SMALL_NUMBER, PilotHoverMinUpDot))
			: FMath::Max(KINDA_SMALL_NUMBER, PilotHoverMinUpDot);
		const float BaseAcceleration = GravityAcceleration / EffectiveLiftDot;
		const float CommandAcceleration = AppliedThrottleInput * PilotHoverCollectiveRange;
		const float TotalAcceleration = FMath::Max(0.0f, BaseAcceleration + CommandAcceleration);

		CurrentHoverBaseAcceleration = BaseAcceleration;
		CurrentHoverCommandInput = AppliedThrottleInput;
		CurrentHoverVerticalAcceleration = TotalAcceleration;
		CurrentHoverLiftDot = LiftDot;

		DroneBody->AddForce(
			DroneBody->GetUpVector() * TotalAcceleration,
			NAME_None,
			true);
	}
	else
	{
		CurrentHoverBaseAcceleration = 0.0f;
		CurrentHoverCommandInput = 0.0f;
		CurrentHoverVerticalAcceleration = 0.0f;
		CurrentHoverLiftDot = 1.0f;
		DroneBody->AddForce(
			DroneBody->GetUpVector() * (AppliedThrottleInput * PilotMaxThrustAcceleration),
			NAME_None,
			true);
	}

	FVector DesiredLocalAngularVelocity(
		PilotRollInput * PilotRollRate,
		-PilotPitchInput * PilotPitchRate,
		PilotYawInput * PilotYawRate);

	if (bPilotStabilizerEnabled)
	{
		DesiredLocalAngularVelocity += BuildUprightAssistAngularVelocity(
			PilotUprightAssist,
			PilotStabilizerMaxCorrectionRate);
	}

	ApplyDesiredAngularVelocity(DesiredLocalAngularVelocity, DeltaSeconds, PilotAngularResponse);
}

void ADroneCompanion::UpdateSimpleFlight(float DeltaSeconds)
{
	if (!DroneBody)
	{
		return;
	}

	CurrentHoverCommandInput = 0.0f;
	CurrentHoverVerticalAcceleration = 0.0f;
	CurrentHoverBaseAcceleration = 0.0f;
	CurrentHoverLiftDot = 1.0f;

	const FRotator ReferenceYawRotation(0.0f, ViewReferenceRotation.Yaw, 0.0f);
	const FVector ForwardDirection = FRotationMatrix(ReferenceYawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(ReferenceYawRotation).GetUnitAxis(EAxis::Y);
	const FVector DesiredHorizontalVelocity = (
		(ForwardDirection * PilotPitchInput) +
		(RightDirection * PilotRollInput)).GetClampedToMaxSize(1.0f) * SimpleMaxHorizontalSpeed;

	FVector DesiredVelocity = DesiredHorizontalVelocity;
	DesiredVelocity.Z = PilotThrottleInput * SimpleMaxVerticalSpeed;

	const FVector CurrentVelocity = DroneBody->GetPhysicsLinearVelocity();
	FVector VelocityCorrection = (DesiredVelocity - CurrentVelocity) * SimpleVelocityResponse;

	if (UWorld* World = GetWorld())
	{
		VelocityCorrection.Z += FMath::Abs(World->GetGravityZ());
	}

	DroneBody->AddForce(VelocityCorrection, NAME_None, true);

	float DesiredYaw = ViewReferenceRotation.Yaw;
	if (!DesiredHorizontalVelocity.IsNearlyZero())
	{
		DesiredYaw = DesiredHorizontalVelocity.Rotation().Yaw;
	}

	const FRotator DesiredRotation(0.0f, DesiredYaw, 0.0f);
	const FRotator DeltaRotation = (DesiredRotation - DroneBody->GetComponentRotation()).GetNormalized();
	FVector DesiredLocalAngularVelocity = FVector::ZeroVector;
	DesiredLocalAngularVelocity = FVector(
		-DeltaRotation.Roll * SimpleRotationResponse,
		-DeltaRotation.Pitch * SimpleRotationResponse,
		DeltaRotation.Yaw * SimpleYawFollowSpeed);

	ApplyDesiredAngularVelocity(
		DesiredLocalAngularVelocity,
		DeltaSeconds,
		FMath::Max(SimpleRotationResponse, SimpleYawFollowSpeed));
}

void ADroneCompanion::UpdateMapFlight(float DeltaSeconds)
{
	if (!DroneBody)
	{
		return;
	}

	CurrentHoverCommandInput = 0.0f;
	CurrentHoverVerticalAcceleration = 0.0f;
	CurrentHoverBaseAcceleration = 0.0f;
	CurrentHoverLiftDot = 1.0f;

	const FRotator ReferenceYawRotation(0.0f, ViewReferenceRotation.Yaw, 0.0f);
	const FVector ForwardDirection = FRotationMatrix(ReferenceYawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(ReferenceYawRotation).GetUnitAxis(EAxis::Y);
	const FVector PanInput = (
		(ForwardDirection * PilotPitchInput) +
		(RightDirection * PilotRollInput)).GetClampedToMaxSize(1.0f);

	MapTargetLocation += PanInput * (MapPanSpeed * DeltaSeconds);
	MapTargetLocation.Z += PilotThrottleInput * (MapVerticalSpeed * DeltaSeconds);

	if (FollowTarget)
	{
		const float BaseHeight = FollowTarget->GetActorLocation().Z;
		const float MinimumHeight = BaseHeight + MapMinHeightAboveTarget;
		const float MaximumHeight = BaseHeight + MapMaxHeightAboveTarget;
		MapTargetLocation.Z = FMath::Clamp(MapTargetLocation.Z, MinimumHeight, MaximumHeight);
	}

	const FVector CurrentVelocity = DroneBody->GetPhysicsLinearVelocity();
	FVector PositionCorrection = ((MapTargetLocation - DroneBody->GetComponentLocation()) * MapPositionGain) -
		(CurrentVelocity * MapVelocityDamping);

	if (UWorld* World = GetWorld())
	{
		PositionCorrection.Z += FMath::Abs(World->GetGravityZ());
	}

	DroneBody->AddForce(PositionCorrection, NAME_None, true);

	const FRotator DesiredRotation(0.0f, ViewReferenceRotation.Yaw, 0.0f);
	const FRotator DeltaRotation = (DesiredRotation - DroneBody->GetComponentRotation()).GetNormalized();
	FVector DesiredLocalAngularVelocity = FVector::ZeroVector;
	DesiredLocalAngularVelocity = FVector(
		-DeltaRotation.Roll * SimpleRotationResponse,
		-DeltaRotation.Pitch * SimpleRotationResponse,
		DeltaRotation.Yaw * SimpleYawFollowSpeed);

	ApplyDesiredAngularVelocity(
		DesiredLocalAngularVelocity,
		DeltaSeconds,
		FMath::Max(SimpleRotationResponse, SimpleYawFollowSpeed));
}

void ADroneCompanion::UpdateAutonomousFlight(float DeltaSeconds)
{
	if (!DroneBody)
	{
		return;
	}

	CurrentHoverCommandInput = 0.0f;
	CurrentHoverVerticalAcceleration = 0.0f;
	CurrentHoverBaseAcceleration = 0.0f;
	CurrentHoverLiftDot = 1.0f;

	const bool bThirdPersonMode = CompanionMode == EDroneCompanionMode::ThirdPersonFollow;
	const bool bHoldPositionMode = CompanionMode == EDroneCompanionMode::HoldPosition;
	const bool bUseExactThirdPersonCameraTarget = bThirdPersonMode && bHasThirdPersonCameraTarget;
	const bool bUseExactTransformTarget = bUseExactThirdPersonCameraTarget || bHoldPositionMode;
	const FVector DesiredLocation = bUseExactThirdPersonCameraTarget
		? ThirdPersonCameraTargetLocation
		: (bHoldPositionMode ? HoldTargetLocation : GetDesiredLocation());
	const FVector ToTarget = DesiredLocation - DroneBody->GetComponentLocation();
	const FVector Velocity = DroneBody->GetPhysicsLinearVelocity();
	const float PositionGain = bUseExactTransformTarget
		? ThirdPersonCameraPositionGain
		: (bThirdPersonMode ? ThirdPersonPositionGain : BuddyPositionGain);
	const float VelocityDamping = bUseExactTransformTarget
		? ThirdPersonCameraVelocityDamping
		: (bThirdPersonMode ? ThirdPersonVelocityDamping : BuddyVelocityDamping);

	DroneBody->AddForce(
		(ToTarget * PositionGain) - (Velocity * VelocityDamping),
		NAME_None,
		true);

	FRotator DesiredRotation = FRotator::ZeroRotator;
	DesiredRotation = DroneBody->GetComponentRotation();
	if (bUseExactThirdPersonCameraTarget)
	{
		DesiredRotation = ThirdPersonCameraTargetRotation;
	}
	else if (bHoldPositionMode)
	{
		DesiredRotation = HoldTargetRotation;
	}
	else
	{
		const FVector DesiredForward = GetDesiredForwardDirection();
		if (!DesiredForward.IsNearlyZero())
		{
			DesiredRotation = DesiredForward.Rotation();
		}
	}

	const FRotator DeltaRotation = (DesiredRotation - DroneBody->GetComponentRotation()).GetNormalized();
	const float RotationResponse = bUseExactTransformTarget
		? ThirdPersonCameraRotationResponse
		: AutopilotAngularResponse;
	const float YawResponse = bUseExactTransformTarget
		? ThirdPersonCameraRotationResponse
		: (bThirdPersonMode ? ThirdPersonYawFollowSpeed : AutopilotAngularResponse);
	FVector DesiredLocalAngularVelocity = FVector::ZeroVector;
	DesiredLocalAngularVelocity = FVector(
		-DeltaRotation.Roll * RotationResponse,
		-DeltaRotation.Pitch * RotationResponse,
		DeltaRotation.Yaw * YawResponse);

	ApplyDesiredAngularVelocity(DesiredLocalAngularVelocity, DeltaSeconds, RotationResponse);
}

void ADroneCompanion::UpdateBuddyDrift(float DeltaSeconds)
{
	BuddyDriftTimeRemaining -= DeltaSeconds;
	if (BuddyDriftTimeRemaining > 0.0f)
	{
		return;
	}

	BuddyDriftTimeRemaining = FMath::Max(0.1f, BuddyDriftUpdateInterval);
	BuddyLocalOffset = FVector(
		BuddyForwardOffset + FMath::FRandRange(-BuddyForwardJitter, BuddyForwardJitter),
		-FMath::FRandRange(BuddyMinLateralDistance, BuddyMaxLateralDistance),
		BuddyHeightOffset + FMath::FRandRange(-BuddyVerticalJitter, BuddyVerticalJitter));
}

void ADroneCompanion::ClampVelocity() const
{
	if (!DroneBody)
	{
		return;
	}

	const FVector CurrentLinearVelocity = DroneBody->GetPhysicsLinearVelocity();
	if (CurrentLinearVelocity.SizeSquared() > FMath::Square(DroneMaxLinearSpeed))
	{
		DroneBody->SetPhysicsLinearVelocity(CurrentLinearVelocity.GetClampedToMaxSize(DroneMaxLinearSpeed));
	}

	const FVector CurrentAngularVelocity = DroneBody->GetPhysicsAngularVelocityInDegrees();
	if (CurrentAngularVelocity.SizeSquared() > FMath::Square(DroneMaxAngularSpeed))
	{
		DroneBody->SetPhysicsAngularVelocityInDegrees(CurrentAngularVelocity.GetClampedToMaxSize(DroneMaxAngularSpeed));
	}
}

void ADroneCompanion::ApplyDesiredAngularVelocity(const FVector& DesiredLocalAngularVelocity, float DeltaSeconds, float ResponseSpeed)
{
	if (!DroneBody)
	{
		return;
	}

	const FVector CurrentWorldAngularVelocity = DroneBody->GetPhysicsAngularVelocityInDegrees();
	const FVector CurrentLocalAngularVelocity = DroneBody->GetComponentTransform().InverseTransformVectorNoScale(CurrentWorldAngularVelocity);
	FVector SmoothedLocalAngularVelocity = FVector::ZeroVector;
	SmoothedLocalAngularVelocity.X = FMath::FInterpTo(CurrentLocalAngularVelocity.X, DesiredLocalAngularVelocity.X, DeltaSeconds, ResponseSpeed);
	SmoothedLocalAngularVelocity.Y = FMath::FInterpTo(CurrentLocalAngularVelocity.Y, DesiredLocalAngularVelocity.Y, DeltaSeconds, ResponseSpeed);
	SmoothedLocalAngularVelocity.Z = FMath::FInterpTo(CurrentLocalAngularVelocity.Z, DesiredLocalAngularVelocity.Z, DeltaSeconds, ResponseSpeed);

	const FVector NewWorldAngularVelocity = DroneBody->GetComponentTransform().TransformVectorNoScale(SmoothedLocalAngularVelocity);
	DroneBody->SetPhysicsAngularVelocityInDegrees(NewWorldAngularVelocity);
}

FVector ADroneCompanion::BuildUprightAssistAngularVelocity(float AssistStrength, float MaxCorrectionRate) const
{
	if (!DroneBody || AssistStrength <= 0.0f || MaxCorrectionRate <= 0.0f)
	{
		return FVector::ZeroVector;
	}

	const FVector CurrentUp = DroneBody->GetUpVector().GetSafeNormal();
	const float UprightDot = FMath::Clamp(FVector::DotProduct(CurrentUp, FVector::UpVector), -1.0f, 1.0f);
	FVector UprightAxisWorld = FVector::CrossProduct(CurrentUp, FVector::UpVector);
	const float UprightAngleRadians = FMath::Acos(UprightDot);

	if (UprightAxisWorld.IsNearlyZero())
	{
		if (UprightDot > 0.0f || UprightAngleRadians <= KINDA_SMALL_NUMBER)
		{
			return FVector::ZeroVector;
		}

		UprightAxisWorld = DroneBody->GetRightVector();
	}

	if (UprightAngleRadians <= KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	const FVector UprightAxisLocal = DroneBody->GetComponentTransform().InverseTransformVectorNoScale(UprightAxisWorld.GetSafeNormal());
	const float CorrectionRate = FMath::Min(
		FMath::RadiansToDegrees(UprightAngleRadians) * AssistStrength,
		MaxCorrectionRate);

	return FVector(
		UprightAxisLocal.X * CorrectionRate,
		UprightAxisLocal.Y * CorrectionRate,
		0.0f);
}

FVector ADroneCompanion::GetDesiredLocation() const
{
	if (!FollowTarget)
	{
		return GetActorLocation();
	}

	const FVector TargetLocation = FollowTarget->GetActorLocation();
	const FRotator ReferenceYawRotation(0.0f, ViewReferenceRotation.Yaw, 0.0f);
	const FRotationMatrix ReferenceMatrix(ReferenceYawRotation);

	if (CompanionMode == EDroneCompanionMode::ThirdPersonFollow)
	{
		return TargetLocation + ReferenceMatrix.TransformVector(ThirdPersonOffset);
	}

	return TargetLocation + ReferenceMatrix.TransformVector(BuddyLocalOffset);
}

FVector ADroneCompanion::GetDesiredForwardDirection() const
{
	if (!FollowTarget)
	{
		return DroneBody ? DroneBody->GetForwardVector() : FVector::ForwardVector;
	}

	if (CompanionMode == EDroneCompanionMode::ThirdPersonFollow)
	{
		const FVector LookTarget = FollowTarget->GetActorLocation() + FVector(0.0f, 0.0f, ThirdPersonLookAtHeight);
		return (LookTarget - GetActorLocation()).GetSafeNormal();
	}

	const FRotator ReferenceYawRotation(0.0f, ViewReferenceRotation.Yaw, 0.0f);
	const FVector ForwardDirection = ReferenceYawRotation.Vector();
	const FVector ToTarget = (FollowTarget->GetActorLocation() - GetActorLocation()).GetSafeNormal();
	return (ForwardDirection * (1.0f - BuddyFacingBlend) + ToTarget * BuddyFacingBlend).GetSafeNormal();
}

void ADroneCompanion::UpdateDebugOutput() const
{
	if (!bShowDebugOverlay || !GEngine || !DroneBody)
	{
		return;
	}

	const TCHAR* ModeLabel = TEXT("ThirdPerson");
	if (CompanionMode == EDroneCompanionMode::PilotControlled)
	{
		ModeLabel = bUseSimplePilotControls ? TEXT("PilotSimple") : TEXT("PilotComplex");
	}
	else if (CompanionMode == EDroneCompanionMode::HoldPosition)
	{
		ModeLabel = TEXT("Hold");
	}
	else if (CompanionMode == EDroneCompanionMode::BuddyFollow)
	{
		ModeLabel = TEXT("Buddy");
	}
	else if (CompanionMode == EDroneCompanionMode::MapMode)
	{
		ModeLabel = TEXT("Map");
	}

	const FVector Velocity = DroneBody->GetPhysicsLinearVelocity();
	const FVector AngularVelocity = DroneBody->GetPhysicsAngularVelocityInDegrees();
	const FString AssistDebugText = FString::Printf(
		TEXT("Complex Assist: %s"),
		bPilotStabilizerEnabled ? TEXT("ANGLE / HORIZON") : TEXT("ACRO / RATE"));
	const FColor AssistDebugColor = bPilotStabilizerEnabled ? FColor::Green : FColor::Red;
	const FString HoverDebugText = FString::Printf(
		TEXT("Hover Assist: %s"),
		bPilotHoverModeEnabled ? TEXT("ON") : TEXT("OFF"));
	const FColor HoverDebugColor = bPilotHoverModeEnabled ? FColor::Green : FColor::Red;
	const FString DebugText = FString::Printf(
		TEXT("Drone Companion\n")
		TEXT("Mode: %s  Stab: %s  Hover: %s  Crashed: %s  Recovery: %.2fs\n")
		TEXT("Speed: %.0f / %.0f uu/s  Angular: %.0f deg/s\n")
		TEXT("Impact: %s  Would Crash: %s  Last Impact: %.0f\n")
		TEXT("Inputs: V %.2f  Y %.2f  X %.2f  F %.2f  Cam Tilt: %.1f\n")
		TEXT("Hover: Base %.1f  Total %.1f  Cmd %.2f  Lift %.2f  VZ %.1f"),
		ModeLabel,
		bPilotStabilizerEnabled ? TEXT("ON") : TEXT("OFF"),
		bPilotHoverModeEnabled ? TEXT("ON") : TEXT("OFF"),
		bCrashed ? TEXT("YES") : TEXT("NO"),
		CrashRecoveryTimeRemaining,
		Velocity.Size(),
		DroneMaxLinearSpeed,
		AngularVelocity.Size(),
		bImpactDetected ? TEXT("YES") : TEXT("NO"),
		bImpactWouldCrash ? TEXT("YES") : TEXT("NO"),
		LastImpactSpeed,
		PilotThrottleInput,
		PilotYawInput,
		PilotRollInput,
		PilotPitchInput,
		CameraTiltDegrees,
		CurrentHoverBaseAcceleration,
		CurrentHoverVerticalAcceleration,
		CurrentHoverCommandInput,
		CurrentHoverLiftDot,
		Velocity.Z);

	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()) + 12000ULL, 0.0f, FColor::Cyan, DebugText);
	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()) + 12001ULL, 0.0f, AssistDebugColor, AssistDebugText);
	GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()) + 12002ULL, 0.0f, HoverDebugColor, HoverDebugText);
}

void ADroneCompanion::ApplyRuntimePhysicalMaterial()
{
	if (!DroneBody)
	{
		return;
	}

	if (!RuntimePhysicalMaterial)
	{
		RuntimePhysicalMaterial = NewObject<UPhysicalMaterial>(this, TEXT("DroneCompanionPhysicalMaterial"));
	}

	if (!RuntimePhysicalMaterial)
	{
		return;
	}

	RuntimePhysicalMaterial->Friction = FMath::Max(0.0f, DroneBodyFriction);
	RuntimePhysicalMaterial->Restitution = FMath::Clamp(DroneBodyRestitution, 0.0f, 1.0f);
	RuntimePhysicalMaterial->bOverrideFrictionCombineMode = true;
	RuntimePhysicalMaterial->FrictionCombineMode = EFrictionCombineMode::Max;
	RuntimePhysicalMaterial->bOverrideRestitutionCombineMode = true;
	RuntimePhysicalMaterial->RestitutionCombineMode = EFrictionCombineMode::Min;
	DroneBody->SetPhysMaterialOverride(RuntimePhysicalMaterial);
}

void ADroneCompanion::RefreshCameraMountRotation()
{
	if (!CameraMount)
	{
		return;
	}

	if (CompanionMode == EDroneCompanionMode::ThirdPersonFollow || CompanionMode == EDroneCompanionMode::HoldPosition)
	{
		CameraMount->SetRelativeRotation(FRotator::ZeroRotator);
		return;
	}

	if (CompanionMode == EDroneCompanionMode::MapMode)
	{
		CameraMount->SetRelativeRotation(FRotator(MapCameraPitchDegrees, 0.0f, 0.0f));
		return;
	}

	CameraMount->SetRelativeRotation(FRotator(CameraTiltDegrees, 0.0f, 0.0f));
}
