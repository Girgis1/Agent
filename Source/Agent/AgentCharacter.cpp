// Copyright Epic Games, Inc. All Rights Reserved.

#include "AgentCharacter.h"
#include "AgentBeamToolComponent.h"
#include "Backpack/Actors/BlackHoleBackpackActor.h"
#include "Backpack/Components/BackAttachmentComponent.h"
#include "Backpack/Components/PlayerMagnetComponent.h"
#include "DroneCompanion.h"
#include "DroneSwarmComponent.h"
#include "Factory/ConveyorBeltStraight.h"
#include "Factory/ConveyorCharacterMovementComponent.h"
#include "Factory/ConveyorPlacementPreview.h"
#include "Factory/ConveyorSurfaceVelocityComponent.h"
#include "Factory/FactoryPlacementHelpers.h"
#include "Factory/MaterialNodeActor.h"
#include "Factory/MinerActor.h"
#include "Factory/MiningBotActor.h"
#include "Factory/MiningSwarmMachine.h"
#include "Machine/MachineActor.h"
#include "Factory/StorageBin.h"
#include "Vehicle/Components/VehicleInteractionComponent.h"
#include "Camera/CameraComponent.h"
#include "CollisionShape.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "PhysicsEngine/PhysicsHandleComponent.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "UObject/ConstructorHelpers.h"
#include "Agent.h"

namespace
{
	USceneComponent* ResolveTaggedSceneComponent(AActor* SourceActor, FName RequiredTag)
	{
		if (!SourceActor || RequiredTag.IsNone())
		{
			return nullptr;
		}

		TArray<USceneComponent*> SceneComponents;
		SourceActor->GetComponents<USceneComponent>(SceneComponents);
		for (USceneComponent* SceneComponent : SceneComponents)
		{
			if (SceneComponent && SceneComponent->ComponentHasTag(RequiredTag))
			{
				return SceneComponent;
			}
		}

		if (SourceActor->ActorHasTag(RequiredTag))
		{
			return Cast<USceneComponent>(SourceActor->GetRootComponent());
		}

		TArray<AActor*> AttachedActors;
		SourceActor->GetAttachedActors(AttachedActors, true, true);
		for (AActor* AttachedActor : AttachedActors)
		{
			if (!AttachedActor)
			{
				continue;
			}

			AttachedActor->GetComponents<USceneComponent>(SceneComponents);
			for (USceneComponent* SceneComponent : SceneComponents)
			{
				if (SceneComponent && SceneComponent->ComponentHasTag(RequiredTag))
				{
					return SceneComponent;
				}
			}

			if (AttachedActor->ActorHasTag(RequiredTag))
			{
				if (USceneComponent* RootSceneComponent = Cast<USceneComponent>(AttachedActor->GetRootComponent()))
				{
					return RootSceneComponent;
				}
			}
		}

		return nullptr;
	}

	const TCHAR* ToRagdollStateString(EAgentRagdollState State)
	{
		switch (State)
		{
		case EAgentRagdollState::Normal:
			return TEXT("Normal");
		case EAgentRagdollState::Ragdoll:
			return TEXT("Ragdoll");
		case EAgentRagdollState::Recovering:
			return TEXT("Recovering");
		default:
			return TEXT("Unknown");
		}
	}

	const TCHAR* ToRagdollReasonString(EAgentRagdollReason Reason)
	{
		switch (Reason)
		{
		case EAgentRagdollReason::ManualInput:
			return TEXT("ManualInput");
		case EAgentRagdollReason::ClumsinessTrip:
			return TEXT("ClumsinessTrip");
		case EAgentRagdollReason::ClumsinessSlip:
			return TEXT("ClumsinessSlip");
		case EAgentRagdollReason::Impact:
			return TEXT("Impact");
		case EAgentRagdollReason::Scripted:
			return TEXT("Scripted");
		default:
			return TEXT("Unknown");
		}
	}
}

AAgentCharacter::AAgentCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UConveyorCharacterMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	PrimaryActorTick.bCanEverTick = true;

	CreateDefaultSubobject<UConveyorSurfaceVelocityComponent>(TEXT("ConveyorSurfaceVelocity"));

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;
	GetCharacterMovement()->InitialPushForceFactor = 0.0f;
	GetCharacterMovement()->PushForceFactor = 200000.0f;
	GetCharacterMovement()->TouchForceFactor = 0.0f;
	GetCharacterMovement()->RepulsionForce = 0.0f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
	FollowCamera->SetActive(true);

	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->bEditableWhenInherited = true;
	FirstPersonCamera->SetupAttachment(GetMesh());
	FirstPersonCamera->SetRelativeLocation(FirstPersonCameraOffset);
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetActive(false);

	ThirdPersonTransitionCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ThirdPersonTransitionCamera"));
	ThirdPersonTransitionCamera->SetupAttachment(GetCapsuleComponent());
	ThirdPersonTransitionCamera->bUsePawnControlRotation = false;
	ThirdPersonTransitionCamera->SetActive(false);

	PickupPhysicsHandle = CreateDefaultSubobject<UPhysicsHandleComponent>(TEXT("PickupPhysicsHandle"));
	VehicleInteractionComponent = CreateDefaultSubobject<UVehicleInteractionComponent>(TEXT("VehicleInteractionComponent"));
	BackAttachmentComponent = CreateDefaultSubobject<UBackAttachmentComponent>(TEXT("BackAttachmentComponent"));
	PlayerMagnetComponent = CreateDefaultSubobject<UPlayerMagnetComponent>(TEXT("PlayerMagnetComponent"));
	PlayerBeamOrigin = CreateDefaultSubobject<USceneComponent>(TEXT("PlayerBeamOrigin"));
	PlayerBeamOrigin->bEditableWhenInherited = true;
	PlayerBeamOrigin->SetupAttachment(GetMesh());
	PlayerBeamOrigin->SetRelativeLocation(FVector(35.0f, 16.0f, 58.0f));
	PlayerBeamToolComponent = CreateDefaultSubobject<UAgentBeamToolComponent>(TEXT("PlayerBeamToolComponent"));
	PlayerBeamToolComponent->BeamSourceName = FName(TEXT("PlayerBeam"));
	PlayerBeamToolComponent->DamagePerSecond = 20.0f;
	PlayerBeamToolComponent->HealingPerSecond = 16.0f;
	PlayerBeamToolComponent->BaseTraceRadius = 6.0f;
	PlayerBeamToolComponent->BeamRange = 3000.0f;
	DroneSwarmComponent = CreateDefaultSubobject<UDroneSwarmComponent>(TEXT("DroneSwarmComponent"));

	ThirdPersonDroneProxyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ThirdPersonDroneProxyMesh"));
	ThirdPersonDroneProxyMesh->SetupAttachment(FollowCamera);
	ThirdPersonDroneProxyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ThirdPersonDroneProxyMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	ThirdPersonDroneProxyMesh->SetGenerateOverlapEvents(false);
	ThirdPersonDroneProxyMesh->CastShadow = true;
	ThirdPersonDroneProxyMesh->bCastHiddenShadow = true;
	ThirdPersonDroneProxyMesh->SetHiddenInGame(true);
	ThirdPersonDroneProxyMesh->SetVisibility(false, true);
	ThirdPersonDroneProxyMesh->SetOwnerNoSee(true);
	ThirdPersonDroneProxyMesh->SetCanEverAffectNavigation(false);
	ThirdPersonDroneProxyMesh->SetRelativeScale3D(FVector(ThirdPersonDroneProxyScale));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DroneProxyMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (DroneProxyMesh.Succeeded())
	{
		ThirdPersonDroneProxyMesh->SetStaticMesh(DroneProxyMesh.Object);
	}

	static ConstructorHelpers::FClassFinder<ADroneCompanion> DroneCompanionBlueprintClass(
		TEXT("/Game/ThirdPerson/Blueprints/BP_DroneCompanion"));
	if (DroneCompanionBlueprintClass.Succeeded())
	{
		DroneCompanionClass = DroneCompanionBlueprintClass.Class;
	}

	static ConstructorHelpers::FClassFinder<AMinerActor> MinerBlueprintClass(TEXT("/Game/Factory/BP_Miner"));
	if (MinerBlueprintClass.Succeeded())
	{
		MinerClass = MinerBlueprintClass.Class;
	}

	static ConstructorHelpers::FClassFinder<AMiningSwarmMachine> MiningSwarmBlueprintClass(
		TEXT("/Game/Factory/Mining/BP_MiningSwarmMachine"));
	if (MiningSwarmBlueprintClass.Succeeded())
	{
		MiningSwarmClass = MiningSwarmBlueprintClass.Class;
	}

}

void AAgentCharacter::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshFirstPersonCameraAttachment();
	RefreshPlayerBeamOriginAttachment();
}

void AAgentCharacter::RefreshFirstPersonCameraReference()
{
	ResolvedFirstPersonCamera = nullptr;

	if (FirstPersonCameraComponentName != NAME_None)
	{
		TArray<UCameraComponent*> CameraComponents;
		GetComponents<UCameraComponent>(CameraComponents);
		for (UCameraComponent* CandidateCamera : CameraComponents)
		{
			if (!CandidateCamera)
			{
				continue;
			}

			if (CandidateCamera->GetFName() == FirstPersonCameraComponentName)
			{
				ResolvedFirstPersonCamera = CandidateCamera;
				break;
			}
		}
	}

	if (!ResolvedFirstPersonCamera)
	{
		ResolvedFirstPersonCamera = FirstPersonCamera;
	}

	if (ResolvedFirstPersonCamera)
	{
		const bool bShouldUsePawnControlRotation = IsRagdolling()
			? bUsePawnControlRotationWhileRagdoll
			: bUsePawnControlRotationInFirstPerson;
		ResolvedFirstPersonCamera->bUsePawnControlRotation = bShouldUsePawnControlRotation;
	}
}

UCameraComponent* AAgentCharacter::ResolveFirstPersonCamera()
{
	if (!ResolvedFirstPersonCamera)
	{
		RefreshFirstPersonCameraReference();
	}

	return ResolvedFirstPersonCamera ? ResolvedFirstPersonCamera.Get() : FirstPersonCamera;
}

UCameraComponent* AAgentCharacter::ResolveFirstPersonCamera() const
{
	return const_cast<AAgentCharacter*>(this)->ResolveFirstPersonCamera();
}

void AAgentCharacter::RefreshFirstPersonCameraAttachment()
{
	RefreshFirstPersonCameraReference();

	UCameraComponent* const ActiveFirstPersonCamera = ResolveFirstPersonCamera();
	if (!ActiveFirstPersonCamera || !GetMesh())
	{
		return;
	}

	RefreshFirstPersonCameraControlRotation();

	// If BP provides FirstPersonCamera1, keep its authored attachment hierarchy untouched.
	if (ActiveFirstPersonCamera != FirstPersonCamera)
	{
		return;
	}

	// Native inherited components can't be reparented in BP hierarchy; expose socket-based attachment instead.
	ActiveFirstPersonCamera->AttachToComponent(
		GetMesh(),
		FAttachmentTransformRules::KeepRelativeTransform,
		FirstPersonCameraSocketName);
}

void AAgentCharacter::RefreshPlayerBeamOriginAttachment()
{
	if (!PlayerBeamOrigin || !GetMesh())
	{
		return;
	}

	// Native inherited components can't be reparented in BP hierarchy; expose socket-based attachment instead.
	PlayerBeamOrigin->AttachToComponent(
		GetMesh(),
		FAttachmentTransformRules::KeepRelativeTransform,
		PlayerBeamOriginSocketName);
}

USceneComponent* AAgentCharacter::ResolveConfiguredPlayerBeamOriginComponent() const
{
	if (USceneComponent* TaggedBeamOrigin = ResolveTaggedSceneComponent(const_cast<AAgentCharacter*>(this), PlayerBeamOriginTag))
	{
		return TaggedBeamOrigin;
	}

	return PlayerBeamOrigin;
}

void AAgentCharacter::RefreshFirstPersonCameraControlRotation()
{
	UCameraComponent* const ActiveFirstPersonCamera = ResolveFirstPersonCamera();
	if (!ActiveFirstPersonCamera)
	{
		return;
	}

	const bool bShouldUsePawnControlRotation = IsRagdolling()
		? bUsePawnControlRotationWhileRagdoll
		: bUsePawnControlRotationInFirstPerson;
	ActiveFirstPersonCamera->bUsePawnControlRotation = bShouldUsePawnControlRotation;
}

void AAgentCharacter::BeginPlay()
{
	Super::BeginPlay();
	RefreshFirstPersonCameraAttachment();
	RefreshPlayerBeamOriginAttachment();

	ClumsinessMinValue = FMath::Max(1.0f, ClumsinessMinValue);
	ClumsinessMaxValue = FMath::Max(ClumsinessMinValue, ClumsinessMaxValue);
	ClumsinessValue = FMath::Clamp(ClumsinessValue, ClumsinessMinValue, ClumsinessMaxValue);
	if (bStartClumsinessAtMinimum)
	{
		ClumsinessValue = ClumsinessMinValue;
	}
	InstabilityMaxValue = FMath::Max(1.0f, InstabilityMaxValue);
	InstabilityRagdollThreshold = FMath::Clamp(InstabilityRagdollThreshold, 0.0f, InstabilityMaxValue);
	InstabilityValue = FMath::Clamp(InstabilityValue, 0.0f, InstabilityMaxValue);
	bTrackingFallHeight = false;
	FallTrackingStartZ = GetActorLocation().Z;
	FallTrackingMinZ = FallTrackingStartZ;
	LastFallHeightCm = 0.0f;

	if (UConveyorCharacterMovementComponent* ConveyorMovement = Cast<UConveyorCharacterMovementComponent>(GetCharacterMovement()))
	{
		ConveyorMovement->OnCharacterStepUp.AddUObject(this, &AAgentCharacter::HandleStepUpEvent);
	}

	bDefaultUseControllerRotationYaw = bUseControllerRotationYaw;
	bDefaultOrientRotationToMovement = GetCharacterMovement() ? GetCharacterMovement()->bOrientRotationToMovement : true;
	DronePilotControlMode = StartDronePilotControlMode;
	if (DronePilotControlMode == EAgentDronePilotControlMode::Complex)
	{
		if (bStartWithSimpleDroneControls)
		{
			DronePilotControlMode = EAgentDronePilotControlMode::Simple;
		}
		else if (bStartWithDroneStabilizerEnabled && bStartWithDroneHoverModeEnabled)
		{
			DronePilotControlMode = EAgentDronePilotControlMode::HorizonHover;
		}
		else if (bStartWithDroneStabilizerEnabled)
		{
			DronePilotControlMode = EAgentDronePilotControlMode::Horizon;
		}
	}
	SetDronePilotControlMode(DronePilotControlMode);
	CurrentViewMode = bStartInThirdPersonDroneView ? EAgentViewMode::ThirdPerson : EAgentViewMode::FirstPerson;
	PersistedPrimaryDroneBatteryPercent = FMath::Clamp(PersistedPrimaryDroneBatteryPercent, 0.0f, 100.0f);
	if (!bPrimaryDroneAvailable)
	{
		CurrentViewMode = EAgentViewMode::FirstPerson;
	}
	bPlayerDroneTorchEnabled = bStartWithDroneTorchEnabled;

	SpawnInitialDroneFleet();

	if (CurrentViewMode == EAgentViewMode::ThirdPerson)
	{
		SetThirdPersonProxyVisible(true);
		AttachThirdPersonProxyToComponent(FollowCamera);
	}
	else
	{
		SetThirdPersonProxyVisible(false);
	}

	if (ThirdPersonDroneProxyMesh)
	{
		ThirdPersonDroneProxyMesh->SetRelativeScale3D(FVector(FMath::Max(0.01f, ThirdPersonDroneProxyScale)));
	}

	FVector InitialThirdPersonLocation = FVector::ZeroVector;
	FRotator InitialThirdPersonRotation = FRotator::ZeroRotator;
	if (GetThirdPersonDroneTarget(InitialThirdPersonLocation, InitialThirdPersonRotation))
	{
		if (DroneCompanion && CurrentViewMode == EAgentViewMode::FirstPerson)
		{
			DroneCompanion->SetCompanionMode(EDroneCompanionMode::BuddyFollow);
		}
	}

	if (USkeletalMeshComponent* MeshComponent = GetMesh())
	{
		CachedMeshRelativeTransform = MeshComponent->GetRelativeTransform();
		CachedMeshCollisionProfileName = MeshComponent->GetCollisionProfileName();
		CachedMeshCollisionEnabled = MeshComponent->GetCollisionEnabled();
	}

	if (UCapsuleComponent* CapsuleComp = GetCapsuleComponent())
	{
		CachedCapsuleCollisionEnabled = CapsuleComp->GetCollisionEnabled();
	}

	if (UCharacterMovementComponent* CharacterMovementComponent = GetCharacterMovement())
	{
		CachedMovementMode = CharacterMovementComponent->MovementMode;
		CachedCustomMovementMode = CharacterMovementComponent->CustomMovementMode;
	}

	UpdateMovementSpeedState(0.0f);
	ApplyBeamModeToEmitters();
}

void AAgentCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	CleanupInvalidDroneCompanions();
	DroneWorldDiscoveryTimeRemaining -= FMath::Max(0.0f, DeltaSeconds);
	if (DroneWorldDiscoveryTimeRemaining <= 0.0f)
	{
		const int32 DroneCountBeforeDiscovery = DroneCompanions.Num();
		DiscoverWorldDrones();
		if (DroneCompanions.Num() != DroneCountBeforeDiscovery)
		{
			ApplyDroneFleetContext(false, false);
		}
		DroneWorldDiscoveryTimeRemaining = FMath::Max(0.1f, DroneWorldDiscoveryInterval);
	}
	EnsureActiveDroneSelection();
	UpdateMovementSpeedState(DeltaSeconds);
	UpdateFallHeightTracking();
	UpdateClumsinessSystems(DeltaSeconds);
	UpdateRagdollAutoRecovery(DeltaSeconds);

	UpdateViewModeButtonHold(DeltaSeconds);
	UpdateKeyboardMapButtonHold(DeltaSeconds);
	UpdateControllerMapButtonHold(DeltaSeconds);
	UpdateBackpackDeployButtonHold(DeltaSeconds);
	UpdateHookJobButtonHold(DeltaSeconds);
	UpdateRollModeJumpHold(DeltaSeconds);
	UpdateFactoryPlacementRotationHold(DeltaSeconds);
	UpdateDronePilotCameraLimits();
	RefreshPrimaryDroneAvailabilityFromCompanion();
	UpdateBeamSystems(DeltaSeconds);
	UpdateBeamAimZoom(DeltaSeconds);
	if (!bPrimaryDroneAvailable
		&& bForceFirstPersonWhenDroneUnavailable
		&& (CurrentViewMode != EAgentViewMode::FirstPerson || bMapModeActive || bMiniMapModeActive))
	{
		ApplyViewMode(EAgentViewMode::FirstPerson, true);
	}

	if (bPendingThirdPersonSwapFromDroneCamera)
	{
		if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
		{
			PlayerController->SetViewTargetWithBlend(this, 0.0f);
		}
		bPendingThirdPersonSwapFromDroneCamera = false;
	}

	if (bConveyorPlacementModeActive)
	{
		if (!CanUseConveyorPlacementMode())
		{
			ExitConveyorPlacementMode();
		}
		else
		{
			UpdateConveyorPlacementPreview();
		}
	}

	if (DroneCompanion && DroneCompanion->IsLiftAssistActive() && !CanMaintainDroneLiftAssist())
	{
		DroneCompanion->StopLiftAssist();
	}

	if (CanUsePickupInteraction() || CanMaintainHeldPickup())
	{
		UpdatePickupInteraction();
	}
	else
	{
		if (HeldPickupComponent.IsValid())
		{
			EndPickup();
		}

		bPickupCandidateValid = false;
		PickupCandidateComponent.Reset();
	}

	UpdateThirdPersonCameraChase(DeltaSeconds);

	if (DroneCompanion && bPrimaryDroneAvailable)
	{
		SyncDroneCompanionControlState(IsDronePilotMode() && !bMapModeActive);
		UpdateDroneCompanionThirdPersonTarget();
		UpdateDroneTorchTarget();

		if (IsRagdolling())
		{
			DroneCompanion->ResetPilotInputs();
		}
		else if (IsDroneInputModeActive())
		{
			UpdateDronePilotInputs(DeltaSeconds);
		}
		else
		{
			DroneCompanion->ResetPilotInputs();
		}
	}
	else if (DroneCompanion)
	{
		UpdateDroneTorchTarget();
		DroneCompanion->ResetPilotInputs();
	}

	UpdateThirdPersonTransition(DeltaSeconds);

	if (!bViewModeInitialized && GetController() != nullptr)
	{
		ApplyViewMode(CurrentViewMode, false);
		bViewModeInitialized = true;
	}
}

void AAgentCharacter::OnMovementModeChanged(EMovementMode PrevMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PrevMovementMode, PreviousCustomMode);

	if (IsRagdolling())
	{
		bTrackingFallHeight = false;
		return;
	}

	UCharacterMovementComponent* CharacterMovementComponent = GetCharacterMovement();
	if (!CharacterMovementComponent)
	{
		bTrackingFallHeight = false;
		return;
	}

	if (CharacterMovementComponent->IsFalling())
	{
		bTrackingFallHeight = true;
		FallTrackingStartZ = GetActorLocation().Z;
		FallTrackingMinZ = FallTrackingStartZ;
		return;
	}

	if (PrevMovementMode == MOVE_Falling)
	{
		bTrackingFallHeight = false;
	}
}

void AAgentCharacter::UpdateFallHeightTracking()
{
	if (!bTrackingFallHeight || IsRagdolling())
	{
		return;
	}

	const float CurrentZ = GetActorLocation().Z;
	FallTrackingMinZ = FMath::Min(FallTrackingMinZ, CurrentZ);
	LastFallHeightCm = FMath::Max(0.0f, FallTrackingStartZ - FallTrackingMinZ);
}

void AAgentCharacter::Landed(const FHitResult& Hit)
{
	const float LandingSpeed = FMath::Abs(FMath::Min(0.0f, GetVelocity().Z));
	Super::Landed(Hit);

	if (bTrackingFallHeight)
	{
		UpdateFallHeightTracking();
	}
	else
	{
		LastFallHeightCm = 0.0f;
	}
	const bool bMeetsFallHeightForRagdoll = LastFallHeightCm >= FMath::Max(0.0f, MinFallHeightForRagdollCm);
	bTrackingFallHeight = false;
	FallTrackingStartZ = GetActorLocation().Z;
	FallTrackingMinZ = FallTrackingStartZ;

	LastFallImpactSpeed = LandingSpeed;
	if (IsRagdolling())
	{
		return;
	}

	if (LandingSpeed >= FMath::Max(0.0f, FallHardRagdollSpeed) && bMeetsFallHeightForRagdoll)
	{
		TryTriggerAutoRagdoll(EAgentRagdollReason::Impact, FName(TEXT("FallHard")));
		return;
	}

	if (!bEnableFallInstability)
	{
		return;
	}

	if (bEnableInstabilityMeter && LandingSpeed >= FMath::Max(0.0f, FallInstabilityStartSpeed))
	{
		const float FallSeverity = FMath::GetMappedRangeValueClamped(
			FVector2D(FallInstabilityStartSpeed, FMath::Max(FallInstabilityStartSpeed + KINDA_SMALL_NUMBER, FallInstabilityMaxSpeed)),
			FVector2D(0.0f, 1.0f),
			LandingSpeed);
		const float InstabilityDelta = FMath::Lerp(
			FMath::Max(0.0f, FallInstabilityMinAdd),
			FMath::Max(FallInstabilityMinAdd, FallInstabilityMaxAdd),
			FallSeverity);
		AddInstability(InstabilityDelta, EAgentRagdollReason::Impact, FName(TEXT("Fall")), bMeetsFallHeightForRagdoll);
	}
}

void AAgentCharacter::NotifyHit(
	UPrimitiveComponent* MyComp,
	AActor* Other,
	UPrimitiveComponent* OtherComp,
	bool bSelfMoved,
	FVector HitLocation,
	FVector HitNormal,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);

	if (IsRagdolling() || Other == this || !OtherComp)
	{
		return;
	}

	const float ImpactMagnitude = ComputeImpactMagnitude(NormalImpulse, OtherComp, HitNormal);
	const float ImpactClosingSpeed = ComputeImpactClosingSpeed(OtherComp, HitNormal);
	LastImpactMagnitude = ImpactMagnitude;
	LastImpactClosingSpeed = ImpactClosingSpeed;

	if (bEnableDroneImpactKnockdownChance && Cast<ADroneCompanion>(Other))
	{
		const float DroneSpeed = OtherComp->GetComponentVelocity().Size();
		const bool bDroneMovingFastEnough = !bDroneImpactRequiresDroneMovement
			|| DroneSpeed >= FMath::Max(0.0f, DroneImpactMinDroneSpeed);
		if (bDroneMovingFastEnough)
		{
			const FVector ImpactPoint = !Hit.ImpactPoint.IsNearlyZero()
				? FVector(Hit.ImpactPoint)
				: HitLocation;
			const UCapsuleComponent* CapsuleComp = GetCapsuleComponent();
			const float CapsuleHalfHeight = CapsuleComp ? CapsuleComp->GetScaledCapsuleHalfHeight() : 0.0f;
			const float FeetZ = GetActorLocation().Z - CapsuleHalfHeight;
			const float HeightFromFeet = ImpactPoint.Z - FeetZ;
			const bool bLegHit = HeightFromFeet <= FMath::Max(0.0f, DroneImpactLegMaxHeightFromFeetCm);

			FVector HeadLocation = GetActorLocation() + FVector(0.0f, 0.0f, CapsuleHalfHeight);
			if (const USkeletalMeshComponent* MeshComp = GetMesh())
			{
				if (DroneImpactHeadBoneName != NAME_None && MeshComp->GetBoneIndex(DroneImpactHeadBoneName) != INDEX_NONE)
				{
					HeadLocation = MeshComp->GetBoneLocation(DroneImpactHeadBoneName);
				}
				else
				{
					static const FName DefaultHeadBoneName(TEXT("head"));
					if (MeshComp->GetBoneIndex(DefaultHeadBoneName) != INDEX_NONE)
					{
						HeadLocation = MeshComp->GetBoneLocation(DefaultHeadBoneName);
					}
				}
			}

			const float HeadRadius = FMath::Max(0.0f, DroneImpactHeadRadiusCm);
			const bool bHeadHit = FVector::DistSquared(ImpactPoint, HeadLocation) <= FMath::Square(HeadRadius);
			const float DroneKnockdownChance = ComputeDroneImpactKnockdownChance(ImpactClosingSpeed, bHeadHit, bLegHit);
			if (DroneKnockdownChance > 0.0f)
			{
				const float DroneKnockdownRoll = FMath::FRand();
				const bool bTriggerDroneKnockdown = DroneKnockdownRoll <= DroneKnockdownChance;

				if (bShowDroneImpactKnockdownDebug && GEngine)
				{
					const FString ImpactMessage = FString::Printf(
						TEXT("Drone Hit V=%.0f  Chance=%.1f%%  Roll=%.2f  Head=%s  Legs=%s"),
						ImpactClosingSpeed,
						DroneKnockdownChance * 100.0f,
						DroneKnockdownRoll,
						bHeadHit ? TEXT("Y") : TEXT("N"),
						bLegHit ? TEXT("Y") : TEXT("N"));
					GEngine->AddOnScreenDebugMessage(
						static_cast<uint64>(GetUniqueID()) + 19106ULL,
						0.85f,
						bTriggerDroneKnockdown ? FColor::Red : FColor::Yellow,
						ImpactMessage);
				}

				if (bTriggerDroneKnockdown)
				{
					const FName SourceTag = bHeadHit
						? FName(TEXT("DroneImpactHead"))
						: (bLegHit ? FName(TEXT("DroneImpactLegs")) : FName(TEXT("DroneImpactBody")));
					if (TryTriggerAutoRagdoll(EAgentRagdollReason::Impact, SourceTag))
					{
						return;
					}
				}
			}
		}
	}

	const float OtherSpeed = OtherComp->GetComponentVelocity().Size();
	const bool bMovingOtherFastEnough = !bImpactVelocityRequiresMovingOther
		|| OtherSpeed >= FMath::Max(0.0f, ImpactVelocityMovingOtherMinSpeed);
	float VelocityKnockdownSpeed = ImpactClosingSpeed;
	if (OtherComp->IsSimulatingPhysics())
	{
		VelocityKnockdownSpeed *= FMath::Max(0.1f, ImpactPhysicsBodyMultiplier);
	}

	if (bEnableImpactVelocityRagdoll
		&& bMovingOtherFastEnough
		&& VelocityKnockdownSpeed >= FMath::Max(0.0f, ImpactVelocityRagdollThreshold))
	{
		TryTriggerAutoRagdoll(EAgentRagdollReason::Impact, FName(TEXT("ImpactVelocityHard")));
		return;
	}

	if (ImpactMagnitude <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	if (ImpactMagnitude >= FMath::Max(0.0f, ImpactHardRagdollImpulse))
	{
		TryTriggerAutoRagdoll(EAgentRagdollReason::Impact, FName(TEXT("ImpactHard")));
		return;
	}

	if (bEnableImpactInstability
		&& bEnableInstabilityMeter
		&& ImpactMagnitude >= FMath::Max(0.0f, ImpactInstabilityStartImpulse))
	{
		const float ImpactSeverity = FMath::GetMappedRangeValueClamped(
			FVector2D(ImpactInstabilityStartImpulse, FMath::Max(ImpactInstabilityStartImpulse + KINDA_SMALL_NUMBER, ImpactInstabilityMaxImpulse)),
			FVector2D(0.0f, 1.0f),
			ImpactMagnitude);
		float InstabilityDelta = FMath::Lerp(
			FMath::Max(0.0f, ImpactInstabilityMinAdd),
			FMath::Max(ImpactInstabilityMinAdd, ImpactInstabilityMaxAdd),
			ImpactSeverity);
		if (OtherComp->IsSimulatingPhysics())
		{
			InstabilityDelta *= FMath::Max(0.1f, ImpactPhysicsBodyMultiplier);
		}
		AddInstability(InstabilityDelta, EAgentRagdollReason::Impact, FName(TEXT("Impact")));
	}
}

void AAgentCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &AAgentCharacter::DoJumpStart);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &AAgentCharacter::DoJumpEnd);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AAgentCharacter::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &AAgentCharacter::StopMove);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Canceled, this, &AAgentCharacter::StopMove);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AAgentCharacter::Look);
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AAgentCharacter::Look);
	}
	else
	{
		UE_LOG(LogAgent, Error, TEXT("'%s' Failed to find an Enhanced Input component!"), *GetNameSafe(this));
	}

	PlayerInputComponent->BindKey(EKeys::V, IE_Pressed, this, &AAgentCharacter::OnViewModeButtonPressed);
	PlayerInputComponent->BindKey(EKeys::V, IE_Released, this, &AAgentCharacter::OnViewModeButtonReleased);
	PlayerInputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &AAgentCharacter::OnCycleActiveDronePressed);
	PlayerInputComponent->BindKey(EKeys::H, IE_Pressed, this, &AAgentCharacter::OnDroneTorchTogglePressed);
	PlayerInputComponent->BindKey(EKeys::Gamepad_RightThumbstick, IE_Pressed, this, &AAgentCharacter::OnDroneTorchTogglePressed);
	PlayerInputComponent->BindKey(EKeys::Gamepad_FaceButton_Top, IE_Pressed, this, &AAgentCharacter::OnViewModeButtonPressed);
	PlayerInputComponent->BindKey(EKeys::Gamepad_FaceButton_Top, IE_Released, this, &AAgentCharacter::OnViewModeButtonReleased);
	PlayerInputComponent->BindKey(EKeys::One, IE_Pressed, this, &AAgentCharacter::OnConveyorPlacementModePressed);
	PlayerInputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AAgentCharacter::OnStorageBinPlacementModePressed);
	PlayerInputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AAgentCharacter::OnMachinePlacementModePressed);
	PlayerInputComponent->BindKey(EKeys::Four, IE_Pressed, this, &AAgentCharacter::OnMinerPlacementModePressed);
	PlayerInputComponent->BindKey(EKeys::Five, IE_Pressed, this, &AAgentCharacter::OnMiningSwarmPlacementModePressed);
	PlayerInputComponent->BindKey(EKeys::LeftAlt, IE_Pressed, this, &AAgentCharacter::OnFactoryFreePlacementTogglePressed);
	PlayerInputComponent->BindKey(EKeys::Gamepad_FaceButton_Left, IE_Pressed, this, &AAgentCharacter::OnGamepadFaceButtonLeftPressed);
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AAgentCharacter::OnLeftMouseButtonPressed);
	PlayerInputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &AAgentCharacter::OnLeftMouseButtonReleased);
	PlayerInputComponent->BindKey(EKeys::Gamepad_RightTrigger, IE_Pressed, this, &AAgentCharacter::OnPickupOrPlacePressed);
	PlayerInputComponent->BindKey(EKeys::Gamepad_RightTrigger, IE_Released, this, &AAgentCharacter::OnPickupOrPlaceReleased);
	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AAgentCharacter::OnRightMouseButtonPressed);
	PlayerInputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &AAgentCharacter::OnRightMouseButtonReleased);
	PlayerInputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &AAgentCharacter::OnMouseScrollUpPressed);
	PlayerInputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &AAgentCharacter::OnMouseScrollDownPressed);
	PlayerInputComponent->BindKey(EKeys::Gamepad_FaceButton_Right, IE_Pressed, this, &AAgentCharacter::OnGamepadFaceButtonRightPressed);
	PlayerInputComponent->BindKey(EKeys::Gamepad_LeftShoulder, IE_Pressed, this, &AAgentCharacter::OnGamepadLeftShoulderPressed);
	PlayerInputComponent->BindKey(EKeys::Gamepad_RightShoulder, IE_Pressed, this, &AAgentCharacter::OnHookJobButtonPressed);
	PlayerInputComponent->BindKey(EKeys::Gamepad_RightShoulder, IE_Released, this, &AAgentCharacter::OnHookJobButtonReleased);
	PlayerInputComponent->BindKey(EKeys::MiddleMouseButton, IE_Pressed, this, &AAgentCharacter::OnHookJobButtonPressed);
	PlayerInputComponent->BindKey(EKeys::MiddleMouseButton, IE_Released, this, &AAgentCharacter::OnHookJobButtonReleased);
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AAgentCharacter::DoJumpStart);
	PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Released, this, &AAgentCharacter::DoJumpEnd);
	PlayerInputComponent->BindKey(EKeys::Gamepad_FaceButton_Bottom, IE_Pressed, this, &AAgentCharacter::DoJumpStart);
	PlayerInputComponent->BindKey(EKeys::Gamepad_FaceButton_Bottom, IE_Released, this, &AAgentCharacter::DoJumpEnd);
	PlayerInputComponent->BindKey(EKeys::B, IE_Pressed, this, &AAgentCharacter::OnBackpackOrDroneModePressed);
	PlayerInputComponent->BindKey(EKeys::B, IE_Released, this, &AAgentCharacter::OnBackpackOrDroneModeReleased);
	PlayerInputComponent->BindKey(EKeys::M, IE_Pressed, this, &AAgentCharacter::OnKeyboardMapButtonPressed);
	PlayerInputComponent->BindKey(EKeys::M, IE_Released, this, &AAgentCharacter::OnKeyboardMapButtonReleased);
	PlayerInputComponent->BindKey(EKeys::BackSpace, IE_Pressed, this, &AAgentCharacter::OnRagdollTogglePressed);
	PlayerInputComponent->BindKey(EKeys::LeftControl, IE_Pressed, this, &AAgentCharacter::OnWalkModifierPressed);
	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &AAgentCharacter::OnSprintModifierPressed);
	PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Released, this, &AAgentCharacter::OnSprintModifierReleased);
	PlayerInputComponent->BindKey(EKeys::RightShift, IE_Pressed, this, &AAgentCharacter::OnSprintModifierPressed);
	PlayerInputComponent->BindKey(EKeys::RightShift, IE_Released, this, &AAgentCharacter::OnSprintModifierReleased);
	PlayerInputComponent->BindKey(EKeys::W, IE_Pressed, this, &AAgentCharacter::OnDronePitchForwardPressed);
	PlayerInputComponent->BindKey(EKeys::W, IE_Released, this, &AAgentCharacter::OnDronePitchForwardReleased);
	PlayerInputComponent->BindKey(EKeys::S, IE_Pressed, this, &AAgentCharacter::OnDronePitchBackwardPressed);
	PlayerInputComponent->BindKey(EKeys::S, IE_Released, this, &AAgentCharacter::OnDronePitchBackwardReleased);
	PlayerInputComponent->BindKey(EKeys::A, IE_Pressed, this, &AAgentCharacter::OnDroneRollLeftPressed);
	PlayerInputComponent->BindKey(EKeys::A, IE_Released, this, &AAgentCharacter::OnDroneRollLeftReleased);
	PlayerInputComponent->BindKey(EKeys::D, IE_Pressed, this, &AAgentCharacter::OnDroneRollRightPressed);
	PlayerInputComponent->BindKey(EKeys::D, IE_Released, this, &AAgentCharacter::OnDroneRollRightReleased);
	PlayerInputComponent->BindKey(EKeys::Q, IE_Pressed, this, &AAgentCharacter::OnDroneYawLeftPressed);
	PlayerInputComponent->BindKey(EKeys::Q, IE_Released, this, &AAgentCharacter::OnDroneYawLeftReleased);
	PlayerInputComponent->BindKey(EKeys::E, IE_Pressed, this, &AAgentCharacter::OnPickupOrDroneYawRightPressed);
	PlayerInputComponent->BindKey(EKeys::E, IE_Released, this, &AAgentCharacter::OnPickupOrDroneYawRightReleased);
	PlayerInputComponent->BindKey(EKeys::LeftBracket, IE_Pressed, this, &AAgentCharacter::OnPickupStrengthDecreasePressed);
	PlayerInputComponent->BindKey(EKeys::RightBracket, IE_Pressed, this, &AAgentCharacter::OnPickupStrengthIncreasePressed);
	PlayerInputComponent->BindKey(EKeys::R, IE_Pressed, this, &AAgentCharacter::OnDroneThrottleUpPressed);
	PlayerInputComponent->BindKey(EKeys::R, IE_Released, this, &AAgentCharacter::OnDroneThrottleUpReleased);
	PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &AAgentCharacter::OnInteractPressed);
	PlayerInputComponent->BindKey(EKeys::F, IE_Released, this, &AAgentCharacter::OnInteractReleased);
	PlayerInputComponent->BindKey(EKeys::T, IE_Pressed, this, &AAgentCharacter::OnVehicleInteractPressed);

	PlayerInputComponent->BindAxisKey(EKeys::Gamepad_LeftX, this, &AAgentCharacter::OnDroneGamepadYawAxis);
	PlayerInputComponent->BindAxisKey(EKeys::Gamepad_LeftY, this, &AAgentCharacter::OnDroneGamepadThrottleAxis);
	PlayerInputComponent->BindAxisKey(EKeys::Gamepad_RightX, this, &AAgentCharacter::OnDroneGamepadRollAxis);
	PlayerInputComponent->BindAxisKey(EKeys::Gamepad_RightY, this, &AAgentCharacter::OnDroneGamepadPitchAxis);
	PlayerInputComponent->BindAxisKey(EKeys::Gamepad_LeftTriggerAxis, this, &AAgentCharacter::OnDroneGamepadLeftTriggerAxis);
	PlayerInputComponent->BindAxisKey(EKeys::Gamepad_RightTriggerAxis, this, &AAgentCharacter::OnDroneGamepadRightTriggerAxis);
	PlayerInputComponent->BindKey(EKeys::Gamepad_DPad_Left, IE_Pressed, this, &AAgentCharacter::OnDroneHoverModePressed);
	PlayerInputComponent->BindKey(EKeys::Gamepad_DPad_Right, IE_Pressed, this, &AAgentCharacter::OnDroneStabilizerTogglePressed);
	PlayerInputComponent->BindKey(EKeys::Gamepad_Special_Left, IE_Pressed, this, &AAgentCharacter::OnControllerMapButtonPressed);
	PlayerInputComponent->BindKey(EKeys::Gamepad_Special_Left, IE_Released, this, &AAgentCharacter::OnControllerMapButtonReleased);
	PlayerInputComponent->BindKey(EKeys::Gamepad_DPad_Up, IE_Pressed, this, &AAgentCharacter::OnDroneCameraTiltUpPressed);
	PlayerInputComponent->BindKey(EKeys::Gamepad_DPad_Down, IE_Pressed, this, &AAgentCharacter::OnBackpackOrDroneCameraDownPressed);
	PlayerInputComponent->BindKey(EKeys::Gamepad_DPad_Down, IE_Released, this, &AAgentCharacter::OnBackpackOrDroneCameraDownReleased);
}

void AAgentCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D MovementVector = Value.Get<FVector2D>();
	DoMove(MovementVector.X, MovementVector.Y);
}

void AAgentCharacter::StopMove(const FInputActionValue& Value)
{
	DoMove(0.0f, 0.0f);
}

void AAgentCharacter::Look(const FInputActionValue& Value)
{
	if (bPickupRotationModeActive && bControllerPickupHeld && HeldPickupComponent.IsValid())
	{
		return;
	}

	const FVector2D LookAxisVector = Value.Get<FVector2D>();
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AAgentCharacter::OnRagdollTogglePressed()
{
	if (!bEnableRagdollToggleInput)
	{
		return;
	}

	// Manual ragdoll key is entry-only; recovery is handled by jump input.
	if (IsRagdolling())
	{
		return;
	}

	RequestEnterRagdoll(EAgentRagdollReason::ManualInput);
}

void AAgentCharacter::OnWalkModifierPressed()
{
	bSafeModeToggled = !bSafeModeToggled;
	UpdateMovementSpeedState(0.0f);
	LastClumsinessEventSource = bSafeModeToggled ? FName(TEXT("SafeModeOn")) : FName(TEXT("SafeModeOff"));

	if (bShowClumsinessDebug && GEngine)
	{
		const FString SafeModeMessage = FString::Printf(
			TEXT("Safe Mode: %s"),
			bSafeModeToggled ? TEXT("ON") : TEXT("OFF"));
		GEngine->AddOnScreenDebugMessage(
			static_cast<uint64>(GetUniqueID()) + 19103ULL,
			1.0f,
			bSafeModeToggled ? FColor::Green : FColor::Silver,
			SafeModeMessage);
	}
}

void AAgentCharacter::OnSprintModifierPressed()
{
	bSprintModifierHeld = true;
}

void AAgentCharacter::OnSprintModifierReleased()
{
	bSprintModifierHeld = false;
}

bool AAgentCharacter::IsSprintModeActive() const
{
	if (!bSprintModifierHeld || IsRagdolling())
	{
		return false;
	}

	if (VehicleInteractionComponent && VehicleInteractionComponent->IsControllingVehicle())
	{
		return false;
	}

	if (IsDroneInputModeActive() && bLockCharacterMovementDuringDronePilot)
	{
		return false;
	}

	return true;
}

bool AAgentCharacter::IsWalkModeActive() const
{
	if (!bSafeModeToggled || IsRagdolling())
	{
		return false;
	}

	if (VehicleInteractionComponent && VehicleInteractionComponent->IsControllingVehicle())
	{
		return false;
	}

	if (IsDroneInputModeActive() && bLockCharacterMovementDuringDronePilot)
	{
		return false;
	}

	return !IsSprintModeActive();
}

float AAgentCharacter::ResolveTargetGroundSpeed() const
{
	const float SafeWalkSpeed = FMath::Max(0.0f, WalkModeSpeed);
	const float SafeRunSpeed = FMath::Max(0.0f, RunModeSpeed);
	const float SafeSprintSpeed = FMath::Max(0.0f, SprintModeSpeed);

	if (IsSprintModeActive())
	{
		return SafeSprintSpeed;
	}

	if (IsWalkModeActive())
	{
		return SafeWalkSpeed;
	}

	return SafeRunSpeed;
}

void AAgentCharacter::UpdateMovementSpeedState(float DeltaSeconds)
{
	UCharacterMovementComponent* CharacterMovementComponent = GetCharacterMovement();
	if (!CharacterMovementComponent)
	{
		return;
	}

	const float TargetGroundSpeed = ResolveTargetGroundSpeed();
	const float InterpSpeed = FMath::Max(0.0f, MovementSpeedInterpSpeed);
	const float SafeDeltaSeconds = FMath::Max(0.0f, DeltaSeconds);
	if (InterpSpeed > KINDA_SMALL_NUMBER && SafeDeltaSeconds > KINDA_SMALL_NUMBER)
	{
		CharacterMovementComponent->MaxWalkSpeed = FMath::FInterpTo(
			CharacterMovementComponent->MaxWalkSpeed,
			TargetGroundSpeed,
			SafeDeltaSeconds,
			InterpSpeed);
		return;
	}

	CharacterMovementComponent->MaxWalkSpeed = TargetGroundSpeed;
}

void AAgentCharacter::ToggleRagdoll()
{
	if (IsRagdolling())
	{
		RequestExitRagdoll(EAgentRagdollReason::ManualInput);
		return;
	}

	RequestEnterRagdoll(EAgentRagdollReason::ManualInput);
}

bool AAgentCharacter::RequestEnterRagdoll(EAgentRagdollReason Reason)
{
	if (!CanEnterRagdoll())
	{
		return false;
	}

	EnterRagdollInternal(Reason);
	return IsRagdolling();
}

bool AAgentCharacter::RequestExitRagdoll(EAgentRagdollReason Reason)
{
	if (!CanExitRagdoll())
	{
		return false;
	}

	ExitRagdollInternal(Reason);
	return !IsRagdolling();
}

bool AAgentCharacter::CanEnterRagdoll() const
{
	return RagdollState == EAgentRagdollState::Normal
		&& GetMesh() != nullptr
		&& GetCapsuleComponent() != nullptr
		&& GetCharacterMovement() != nullptr;
}

bool AAgentCharacter::CanExitRagdoll() const
{
	return RagdollState == EAgentRagdollState::Ragdoll
		&& GetMesh() != nullptr
		&& GetCapsuleComponent() != nullptr
		&& GetCharacterMovement() != nullptr;
}

void AAgentCharacter::SetRagdollState(EAgentRagdollState NewState, EAgentRagdollReason Reason)
{
	const EAgentRagdollState PreviousState = RagdollState;
	RagdollState = NewState;
	LastRagdollReason = Reason;
	RefreshFirstPersonCameraControlRotation();

	if (!bLogRagdollStateChanges || PreviousState == NewState)
	{
		return;
	}

	UE_LOG(
		LogAgent,
		Log,
		TEXT("Ragdoll state changed: %s -> %s (Reason: %s)"),
		ToRagdollStateString(PreviousState),
		ToRagdollStateString(NewState),
		ToRagdollReasonString(Reason));
}

EAgentViewMode AAgentCharacter::ResolveRagdollEntryViewMode() const
{
	if (RagdollEntryViewMode == EAgentViewMode::FirstPerson)
	{
		return EAgentViewMode::FirstPerson;
	}

	return EAgentViewMode::ThirdPerson;
}

FName AAgentCharacter::ResolveRagdollAnchorBoneName() const
{
	const USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!MeshComponent)
	{
		return NAME_None;
	}

	if (RagdollAnchorBoneName != NAME_None && MeshComponent->GetBoneIndex(RagdollAnchorBoneName) != INDEX_NONE)
	{
		return RagdollAnchorBoneName;
	}

	static const FName DefaultPelvisBoneName(TEXT("pelvis"));
	if (MeshComponent->GetBoneIndex(DefaultPelvisBoneName) != INDEX_NONE)
	{
		return DefaultPelvisBoneName;
	}

	return MeshComponent->GetBoneName(0);
}

FVector AAgentCharacter::GetRagdollAnchorLocation() const
{
	const USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!MeshComponent)
	{
		return GetActorLocation();
	}

	const FName AnchorBoneName = ResolveRagdollAnchorBoneName();
	if (AnchorBoneName != NAME_None)
	{
		return MeshComponent->GetBoneLocation(AnchorBoneName);
	}

	return MeshComponent->GetComponentLocation();
}

FRotator AAgentCharacter::GetRagdollAnchorRotation() const
{
	const USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!MeshComponent)
	{
		return GetActorRotation();
	}

	const FName AnchorBoneName = ResolveRagdollAnchorBoneName();
	if (AnchorBoneName != NAME_None)
	{
		return MeshComponent->GetBoneQuaternion(AnchorBoneName).Rotator();
	}

	return MeshComponent->GetComponentRotation();
}

void AAgentCharacter::UpdateRagdollAutoRecovery(float DeltaSeconds)
{
	if (!IsRagdolling())
	{
		RagdollElapsedDuration = 0.0f;
		RagdollStableDuration = 0.0f;
		LastRagdollLinearSpeed = 0.0f;
		LastRagdollAngularSpeedDeg = 0.0f;
		return;
	}

	const float SafeDeltaSeconds = FMath::Max(0.0f, DeltaSeconds);
	RagdollElapsedDuration += SafeDeltaSeconds;

	if (!bEnableAutoRagdollRecovery || RagdollElapsedDuration < FMath::Max(0.0f, AutoRagdollRecoveryMinDuration))
	{
		RagdollStableDuration = 0.0f;
		return;
	}

	USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!MeshComponent)
	{
		RagdollStableDuration = 0.0f;
		return;
	}

	const FName AnchorBoneName = ResolveRagdollAnchorBoneName();
	const FVector LinearVelocity = AnchorBoneName != NAME_None
		? MeshComponent->GetPhysicsLinearVelocity(AnchorBoneName)
		: MeshComponent->GetPhysicsLinearVelocity();
	const FVector AngularVelocityDeg = AnchorBoneName != NAME_None
		? MeshComponent->GetPhysicsAngularVelocityInDegrees(AnchorBoneName)
		: MeshComponent->GetPhysicsAngularVelocityInDegrees();

	LastRagdollLinearSpeed = LinearVelocity.Size();
	LastRagdollAngularSpeedDeg = AngularVelocityDeg.Size();

	const bool bLinearSettled = LastRagdollLinearSpeed <= FMath::Max(0.0f, AutoRagdollRecoveryMaxLinearSpeed);
	const bool bAngularSettled = LastRagdollAngularSpeedDeg <= FMath::Max(0.0f, AutoRagdollRecoveryMaxAngularSpeedDeg);
	if (bLinearSettled && bAngularSettled)
	{
		RagdollStableDuration += SafeDeltaSeconds;
	}
	else
	{
		RagdollStableDuration = 0.0f;
	}

	if (RagdollStableDuration < FMath::Max(0.0f, AutoRagdollRecoveryStableDuration))
	{
		return;
	}

	const bool bExited = RequestExitRagdoll(EAgentRagdollReason::Scripted);
	if (bExited && bLogAutoRagdollRecovery)
	{
		UE_LOG(
			LogAgent,
			Log,
			TEXT("Auto ragdoll recovery triggered at %.2fs (Linear=%.2f, Angular=%.2f)."),
			RagdollElapsedDuration,
			LastRagdollLinearSpeed,
			LastRagdollAngularSpeedDeg);
	}
}

void AAgentCharacter::ResetRagdollInputState()
{
	bViewModeButtonHeld = false;
	bViewModeHoldTriggered = false;
	ViewModeButtonHeldDuration = 0.0f;

	bKeyboardMapButtonHeld = false;
	bKeyboardMapButtonTriggeredMiniMap = false;
	KeyboardMapButtonHeldDuration = 0.0f;

	bControllerMapButtonHeld = false;
	bControllerMapButtonTriggeredMiniMap = false;
	ControllerMapButtonHeldDuration = 0.0f;

	ResetBackpackDeployButtonHoldState();

	bHookJobButtonHeld = false;
	bHookJobButtonTriggeredHoldRelease = false;
	HookJobButtonHeldDuration = 0.0f;

	bKeyboardPickupHeld = false;
	bControllerPickupHeld = false;
	bPickupRotationModeActive = false;
	bInteractKeyDrivingDroneThrottle = false;
	bSprintModifierHeld = false;
	OnDroneThrottleDownReleased();

	ResetDroneInputState();
}

void AAgentCharacter::EnterRagdollInternal(EAgentRagdollReason Reason)
{
	USkeletalMeshComponent* MeshComponent = GetMesh();
	UCapsuleComponent* CapsuleComp = GetCapsuleComponent();
	UCharacterMovementComponent* CharacterMovementComponent = GetCharacterMovement();
	if (!MeshComponent || !CapsuleComp || !CharacterMovementComponent)
	{
		return;
	}

	PreRagdollViewMode = CurrentViewMode;
	CachedMeshRelativeTransform = MeshComponent->GetRelativeTransform();
	CachedMeshCollisionProfileName = MeshComponent->GetCollisionProfileName();
	CachedMeshCollisionEnabled = MeshComponent->GetCollisionEnabled();
	CachedCapsuleCollisionEnabled = CapsuleComp->GetCollisionEnabled();
	CachedMovementMode = CharacterMovementComponent->MovementMode;
	CachedCustomMovementMode = CharacterMovementComponent->CustomMovementMode;

	if (bAttemptVehicleExitOnRagdollEntry
		&& VehicleInteractionComponent
		&& VehicleInteractionComponent->IsControllingVehicle())
	{
		VehicleInteractionComponent->TryExitCurrentVehicle();
	}

	if (bForceCharacterViewOnRagdollEntry)
	{
		const EAgentViewMode EntryViewMode = ResolveRagdollEntryViewMode();
		if (CurrentViewMode != EntryViewMode || bMapModeActive || bMiniMapModeActive)
		{
			ApplyViewMode(EntryViewMode, true);
		}
	}

	if (bExitConveyorPlacementOnRagdollEntry && bConveyorPlacementModeActive)
	{
		ExitConveyorPlacementMode();
	}

	if (bReleaseHeldPickupOnRagdollEntry && HeldPickupComponent.IsValid())
	{
		EndPickup();
	}

	if (bReleaseHookJobDronesOnRagdollEntry)
	{
		TryReleaseAllHookJobDrones();
	}

	ResetRagdollInputState();

	CharacterMovementComponent->StopMovementImmediately();
	CharacterMovementComponent->DisableMovement();

	CapsuleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	MeshComponent->SetCollisionProfileName(RagdollCollisionProfileName);
	MeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComponent->SetAllBodiesSimulatePhysics(true);
	MeshComponent->SetSimulatePhysics(true);
	MeshComponent->WakeAllRigidBodies();

	if (BackAttachmentComponent)
	{
		BackAttachmentComponent->SetOwnerRagdolling(true);
	}

	RagdollElapsedDuration = 0.0f;
	RagdollStableDuration = 0.0f;
	LastRagdollLinearSpeed = 0.0f;
	LastRagdollAngularSpeedDeg = 0.0f;
	bTrackingFallHeight = false;
	FallTrackingStartZ = GetActorLocation().Z;
	FallTrackingMinZ = FallTrackingStartZ;
	SetRagdollState(EAgentRagdollState::Ragdoll, Reason);
	AutoRagdollCooldownRemaining = FMath::Max(AutoRagdollCooldownRemaining, FMath::Max(0.0f, AutoRagdollCooldownSeconds));
	TripEventCooldownRemaining = FMath::Max(TripEventCooldownRemaining, FMath::Max(0.0f, TripEventCooldownSeconds));
	if (bResetInstabilityOnRagdollEntry)
	{
		InstabilityValue = 0.0f;
	}
}

void AAgentCharacter::ExitRagdollInternal(EAgentRagdollReason Reason)
{
	USkeletalMeshComponent* MeshComponent = GetMesh();
	UCapsuleComponent* CapsuleComp = GetCapsuleComponent();
	UCharacterMovementComponent* CharacterMovementComponent = GetCharacterMovement();
	if (!MeshComponent || !CapsuleComp || !CharacterMovementComponent)
	{
		SetRagdollState(EAgentRagdollState::Normal, Reason);
		return;
	}

	SetRagdollState(EAgentRagdollState::Recovering, Reason);

	const FVector RagdollAnchorLocation = GetRagdollAnchorLocation();
	const FRotator RagdollAnchorRotation = GetRagdollAnchorRotation();
	const float CapsuleHalfHeight = CapsuleComp->GetScaledCapsuleHalfHeight();
	const FVector RecoveryLocation = RagdollAnchorLocation + FVector(0.0f, 0.0f, CapsuleHalfHeight + RagdollRecoveryHeightOffset);
	const float RecoveryYaw = bUseRagdollYawOnRecovery ? RagdollAnchorRotation.Yaw : GetActorRotation().Yaw;
	const FRotator RecoveryRotation(0.0f, RecoveryYaw, 0.0f);

	MeshComponent->SetSimulatePhysics(false);
	MeshComponent->SetAllBodiesSimulatePhysics(false);
	MeshComponent->SetRelativeTransform(CachedMeshRelativeTransform);
	MeshComponent->SetCollisionProfileName(CachedMeshCollisionProfileName);
	MeshComponent->SetCollisionEnabled(CachedMeshCollisionEnabled);

	SetActorRotation(RecoveryRotation, ETeleportType::TeleportPhysics);

	FHitResult MoveHit;
	const bool bMoved = SetActorLocation(
		RecoveryLocation,
		bSweepCapsuleOnRagdollRecovery,
		bSweepCapsuleOnRagdollRecovery ? &MoveHit : nullptr,
		ETeleportType::TeleportPhysics);

	if (!bMoved && bAllowTeleportRecoveryFallback)
	{
		SetActorLocation(RecoveryLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}

	CapsuleComp->SetCollisionEnabled(CachedCapsuleCollisionEnabled);

	if (bRestorePreRagdollMovementMode && CachedMovementMode != MOVE_None)
	{
		CharacterMovementComponent->SetMovementMode(CachedMovementMode, CachedCustomMovementMode);
	}
	else
	{
		CharacterMovementComponent->SetMovementMode(MOVE_Walking);
	}

	if (BackAttachmentComponent)
	{
		BackAttachmentComponent->SetOwnerRagdolling(false);
	}

	RagdollElapsedDuration = 0.0f;
	RagdollStableDuration = 0.0f;
	LastRagdollLinearSpeed = 0.0f;
	LastRagdollAngularSpeedDeg = 0.0f;
	bTrackingFallHeight = false;
	FallTrackingStartZ = GetActorLocation().Z;
	FallTrackingMinZ = FallTrackingStartZ;
	CharacterMovementComponent->StopMovementImmediately();
	ResetRagdollInputState();
	SetRagdollState(EAgentRagdollState::Normal, Reason);
}

void AAgentCharacter::OnViewModeButtonPressed()
{
	if (IsRagdolling())
	{
		return;
	}

	bViewModeButtonHeld = true;
	bViewModeHoldTriggered = false;
	ViewModeButtonHeldDuration = 0.0f;
}

void AAgentCharacter::OnViewModeButtonReleased()
{
	if (IsRagdolling())
	{
		bViewModeButtonHeld = false;
		bViewModeHoldTriggered = false;
		ViewModeButtonHeldDuration = 0.0f;
		return;
	}

	if (!bViewModeButtonHeld)
	{
		return;
	}

	const bool bWasHoldTriggered = bViewModeHoldTriggered;
	bViewModeButtonHeld = false;
	bViewModeHoldTriggered = false;
	ViewModeButtonHeldDuration = 0.0f;

	if (!bWasHoldTriggered)
	{
		CycleViewMode();
	}
}

void AAgentCharacter::OnCycleActiveDronePressed()
{
	CycleActiveDrone(1);
}

void AAgentCharacter::OnDroneTorchTogglePressed()
{
	bPlayerDroneTorchEnabled = !bPlayerDroneTorchEnabled;

	for (ADroneCompanion* CandidateDrone : DroneCompanions)
	{
		if (!IsValid(CandidateDrone))
		{
			continue;
		}

		const bool bShouldEnableTorch = CandidateDrone == DroneCompanion && bPlayerDroneTorchEnabled;
		CandidateDrone->SetTorchEnabled(bShouldEnableTorch);
		if (!bShouldEnableTorch)
		{
			CandidateDrone->ClearTorchAimTarget();
		}
	}

	UpdateDroneTorchTarget();
}

void AAgentCharacter::CycleBeamMode(int32 Direction)
{
	const int32 ModeCount = 2;
	const int32 CurrentIndex = static_cast<int32>(CurrentBeamMode);
	const int32 Step = Direction >= 0 ? 1 : -1;
	const int32 WrappedIndex = (CurrentIndex + Step + ModeCount) % ModeCount;
	CurrentBeamMode = static_cast<EAgentBeamMode>(WrappedIndex);
	ApplyBeamModeToEmitters();
}

void AAgentCharacter::ApplyBeamModeToEmitters()
{
	if (PlayerBeamToolComponent)
	{
		PlayerBeamToolComponent->SetBeamMode(CurrentBeamMode);
	}

	for (ADroneCompanion* CandidateDrone : DroneCompanions)
	{
		if (!IsValid(CandidateDrone))
		{
			continue;
		}

		if (UAgentBeamToolComponent* DroneBeamToolComponent = CandidateDrone->GetBeamToolComponent())
		{
			DroneBeamToolComponent->SetBeamMode(CurrentBeamMode);
		}
	}
}

void AAgentCharacter::StopAllBeamTools()
{
	if (PlayerBeamToolComponent)
	{
		PlayerBeamToolComponent->StopBeam();
	}

	for (ADroneCompanion* CandidateDrone : DroneCompanions)
	{
		if (!IsValid(CandidateDrone))
		{
			continue;
		}

		if (UAgentBeamToolComponent* DroneBeamToolComponent = CandidateDrone->GetBeamToolComponent())
		{
			DroneBeamToolComponent->StopBeam();
		}
	}
}

bool AAgentCharacter::IsRawMouseBeamAimModifierHeld() const
{
	return bRightMouseBeamAimHeld;
}

bool AAgentCharacter::IsRawControllerBeamAimModifierHeld() const
{
	return DroneGamepadLeftTriggerInput >= FMath::Clamp(ControllerBeamAimTriggerThreshold, 0.0f, 1.0f);
}

bool AAgentCharacter::IsBeamAimModifierActive() const
{
	return CanUseBeamTool() && (IsRawMouseBeamAimModifierHeld() || IsRawControllerBeamAimModifierHeld());
}

bool AAgentCharacter::IsBeamFireInputHeld() const
{
	return bMouseBeamFireHeld || bControllerBeamFireHeld;
}

bool AAgentCharacter::CanUseBeamTool() const
{
	if (IsRagdolling())
	{
		return false;
	}

	if (bConveyorPlacementModeActive || bMiniMapModeActive)
	{
		return false;
	}

	if (VehicleInteractionComponent && VehicleInteractionComponent->IsControllingVehicle())
	{
		return false;
	}

	if (HeldPickupComponent.IsValid() || bKeyboardPickupHeld || bControllerPickupHeld || bPickupRotationModeActive)
	{
		return false;
	}

	if (bMapModeActive)
	{
		return bAllowBeamInMapMode && bPrimaryDroneAvailable && DroneCompanion && DroneCompanion->GetBeamToolComponent();
	}

	if (CurrentViewMode == EAgentViewMode::DronePilot)
	{
		return bAllowBeamInDronePilotMode && bPrimaryDroneAvailable && DroneCompanion && DroneCompanion->GetBeamToolComponent();
	}

	return (CurrentViewMode == EAgentViewMode::FirstPerson || CurrentViewMode == EAgentViewMode::ThirdPerson)
		&& PlayerBeamToolComponent != nullptr;
}

UAgentBeamToolComponent* AAgentCharacter::ResolveActiveBeamToolComponent() const
{
	if ((bMapModeActive || CurrentViewMode == EAgentViewMode::DronePilot) && DroneCompanion)
	{
		return DroneCompanion->GetBeamToolComponent();
	}

	return PlayerBeamToolComponent;
}

USceneComponent* AAgentCharacter::ResolveActiveBeamOriginComponent() const
{
	if ((bMapModeActive || CurrentViewMode == EAgentViewMode::DronePilot) && DroneCompanion)
	{
		return DroneCompanion->GetBeamOriginComponent();
	}

	return ResolveConfiguredPlayerBeamOriginComponent();
}

bool AAgentCharacter::ResolveActiveBeamPose(
	FVector& OutViewOrigin,
	FVector& OutViewDirection,
	FVector& OutVisualOrigin) const
{
	OutViewOrigin = FVector::ZeroVector;
	OutViewDirection = FVector::ForwardVector;
	OutVisualOrigin = FVector::ZeroVector;

	FRotator ViewRotation = FRotator::ZeroRotator;
	if ((bMapModeActive || CurrentViewMode == EAgentViewMode::DronePilot) && DroneCompanion)
	{
		if (!DroneCompanion->GetDroneCameraTransform(OutViewOrigin, ViewRotation))
		{
			return false;
		}
	}
	else if (!GetCharacterPickupView(OutViewOrigin, ViewRotation))
	{
		return false;
	}

	OutViewDirection = ViewRotation.Vector().GetSafeNormal();
	if (OutViewDirection.IsNearlyZero())
	{
		return false;
	}

	if (const USceneComponent* BeamOriginComponent = ResolveActiveBeamOriginComponent())
	{
		OutVisualOrigin = BeamOriginComponent->GetComponentLocation();
	}
	else
	{
		OutVisualOrigin = OutViewOrigin;
	}

	return true;
}

void AAgentCharacter::UpdateBeamSystems(float DeltaSeconds)
{
	(void)DeltaSeconds;

	const bool bShouldFireBeam = IsBeamAimModifierActive() && IsBeamFireInputHeld();
	if (!bShouldFireBeam)
	{
		StopAllBeamTools();
		return;
	}

	UAgentBeamToolComponent* ActiveBeamToolComponent = ResolveActiveBeamToolComponent();
	if (!ActiveBeamToolComponent)
	{
		StopAllBeamTools();
		return;
	}

	FVector ViewOrigin = FVector::ZeroVector;
	FVector ViewDirection = FVector::ForwardVector;
	FVector VisualOrigin = FVector::ZeroVector;
	if (!ResolveActiveBeamPose(ViewOrigin, ViewDirection, VisualOrigin))
	{
		StopAllBeamTools();
		return;
	}

	if (PlayerBeamToolComponent && PlayerBeamToolComponent != ActiveBeamToolComponent)
	{
		PlayerBeamToolComponent->StopBeam();
	}

	for (ADroneCompanion* CandidateDrone : DroneCompanions)
	{
		if (!IsValid(CandidateDrone))
		{
			continue;
		}

		if (UAgentBeamToolComponent* DroneBeamToolComponent = CandidateDrone->GetBeamToolComponent())
		{
			if (DroneBeamToolComponent != ActiveBeamToolComponent)
			{
				DroneBeamToolComponent->StopBeam();
			}
		}
	}

	ActiveBeamToolComponent->SetBeamMode(CurrentBeamMode);
	ActiveBeamToolComponent->SetBeamPose(ViewOrigin, ViewDirection, VisualOrigin);
	ActiveBeamToolComponent->StartBeam();
}

void AAgentCharacter::UpdateBeamAimZoom(float DeltaSeconds)
{
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	APlayerCameraManager* CameraManager = PlayerController ? PlayerController->PlayerCameraManager : nullptr;
	if (!CameraManager)
	{
		bBeamAimZoomLocked = false;
		BeamAimZoomViewTarget.Reset();
		return;
	}

	const bool bShouldZoom = IsBeamAimModifierActive();
	AActor* CurrentViewTarget = PlayerController->GetViewTarget();
	const bool bViewTargetChanged = BeamAimZoomViewTarget.Get() != CurrentViewTarget;
	if (bShouldZoom && (!bBeamAimZoomLocked || bViewTargetChanged))
	{
		if (bViewTargetChanged && bBeamAimZoomLocked)
		{
			CameraManager->UnlockFOV();
		}

		BeamAimBaseFov = CameraManager->GetFOVAngle();
		BeamAimCurrentFov = BeamAimBaseFov;
		BeamAimZoomViewTarget = CurrentViewTarget;
		bBeamAimZoomLocked = true;
	}

	if (!bBeamAimZoomLocked)
	{
		return;
	}

	const float TargetFov = bShouldZoom
		? FMath::Max(5.0f, BeamAimBaseFov - FMath::Max(0.0f, BeamAimZoomFovReduction))
		: BeamAimBaseFov;
	const float InterpSpeed = bShouldZoom
		? FMath::Max(0.0f, BeamAimZoomInInterpSpeed)
		: FMath::Max(0.0f, BeamAimZoomOutInterpSpeed);
	BeamAimCurrentFov = InterpSpeed > KINDA_SMALL_NUMBER
		? FMath::FInterpTo(BeamAimCurrentFov, TargetFov, DeltaSeconds, InterpSpeed)
		: TargetFov;
	CameraManager->SetFOV(BeamAimCurrentFov);

	if (!bShouldZoom && FMath::IsNearlyEqual(BeamAimCurrentFov, BeamAimBaseFov, 0.05f))
	{
		CameraManager->UnlockFOV();
		bBeamAimZoomLocked = false;
		BeamAimZoomViewTarget.Reset();
	}
}

void AAgentCharacter::OnLeftMouseButtonPressed()
{
	if (IsRawMouseBeamAimModifierHeld() && CanUseBeamTool())
	{
		bMouseBeamFireHeld = true;
		return;
	}

	if (TryToggleHeldBackpackPortalFromPickup())
	{
		return;
	}

	if (TryToggleHeldDronePowerFromPickup())
	{
		return;
	}

	OnConveyorPlacePressed();
}

void AAgentCharacter::OnLeftMouseButtonReleased()
{
	bMouseBeamFireHeld = false;
	StopAllBeamTools();
}

void AAgentCharacter::OnRightMouseButtonPressed()
{
	if (bConveyorPlacementModeActive)
	{
		OnConveyorCancelPressed();
		return;
	}

	bRightMouseBeamAimHeld = true;
}

void AAgentCharacter::OnRightMouseButtonReleased()
{
	bRightMouseBeamAimHeld = false;
	bMouseBeamFireHeld = false;
	StopAllBeamTools();
}

void AAgentCharacter::OnMouseScrollUpPressed()
{
	if (IsRawMouseBeamAimModifierHeld() && CanUseBeamTool())
	{
		CycleBeamMode(1);
	}
}

void AAgentCharacter::OnMouseScrollDownPressed()
{
	if (IsRawMouseBeamAimModifierHeld() && CanUseBeamTool())
	{
		CycleBeamMode(-1);
	}
}

void AAgentCharacter::OnGamepadFaceButtonLeftPressed()
{
	if (TryToggleHeldBackpackPortalFromPickup())
	{
		return;
	}

	if (TryToggleHeldDronePowerFromPickup())
	{
		return;
	}

	OnFactoryPlacementTogglePressed();
}

void AAgentCharacter::OnGamepadFaceButtonRightPressed()
{
	if (bConveyorPlacementModeActive)
	{
		OnConveyorCancelPressed();
		return;
	}

	if (!bEnableControllerSafeModeToggle)
	{
		return;
	}

	OnWalkModifierPressed();
}

void AAgentCharacter::OnGamepadLeftShoulderPressed()
{
	if (IsRawControllerBeamAimModifierHeld() && CanUseBeamTool())
	{
		CycleBeamMode(1);
		return;
	}

	if (bConveyorPlacementModeActive)
	{
		OnConveyorRotateLeftPressed();
		return;
	}

	CycleActiveDrone(1);
}

void AAgentCharacter::UpdateViewModeButtonHold(float DeltaSeconds)
{
	if (IsRagdolling())
	{
		return;
	}

	if (!bViewModeButtonHeld || bViewModeHoldTriggered)
	{
		return;
	}

	ViewModeButtonHeldDuration += FMath::Max(0.0f, DeltaSeconds);
	if (ViewModeButtonHeldDuration < FMath::Max(0.0f, DroneViewHoldTime))
	{
		return;
	}

	bViewModeHoldTriggered = true;
	if (CurrentViewMode != EAgentViewMode::DronePilot)
	{
		ApplyViewMode(EAgentViewMode::DronePilot, true);
	}
}

void AAgentCharacter::CycleViewMode()
{
	if (IsRagdolling() && !bAllowViewModeChangesWhileRagdoll)
	{
		return;
	}

	if (bMiniMapModeActive)
	{
		ExitMiniMapMode();
		return;
	}

	if (bMapModeActive)
	{
		ToggleMapMode();
		return;
	}

	EAgentViewMode NextMode = EAgentViewMode::FirstPerson;
	if (CurrentViewMode == EAgentViewMode::FirstPerson)
	{
		NextMode = EAgentViewMode::ThirdPerson;
	}

	ApplyViewMode(NextMode, true);
}

void AAgentCharacter::ApplyViewMode(EAgentViewMode NewMode, bool bBlend)
{
	StopAllBeamTools();
	bBeamAimZoomLocked = false;
	BeamAimZoomViewTarget.Reset();
	if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
	{
		if (APlayerCameraManager* CameraManager = PlayerController->PlayerCameraManager)
		{
			CameraManager->UnlockFOV();
		}
	}

	if (bConveyorPlacementModeActive && NewMode != CurrentViewMode)
	{
		ExitConveyorPlacementMode();
	}

	if (!bPrimaryDroneAvailable && NewMode != EAgentViewMode::FirstPerson)
	{
		NewMode = EAgentViewMode::FirstPerson;
	}

	bMapModeActive = false;
	bMiniMapModeActive = false;
	bMiniMapViewingDroneCamera = false;
	const EAgentViewMode PreviousViewMode = CurrentViewMode;
	const bool bWasDronePilot = PreviousViewMode == EAgentViewMode::DronePilot;
	if (NewMode != EAgentViewMode::DronePilot)
	{
		ResetCharacterCameraRoll();
	}
	FVector ThirdPersonTargetLocation = GetActorLocation();
	FRotator ThirdPersonTargetRotation = GetControlRotation();
	GetThirdPersonDroneTarget(ThirdPersonTargetLocation, ThirdPersonTargetRotation);
	FVector ThirdPersonSwapStartLocation = ThirdPersonTargetLocation;
	FRotator ThirdPersonSwapStartRotation = ThirdPersonTargetRotation;
	const bool bUseFirstToThirdPersonSwap = NewMode == EAgentViewMode::ThirdPerson
		&& PreviousViewMode == EAgentViewMode::FirstPerson
		&& !bMiniMapModeActive;

	CurrentViewMode = NewMode;
	if (NewMode != EAgentViewMode::ThirdPerson || PreviousViewMode != EAgentViewMode::ThirdPerson)
	{
		ResetThirdPersonCameraChaseState();
	}
	bDroneEntryAssistActive = NewMode == EAgentViewMode::DronePilot && !bWasDronePilot;
	bThirdPersonTransitionActive = false;
	bPendingThirdPersonSwapFromDroneCamera = false;
	ThirdPersonTransitionElapsed = 0.0f;
	bool bAutoCycleToBuddyAfterRoleAssignment = false;

	if (NewMode == EAgentViewMode::ThirdPerson)
	{
		EnsureActiveDroneSelection();
		const bool bAssignedDifferentThirdPersonDrone = ThirdPersonLockedDrone.Get() != DroneCompanion.Get();
		ThirdPersonLockedDrone = DroneCompanion;
		bAutoCycleToBuddyAfterRoleAssignment = bAssignedDifferentThirdPersonDrone;
	}
	else if (PreviousViewMode == EAgentViewMode::ThirdPerson
		&& NewMode == EAgentViewMode::FirstPerson)
	{
		ThirdPersonLockedDrone = nullptr;
	}

	const float BlendTime = bBlend ? ViewBlendTime : 0.0f;
	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	UCharacterMovementComponent* CharacterMovementComponent = GetCharacterMovement();
	UCameraComponent* const ActiveFirstPersonCamera = ResolveFirstPersonCamera();
	RefreshFirstPersonCameraControlRotation();

	if (FirstPersonCamera)
	{
		FirstPersonCamera->SetActive(false);
	}

	if (ActiveFirstPersonCamera && ActiveFirstPersonCamera != FirstPersonCamera)
	{
		ActiveFirstPersonCamera->SetActive(false);
	}

	if (FollowCamera)
	{
		FollowCamera->SetActive(false);
	}

	if (ThirdPersonTransitionCamera)
	{
		ThirdPersonTransitionCamera->SetActive(false);
	}

	if (CharacterMovementComponent)
	{
		if (NewMode == EAgentViewMode::FirstPerson)
		{
			bUseControllerRotationYaw = true;
			CharacterMovementComponent->bOrientRotationToMovement = false;
		}
		else
		{
			bUseControllerRotationYaw = bDefaultUseControllerRotationYaw;
			CharacterMovementComponent->bOrientRotationToMovement = bDefaultOrientRotationToMovement;
		}

		if (NewMode == EAgentViewMode::DronePilot)
		{
			CharacterMovementComponent->StopMovementImmediately();
		}
	}

	switch (NewMode)
	{
	case EAgentViewMode::ThirdPerson:
	{
		EnsureActiveDroneSelection();
		if (bUseFirstToThirdPersonSwap && DroneCompanion)
		{
			DroneCompanion->GetDroneCameraTransform(ThirdPersonSwapStartLocation, ThirdPersonSwapStartRotation);
		}

		if (!bUseFirstToThirdPersonSwap)
		{
			GetThirdPersonDroneTarget(ThirdPersonTargetLocation, ThirdPersonTargetRotation);
			ThirdPersonSwapStartLocation = ThirdPersonTargetLocation;
			ThirdPersonSwapStartRotation = ThirdPersonTargetRotation;
		}
		SetThirdPersonProxyVisible(true);

		if (bUseFirstToThirdPersonSwap && ThirdPersonTransitionCamera)
		{
			if (DroneCompanion)
			{
				DroneCompanion->SetHideStaticMeshesFromOwnerCamera(true);
			}

			if (PlayerController && DroneCompanion)
			{
				// Step 1 of pseudo-drone handoff: instant snap to real drone camera.
				PlayerController->SetViewTargetWithBlend(DroneCompanion.Get(), 0.0f);
			}

			AttachThirdPersonProxyToComponent(ThirdPersonTransitionCamera);
			ThirdPersonTransitionCamera->SetWorldLocationAndRotation(ThirdPersonSwapStartLocation, ThirdPersonSwapStartRotation);
			ThirdPersonTransitionStartLocation = ThirdPersonSwapStartLocation;
			ThirdPersonTransitionStartRotation = ThirdPersonSwapStartRotation;
			bThirdPersonTransitionActive = bBlend && ThirdPersonDroneTransitionDuration > KINDA_SMALL_NUMBER;
			bPendingThirdPersonSwapFromDroneCamera = bThirdPersonTransitionActive;
			ThirdPersonTransitionCamera->SetActive(true);

			if (!bThirdPersonTransitionActive && FollowCamera)
			{
				AttachThirdPersonProxyToComponent(FollowCamera);
				FollowCamera->SetActive(true);
				ThirdPersonTransitionCamera->SetActive(false);
			}
		}
		else
		{
			AttachThirdPersonProxyToComponent(FollowCamera);
			if (FollowCamera)
			{
				FollowCamera->SetActive(true);
			}
		}

		if (PlayerController)
		{
			const bool bDeferCharacterViewTarget = bPendingThirdPersonSwapFromDroneCamera;
			if (!bDeferCharacterViewTarget)
			{
				PlayerController->SetViewTargetWithBlend(this, bUseFirstToThirdPersonSwap ? 0.0f : (DroneCompanion ? 0.0f : BlendTime));
			}
		}

		if (!DroneCompanion && bPrimaryDroneAvailable)
		{
			SpawnDroneCompanion();
		}
		ResetDroneInputState();
		break;
	}
	case EAgentViewMode::DronePilot:
	{
		SetThirdPersonProxyVisible(false);
		EnsureActiveDroneSelection();
		if (PreviousViewMode == EAgentViewMode::ThirdPerson && IsValid(ThirdPersonLockedDrone.Get()))
		{
			const int32 ThirdPersonLockedIndex = DroneCompanions.IndexOfByKey(ThirdPersonLockedDrone.Get());
			if (ThirdPersonLockedIndex != INDEX_NONE)
			{
				SetActiveDroneIndex(ThirdPersonLockedIndex, false, false);
			}
		}

		FVector DroneSpawnLocation = FVector::ZeroVector;
		FRotator DroneSpawnRotation = FRotator::ZeroRotator;
		if (PreviousViewMode == EAgentViewMode::ThirdPerson
			&& ThirdPersonTransitionCamera
			&& ThirdPersonTransitionCamera->IsActive())
		{
			DroneSpawnLocation = ThirdPersonTransitionCamera->GetComponentLocation();
			DroneSpawnRotation = ThirdPersonTransitionCamera->GetComponentRotation();
		}
		else
		{
			GetThirdPersonDroneTarget(DroneSpawnLocation, DroneSpawnRotation);
		}

		if (!DroneCompanion)
		{
			SpawnDroneCompanionAtTransform(DroneSpawnLocation, DroneSpawnRotation, EDroneCompanionMode::PilotControlled, true);
		}

		if (DroneCompanion)
		{
			DroneCompanion->SetHideStaticMeshesFromOwnerCamera(true);
		}

		if (DroneCompanion && PreviousViewMode == EAgentViewMode::ThirdPerson)
		{
			DroneCompanion->StartPilotCameraTransitionFromThirdPerson();
		}

		if (DroneCompanion && PlayerController)
		{
			PlayerController->SetViewTargetWithBlend(
				DroneCompanion.Get(),
				PreviousViewMode == EAgentViewMode::ThirdPerson ? 0.0f : BlendTime);
		}
		break;
	}
	case EAgentViewMode::FirstPerson:
	default:
		SetThirdPersonProxyVisible(false);
		EnsureActiveDroneSelection();
		if (ActiveFirstPersonCamera)
		{
			ActiveFirstPersonCamera->SetActive(true);
		}

		if (PlayerController)
		{
			PlayerController->SetViewTargetWithBlend(this, bWasDronePilot ? 0.0f : BlendTime);
		}

		if (!bPrimaryDroneAvailable)
		{
			ResetDroneInputState();
			break;
		}

		if (!DroneCompanion)
		{
			FVector DroneSpawnLocation = FVector::ZeroVector;
			FRotator DroneSpawnRotation = FRotator::ZeroRotator;
			if (ThirdPersonTransitionCamera && ThirdPersonTransitionCamera->IsActive())
			{
				DroneSpawnLocation = ThirdPersonTransitionCamera->GetComponentLocation();
				DroneSpawnRotation = ThirdPersonTransitionCamera->GetComponentRotation();
			}
			else
			{
				GetThirdPersonDroneTarget(DroneSpawnLocation, DroneSpawnRotation);
			}
			SpawnDroneCompanionAtTransform(DroneSpawnLocation, DroneSpawnRotation, EDroneCompanionMode::BuddyFollow, true);
		}

		ResetDroneInputState();
		break;
	}

	ApplyDroneFleetContext(false, false);
	const bool bThirdPersonLockStillActive = CurrentViewMode == EAgentViewMode::ThirdPerson
		&& ThirdPersonLockedDrone
		&& DroneCompanion == ThirdPersonLockedDrone.Get();
	if (bAutoCycleToBuddyAfterRoleAssignment || bThirdPersonLockStillActive)
	{
		CycleActiveDrone(1);
	}
}

void AAgentCharacter::ApplyDroneAssistState()
{
	if (!DroneCompanion)
	{
		return;
	}

	if (IsRollDronePilotMode())
	{
		DroneCompanion->SetPilotStabilizerEnabled(false);
		DroneCompanion->SetPilotHoverModeEnabled(false);
		return;
	}

	if (RollToFlightAssistTimeRemaining > 0.0f)
	{
		DroneCompanion->SetPilotStabilizerEnabled(true);
		DroneCompanion->SetPilotHoverModeEnabled(true);
		return;
	}

	if (IsFreeFlyDronePilotMode() || IsSpectatorDronePilotMode())
	{
		DroneCompanion->SetPilotStabilizerEnabled(false);
		DroneCompanion->SetPilotHoverModeEnabled(false);
		return;
	}

	DroneCompanion->SetPilotStabilizerEnabled(bDroneEntryAssistActive || bDroneStabilizerEnabled);
	DroneCompanion->SetPilotHoverModeEnabled(bDroneEntryAssistActive || bDroneHoverModeEnabled);
}

void AAgentCharacter::EnterRollTransitionMode(bool bTrackHeldReturn, bool bFromCrashRecovery)
{
	if (!DroneCompanion || !IsDronePilotMode() || bMapModeActive || bMiniMapModeActive)
	{
		return;
	}

	CrashRollRecoveryStableTime = 0.0f;
	bRollModeHoldStartedInFlight = bTrackHeldReturn;
	bCrashRollRecoveryActive = bFromCrashRecovery;
	RollModeJumpHeldDuration = 0.0f;
	SetDronePilotControlMode(EAgentDronePilotControlMode::Roll);
}

void AAgentCharacter::ExitRollTransitionMode(bool bTryJumpLift)
{
	if (!DroneCompanion || !IsDronePilotMode())
	{
		return;
	}

	bRollModeJumpHeld = false;
	bRollModeHoldStartedInFlight = false;
	bCrashRollRecoveryActive = false;
	CrashRollRecoveryStableTime = 0.0f;
	RollModeJumpHeldDuration = 0.0f;

	const EAgentDronePilotControlMode ReturnMode = LastFlightDronePilotControlMode == EAgentDronePilotControlMode::Roll
		? EAgentDronePilotControlMode::Complex
		: LastFlightDronePilotControlMode;
	RollToFlightAssistTimeRemaining = bTryJumpLift ? FMath::Max(0.0f, RollToFlightAssistDuration) : 0.0f;
	SetDronePilotControlMode(ReturnMode);
}

void AAgentCharacter::SyncDroneCompanionControlState(bool bAllowRollControls, bool bSuppressCameraMountRefresh)
{
	if (!DroneCompanion)
	{
		return;
	}

	if (!bPrimaryDroneAvailable)
	{
		DroneCompanion->ResetPilotInputs();
		return;
	}

	DroneCompanion->SetFollowTarget(this);
	DroneCompanion->SetViewReferenceRotation(GetControlRotation());
	if (bSuppressCameraMountRefresh)
	{
		DroneCompanion->SetSuppressCameraMountRefresh(true);
	}

	DroneCompanion->SetUseSimplePilotControls(bUseSimpleDronePilotControls);
	DroneCompanion->SetUseFreeFlyPilotControls(IsFreeFlyDronePilotMode());
	DroneCompanion->SetUseSpectatorPilotControls(IsSpectatorDronePilotMode());
	DroneCompanion->SetUseRollPilotControls(bAllowRollControls && IsRollDronePilotMode());
	DroneCompanion->SetUseCrashRollRecoveryMode(bUseCrashRollRecovery);

	if (bSuppressCameraMountRefresh)
	{
		DroneCompanion->SetSuppressCameraMountRefresh(false);
	}

	ApplyDroneAssistState();
}

void AAgentCharacter::ResetThirdPersonCameraChaseState()
{
	bHasThirdPersonCameraChaseState = false;
	ThirdPersonCameraChaseLocation = FVector::ZeroVector;
	ThirdPersonCameraChaseRotation = FRotator::ZeroRotator;
	if (CameraBoom && !CameraBoom->GetRelativeLocation().IsNearlyZero(0.1f))
	{
		CameraBoom->SetRelativeLocation(FVector::ZeroVector);
	}
}

float AAgentCharacter::ComputeThirdPersonCameraChaseAlpha(float DistanceToTarget) const
{
	const float NearDistance = FMath::Max(0.0f, ThirdPersonCameraChaseNearDistance);
	const float FarDistance = FMath::Max(NearDistance + 1.0f, ThirdPersonCameraChaseFarDistance);
	const float RawAlpha = FMath::GetMappedRangeValueClamped(
		FVector2D(NearDistance, FarDistance),
		FVector2D(0.0f, 1.0f),
		FMath::Max(0.0f, DistanceToTarget));
	const float AlphaExponent = FMath::Max(0.1f, ThirdPersonCameraChaseDistanceExponent);
	return FMath::Pow(RawAlpha, AlphaExponent);
}

FVector AAgentCharacter::ResolveThirdPersonCameraFocusLocation() const
{
	if (!bThirdPersonCameraTargetTracksCharacterMesh)
	{
		return GetActorLocation();
	}

	const USkeletalMeshComponent* MeshComponent = GetMesh();
	if (!MeshComponent)
	{
		return GetActorLocation();
	}

	if (ThirdPersonCameraTrackBoneName != NAME_None
		&& MeshComponent->GetBoneIndex(ThirdPersonCameraTrackBoneName) != INDEX_NONE)
	{
		return MeshComponent->GetBoneLocation(ThirdPersonCameraTrackBoneName);
	}

	return MeshComponent->GetComponentLocation();
}

void AAgentCharacter::UpdateThirdPersonCameraChase(float DeltaSeconds)
{
	(void)DeltaSeconds;

	if (!CameraBoom || !FollowCamera)
	{
		ResetThirdPersonCameraChaseState();
		return;
	}

	const bool bShouldFollowRagdoll =
		bEnableThirdPersonCameraChase
		&& CurrentViewMode == EAgentViewMode::ThirdPerson
		&& IsRagdolling();
	if (!bShouldFollowRagdoll)
	{
		ResetThirdPersonCameraChaseState();
		return;
	}

	const FVector FocusLocation = ResolveThirdPersonCameraFocusLocation();
	const FVector LocalFollowOffset = GetActorTransform().InverseTransformPosition(FocusLocation);
	CameraBoom->SetRelativeLocation(LocalFollowOffset);
	ThirdPersonCameraChaseLocation = FocusLocation;
	ThirdPersonCameraChaseRotation = FollowCamera->GetComponentRotation();
	bHasThirdPersonCameraChaseState = true;
}

void AAgentCharacter::UpdateDroneCompanionThirdPersonTarget()
{
	if (!DroneCompanion)
	{
		return;
	}

	FVector ThirdPersonTargetLocation = FVector::ZeroVector;
	FRotator ThirdPersonTargetRotation = FRotator::ZeroRotator;
	if (GetThirdPersonDroneTarget(ThirdPersonTargetLocation, ThirdPersonTargetRotation))
	{
		DroneCompanion->SetThirdPersonCameraTarget(ThirdPersonTargetLocation, ThirdPersonTargetRotation);
	}
}

void AAgentCharacter::UpdateDroneTorchTarget()
{
	if (!DroneCompanion)
	{
		return;
	}

	const bool bShouldEnableTorchForActiveDrone = bPrimaryDroneAvailable && bPlayerDroneTorchEnabled;
	DroneCompanion->SetTorchEnabled(bShouldEnableTorchForActiveDrone);

	const bool bShouldAimTorch =
		bShouldEnableTorchForActiveDrone
		&& !bMapModeActive
		&& !bMiniMapModeActive
		&& CurrentViewMode == EAgentViewMode::FirstPerson
		&& DroneCompanion->GetCompanionMode() == EDroneCompanionMode::BuddyFollow;
	if (!bShouldAimTorch)
	{
		DroneCompanion->ClearTorchAimTarget();
		return;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	if (!GetCharacterPickupView(ViewLocation, ViewRotation))
	{
		DroneCompanion->ClearTorchAimTarget();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		DroneCompanion->ClearTorchAimTarget();
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DroneTorchAimTrace), false, this);
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(DroneCompanion);

	const float TraceDistance = FMath::Max(100.0f, DroneTorchAimTraceDistance);
	const FVector TraceEnd = ViewLocation + (ViewRotation.Vector() * TraceDistance);
	FHitResult HitResult;
	const bool bHasHit = World->LineTraceSingleByChannel(
		HitResult,
		ViewLocation,
		TraceEnd,
		ECC_Visibility,
		QueryParams);

	DroneCompanion->SetTorchAimTarget(bHasHit ? HitResult.ImpactPoint : TraceEnd);
}

bool AAgentCharacter::GetThirdPersonDroneTarget(FVector& OutLocation, FRotator& OutRotation) const
{
	if (FollowCamera)
	{
		OutLocation = FollowCamera->GetComponentLocation();
		OutRotation = FollowCamera->GetComponentRotation();
		return true;
	}

	OutLocation = GetActorLocation();
	OutRotation = GetControlRotation();
	return true;
}

void AAgentCharacter::DiscoverWorldDrones()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<ADroneCompanion> DroneIt(World); DroneIt; ++DroneIt)
	{
		ADroneCompanion* ExistingDrone = *DroneIt;
		if (!IsValid(ExistingDrone))
		{
			continue;
		}

		DroneCompanions.AddUnique(ExistingDrone);
	}

	SyncSwarmFromDroneFleet();
}

void AAgentCharacter::SyncSwarmFromDroneFleet()
{
	if (!DroneSwarmComponent)
	{
		return;
	}

	DroneSwarmComponent->SyncFromDrones(DroneCompanions);
	DroneSwarmComponent->SetActiveDrone(DroneCompanion);
}

void AAgentCharacter::CleanupInvalidDroneJobLocks()
{
	auto IsDroneValidForJob = [this](const ADroneCompanion* CandidateDrone)
	{
		return IsValid(CandidateDrone)
			&& DroneCompanions.Contains(CandidateDrone)
			&& !CandidateDrone->IsBatteryDepleted()
			&& !CandidateDrone->IsManualPowerOffRequested();
	};

	if (!IsDroneValidForJob(ThirdPersonLockedDrone.Get()))
	{
		ThirdPersonLockedDrone = nullptr;
	}

	if (!IsDroneValidForJob(MiniMapLockedDrone.Get()))
	{
		MiniMapLockedDrone = nullptr;
	}

	for (int32 HookIndex = HookJobLockedDrones.Num() - 1; HookIndex >= 0; --HookIndex)
	{
		ADroneCompanion* HookDrone = HookJobLockedDrones[HookIndex].Get();
		const bool bHookLiftStillActive = HookDrone && HookDrone->IsLiftAssistActive();
		if (IsDroneValidForJob(HookDrone) && bHookLiftStillActive)
		{
			continue;
		}

		// Safety: if a hook-worker becomes invalid (for example battery-flat),
		// force release lift assist so carried payload drops immediately.
		if (HookDrone && HookDrone->IsLiftAssistActive())
		{
			HookDrone->StopLiftAssist();
		}

		if (DroneSwarmComponent && HookDrone)
		{
			DroneSwarmComponent->SetHookWorker(HookDrone, false);
		}

		HookJobLockedDrones.RemoveAtSwap(HookIndex);
	}

	if (!IsDroneInHookJobLock(LastHookJobLockedDrone.Get()))
	{
		LastHookJobLockedDrone = HookJobLockedDrones.Num() > 0
			? HookJobLockedDrones.Last()
			: nullptr;
	}
}

bool AAgentCharacter::IsDroneInHookJobLock(const ADroneCompanion* CandidateDrone) const
{
	if (!IsValid(CandidateDrone))
	{
		return false;
	}

	for (const TObjectPtr<ADroneCompanion>& HookDrone : HookJobLockedDrones)
	{
		if (HookDrone.Get() == CandidateDrone)
		{
			return true;
		}
	}

	return false;
}

bool AAgentCharacter::IsDroneLockedToPersistentJob(const ADroneCompanion* CandidateDrone) const
{
	if (!IsValid(CandidateDrone))
	{
		return false;
	}

	return CandidateDrone == ThirdPersonLockedDrone.Get()
		|| CandidateDrone == MiniMapLockedDrone.Get()
		|| IsDroneInHookJobLock(CandidateDrone);
}

EDroneSwarmRoleSlot AAgentCharacter::ResolveDesiredActiveSwarmRole() const
{
	if (!bPrimaryDroneAvailable || !DroneCompanion)
	{
		return EDroneSwarmRoleSlot::None;
	}

	if (bMiniMapModeActive)
	{
		return EDroneSwarmRoleSlot::MiniMap;
	}

	if (bMapModeActive)
	{
		return EDroneSwarmRoleSlot::Map;
	}

	if (CurrentViewMode == EAgentViewMode::DronePilot)
	{
		return EDroneSwarmRoleSlot::Pilot;
	}

	if (CurrentViewMode == EAgentViewMode::ThirdPerson)
	{
		return EDroneSwarmRoleSlot::ThirdPersonCamera;
	}

	return EDroneSwarmRoleSlot::Buddy;
}

void AAgentCharacter::RefreshSwarmRoleAssignments()
{
	if (!DroneSwarmComponent)
	{
		return;
	}

	CleanupInvalidDroneJobLocks();

	DroneSwarmComponent->SetActiveDrone(DroneCompanion);
	DroneSwarmComponent->ClearRole(EDroneSwarmRoleSlot::Buddy);
	DroneSwarmComponent->ClearRole(EDroneSwarmRoleSlot::ThirdPersonCamera);
	DroneSwarmComponent->ClearRole(EDroneSwarmRoleSlot::MiniMap);
	DroneSwarmComponent->ClearRole(EDroneSwarmRoleSlot::Pilot);
	DroneSwarmComponent->ClearRole(EDroneSwarmRoleSlot::Map);
	DroneSwarmComponent->ClearHookWorkers();

	const bool bThirdPersonRoleActive = CurrentViewMode == EAgentViewMode::ThirdPerson;
	if (bThirdPersonRoleActive && ThirdPersonLockedDrone)
	{
		DroneSwarmComponent->AssignRole(EDroneSwarmRoleSlot::ThirdPersonCamera, ThirdPersonLockedDrone.Get());
	}

	if (MiniMapLockedDrone)
	{
		DroneSwarmComponent->AssignRole(EDroneSwarmRoleSlot::MiniMap, MiniMapLockedDrone.Get());
	}

	for (ADroneCompanion* HookDrone : HookJobLockedDrones)
	{
		if (!IsValid(HookDrone))
		{
			continue;
		}

		DroneSwarmComponent->SetHookWorker(HookDrone, true);
	}

	if (DroneCompanion)
	{
		const EDroneSwarmRoleSlot ActiveRole = ResolveDesiredActiveSwarmRole();
		switch (ActiveRole)
		{
		case EDroneSwarmRoleSlot::ThirdPersonCamera:
			if (!ThirdPersonLockedDrone)
			{
				ThirdPersonLockedDrone = DroneCompanion;
			}
			if (ThirdPersonLockedDrone == DroneCompanion)
			{
				DroneSwarmComponent->AssignRole(EDroneSwarmRoleSlot::ThirdPersonCamera, ThirdPersonLockedDrone.Get());
			}
			else
			{
				DroneSwarmComponent->AssignRole(EDroneSwarmRoleSlot::Buddy, DroneCompanion);
			}
			break;
		case EDroneSwarmRoleSlot::MiniMap:
			if (!MiniMapLockedDrone)
			{
				MiniMapLockedDrone = DroneCompanion;
			}
			if (MiniMapLockedDrone == DroneCompanion)
			{
				DroneSwarmComponent->AssignRole(EDroneSwarmRoleSlot::MiniMap, MiniMapLockedDrone.Get());
			}
			else
			{
				DroneSwarmComponent->AssignRole(EDroneSwarmRoleSlot::Buddy, DroneCompanion);
			}
			break;
		case EDroneSwarmRoleSlot::None:
			break;
		default:
			DroneSwarmComponent->AssignRole(ActiveRole, DroneCompanion);
			break;
		}
	}

	if (!bAutoAssignInactiveDronesAsHookWorkers)
	{
		return;
	}

	for (ADroneCompanion* CandidateDrone : DroneCompanions)
	{
		if (!IsValid(CandidateDrone) || CandidateDrone == DroneCompanion)
		{
			continue;
		}

		const bool bCanWorkHookRole =
			!CandidateDrone->IsBatteryDepleted()
			&& !CandidateDrone->IsManualPowerOffRequested();
		if (bCanWorkHookRole)
		{
			DroneSwarmComponent->SetHookWorker(CandidateDrone, true);
		}
	}
}

EDroneCompanionMode AAgentCharacter::ResolveCompanionModeForRole(EDroneSwarmRoleSlot InRole) const
{
	switch (InRole)
	{
	case EDroneSwarmRoleSlot::ThirdPersonCamera:
		return EDroneCompanionMode::ThirdPersonFollow;
	case EDroneSwarmRoleSlot::MiniMap:
		return EDroneCompanionMode::MiniMapFollow;
	case EDroneSwarmRoleSlot::Pilot:
		return EDroneCompanionMode::PilotControlled;
	case EDroneSwarmRoleSlot::Map:
		return EDroneCompanionMode::MapMode;
	case EDroneSwarmRoleSlot::Buddy:
		return EDroneCompanionMode::BuddyFollow;
	case EDroneSwarmRoleSlot::HookWorker:
		return EDroneCompanionMode::BuddyFollow;
	case EDroneSwarmRoleSlot::None:
	default:
		return EDroneCompanionMode::Idle;
	}
}

void AAgentCharacter::SpawnInitialDroneFleet()
{
	CleanupInvalidDroneCompanions();
	DiscoverWorldDrones();

	int32 ClosestDroneIndex = INDEX_NONE;
	float ClosestDistanceSq = TNumericLimits<float>::Max();
	const FVector CharacterLocation = GetActorLocation();
	for (int32 DroneIndex = 0; DroneIndex < DroneCompanions.Num(); ++DroneIndex)
	{
		ADroneCompanion* CandidateDrone = DroneCompanions[DroneIndex];
		if (!IsValid(CandidateDrone))
		{
			continue;
		}

		const float DistanceSq = FVector::DistSquared(CharacterLocation, CandidateDrone->GetActorLocation());
		if (DistanceSq < ClosestDistanceSq)
		{
			ClosestDistanceSq = DistanceSq;
			ClosestDroneIndex = DroneIndex;
		}
	}

	if (ClosestDroneIndex != INDEX_NONE)
	{
		SetActiveDroneIndex(ClosestDroneIndex, false, false);
	}
	else if (bPrimaryDroneAvailable)
	{
		const FRotator SpawnYawRotation(0.0f, GetControlRotation().Yaw, 0.0f);
		const FVector SpawnLocation = GetActorLocation() + FRotationMatrix(SpawnYawRotation).TransformVector(DroneSpawnOffset);
		const EDroneCompanionMode InitialMode = CurrentViewMode == EAgentViewMode::DronePilot
			? EDroneCompanionMode::PilotControlled
			: (CurrentViewMode == EAgentViewMode::ThirdPerson
				? EDroneCompanionMode::ThirdPersonFollow
				: EDroneCompanionMode::BuddyFollow);
		SpawnDroneCompanionAtTransform(SpawnLocation, SpawnYawRotation, InitialMode, true);
	}

	ApplyDroneFleetContext(false, false);
	RefreshPrimaryDroneAvailabilityFromCompanion();
}

void AAgentCharacter::SpawnDroneCompanion()
{
	CleanupInvalidDroneCompanions();
	EnsureActiveDroneSelection();
	if (DroneCompanion || !bPrimaryDroneAvailable)
	{
		return;
	}

	const FRotator SpawnYawRotation(0.0f, GetControlRotation().Yaw, 0.0f);
	const FVector SpawnLocation = GetActorLocation() + FRotationMatrix(SpawnYawRotation).TransformVector(DroneSpawnOffset);
	const EDroneCompanionMode InitialMode = CurrentViewMode == EAgentViewMode::DronePilot
		? EDroneCompanionMode::PilotControlled
		: EDroneCompanionMode::BuddyFollow;
	SpawnDroneCompanionAtTransform(SpawnLocation, SpawnYawRotation, InitialMode, true);
}

ADroneCompanion* AAgentCharacter::SpawnDroneCompanionAtTransform(
	const FVector& SpawnLocation,
	const FRotator& SpawnRotation,
	EDroneCompanionMode InitialMode,
	bool bMakeActive)
{
	if (!GetWorld())
	{
		return nullptr;
	}

	UClass* SpawnClass = DroneCompanionClass.Get();
	if (!SpawnClass)
	{
		SpawnClass = ADroneCompanion::StaticClass();
	}
	if (!SpawnClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters{};
	SpawnParameters.Owner = this;
	SpawnParameters.Instigator = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ADroneCompanion* SpawnedDrone = GetWorld()->SpawnActor<ADroneCompanion>(SpawnClass, SpawnLocation, SpawnRotation, SpawnParameters);
	if (!SpawnedDrone)
	{
		return nullptr;
	}

	SpawnedDrone->SetActorScale3D(FVector::OneVector);
	SpawnedDrone->SetBatteryPercent(PersistedPrimaryDroneBatteryPercent);
	SpawnedDrone->SetFollowTarget(this);
	SpawnedDrone->SetViewReferenceRotation(GetControlRotation());
	SpawnedDrone->SetUseSimplePilotControls(bUseSimpleDronePilotControls);
	SpawnedDrone->SetUseFreeFlyPilotControls(IsFreeFlyDronePilotMode());
	SpawnedDrone->SetUseSpectatorPilotControls(IsSpectatorDronePilotMode());
	SpawnedDrone->SetUseRollPilotControls(false);
	SpawnedDrone->SetUseCrashRollRecoveryMode(bUseCrashRollRecovery);

	DroneCompanions.Add(SpawnedDrone);
	SyncSwarmFromDroneFleet();

	if (bMakeActive || !DroneCompanion)
	{
		SetActiveDroneIndex(DroneCompanions.Num() - 1, false, false);
	}
	else
	{
		SpawnedDrone->SetCompanionMode(InitialMode);
	}

	return SpawnedDrone;
}

void AAgentCharacter::DespawnDroneCompanion()
{
	if (!DroneCompanion)
	{
		return;
	}

	PersistedPrimaryDroneBatteryPercent = FMath::Clamp(DroneCompanion->GetBatteryPercent(), 0.0f, 100.0f);

	DroneCompanions.Remove(DroneCompanion);
	SyncSwarmFromDroneFleet();
	DroneCompanion->Destroy();
	DroneCompanion = nullptr;
	ActiveDroneIndex = INDEX_NONE;
	EnsureActiveDroneSelection();
	RefreshPrimaryDroneAvailabilityFromCompanion();
	ApplyDroneFleetContext(true, false);
}

void AAgentCharacter::CleanupInvalidDroneCompanions()
{
	DroneCompanions.RemoveAll(
		[](const TObjectPtr<ADroneCompanion>& Drone)
		{
			return !IsValid(Drone);
		});

	if (DroneCompanion && !IsValid(DroneCompanion))
	{
		DroneCompanion = nullptr;
	}

	if (DroneCompanion)
	{
		ActiveDroneIndex = DroneCompanions.IndexOfByKey(DroneCompanion);
		if (ActiveDroneIndex == INDEX_NONE)
		{
			DroneCompanion = nullptr;
		}
	}
	else if (DroneCompanions.IsValidIndex(ActiveDroneIndex) && IsValid(DroneCompanions[ActiveDroneIndex]))
	{
		DroneCompanion = DroneCompanions[ActiveDroneIndex];
	}
	else
	{
		ActiveDroneIndex = INDEX_NONE;
	}

	CleanupInvalidDroneJobLocks();
	SyncSwarmFromDroneFleet();
}

void AAgentCharacter::EnsureActiveDroneSelection()
{
	CleanupInvalidDroneCompanions();
	if (DroneCompanion)
	{
		return;
	}

	const int32 AvailableIndex = FindNextDroneIndex(INDEX_NONE, 1, true, true);
	const int32 FallbackIndex = AvailableIndex != INDEX_NONE
		? AvailableIndex
		: FindNextDroneIndex(INDEX_NONE, 1, false, true);
	const int32 AnyDroneFallbackIndex = FallbackIndex != INDEX_NONE
		? FallbackIndex
		: FindNextDroneIndex(INDEX_NONE, 1, false, false);
	if (!DroneCompanions.IsValidIndex(AnyDroneFallbackIndex))
	{
		return;
	}

	ActiveDroneIndex = AnyDroneFallbackIndex;
	DroneCompanion = DroneCompanions[AnyDroneFallbackIndex];
}

bool AAgentCharacter::IsDroneAvailableForActivation(const ADroneCompanion* CandidateDrone) const
{
	if (!IsValid(CandidateDrone))
	{
		return false;
	}

	return !CandidateDrone->IsBatteryDepleted()
		&& !CandidateDrone->IsManualPowerOffRequested();
}

int32 AAgentCharacter::FindNextDroneIndex(
	int32 StartIndex,
	int32 Direction,
	bool bPreferAvailable,
	bool bSkipJobLockedDrones) const
{
	const int32 DroneCount = DroneCompanions.Num();
	if (DroneCount <= 0)
	{
		return INDEX_NONE;
	}

	const int32 SafeDirection = Direction < 0 ? -1 : 1;
	for (int32 Step = 1; Step <= DroneCount; ++Step)
	{
		int32 CandidateIndex = INDEX_NONE;
		if (StartIndex == INDEX_NONE)
		{
			CandidateIndex = SafeDirection > 0 ? (Step - 1) : (DroneCount - Step);
		}
		else
		{
			CandidateIndex = (StartIndex + (SafeDirection * Step)) % DroneCount;
			if (CandidateIndex < 0)
			{
				CandidateIndex += DroneCount;
			}
		}

		if (!DroneCompanions.IsValidIndex(CandidateIndex))
		{
			continue;
		}

		const ADroneCompanion* CandidateDrone = DroneCompanions[CandidateIndex];
		if (!IsValid(CandidateDrone))
		{
			continue;
		}

		if (bSkipJobLockedDrones && IsDroneLockedToPersistentJob(CandidateDrone))
		{
			continue;
		}

		if (!bPreferAvailable || IsDroneAvailableForActivation(CandidateDrone))
		{
			return CandidateIndex;
		}
	}

	return INDEX_NONE;
}

bool AAgentCharacter::SetActiveDroneIndex(int32 NewActiveIndex, bool bUpdateViewTarget, bool bBlendViewTarget)
{
	CleanupInvalidDroneCompanions();
	if (!DroneCompanions.IsValidIndex(NewActiveIndex) || !IsValid(DroneCompanions[NewActiveIndex]))
	{
		return false;
	}

	ADroneCompanion* PreviousDrone = DroneCompanion;
	ActiveDroneIndex = NewActiveIndex;
	DroneCompanion = DroneCompanions[NewActiveIndex];
	if (DroneSwarmComponent)
	{
		DroneSwarmComponent->SetActiveDrone(DroneCompanion);
	}

	if (PreviousDrone && PreviousDrone != DroneCompanion)
	{
		if (!IsDroneLockedToPersistentJob(PreviousDrone))
		{
			PreviousDrone->StopLiftAssist();
			PreviousDrone->ResetPilotInputs();
			PreviousDrone->SetActorEnableCollision(true);
			PreviousDrone->SetCompanionMode(EDroneCompanionMode::Idle);
		}
		else
		{
			PreviousDrone->ResetPilotInputs();
		}
	}

	RefreshPrimaryDroneAvailabilityFromCompanion();
	ApplyDroneFleetContext(bUpdateViewTarget, bBlendViewTarget);
	return true;
}

void AAgentCharacter::CycleActiveDrone(int32 Direction)
{
	CleanupInvalidDroneCompanions();
	if (DroneCompanions.Num() <= 1)
	{
		return;
	}

	const int32 StartIndex = DroneCompanion
		? ActiveDroneIndex
		: INDEX_NONE;
	const int32 NextIndex = FindNextDroneIndex(StartIndex, Direction, true, true);

	if (NextIndex == INDEX_NONE || NextIndex == ActiveDroneIndex)
	{
		return;
	}

	SetActiveDroneIndex(NextIndex, true, true);
}

void AAgentCharacter::ApplyDroneFleetContext(bool bUpdateViewTarget, bool bBlendViewTarget)
{
	CleanupInvalidDroneCompanions();
	SyncSwarmFromDroneFleet();
	RefreshSwarmRoleAssignments();

	for (ADroneCompanion* CandidateDrone : DroneCompanions)
	{
		if (!IsValid(CandidateDrone))
		{
			continue;
		}

		const bool bIsActiveDrone = CandidateDrone == DroneCompanion;
		const bool bThirdPersonRoleActive = CurrentViewMode == EAgentViewMode::ThirdPerson;
		EDroneSwarmRoleSlot AssignedRole = DroneSwarmComponent
			? DroneSwarmComponent->GetAssignedRoleForDrone(CandidateDrone)
			: EDroneSwarmRoleSlot::None;

		if (bIsActiveDrone && CurrentViewMode == EAgentViewMode::DronePilot)
		{
			AssignedRole = EDroneSwarmRoleSlot::Pilot;
		}
		else if (bThirdPersonRoleActive && CandidateDrone == ThirdPersonLockedDrone.Get())
		{
			AssignedRole = EDroneSwarmRoleSlot::ThirdPersonCamera;
		}
		else if (CandidateDrone == MiniMapLockedDrone.Get())
		{
			AssignedRole = EDroneSwarmRoleSlot::MiniMap;
		}
		else if (IsDroneInHookJobLock(CandidateDrone))
		{
			AssignedRole = EDroneSwarmRoleSlot::HookWorker;
		}
		else if (bIsActiveDrone && AssignedRole == EDroneSwarmRoleSlot::None)
		{
			const EDroneSwarmRoleSlot DesiredActiveRole = ResolveDesiredActiveSwarmRole();
			const bool bRoleLockedToAnotherDrone =
				(DesiredActiveRole == EDroneSwarmRoleSlot::ThirdPersonCamera
					&& ThirdPersonLockedDrone
					&& ThirdPersonLockedDrone.Get() != CandidateDrone)
				|| (DesiredActiveRole == EDroneSwarmRoleSlot::MiniMap
					&& MiniMapLockedDrone
					&& MiniMapLockedDrone.Get() != CandidateDrone);
			AssignedRole = bRoleLockedToAnotherDrone
				? EDroneSwarmRoleSlot::Buddy
				: DesiredActiveRole;
		}

		EDroneCompanionMode DesiredMode = ResolveCompanionModeForRole(AssignedRole);
		if (!bIsActiveDrone && AssignedRole == EDroneSwarmRoleSlot::None)
		{
			DesiredMode = EDroneCompanionMode::Idle;
		}
		if (bIsActiveDrone && !bPrimaryDroneAvailable)
		{
			DesiredMode = EDroneCompanionMode::Idle;
		}

		if (bIsActiveDrone)
		{
			SyncDroneCompanionControlState(IsDronePilotMode() && !bMapModeActive, bMapModeActive || bMiniMapModeActive);
			UpdateThirdPersonCameraChase(0.0f);
			UpdateDroneCompanionThirdPersonTarget();
		}
		else
		{
			CandidateDrone->ResetPilotInputs();
		}

		const bool bEnableDroneTorch = bIsActiveDrone && bPrimaryDroneAvailable && bPlayerDroneTorchEnabled;
		CandidateDrone->SetTorchEnabled(bEnableDroneTorch);
		if (!bEnableDroneTorch
			|| CurrentViewMode != EAgentViewMode::FirstPerson
			|| DesiredMode != EDroneCompanionMode::BuddyFollow)
		{
			CandidateDrone->ClearTorchAimTarget();
		}

		const bool bShadowOnlyDroneMeshes =
			bIsActiveDrone
			&& (CurrentViewMode == EAgentViewMode::ThirdPerson || CurrentViewMode == EAgentViewMode::DronePilot);
		CandidateDrone->SetHideStaticMeshesFromOwnerCamera(bShadowOnlyDroneMeshes);

		if (DesiredMode == EDroneCompanionMode::ThirdPersonFollow)
		{
			FVector ThirdPersonTargetLocation = FVector::ZeroVector;
			FRotator ThirdPersonTargetRotation = FRotator::ZeroRotator;
			GetThirdPersonDroneTarget(ThirdPersonTargetLocation, ThirdPersonTargetRotation);
			CandidateDrone->SetThirdPersonCameraTarget(ThirdPersonTargetLocation, ThirdPersonTargetRotation);
			CandidateDrone->TeleportDroneToTransform(ThirdPersonTargetLocation, ThirdPersonTargetRotation);
			CandidateDrone->SetActorEnableCollision(false);
		}
		else
		{
			CandidateDrone->SetActorEnableCollision(true);
			if (DesiredMode == EDroneCompanionMode::MapMode)
			{
				CandidateDrone->SetNextMapModeUsesEntryLift(CurrentViewMode != EAgentViewMode::DronePilot);
			}
		}

		// Keep only explicitly hook-locked drones carrying with lift assist.
		if (!IsDroneInHookJobLock(CandidateDrone) && CandidateDrone->IsLiftAssistActive())
		{
			CandidateDrone->StopLiftAssist();
		}

		CandidateDrone->SetCompanionMode(DesiredMode);
	}

	if (!bUpdateViewTarget)
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return;
	}

	const float BlendTime = bBlendViewTarget ? ViewBlendTime : 0.0f;
	if (!DroneCompanion || !bPrimaryDroneAvailable)
	{
		PlayerController->SetViewTargetWithBlend(this, BlendTime);
		return;
	}

	ADroneCompanion* MiniMapViewDrone = MiniMapLockedDrone ? MiniMapLockedDrone.Get() : DroneCompanion.Get();
	if (bMapModeActive || (bMiniMapModeActive && bMiniMapViewingDroneCamera) || CurrentViewMode == EAgentViewMode::DronePilot)
	{
		PlayerController->SetViewTargetWithBlend(
			bMiniMapModeActive && bMiniMapViewingDroneCamera && MiniMapViewDrone
				? MiniMapViewDrone
				: DroneCompanion.Get(),
			BlendTime);
	}
	else
	{
		PlayerController->SetViewTargetWithBlend(this, BlendTime);
	}
}

void AAgentCharacter::UpdateThirdPersonTransition(float DeltaSeconds)
{
	if (!bThirdPersonTransitionActive || !ThirdPersonTransitionCamera || !FollowCamera)
	{
		return;
	}

	ThirdPersonTransitionElapsed = FMath::Min(
		ThirdPersonTransitionElapsed + FMath::Max(0.0f, DeltaSeconds),
		FMath::Max(KINDA_SMALL_NUMBER, ThirdPersonDroneTransitionDuration));
	const float RawAlpha = ThirdPersonTransitionElapsed / FMath::Max(KINDA_SMALL_NUMBER, ThirdPersonDroneTransitionDuration);
	const float Alpha = FMath::InterpEaseInOut(0.0f, 1.0f, RawAlpha, 2.0f);
	const FVector TargetLocation = FollowCamera->GetComponentLocation();
	const FQuat StartRotation = ThirdPersonTransitionStartRotation.Quaternion();
	const FQuat TargetRotation = FollowCamera->GetComponentRotation().Quaternion();

	ThirdPersonTransitionCamera->SetWorldLocationAndRotation(
		FMath::Lerp(ThirdPersonTransitionStartLocation, TargetLocation, Alpha),
		FQuat::Slerp(StartRotation, TargetRotation, Alpha));

	if (RawAlpha >= 1.0f)
	{
		bThirdPersonTransitionActive = false;
		AttachThirdPersonProxyToComponent(FollowCamera);
		FollowCamera->SetActive(true);
		ThirdPersonTransitionCamera->SetActive(false);
	}
}

void AAgentCharacter::SetThirdPersonProxyVisible(bool bVisible)
{
	if (!ThirdPersonDroneProxyMesh)
	{
		return;
	}

	const bool bShadowOnlyInThirdPerson = bVisible && CurrentViewMode == EAgentViewMode::ThirdPerson;
	const bool bShouldRenderProxy = bVisible && !bShadowOnlyInThirdPerson;
	const bool bShouldCastShadow = bVisible;

	// Keep the proxy hidden in third-person while still allowing it to cast hidden shadow.
	ThirdPersonDroneProxyMesh->SetOwnerNoSee(true);
	ThirdPersonDroneProxyMesh->SetHiddenInGame(!bShouldRenderProxy, true);
	ThirdPersonDroneProxyMesh->SetVisibility(bShouldRenderProxy, true);
	ThirdPersonDroneProxyMesh->SetCastShadow(bShouldCastShadow);
	ThirdPersonDroneProxyMesh->bCastHiddenShadow = bShouldCastShadow;
}

void AAgentCharacter::AttachThirdPersonProxyToComponent(USceneComponent* AttachmentParent)
{
	if (!ThirdPersonDroneProxyMesh || !AttachmentParent)
	{
		return;
	}

	ThirdPersonDroneProxyMesh->AttachToComponent(
		AttachmentParent,
		FAttachmentTransformRules::SnapToTargetIncludingScale);
	ThirdPersonDroneProxyMesh->SetRelativeLocation(FVector::ZeroVector);
	ThirdPersonDroneProxyMesh->SetRelativeRotation(FRotator::ZeroRotator);
}

bool AAgentCharacter::CanUseConveyorPlacementMode() const
{
	return !IsRagdolling()
		&& !bMapModeActive
		&& !bMiniMapModeActive
		&& (CurrentViewMode == EAgentViewMode::ThirdPerson || CurrentViewMode == EAgentViewMode::FirstPerson);
}

bool AAgentCharacter::CanUseCharacterInteraction() const
{
	return !IsRagdolling()
		&& !bMapModeActive
		&& !bMiniMapModeActive
		&& !bConveyorPlacementModeActive
		&& (CurrentViewMode == EAgentViewMode::ThirdPerson || CurrentViewMode == EAgentViewMode::FirstPerson);
}

void AAgentCharacter::ToggleConveyorPlacementMode()
{
	if (bConveyorPlacementModeActive)
	{
		ExitConveyorPlacementMode();
		return;
	}

	EnterConveyorPlacementMode();
}

void AAgentCharacter::EnterConveyorPlacementMode()
{
	if (bConveyorPlacementModeActive)
	{
		return;
	}

	if (!CanUseConveyorPlacementMode())
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				static_cast<uint64>(GetUniqueID()) + 18000ULL,
				1.5f,
				FColor::Yellow,
				TEXT("Factory placement only works in first and third person."));
		}
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UClass* PreviewClass = ConveyorPlacementPreviewClass.Get();
	if (!PreviewClass)
	{
		PreviewClass = AConveyorPlacementPreview::StaticClass();
	}

	FActorSpawnParameters SpawnParameters{};
	SpawnParameters.Owner = this;
	SpawnParameters.Instigator = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ConveyorPlacementPreview = World->SpawnActor<AConveyorPlacementPreview>(
		PreviewClass,
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParameters);

	bConveyorPlacementModeActive = ConveyorPlacementPreview != nullptr;
	bConveyorPlacementValid = false;
	bFactoryFreePlacementEnabled = false;
	bPlacementRotateLeftHeld = false;
	bPlacementRotateRightHeld = false;
	FactoryPlacementRotationYawDegrees = 0.0f;
	FactoryPlacementRotationHoldTimeRemaining = 0.0f;
	FactoryPlacementRotationHoldDirection = 0;
	PendingConveyorPlacementLocation = FVector::ZeroVector;
	PendingConveyorPlacementRotation = FRotator::ZeroRotator;

	if (ConveyorPlacementPreview)
	{
		ConveyorPlacementPreview->SetActorHiddenInGame(true);
		ConveyorPlacementPreview->SetPreviewActorClass(ResolveFactoryBuildableClass(CurrentFactoryPlacementType));
	}

	UpdateConveyorPlacementPreview();
}

void AAgentCharacter::ExitConveyorPlacementMode()
{
	bConveyorPlacementModeActive = false;
	bConveyorPlacementValid = false;
	bFactoryFreePlacementEnabled = false;
	bPlacementRotateLeftHeld = false;
	bPlacementRotateRightHeld = false;
	FactoryPlacementRotationYawDegrees = 0.0f;
	FactoryPlacementRotationHoldTimeRemaining = 0.0f;
	FactoryPlacementRotationHoldDirection = 0;
	PendingConveyorPlacementLocation = FVector::ZeroVector;
	PendingConveyorPlacementRotation = FRotator::ZeroRotator;

	if (ConveyorPlacementPreview)
	{
		ConveyorPlacementPreview->Destroy();
		ConveyorPlacementPreview = nullptr;
	}
}

void AAgentCharacter::UpdateConveyorPlacementPreview()
{
	if (!bConveyorPlacementModeActive || !ConveyorPlacementPreview)
	{
		return;
	}

	FVector PreviewLocation = PendingConveyorPlacementLocation;
	FRotator PreviewRotation = PendingConveyorPlacementRotation;
	bool bPlacementIsValid = false;
	const bool bHasPreviewLocation = EvaluateConveyorPlacement(PreviewLocation, PreviewRotation, bPlacementIsValid);

	bConveyorPlacementValid = bHasPreviewLocation && bPlacementIsValid;

	if (!bHasPreviewLocation)
	{
		ConveyorPlacementPreview->SetActorHiddenInGame(true);
		return;
	}

	PendingConveyorPlacementLocation = PreviewLocation;
	PendingConveyorPlacementRotation = PreviewRotation;

	ConveyorPlacementPreview->SetActorHiddenInGame(false);
	ConveyorPlacementPreview->SetActorLocationAndRotation(PendingConveyorPlacementLocation, PendingConveyorPlacementRotation);
	ConveyorPlacementPreview->SetPlacementState(bConveyorPlacementValid);

	if (UWorld* World = GetWorld())
	{
		const FColor DebugColor = bConveyorPlacementValid ? FColor::Green : FColor::Red;
		const FVector BoxCenter = PendingConveyorPlacementLocation + FVector(0.0f, 0.0f, 20.0f + ConveyorPlacementClearanceExtents.Z);
		DrawDebugBox(
			World,
			BoxCenter,
			ConveyorPlacementClearanceExtents,
			PendingConveyorPlacementRotation.Quaternion(),
			DebugColor,
			false,
			0.0f,
			0,
			1.5f);

		const FVector ArrowStart = PendingConveyorPlacementLocation + FVector(0.0f, 0.0f, 50.0f);
		const FVector ArrowEnd = ArrowStart + PendingConveyorPlacementRotation.Vector() * 70.0f;
		DrawDebugDirectionalArrow(
			World,
			ArrowStart,
			ArrowEnd,
			25.0f,
			DebugColor,
			false,
			0.0f,
			0,
			2.0f);
	}
}

UCameraComponent* AAgentCharacter::GetActivePlacementCamera() const
{
	if (CurrentViewMode == EAgentViewMode::FirstPerson)
	{
		if (UCameraComponent* const ActiveFirstPersonCamera = ResolveFirstPersonCamera())
		{
			return ActiveFirstPersonCamera;
		}
	}

	if (CurrentViewMode == EAgentViewMode::ThirdPerson)
	{
		if (ThirdPersonTransitionCamera && ThirdPersonTransitionCamera->IsActive())
		{
			return ThirdPersonTransitionCamera;
		}

		if (FollowCamera)
		{
			return FollowCamera;
		}
	}

	return nullptr;
}

UClass* AAgentCharacter::ResolveFactoryBuildableClass(EAgentFactoryPlacementType PlacementType) const
{
	switch (PlacementType)
	{
	case EAgentFactoryPlacementType::StorageBin:
		return StorageBinClass.Get() ? StorageBinClass.Get() : AStorageBin::StaticClass();
	case EAgentFactoryPlacementType::Machine:
		return MachineClass.Get() ? MachineClass.Get() : AMachineActor::StaticClass();
	case EAgentFactoryPlacementType::Miner:
		return MinerClass.Get() ? MinerClass.Get() : AMinerActor::StaticClass();
	case EAgentFactoryPlacementType::MiningSwarm:
		return MiningSwarmClass.Get() ? MiningSwarmClass.Get() : AMiningSwarmMachine::StaticClass();
	case EAgentFactoryPlacementType::Conveyor:
	default:
		return ConveyorBeltClass.Get() ? ConveyorBeltClass.Get() : AConveyorBeltStraight::StaticClass();
	}
}

void AAgentCharacter::TryPlaceConveyor()
{
	if (!bConveyorPlacementModeActive)
	{
		return;
	}

	FVector SpawnLocation = FVector::ZeroVector;
	FRotator SpawnRotation = FRotator::ZeroRotator;
	bool bPlacementIsValid = false;
	const bool bHasPreviewLocation = EvaluateConveyorPlacement(SpawnLocation, SpawnRotation, bPlacementIsValid);

	if (!bHasPreviewLocation || !bPlacementIsValid)
	{
		bConveyorPlacementValid = false;
		if (ConveyorPlacementPreview)
		{
			ConveyorPlacementPreview->SetPlacementState(false);
		}

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				static_cast<uint64>(GetUniqueID()) + 18001ULL,
				1.0f,
				FColor::Red,
				TEXT("Invalid factory placement."));
		}
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UClass* BuildableClass = ResolveFactoryBuildableClass(CurrentFactoryPlacementType);

	if (!BuildableClass)
	{
		return;
	}

	FActorSpawnParameters SpawnParameters{};
	SpawnParameters.Owner = this;
	SpawnParameters.Instigator = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* SpawnedBuildable = World->SpawnActor<AActor>(
		BuildableClass,
		SpawnLocation,
		SpawnRotation,
		SpawnParameters);

	if (AMinerActor* SpawnedMiner = Cast<AMinerActor>(SpawnedBuildable))
	{
		if (AMaterialNodeActor* PlacementNode = ResolveMaterialNodeAtPlacementLocation(SpawnLocation))
		{
			SpawnedMiner->SetTargetNode(PlacementNode);
		}
	}

	UpdateConveyorPlacementPreview();
}

void AAgentCharacter::CancelConveyorPlacement()
{
	if (bConveyorPlacementModeActive)
	{
		ExitConveyorPlacementMode();
	}
}

void AAgentCharacter::RotateConveyorPlacement(int32 Direction)
{
	if (!bConveyorPlacementModeActive || Direction == 0)
	{
		return;
	}

	const bool bUseFineRotation = IsFreeFactoryPlacementActive();
	const float RotationStepDegrees = bUseFineRotation
		? FMath::Max(0.1f, FreePlacementRotationStepDegrees)
		: AgentFactory::QuarterTurnDegrees;
	FactoryPlacementRotationYawDegrees = FRotator::NormalizeAxis(
		FactoryPlacementRotationYawDegrees + static_cast<float>(Direction) * RotationStepDegrees);

	if (!bUseFineRotation)
	{
		FactoryPlacementRotationYawDegrees = AgentFactory::SnapYawToGrid(FactoryPlacementRotationYawDegrees).Yaw;
	}

	UpdateConveyorPlacementPreview();
}

bool AAgentCharacter::EvaluateConveyorPlacement(FVector& OutLocation, FRotator& OutRotation, bool& bOutIsValid) const
{
	OutLocation = FVector::ZeroVector;
	OutRotation = FRotator::ZeroRotator;
	bOutIsValid = false;

	if (!CanUseConveyorPlacementMode())
	{
		return false;
	}

	UWorld* World = GetWorld();
	UCameraComponent* PlacementCamera = GetActivePlacementCamera();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ConveyorPlacementTrace), false, this);
	QueryParams.AddIgnoredActor(this);

	if (DroneCompanion)
	{
		QueryParams.AddIgnoredActor(DroneCompanion);
	}

	if (ConveyorPlacementPreview)
	{
		QueryParams.AddIgnoredActor(ConveyorPlacementPreview);
	}

	const bool bIsMinerPlacement = CurrentFactoryPlacementType == EAgentFactoryPlacementType::Miner;
	const bool bUseFreePlacement = IsFreeFactoryPlacementActive();
	OutRotation = bUseFreePlacement
		? FRotator(0.0f, FRotator::NormalizeAxis(FactoryPlacementRotationYawDegrees), 0.0f)
		: AgentFactory::SnapYawToGrid(FactoryPlacementRotationYawDegrees);

	if (!PlacementCamera)
	{
		return false;
	}

	const FVector TraceStart = PlacementCamera->GetComponentLocation();
	const FVector TraceDirection = PlacementCamera->GetForwardVector();
	const FVector TraceEnd = TraceStart + TraceDirection * FMath::Max(100.0f, ConveyorPlacementTraceDistance);

	FHitResult AimHit;
	bool bHasAimHit = World->LineTraceSingleByChannel(
		AimHit,
		TraceStart,
		TraceEnd,
		AgentFactory::BuildPlacementTraceChannel,
		QueryParams);

	if (!bHasAimHit)
	{
		bHasAimHit = World->LineTraceSingleByChannel(AimHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
	}

	if (!bHasAimHit)
	{
		return false;
	}

	AActor* AllowedOverlapActor = nullptr;
	if (bIsMinerPlacement)
	{
		AMaterialNodeActor* TargetNode = ResolveMaterialNodeFromPlacementHit(AimHit);
		if (!TargetNode)
		{
			OutLocation = bUseFreePlacement ? AimHit.ImpactPoint : AgentFactory::SnapLocationToGrid(AimHit.ImpactPoint);
			return true;
		}

		AllowedOverlapActor = TargetNode;
		OutLocation = bUseFreePlacement ? AimHit.ImpactPoint : AgentFactory::SnapLocationToGrid(AimHit.ImpactPoint);
	}
	else
	{
		const bool bSnappedToConveyorFace = TryGetFactoryBuildableFaceSnapLocation(AimHit, OutLocation);
		if (!bSnappedToConveyorFace)
		{
			const FVector PlacementProbeLocation = AimHit.ImpactPoint;

			const FVector SurfaceTraceStart = PlacementProbeLocation + FVector(0.0f, 0.0f, FMath::Max(100.0f, ConveyorPlacementSurfaceTraceHeight));
			const FVector SurfaceTraceEnd = PlacementProbeLocation - FVector(0.0f, 0.0f, FMath::Max(100.0f, ConveyorPlacementSurfaceTraceDepth));

			FHitResult SurfaceHit;
			bool bHasSurfaceHit = World->LineTraceSingleByChannel(
				SurfaceHit,
				SurfaceTraceStart,
				SurfaceTraceEnd,
				AgentFactory::BuildPlacementTraceChannel,
				QueryParams);

			if (!bHasSurfaceHit)
			{
				bHasSurfaceHit = World->LineTraceSingleByChannel(SurfaceHit, SurfaceTraceStart, SurfaceTraceEnd, ECC_Visibility, QueryParams);
			}

			if (!bHasSurfaceHit)
			{
				return false;
			}

			OutLocation = bUseFreePlacement ? SurfaceHit.ImpactPoint : AgentFactory::SnapLocationToGrid(SurfaceHit.ImpactPoint);

			if (SurfaceHit.ImpactNormal.Z < 0.85f)
			{
				return true;
			}
		}
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQueryParams.AddObjectTypesToQuery(AgentFactory::FactoryBuildableChannel);

	FCollisionQueryParams OverlapParams(SCENE_QUERY_STAT(ConveyorPlacementOverlap), false, this);
	OverlapParams.AddIgnoredActor(this);

	if (DroneCompanion)
	{
		OverlapParams.AddIgnoredActor(DroneCompanion);
	}

	if (ConveyorPlacementPreview)
	{
		OverlapParams.AddIgnoredActor(ConveyorPlacementPreview);
	}

	TArray<FOverlapResult> OverlapResults;
	const FVector OverlapCenter = OutLocation + FVector(0.0f, 0.0f, 20.0f + ConveyorPlacementClearanceExtents.Z);
	const bool bHasOverlap = World->OverlapMultiByObjectType(
		OverlapResults,
		OverlapCenter,
		OutRotation.Quaternion(),
		ObjectQueryParams,
		FCollisionShape::MakeBox(ConveyorPlacementClearanceExtents),
		OverlapParams);

	if (bHasOverlap)
	{
		for (const FOverlapResult& OverlapResult : OverlapResults)
		{
			AActor* OverlapActor = OverlapResult.GetActor();
			if (!OverlapActor)
			{
				continue;
			}

			if (OverlapActor == this
				|| OverlapActor == DroneCompanion.Get()
				|| OverlapActor == ConveyorPlacementPreview.Get())
			{
				continue;
			}

			if (AllowedOverlapActor
				&& (OverlapActor == AllowedOverlapActor
					|| OverlapActor->IsOwnedBy(AllowedOverlapActor)
					|| OverlapActor->GetOwner() == AllowedOverlapActor
					|| OverlapActor->GetAttachParentActor() == AllowedOverlapActor))
			{
				continue;
			}

			return true;
		}
	}

	bOutIsValid = true;
	return true;
}

bool AAgentCharacter::TryGetFactoryBuildableFaceSnapLocation(const FHitResult& AimHit, FVector& OutLocation) const
{
	AActor* HitActor = AimHit.GetActor();
	if (!HitActor)
	{
		return false;
	}

	const bool bIsSupportedBuildable = HitActor->IsA<AConveyorBeltStraight>()
		|| HitActor->IsA<AStorageBin>()
		|| HitActor->IsA<AMachineActor>()
		|| HitActor->IsA<AMinerActor>()
		|| HitActor->IsA<AMiningSwarmMachine>();
	if (!bIsSupportedBuildable)
	{
		return false;
	}

	const FTransform BuildableTransform = HitActor->GetActorTransform();
	FVector LocalImpact = BuildableTransform.InverseTransformPosition(AimHit.ImpactPoint);
	LocalImpact.Z = 0.0f;

	if (LocalImpact.IsNearlyZero())
	{
		LocalImpact.X = AgentFactory::GridSize * 0.5f;
	}

	FVector SnapDirection = HitActor->GetActorForwardVector();
	if (FMath::Abs(LocalImpact.X) >= FMath::Abs(LocalImpact.Y))
	{
		SnapDirection *= (LocalImpact.X >= 0.0f) ? 1.0f : -1.0f;
	}
	else
	{
		SnapDirection = HitActor->GetActorRightVector() * ((LocalImpact.Y >= 0.0f) ? 1.0f : -1.0f);
	}

	OutLocation = AgentFactory::SnapLocationToGrid(HitActor->GetActorLocation() + SnapDirection * AgentFactory::GridSize);
	return true;
}

AMaterialNodeActor* AAgentCharacter::ResolveMaterialNodeFromPlacementHit(const FHitResult& AimHit) const
{
	AActor* HitActor = AimHit.GetActor();
	if (!HitActor)
	{
		return nullptr;
	}

	if (AMaterialNodeActor* HitNode = Cast<AMaterialNodeActor>(HitActor))
	{
		return HitNode;
	}

	if (AMaterialNodeActor* OwnerNode = Cast<AMaterialNodeActor>(HitActor->GetOwner()))
	{
		return OwnerNode;
	}

	for (AActor* ParentActor = HitActor->GetAttachParentActor(); ParentActor; ParentActor = ParentActor->GetAttachParentActor())
	{
		if (AMaterialNodeActor* ParentNode = Cast<AMaterialNodeActor>(ParentActor))
		{
			return ParentNode;
		}
	}

	return nullptr;
}

AMaterialNodeActor* AAgentCharacter::ResolveMaterialNodeAtPlacementLocation(const FVector& PlacementLocation) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MinerPlacementNodeResolve), false, this);
	QueryParams.AddIgnoredActor(this);
	if (DroneCompanion)
	{
		QueryParams.AddIgnoredActor(DroneCompanion);
	}
	if (ConveyorPlacementPreview)
	{
		QueryParams.AddIgnoredActor(ConveyorPlacementPreview);
	}

	const float VerticalTraceDistance = FMath::Max(200.0f, ConveyorPlacementSurfaceTraceHeight + ConveyorPlacementSurfaceTraceDepth);
	const FVector TraceStart = PlacementLocation + FVector(0.0f, 0.0f, VerticalTraceDistance * 0.5f);
	const FVector TraceEnd = PlacementLocation - FVector(0.0f, 0.0f, VerticalTraceDistance * 0.5f);

	FHitResult NodeHit;
	bool bHasNodeHit = World->LineTraceSingleByChannel(
		NodeHit,
		TraceStart,
		TraceEnd,
		AgentFactory::BuildPlacementTraceChannel,
		QueryParams);
	if (!bHasNodeHit)
	{
		bHasNodeHit = World->LineTraceSingleByChannel(NodeHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
	}

	if (bHasNodeHit)
	{
		if (AMaterialNodeActor* ResolvedNode = ResolveMaterialNodeFromPlacementHit(NodeHit))
		{
			return ResolvedNode;
		}
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectQueryParams.AddObjectTypesToQuery(AgentFactory::FactoryBuildableChannel);

	TArray<FOverlapResult> OverlapResults;
	const float SearchRadius = FMath::Max3(
		ConveyorPlacementClearanceExtents.X,
		ConveyorPlacementClearanceExtents.Y,
		ConveyorPlacementClearanceExtents.Z) * 2.0f;
	const bool bHasOverlap = World->OverlapMultiByObjectType(
		OverlapResults,
		PlacementLocation,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(FMath::Max(50.0f, SearchRadius)),
		QueryParams);

	if (!bHasOverlap)
	{
		return nullptr;
	}

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		AActor* OverlapActor = OverlapResult.GetActor();
		if (!OverlapActor)
		{
			continue;
		}

		if (AMaterialNodeActor* ResolvedNode = Cast<AMaterialNodeActor>(OverlapActor))
		{
			return ResolvedNode;
		}

		if (AMaterialNodeActor* OwnerNode = Cast<AMaterialNodeActor>(OverlapActor->GetOwner()))
		{
			return OwnerNode;
		}

		for (AActor* ParentActor = OverlapActor->GetAttachParentActor(); ParentActor; ParentActor = ParentActor->GetAttachParentActor())
		{
			if (AMaterialNodeActor* ParentNode = Cast<AMaterialNodeActor>(ParentActor))
			{
				return ParentNode;
			}
		}
	}

	return nullptr;
}

bool AAgentCharacter::IsFreeFactoryPlacementActive() const
{
	return bConveyorPlacementModeActive
		&& bFactoryFreePlacementEnabled
		&& CurrentFactoryPlacementType == EAgentFactoryPlacementType::Miner;
}

void AAgentCharacter::SelectFactoryPlacementType(EAgentFactoryPlacementType NewType, bool bToggleIfAlreadySelected)
{
	const bool bWasActive = bConveyorPlacementModeActive;
	const bool bWasSameType = CurrentFactoryPlacementType == NewType;
	CurrentFactoryPlacementType = NewType;
	if (!IsFreeFactoryPlacementActive())
	{
		FactoryPlacementRotationYawDegrees = AgentFactory::SnapYawToGrid(FactoryPlacementRotationYawDegrees).Yaw;
	}

	if (!bWasActive)
	{
		EnterConveyorPlacementMode();
		return;
	}

	if (bToggleIfAlreadySelected && bWasSameType)
	{
		ExitConveyorPlacementMode();
		return;
	}

	if (ConveyorPlacementPreview)
	{
		ConveyorPlacementPreview->SetPreviewActorClass(ResolveFactoryBuildableClass(CurrentFactoryPlacementType));
	}

	UpdateConveyorPlacementPreview();
}

bool AAgentCharacter::CanUsePickupInteraction() const
{
	if (IsRagdolling())
	{
		return false;
	}

	if (bConveyorPlacementModeActive)
	{
		return false;
	}

	if (bMiniMapModeActive && bMiniMapViewingDroneCamera)
	{
		return false;
	}

	if (bMapModeActive)
	{
		return CanUseMapModeDronePickup();
	}

	return CurrentViewMode == EAgentViewMode::ThirdPerson
		|| CurrentViewMode == EAgentViewMode::FirstPerson
		|| CurrentViewMode == EAgentViewMode::DronePilot;
}

bool AAgentCharacter::CanMaintainHeldPickup() const
{
	if (IsRagdolling())
	{
		return false;
	}

	if (!HeldPickupComponent.IsValid())
	{
		return false;
	}

	if (bConveyorPlacementModeActive)
	{
		return false;
	}

	if (bMiniMapModeActive && bMiniMapViewingDroneCamera)
	{
		return false;
	}

	if (bHeldPickupUsesDroneView)
	{
		return DroneCompanion != nullptr;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	return GetCharacterPickupView(ViewLocation, ViewRotation);
}

bool AAgentCharacter::CanUseDroneLiftAssist() const
{
	if (!DroneCompanion || bConveyorPlacementModeActive)
	{
		return false;
	}

	if (bMiniMapModeActive && bMiniMapViewingDroneCamera)
	{
		return false;
	}

	if (bMapModeActive)
	{
		return true;
	}

	return CurrentViewMode == EAgentViewMode::FirstPerson;
}

bool AAgentCharacter::CanMaintainDroneLiftAssist() const
{
	if (!DroneCompanion || bConveyorPlacementModeActive)
	{
		return false;
	}

	if (bMiniMapModeActive && bMiniMapViewingDroneCamera)
	{
		return false;
	}

	return bMapModeActive
		|| CurrentViewMode == EAgentViewMode::FirstPerson
		|| CurrentViewMode == EAgentViewMode::DronePilot;
}

bool AAgentCharacter::CanUseMapModeDronePickup() const
{
	if (!bMapModeActive || !DroneCompanion)
	{
		return false;
	}

	FVector DroneViewLocation = FVector::ZeroVector;
	FRotator DroneViewRotation = FRotator::ZeroRotator;
	if (!DroneCompanion->GetDroneCameraTransform(DroneViewLocation, DroneViewRotation))
	{
		return false;
	}

	const FVector DroneViewForward = DroneViewRotation.Vector().GetSafeNormal();
	const float DownwardDot = FVector::DotProduct(DroneViewForward, FVector::DownVector);
	return DownwardDot >= FMath::Clamp(MapModePickupDownwardDotThreshold, -1.0f, 1.0f);
}

bool AAgentCharacter::GetCharacterPickupView(FVector& OutLocation, FRotator& OutRotation) const
{
	if (CurrentViewMode == EAgentViewMode::FirstPerson)
	{
		if (UCameraComponent* const ActiveFirstPersonCamera = ResolveFirstPersonCamera())
		{
			OutLocation = ActiveFirstPersonCamera->GetComponentLocation();
			OutRotation = ActiveFirstPersonCamera->GetComponentRotation();
			return true;
		}
	}

	if (ThirdPersonTransitionCamera && ThirdPersonTransitionCamera->IsActive())
	{
		OutLocation = ThirdPersonTransitionCamera->GetComponentLocation();
		OutRotation = ThirdPersonTransitionCamera->GetComponentRotation();
		return true;
	}

	if (FollowCamera)
	{
		OutLocation = FollowCamera->GetComponentLocation();
		OutRotation = FollowCamera->GetComponentRotation();
		return true;
	}

	if (UCameraComponent* const ActiveFirstPersonCamera = ResolveFirstPersonCamera())
	{
		OutLocation = ActiveFirstPersonCamera->GetComponentLocation();
		OutRotation = ActiveFirstPersonCamera->GetComponentRotation();
		return true;
	}

	return false;
}

bool AAgentCharacter::GetActivePickupView(FVector& OutLocation, FRotator& OutRotation) const
{
	if (HeldPickupComponent.IsValid())
	{
		if (bHeldPickupUsesDroneView)
		{
			return DroneCompanion && DroneCompanion->GetDroneCameraTransform(OutLocation, OutRotation);
		}

		return GetCharacterPickupView(OutLocation, OutRotation);
	}

	if (bMapModeActive && DroneCompanion)
	{
		return DroneCompanion->GetDroneCameraTransform(OutLocation, OutRotation);
	}

	if ((CurrentViewMode == EAgentViewMode::FirstPerson || CurrentViewMode == EAgentViewMode::ThirdPerson)
		&& GetCharacterPickupView(OutLocation, OutRotation))
	{
		return true;
	}

	if (CurrentViewMode == EAgentViewMode::DronePilot && DroneCompanion)
	{
		return DroneCompanion->GetDroneCameraTransform(OutLocation, OutRotation);
	}

	return false;
}

void AAgentCharacter::UpdatePickupInteraction()
{
	if (HeldPickupComponent.IsValid())
	{
		UpdateHeldPickup();
		if (HeldPickupComponent.IsValid())
		{
			return;
		}
	}

	UpdatePickupCandidate();
}

bool AAgentCharacter::UpdatePickupCandidate()
{
	bPickupCandidateValid = false;
	PickupCandidateComponent.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	if (!GetActivePickupView(ViewLocation, ViewRotation))
	{
		return false;
	}

	const float TraceDistance = FMath::Max(0.0f, PickupTraceDistance);
	const float TraceRadius = FMath::Max(0.0f, PickupTraceRadius);
	const FVector TraceEnd = ViewLocation + ViewRotation.Vector() * TraceDistance;

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PickupTrace), false, this);
	QueryParams.bFindInitialOverlaps = true;
	QueryParams.AddIgnoredActor(this);
	if (DroneCompanion
		&& (CurrentViewMode == EAgentViewMode::DronePilot || bMapModeActive))
	{
		QueryParams.AddIgnoredActor(DroneCompanion.Get());
	}

	if (ConveyorPlacementPreview)
	{
		QueryParams.AddIgnoredActor(ConveyorPlacementPreview.Get());
	}

	TArray<FHitResult> Hits;
	const bool bHitAnything = World->SweepMultiByObjectType(
		Hits,
		ViewLocation,
		TraceEnd,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(TraceRadius),
		QueryParams);

	FHitResult BestHit;
	float BestDistanceSq = TNumericLimits<float>::Max();

	if (bHitAnything)
	{
		for (const FHitResult& Hit : Hits)
		{
			UPrimitiveComponent* HitComponent = Hit.GetComponent();
			if (!CanPickupComponent(HitComponent))
			{
				continue;
			}

			const FVector HitLocation = Hit.ImpactPoint.IsNearlyZero() ? Hit.Location : Hit.ImpactPoint;
			const float DistanceSq = FVector::DistSquared(ViewLocation, HitLocation);
			if (DistanceSq < BestDistanceSq)
			{
				BestDistanceSq = DistanceSq;
				BestHit = Hit;
			}
		}
	}

	if (BestDistanceSq < TNumericLimits<float>::Max())
	{
		PickupCandidateComponent = BestHit.GetComponent();
		PickupCandidateLocation = BestHit.ImpactPoint.IsNearlyZero() ? BestHit.Location : BestHit.ImpactPoint;
		bPickupCandidateValid = true;
		DrawPickupInfluenceDebug(PickupCandidateLocation, FColor::Green);
		return true;
	}

	PickupCandidateLocation = TraceEnd;
	DrawPickupInfluenceDebug(TraceEnd, FColor(200, 200, 200));
	return false;
}

void AAgentCharacter::DrawPickupInfluenceDebug(
	const FVector& SphereCenter,
	const FColor& Color) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	if (!bShowPickupDebug)
	{
		return;
	}

	DrawDebugSphere(World, SphereCenter, FMath::Max(0.0f, PickupTraceRadius), 18, Color, false, 0.0f, 0, 1.25f);
}

bool AAgentCharacter::CanPickupComponent(const UPrimitiveComponent* PrimitiveComponent) const
{
	return ResolvePickupPhysicsComponent(const_cast<UPrimitiveComponent*>(PrimitiveComponent)) != nullptr;
}

UPrimitiveComponent* AAgentCharacter::ResolvePickupPhysicsComponent(UPrimitiveComponent* PrimitiveComponent) const
{
	if (!PrimitiveComponent)
	{
		return nullptr;
	}

	if (PrimitiveComponent->IsSimulatingPhysics()
		&& PrimitiveComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
	{
		return PrimitiveComponent;
	}

	AMiningBotActor* MiningBotOwner = Cast<AMiningBotActor>(PrimitiveComponent->GetOwner());
	if (MiningBotOwner)
	{
		if (UPrimitiveComponent* BotPickupPhysicsComponent = MiningBotOwner->GetPickupPhysicsComponent())
		{
			if (BotPickupPhysicsComponent->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
			{
				return BotPickupPhysicsComponent;
			}
		}
	}

	ADroneCompanion* DroneOwner = Cast<ADroneCompanion>(PrimitiveComponent->GetOwner());
	if (!DroneOwner)
	{
		return nullptr;
	}

	UPrimitiveComponent* DronePickupPhysicsComponent = DroneOwner->GetPickupPhysicsComponent();
	if (!DronePickupPhysicsComponent
		|| !DronePickupPhysicsComponent->IsSimulatingPhysics()
		|| DronePickupPhysicsComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
	{
		return nullptr;
	}

	return DronePickupPhysicsComponent;
}

bool AAgentCharacter::TryBeginPickup()
{
	if (!PickupPhysicsHandle)
	{
		return false;
	}

	if (HeldPickupComponent.IsValid())
	{
		return true;
	}

	if ((!bPickupCandidateValid || !PickupCandidateComponent.IsValid()) && !UpdatePickupCandidate())
	{
		return false;
	}

	UPrimitiveComponent* PrimitiveComponent = ResolvePickupPhysicsComponent(PickupCandidateComponent.Get());
	if (!PrimitiveComponent)
	{
		return false;
	}

	if (AMiningBotActor* MiningBotOwner = Cast<AMiningBotActor>(PrimitiveComponent->GetOwner()))
	{
		MiningBotOwner->NotifyPickedUpByPhysicsHandle(true);
		if (UPrimitiveComponent* BotPickupPhysicsComponent = MiningBotOwner->GetPickupPhysicsComponent())
		{
			PrimitiveComponent = BotPickupPhysicsComponent;
		}
	}

	if (!PrimitiveComponent->IsSimulatingPhysics())
	{
		PrimitiveComponent->SetSimulatePhysics(true);
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	if (!GetActivePickupView(ViewLocation, ViewRotation))
	{
		return false;
	}

	PrimitiveComponent->WakeAllRigidBodies();
	PickupPhysicsHandle->ReleaseComponent();

	bHeldPickupUsesDroneView = bMapModeActive || CurrentViewMode == EAgentViewMode::DronePilot;
	HeldPickupComponent = PrimitiveComponent;
	HeldPickupDistance = FMath::Clamp(
		FVector::Dist(ViewLocation, PickupCandidateLocation),
		FMath::Max(0.0f, PickupMinHoldDistance),
		FMath::Max(PickupMinHoldDistance, PickupMaxHoldDistance));
	HeldPickupMassKg = FMath::Max(0.0f, PrimitiveComponent->GetMass());
	HeldPickupOriginalLinearDamping = PrimitiveComponent->GetLinearDamping();
	HeldPickupOriginalAngularDamping = PrimitiveComponent->GetAngularDamping();
	HeldPickupLocalGrabOffset = PrimitiveComponent->GetComponentTransform().InverseTransformPosition(PickupCandidateLocation);
	HeldPickupTargetRotation = PrimitiveComponent->GetComponentRotation();
	HeldPickupViewRelativeRotationOffset = (HeldPickupTargetRotation - ViewRotation).GetNormalized();
	bPickupRotationModeActive = false;
	SyncPickupHandleSettings();
	RefreshHeldPickupConstraintMode();
	return true;
}

void AAgentCharacter::EndPickup()
{
	if (UPrimitiveComponent* HeldComponent = HeldPickupComponent.Get())
	{
		if (AMiningBotActor* MiningBotOwner = Cast<AMiningBotActor>(HeldComponent->GetOwner()))
		{
			MiningBotOwner->NotifyPickedUpByPhysicsHandle(false);
		}
		HeldComponent->SetLinearDamping(HeldPickupOriginalLinearDamping);
		HeldComponent->SetAngularDamping(HeldPickupOriginalAngularDamping);
	}

	if (PickupPhysicsHandle && PickupPhysicsHandle->GetGrabbedComponent() != nullptr)
	{
		PickupPhysicsHandle->ReleaseComponent();
	}

	HeldPickupComponent.Reset();
	HeldPickupDistance = 0.0f;
	HeldPickupMassKg = 0.0f;
	HeldPickupOriginalLinearDamping = 0.0f;
	HeldPickupOriginalAngularDamping = 0.0f;
	HeldPickupLocalGrabOffset = FVector::ZeroVector;
	HeldPickupViewRelativeRotationOffset = FRotator::ZeroRotator;
	HeldPickupTargetRotation = FRotator::ZeroRotator;
	bPickupRotationModeActive = false;
	bHeldPickupUsesDroneView = false;
}

void AAgentCharacter::UpdateHeldPickup()
{
	if (!PickupPhysicsHandle)
	{
		EndPickup();
		return;
	}

	UPrimitiveComponent* HeldComponent = HeldPickupComponent.Get();
	if (!CanPickupComponent(HeldComponent))
	{
		EndPickup();
		return;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	if (!GetActivePickupView(ViewLocation, ViewRotation))
	{
		EndPickup();
		return;
	}

	HeldPickupMassKg = FMath::Max(0.0f, HeldComponent->GetMass());
	SyncPickupHandleSettings();

	const float HoldDistance = FMath::Clamp(
		HeldPickupDistance,
		FMath::Max(0.0f, PickupMinHoldDistance),
		FMath::Max(PickupMinHoldDistance, PickupMaxHoldDistance));
	FVector DesiredTargetLocation = ViewLocation + ViewRotation.Vector() * HoldDistance;
	if (CurrentViewMode == EAgentViewMode::ThirdPerson)
	{
		DesiredTargetLocation += ViewRotation.RotateVector(ThirdPersonPickupHoldOffset);
	}

	if (bPickupRotationModeActive)
	{
		HeldPickupTargetRotation = (ViewRotation + HeldPickupViewRelativeRotationOffset).GetNormalized();
		PickupPhysicsHandle->SetTargetLocationAndRotation(DesiredTargetLocation, HeldPickupTargetRotation);
	}
	else
	{
		PickupPhysicsHandle->SetTargetLocation(DesiredTargetLocation);
	}
	DrawPickupInfluenceDebug(DesiredTargetLocation, FColor::Cyan);
}

void AAgentCharacter::RebaseHeldPickupRotationToView()
{
	UPrimitiveComponent* HeldComponent = HeldPickupComponent.Get();
	if (!HeldComponent)
	{
		HeldPickupViewRelativeRotationOffset = FRotator::ZeroRotator;
		return;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	if (!GetActivePickupView(ViewLocation, ViewRotation))
	{
		return;
	}

	HeldPickupTargetRotation = HeldComponent->GetComponentRotation();
	HeldPickupViewRelativeRotationOffset = (HeldPickupTargetRotation - ViewRotation).GetNormalized();
}

void AAgentCharacter::RefreshHeldPickupConstraintMode()
{
	if (!PickupPhysicsHandle)
	{
		return;
	}

	UPrimitiveComponent* HeldComponent = HeldPickupComponent.Get();
	if (!CanPickupComponent(HeldComponent))
	{
		return;
	}

	SyncPickupHandleSettings();
	const FVector GrabLocation = HeldComponent->GetComponentTransform().TransformPosition(HeldPickupLocalGrabOffset);
	HeldComponent->WakeAllRigidBodies();
	PickupPhysicsHandle->ReleaseComponent();

	if (bPickupRotationModeActive)
	{
		PickupPhysicsHandle->GrabComponentAtLocationWithRotation(
			HeldComponent,
			NAME_None,
			GrabLocation,
			HeldPickupTargetRotation);
	}
	else
	{
		PickupPhysicsHandle->GrabComponentAtLocation(
			HeldComponent,
			NAME_None,
			GrabLocation);
	}
}

float AAgentCharacter::GetActivePickupStrengthValue() const
{
	const bool bUseDronePickupStrength = HeldPickupComponent.IsValid()
		? bHeldPickupUsesDroneView
		: (CurrentViewMode == EAgentViewMode::DronePilot || bMapModeActive);
	const float ActiveStrengthValue = bUseDronePickupStrength
		? DronePickupStrengthMultiplier
		: CharacterPickupStrengthMultiplier;
	return FMath::Max(0.0f, ActiveStrengthValue);
}

float AAgentCharacter::GetHeldPickupResponsivenessScale() const
{
	const float StrengthValue = GetActivePickupStrengthValue();
	const float CarryMassScaleKg = FMath::Max(1.0f, PickupStrengthMassScaleKg);
	const float MaxCarryMassKg = FMath::Max(1.0f, StrengthValue * CarryMassScaleKg);
	const float EffectiveHeldMassKg = HeldPickupMassKg * FMath::Max(0.0f, PickupMassFeelMultiplier);
	const float LoadRatio = EffectiveHeldMassKg / MaxCarryMassKg;
	const float ResponseMassFraction = FMath::Clamp(PickupSoftCapResponseMassFraction, 0.01f, 1.0f);
	const float ResponseLoadRatio = LoadRatio / ResponseMassFraction;
	const float OverloadRatio = FMath::Max(0.0f, ResponseLoadRatio - 1.0f);
	return 1.0f / (1.0f + OverloadRatio);
}

float AAgentCharacter::GetHeldPickupHeavyDampingMultiplier() const
{
	const float ReferenceStrength = FMath::Max(0.01f, PickupHeavyDampingReferenceStrength);
	const float StrengthScale = FMath::Max(0.05f, GetActivePickupStrengthValue() / ReferenceStrength);
	const float HeavyDampingStartMassKg = FMath::Max(1.0f, PickupHeavyDampingStartMassKg * StrengthScale);
	const float HeavyMassRatio = HeldPickupMassKg / HeavyDampingStartMassKg;
	const float HeavyOverRatio = FMath::Max(0.0f, HeavyMassRatio - 1.0f);
	return 1.0f + (HeavyOverRatio * FMath::Max(0.0f, PickupHeavyDampingMultiplier));
}

float AAgentCharacter::GetHeldPickupRotationLeverageMultiplier() const
{
	const UPrimitiveComponent* HeldComponent = HeldPickupComponent.Get();
	if (!HeldComponent)
	{
		return 1.0f;
	}

	const FVector GrabLocation = HeldComponent->GetComponentTransform().TransformPosition(HeldPickupLocalGrabOffset);
	const FVector CenterOfMass = HeldComponent->GetCenterOfMass(NAME_None);
	const float LeverageDistanceCm = FVector::Dist(GrabLocation, CenterOfMass);
	const float LeverageReferenceCm = FMath::Max(1.0f, PickupRotationLeverageReferenceCm);
	return 1.0f + (LeverageDistanceCm / LeverageReferenceCm);
}

float AAgentCharacter::GetHeldPickupRotationDriveScale() const
{
	const float StrengthValue = GetActivePickupStrengthValue();
	const float CarryMassScaleKg = FMath::Max(1.0f, PickupStrengthMassScaleKg);
	const float CarryMassKg = FMath::Max(1.0f, StrengthValue * CarryMassScaleKg);
	const float ComfortMassKg = FMath::Max(
		1.0f,
		CarryMassKg * FMath::Clamp(PickupRotationComfortMassFraction, 0.01f, 1.0f));
	const float RotationEffectiveMassKg = HeldPickupMassKg * GetHeldPickupRotationLeverageMultiplier();
	const float RotationLoadRatio = RotationEffectiveMassKg / ComfortMassKg;
	const float RotationOverRatio = FMath::Max(0.0f, RotationLoadRatio - 1.0f);
	const float ResistanceMultiplier = FMath::Max(0.1f, PickupRotationResistanceMultiplier);
	const float Maneuverability = 1.0f / (1.0f + (RotationOverRatio * ResistanceMultiplier));
	const float GuideInputScale = FMath::Clamp(PickupRotationGuideInputScale, 0.1f, 2.0f);
	return FMath::Max(0.08f, GuideInputScale * Maneuverability);
}

void AAgentCharacter::ApplyHeldPickupRotationPhysicsInput(const FVector& RotationAxis, float InputValue, float DeltaSeconds)
{
	UPrimitiveComponent* HeldComponent = HeldPickupComponent.Get();
	if (!HeldComponent || FMath::IsNearlyZero(InputValue))
	{
		return;
	}

	const float RotationDriveScale = GetHeldPickupRotationDriveScale();
	const float AngularVelocityDelta = FMath::Max(0.0f, PickupRotationAngularImpulse)
		* RotationDriveScale
		* FMath::Max(0.0f, DeltaSeconds);
	const FVector AngularVelocity = RotationAxis * (InputValue * AngularVelocityDelta);

	HeldComponent->WakeAllRigidBodies();
	HeldComponent->SetPhysicsAngularVelocityInDegrees(AngularVelocity, true, NAME_None);
}

ADroneCompanion* AAgentCharacter::GetHeldDroneFromPickup() const
{
	UPrimitiveComponent* HeldComponent = HeldPickupComponent.Get();
	if (!HeldComponent && PickupPhysicsHandle)
	{
		HeldComponent = PickupPhysicsHandle->GetGrabbedComponent();
	}

	if (!HeldComponent)
	{
		return nullptr;
	}

	return Cast<ADroneCompanion>(HeldComponent->GetOwner());
}

bool AAgentCharacter::TryToggleHeldBackpackPortalFromPickup()
{
	UPrimitiveComponent* HeldComponent = HeldPickupComponent.Get();
	if (!HeldComponent && PickupPhysicsHandle)
	{
		HeldComponent = PickupPhysicsHandle->GetGrabbedComponent();
	}

	if (!HeldComponent)
	{
		return false;
	}

	ABlackHoleBackpackActor* HeldBackpack = Cast<ABlackHoleBackpackActor>(HeldComponent->GetOwner());
	if (!HeldBackpack)
	{
		return false;
	}

	const bool bWasPortalEnabled = HeldBackpack->IsPortalEnabled();
	HeldBackpack->TogglePortalEnabled();

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			static_cast<uint64>(GetUniqueID()) + 20510ULL,
			1.25f,
			bWasPortalEnabled ? FColor::Yellow : FColor::Green,
			bWasPortalEnabled
				? TEXT("Held backpack portal OFF")
				: TEXT("Held backpack portal ON"));
	}

	return true;
}

bool AAgentCharacter::TryToggleHeldDronePowerFromPickup()
{
	ADroneCompanion* HeldDrone = GetHeldDroneFromPickup();
	if (!HeldDrone)
	{
		return false;
	}

	const bool bWasManualOff = HeldDrone->IsManualPowerOffRequested();
	HeldDrone->ToggleManualPowerOff();
	RefreshPrimaryDroneAvailabilityFromCompanion();
	ApplyDroneFleetContext(false, false);

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			static_cast<uint64>(GetUniqueID()) + 20500ULL,
			1.25f,
			bWasManualOff ? FColor::Green : FColor::Yellow,
			bWasManualOff
				? TEXT("Held drone powered ON")
				: TEXT("Held drone powered OFF"));
	}

	return true;
}

bool AAgentCharacter::TryToggleDroneLiftAssist()
{
	if (!DroneCompanion)
	{
		return false;
	}

	if (DroneCompanion->IsLiftAssistActive())
	{
		if (DroneSwarmComponent)
		{
			DroneSwarmComponent->SetHookWorker(DroneCompanion, false);
		}
		HookJobLockedDrones.RemoveSingleSwap(DroneCompanion);
		if (LastHookJobLockedDrone.Get() == DroneCompanion)
		{
			LastHookJobLockedDrone = HookJobLockedDrones.Num() > 0
				? HookJobLockedDrones.Last()
				: nullptr;
		}

		DroneCompanion->StopLiftAssist();
		ApplyDroneFleetContext(false, false);
		return true;
	}

	if (!CanUseDroneLiftAssist())
	{
		return false;
	}

	UPrimitiveComponent* PrimitiveComponent = nullptr;
	FVector GrabLocation = FVector::ZeroVector;

	if (HeldPickupComponent.IsValid() && CanPickupComponent(HeldPickupComponent.Get()))
	{
		PrimitiveComponent = HeldPickupComponent.Get();
		GrabLocation = PrimitiveComponent->GetComponentTransform().TransformPosition(HeldPickupLocalGrabOffset);
	}
	else if ((!bPickupCandidateValid || !PickupCandidateComponent.IsValid()) && !UpdatePickupCandidate())
	{
		return false;
	}

	if (!PrimitiveComponent)
	{
		PrimitiveComponent = ResolvePickupPhysicsComponent(PickupCandidateComponent.Get());
		GrabLocation = PickupCandidateLocation;
	}

	if (!PrimitiveComponent)
	{
		return false;
	}

	if (!DroneCompanion->StartLiftAssist(PrimitiveComponent, GrabLocation))
	{
		return false;
	}

	HookJobLockedDrones.AddUnique(DroneCompanion);
	LastHookJobLockedDrone = DroneCompanion;
	if (DroneSwarmComponent)
	{
		DroneSwarmComponent->SetHookWorker(DroneCompanion, true);
	}

	SyncDroneLiftAssistTuning();
	ApplyDroneFleetContext(false, false);
	return true;
}

bool AAgentCharacter::TryAssignActiveDroneToHookJob(bool bCycleAfterAssign)
{
	if (!DroneCompanion)
	{
		return false;
	}

	if (!DroneCompanion->IsLiftAssistActive())
	{
		if (!TryToggleDroneLiftAssist())
		{
			return false;
		}
	}
	else
	{
		HookJobLockedDrones.AddUnique(DroneCompanion);
		LastHookJobLockedDrone = DroneCompanion;
		if (DroneSwarmComponent)
		{
			DroneSwarmComponent->SetHookWorker(DroneCompanion, true);
		}
	}

	if (bCycleAfterAssign)
	{
		CycleActiveDrone(1);
	}

	return true;
}

bool AAgentCharacter::TryReleaseAllHookJobDrones()
{
	const bool bHadHookJobs = HookJobLockedDrones.Num() > 0;
	if (!bHadHookJobs)
	{
		return false;
	}

	TArray<TObjectPtr<ADroneCompanion>> DronesToRelease = HookJobLockedDrones;
	for (const TObjectPtr<ADroneCompanion>& HookDronePtr : DronesToRelease)
	{
		ADroneCompanion* HookDrone = HookDronePtr.Get();
		if (!IsValid(HookDrone))
		{
			continue;
		}

		if (DroneSwarmComponent)
		{
			DroneSwarmComponent->SetHookWorker(HookDrone, false);
		}

		if (HookDrone->IsLiftAssistActive())
		{
			HookDrone->StopLiftAssist();
		}
	}

	HookJobLockedDrones.Reset();
	LastHookJobLockedDrone = nullptr;

	ApplyDroneFleetContext(false, false);
	return true;
}

void AAgentCharacter::UpdateHookJobButtonHold(float DeltaSeconds)
{
	if (!bHookJobButtonHeld || bHookJobButtonTriggeredHoldRelease)
	{
		return;
	}

	HookJobButtonHeldDuration += FMath::Max(0.0f, DeltaSeconds);
	if (HookJobButtonHeldDuration < FMath::Max(0.0f, HookJobReleaseHoldTime))
	{
		return;
	}

	bHookJobButtonTriggeredHoldRelease = true;
	TryReleaseAllHookJobDrones();
}

void AAgentCharacter::UpdateFactoryPlacementRotationHold(float DeltaSeconds)
{
	if (!bConveyorPlacementModeActive)
	{
		bPlacementRotateLeftHeld = false;
		bPlacementRotateRightHeld = false;
		FactoryPlacementRotationHoldDirection = 0;
		FactoryPlacementRotationHoldTimeRemaining = 0.0f;
		return;
	}

	int32 DesiredDirection = 0;
	if (bPlacementRotateLeftHeld != bPlacementRotateRightHeld)
	{
		DesiredDirection = bPlacementRotateRightHeld ? 1 : -1;
	}

	if (DesiredDirection == 0)
	{
		FactoryPlacementRotationHoldDirection = 0;
		FactoryPlacementRotationHoldTimeRemaining = 0.0f;
		return;
	}

	if (FactoryPlacementRotationHoldDirection != DesiredDirection)
	{
		FactoryPlacementRotationHoldDirection = DesiredDirection;
		FactoryPlacementRotationHoldTimeRemaining = FMath::Max(0.0f, FreePlacementRotationHoldDelay);
		return;
	}

	FactoryPlacementRotationHoldTimeRemaining -= FMath::Max(0.0f, DeltaSeconds);
	const float RepeatInterval = FMath::Max(0.01f, FreePlacementRotationHoldInterval);
	while (FactoryPlacementRotationHoldTimeRemaining <= 0.0f)
	{
		RotateConveyorPlacement(DesiredDirection);
		FactoryPlacementRotationHoldTimeRemaining += RepeatInterval;
	}
}

bool AAgentCharacter::CanUseBackpackDeployInput() const
{
	return CanUseCharacterInteraction();
}

void AAgentCharacter::UpdateBackpackDeployButtonHold(float DeltaSeconds)
{
	if (!CanUseBackpackDeployInput())
	{
		ResetBackpackDeployButtonHoldState();
		return;
	}

	const float ChargeTime = FMath::Max(0.05f, BackpackDeployHoldTime);
	const float SafeDelta = FMath::Max(0.0f, DeltaSeconds);

	if (bBackpackKeyboardButtonHeld)
	{
		BackpackKeyboardButtonHeldDuration += SafeDelta;
	}

	if (bBackpackControllerButtonHeld)
	{
		BackpackControllerButtonHeldDuration += SafeDelta;
	}

	if (BackAttachmentComponent)
	{
		const bool bMagnetHoldActive = bBackpackKeyboardButtonHeld || bBackpackControllerButtonHeld;
		const float KeyboardHeldDuration = bBackpackKeyboardButtonHeld ? BackpackKeyboardButtonHeldDuration : 0.0f;
		const float ControllerHeldDuration = bBackpackControllerButtonHeld ? BackpackControllerButtonHeldDuration : 0.0f;
		const float ChargeAlpha = bMagnetHoldActive
			? FMath::Clamp(FMath::Max(KeyboardHeldDuration, ControllerHeldDuration) / ChargeTime, 0.0f, 1.0f)
			: 0.0f;
		BackAttachmentComponent->SetManualMagnetRequested(bMagnetHoldActive);
		BackAttachmentComponent->SetManualMagnetChargeAlpha(ChargeAlpha);
	}
}

void AAgentCharacter::ResetBackpackDeployButtonHoldState()
{
	bBackpackKeyboardButtonHeld = false;
	BackpackKeyboardButtonHeldDuration = 0.0f;

	bBackpackControllerButtonHeld = false;
	BackpackControllerButtonHeldDuration = 0.0f;

	if (BackAttachmentComponent)
	{
		BackAttachmentComponent->SetManualMagnetRequested(false);
		BackAttachmentComponent->SetManualMagnetChargeAlpha(0.0f);
	}
}

void AAgentCharacter::SyncDroneLiftAssistTuning() const
{
	if (!DroneCompanion)
	{
		return;
	}

	FDroneLiftAssistForceTuning Tuning;
	Tuning.StrengthValue = FMath::Max(0.0f, DronePickupStrengthMultiplier);
	Tuning.StrengthMassScaleKg = FMath::Max(1.0f, PickupStrengthMassScaleKg);
	DroneCompanion->SetLiftAssistForceTuning(Tuning);
}

void AAgentCharacter::AdjustDroneLiftAssistStrength(float Delta)
{
	const float NewStrength = FMath::Max(0.0f, DronePickupStrengthMultiplier + Delta);
	if (FMath::IsNearlyEqual(NewStrength, DronePickupStrengthMultiplier))
	{
		ShowPickupStrengthDebug();
		return;
	}

	DronePickupStrengthMultiplier = NewStrength;

	if (DroneCompanion && DroneCompanion->IsLiftAssistActive())
	{
		SyncDroneLiftAssistTuning();
	}

	ShowPickupStrengthDebug();
}

void AAgentCharacter::SyncPickupHandleSettings() const
{
	if (!PickupPhysicsHandle)
	{
		return;
	}

	PickupPhysicsHandle->bInterpolateTarget = true;
	PickupPhysicsHandle->bSoftLinearConstraint = true;
	PickupPhysicsHandle->bSoftAngularConstraint = bPickupRotationModeActive;
	const float StrengthValue = GetActivePickupStrengthValue();
	const float CarryMassScaleKg = FMath::Max(1.0f, PickupStrengthMassScaleKg);
	const float MaxCarryMassKg = FMath::Max(1.0f, StrengthValue * CarryMassScaleKg);
	const float EffectiveHeldMassKg = HeldPickupMassKg * FMath::Max(0.0f, PickupMassFeelMultiplier);
	const float LoadRatio = EffectiveHeldMassKg / MaxCarryMassKg;
	const float LoadAlpha = FMath::Clamp(LoadRatio, 0.0f, 1.0f);
	const float ResponsivenessScale = GetHeldPickupResponsivenessScale();
	const float HeavyDampingMultiplier = GetHeldPickupHeavyDampingMultiplier();
	const float LightSwingMultiplier = FMath::Lerp(
		FMath::Max(1.0f, PickupLightSwingDampingMultiplier),
		1.0f,
		LoadAlpha);
	const float MovementGuideScale = ResponsivenessScale / HeavyDampingMultiplier;
	const float RotationDriveScale = bPickupRotationModeActive ? GetHeldPickupRotationDriveScale() : 1.0f;
	const float RotationGuideStrength = bPickupRotationModeActive
		? FMath::Clamp(PickupRotationGuideStrength * RotationDriveScale, 0.05f, 0.85f)
		: 1.0f;
	const float RotationDampingMultiplier = bPickupRotationModeActive
		? FMath::Lerp(1.6f, 0.65f, FMath::Clamp(RotationDriveScale, 0.0f, 1.0f))
		: 1.0f;

	PickupPhysicsHandle->LinearStiffness = FMath::Max(0.0f, PickupHandleLinearStiffness * MovementGuideScale);
	PickupPhysicsHandle->LinearDamping = FMath::Max(
		0.0f,
		PickupHandleLinearDamping * LightSwingMultiplier * HeavyDampingMultiplier);
	PickupPhysicsHandle->AngularStiffness = FMath::Max(
		0.0f,
		PickupHandleAngularStiffness * MovementGuideScale * RotationGuideStrength);
	PickupPhysicsHandle->AngularDamping = FMath::Max(
		0.0f,
		PickupHandleAngularDamping * LightSwingMultiplier * RotationDampingMultiplier * HeavyDampingMultiplier);

	const float BaseInterpolationSpeed = FMath::Lerp(
		PickupLightInterpolationSpeed,
		PickupHeavyInterpolationSpeed,
		LoadAlpha);
	PickupPhysicsHandle->SetInterpolationSpeed(FMath::Max(0.05f, BaseInterpolationSpeed * MovementGuideScale));

	if (UPrimitiveComponent* HeldComponent = HeldPickupComponent.Get())
	{
		HeldComponent->SetLinearDamping(
			HeldPickupOriginalLinearDamping
			+ (PickupHeldBodyLinearDamping * LightSwingMultiplier * HeavyDampingMultiplier));
		HeldComponent->SetAngularDamping(
			HeldPickupOriginalAngularDamping
			+ (PickupHeldBodyAngularDamping * RotationDampingMultiplier * HeavyDampingMultiplier));
	}
}

void AAgentCharacter::AdjustClumsiness(float Delta, bool bShowFeedback)
{
	const float SafeMin = FMath::Max(1.0f, ClumsinessMinValue);
	const float SafeMax = FMath::Max(SafeMin, ClumsinessMaxValue);
	const float PreviousClumsinessValue = ClumsinessValue;
	const float NewClumsinessValue = FMath::Clamp(ClumsinessValue + Delta, SafeMin, SafeMax);
	if (FMath::IsNearlyEqual(NewClumsinessValue, PreviousClumsinessValue))
	{
		if (bShowClumsinessDebug)
		{
			ShowClumsinessDebug();
		}
		return;
	}

	ClumsinessValue = NewClumsinessValue;
	LastClumsinessEventSource = FName(TEXT("ClumsinessInput"));
	const float AppliedDelta = ClumsinessValue - PreviousClumsinessValue;
	const bool bLeveledUp = AppliedDelta > KINDA_SMALL_NUMBER;

	if (bLogClumsinessEvents)
	{
		UE_LOG(
			LogAgent,
			Log,
			TEXT("Clumsiness adjusted by %.4f to %.2f (range %.2f - %.2f)."),
			AppliedDelta,
			ClumsinessValue,
			SafeMin,
			SafeMax);
	}

	if (bLeveledUp)
	{
		UE_LOG(
			LogAgent,
			Log,
			TEXT("Clumsiness level up: +%.4f -> %.2f"),
			AppliedDelta,
			ClumsinessValue);

		if (GEngine)
		{
			const FString LevelUpMessage = FString::Printf(
				TEXT("Clumsiness Level Up: +%.4f  New Level: %.2f"),
				AppliedDelta,
				ClumsinessValue);
			GEngine->AddOnScreenDebugMessage(
				static_cast<uint64>(GetUniqueID()) + 19104ULL,
				1.5f,
				FColor::Green,
				LevelUpMessage);
		}
	}
	else if (bShowFeedback && GEngine)
	{
		const FString ClumsinessMessage = FString::Printf(
			TEXT("Clumsiness: %.2f  (Higher Value = %s clumsy)"),
			ClumsinessValue,
			bHigherClumsinessValueMeansMoreClumsy ? TEXT("more") : TEXT("less"));
		GEngine->AddOnScreenDebugMessage(
			static_cast<uint64>(GetUniqueID()) + 19100ULL,
			2.5f,
			FColor::Orange,
			ClumsinessMessage);
	}
}

float AAgentCharacter::GetMostClumsyAlpha() const
{
	const float SafeMin = FMath::Max(1.0f, ClumsinessMinValue);
	const float SafeMax = FMath::Max(SafeMin, ClumsinessMaxValue);
	const float NormalizedValue = FMath::GetMappedRangeValueClamped(
		FVector2D(SafeMin, SafeMax),
		FVector2D(0.0f, 1.0f),
		ClumsinessValue);
	return bHigherClumsinessValueMeansMoreClumsy ? NormalizedValue : (1.0f - NormalizedValue);
}

float AAgentCharacter::ComputeTripChance(float StepUpHeightCm, float HorizontalSpeedCmPerSec, bool bMovingBackward) const
{
	if (!bEnableClumsinessTrip)
	{
		return 0.0f;
	}

	const float StepHeightAlpha = FMath::GetMappedRangeValueClamped(
		FVector2D(TripStepUpMinHeightCm, FMath::Max(TripStepUpMinHeightCm + KINDA_SMALL_NUMBER, TripStepUpMaxHeightCm)),
		FVector2D(0.0f, 1.0f),
		StepUpHeightCm);
	const float CurvedStepAlpha = FMath::Pow(StepHeightAlpha, FMath::Max(0.1f, TripStepUpExponent));
	const float StepMultiplier = FMath::Lerp(
		FMath::Max(0.0f, TripStepUpMinMultiplier),
		FMath::Max(TripStepUpMinMultiplier, TripStepUpMaxMultiplier),
		CurvedStepAlpha);

	const float SpeedAlpha = FMath::GetMappedRangeValueClamped(
		FVector2D(TripSpeedMinCmPerSecond, FMath::Max(TripSpeedMinCmPerSecond + KINDA_SMALL_NUMBER, TripSpeedMaxCmPerSecond)),
		FVector2D(0.0f, 1.0f),
		HorizontalSpeedCmPerSec);
	const float SpeedMultiplier = FMath::Lerp(
		FMath::Max(0.0f, TripSpeedMinMultiplier),
		FMath::Max(TripSpeedMinMultiplier, TripSpeedMaxMultiplier),
		SpeedAlpha);

	const float ClumsinessMultiplier = FMath::Lerp(
		FMath::Max(0.0f, TripLeastClumsyMultiplier),
		FMath::Max(TripLeastClumsyMultiplier, TripMostClumsyMultiplier),
		GetMostClumsyAlpha());
	const float BackwardMultiplier = bMovingBackward ? FMath::Max(0.0f, TripBackwardMultiplier) : 1.0f;
	const float RawChance = FMath::Max(0.0f, TripBaseChance) * ClumsinessMultiplier * StepMultiplier * SpeedMultiplier * BackwardMultiplier;
	return FMath::Clamp(RawChance, 0.0f, FMath::Clamp(TripChanceMaxClamp, 0.0f, 1.0f));
}

float AAgentCharacter::ComputeTripRagdollFollowThroughChance() const
{
	const float LowSkill = FMath::Max(1.0f, TripFollowThroughLowSkillLevel);
	const float HighSkill = FMath::Max(LowSkill + KINDA_SMALL_NUMBER, TripFollowThroughHighSkillLevel);
	const float SkillAlpha = FMath::GetMappedRangeValueClamped(
		FVector2D(LowSkill, HighSkill),
		FVector2D(0.0f, 1.0f),
		ClumsinessValue);
	const float LowSkillChance = FMath::Clamp(TripFollowThroughRagdollChanceAtLowSkill, 0.0f, 1.0f);
	const float HighSkillChance = FMath::Clamp(TripFollowThroughRagdollChanceAtHighSkill, 0.0f, 1.0f);
	return FMath::Lerp(LowSkillChance, HighSkillChance, SkillAlpha);
}

float AAgentCharacter::ComputeDroneImpactKnockdownChance(float ClosingSpeed, bool bHeadHit, bool bLegHit) const
{
	if (!bEnableDroneImpactKnockdownChance)
	{
		return 0.0f;
	}

	const float MinSpeed = FMath::Max(0.0f, DroneImpactChanceMinClosingSpeed);
	const float MaxSpeed = FMath::Max(MinSpeed + KINDA_SMALL_NUMBER, DroneImpactChanceMaxClosingSpeed);
	const float SpeedAlpha = FMath::GetMappedRangeValueClamped(
		FVector2D(MinSpeed, MaxSpeed),
		FVector2D(0.0f, 1.0f),
		ClosingSpeed);
	float Chance = FMath::Lerp(
		FMath::Clamp(DroneImpactMinKnockdownChance, 0.0f, 1.0f),
		FMath::Clamp(DroneImpactMaxKnockdownChance, 0.0f, 1.0f),
		SpeedAlpha);

	if (bHeadHit)
	{
		Chance *= FMath::Max(0.0f, DroneImpactHeadHitChanceMultiplier);
	}

	if (bLegHit)
	{
		Chance *= FMath::Max(0.0f, DroneImpactLegHitChanceMultiplier);
	}

	return FMath::Clamp(Chance, 0.0f, FMath::Clamp(DroneImpactChanceMaxClamp, 0.0f, 1.0f));
}

void AAgentCharacter::HandleStepUpEvent(float StepUpHeightCm)
{
	LastStepUpHeightCm = StepUpHeightCm;

	if (!bEnableClumsinessTrip || IsRagdolling() || TripEventCooldownRemaining > 0.0f)
	{
		return;
	}

	const UCharacterMovementComponent* CharacterMovementComponent = GetCharacterMovement();
	if (!CharacterMovementComponent || !CharacterMovementComponent->IsMovingOnGround())
	{
		return;
	}

	const FVector HorizontalVelocity(GetVelocity().X, GetVelocity().Y, 0.0f);
	const float HorizontalSpeed = HorizontalVelocity.Size();
	LastTripHorizontalSpeed = HorizontalSpeed;
	if (bDisableTripWhileWalkMode && IsWalkModeActive())
	{
		LastComputedTripChance = 0.0f;
		LastTripRoll = -1.0f;
		LastTripFollowThroughRoll = -1.0f;
		LastTripFollowThroughChance = 0.0f;
		bLastTripMovingBackward = false;
		bLastTripTriggeredRagdoll = false;
		bLastTripFollowThroughPreventedRagdoll = false;
		LastClumsinessEventSource = FName(TEXT("WalkModeNoTrip"));
		return;
	}

	const bool bMovingBackward = !HorizontalVelocity.IsNearlyZero()
		&& FVector::DotProduct(HorizontalVelocity.GetSafeNormal(), GetActorForwardVector()) <= -FMath::Clamp(TripBackwardDotThreshold, 0.0f, 1.0f);
	bLastTripMovingBackward = bMovingBackward;

	const float TripChance = ComputeTripChance(StepUpHeightCm, HorizontalSpeed, bMovingBackward);
	LastComputedTripChance = TripChance;
	if (TripChance <= 0.0f)
	{
		LastTripRoll = -1.0f;
		LastTripFollowThroughRoll = -1.0f;
		LastTripFollowThroughChance = 0.0f;
		bLastTripFollowThroughPreventedRagdoll = false;
		bLastTripTriggeredRagdoll = false;
		return;
	}

	const float TripRoll = FMath::FRand();
	LastTripRoll = TripRoll;
	const bool bTripTriggered = TripRoll <= TripChance;
	bLastTripTriggeredRagdoll = bTripTriggered;
	bLastTripFollowThroughPreventedRagdoll = false;
	LastTripFollowThroughChance = 0.0f;
	LastTripFollowThroughRoll = -1.0f;

	bool bAllowRagdollFromTrip = true;
	if (bTripTriggered)
	{
		TripEventCooldownRemaining = FMath::Max(0.0f, TripEventCooldownSeconds);
		const float TripChanceSkillGain = FMath::Max(0.0f, TripSkillGainOnTrigger) * FMath::Clamp(TripChance, 0.0f, 1.0f);
		if (TripChanceSkillGain > KINDA_SMALL_NUMBER)
		{
			AdjustClumsiness(TripChanceSkillGain, false);
		}

		if (bEnableTripFollowThroughRoll)
		{
			LastTripFollowThroughChance = ComputeTripRagdollFollowThroughChance();
			LastTripFollowThroughRoll = FMath::FRand();
			bAllowRagdollFromTrip = LastTripFollowThroughRoll <= LastTripFollowThroughChance;
			bLastTripFollowThroughPreventedRagdoll = !bAllowRagdollFromTrip;
		}
		else
		{
			LastTripFollowThroughChance = 1.0f;
			LastTripFollowThroughRoll = 0.0f;
		}
	}
	bLastTripTriggeredRagdoll = bTripTriggered && bAllowRagdollFromTrip;

	if (bShowTripEventDebug && GEngine)
	{
		const FString FollowThroughChanceText = LastTripFollowThroughRoll < 0.0f
			? TEXT("--")
			: FString::Printf(TEXT("%.2f%%"), LastTripFollowThroughChance * 100.0f);
		const FString FollowThroughRollText = LastTripFollowThroughRoll < 0.0f
			? TEXT("--")
			: FString::Printf(TEXT("%.2f"), LastTripFollowThroughRoll);
		const FString TripEventMessage = FString::Printf(
			TEXT("Trip %.1f%%/%.2f  Save %s/%s  Step %.1fcm  Speed %.0f  Back %s  Inst %.1f"),
			TripChance * 100.0f,
			TripRoll,
			*FollowThroughChanceText,
			*FollowThroughRollText,
			StepUpHeightCm,
			HorizontalSpeed,
			bMovingBackward ? TEXT("Y") : TEXT("N"),
			InstabilityValue);
		GEngine->AddOnScreenDebugMessage(
			static_cast<uint64>(GetUniqueID()) + 19102ULL,
			FMath::Max(0.0f, TripEventDebugDuration),
			bLastTripTriggeredRagdoll ? FColor::Red : FColor::Green,
			TripEventMessage);
	}

	if (!bTripTriggered)
	{
		return;
	}

	if (bAllowRagdollFromTrip)
	{
		TryTriggerAutoRagdoll(EAgentRagdollReason::ClumsinessTrip, FName(TEXT("Trip")));
	}
}

void AAgentCharacter::UpdateClumsinessSystems(float DeltaSeconds)
{
	const float SafeDeltaSeconds = FMath::Max(0.0f, DeltaSeconds);
	AutoRagdollCooldownRemaining = FMath::Max(0.0f, AutoRagdollCooldownRemaining - SafeDeltaSeconds);
	TripEventCooldownRemaining = FMath::Max(0.0f, TripEventCooldownRemaining - SafeDeltaSeconds);

	const float SafeClumsinessMin = FMath::Max(1.0f, ClumsinessMinValue);
	const float SafeClumsinessMax = FMath::Max(SafeClumsinessMin, ClumsinessMaxValue);
	ClumsinessValue = FMath::Clamp(ClumsinessValue, SafeClumsinessMin, SafeClumsinessMax);
	TripFollowThroughLowSkillLevel = FMath::Max(1.0f, TripFollowThroughLowSkillLevel);
	TripFollowThroughHighSkillLevel = FMath::Max(TripFollowThroughLowSkillLevel, TripFollowThroughHighSkillLevel);
	MinFallHeightForRagdollCm = FMath::Max(0.0f, MinFallHeightForRagdollCm);
	ImpactVelocityMovingOtherMinSpeed = FMath::Max(0.0f, ImpactVelocityMovingOtherMinSpeed);
	ImpactVelocityRagdollThreshold = FMath::Max(0.0f, ImpactVelocityRagdollThreshold);
	DroneImpactMinDroneSpeed = FMath::Max(0.0f, DroneImpactMinDroneSpeed);
	DroneImpactChanceMinClosingSpeed = FMath::Max(0.0f, DroneImpactChanceMinClosingSpeed);
	DroneImpactChanceMaxClosingSpeed = FMath::Max(DroneImpactChanceMinClosingSpeed, DroneImpactChanceMaxClosingSpeed);
	DroneImpactHeadRadiusCm = FMath::Max(0.0f, DroneImpactHeadRadiusCm);
	DroneImpactLegMaxHeightFromFeetCm = FMath::Max(0.0f, DroneImpactLegMaxHeightFromFeetCm);

	const float SafeInstabilityMax = FMath::Max(1.0f, InstabilityMaxValue);
	InstabilityValue = FMath::Clamp(InstabilityValue, 0.0f, SafeInstabilityMax);

	if (bEnableInstabilityMeter && !IsRagdolling())
	{
		const float DecayAmount = FMath::Max(0.0f, InstabilityDecayPerSecond) * SafeDeltaSeconds;
		InstabilityValue = FMath::Clamp(InstabilityValue - DecayAmount, 0.0f, SafeInstabilityMax);

		if (InstabilityValue >= FMath::Clamp(InstabilityRagdollThreshold, 0.0f, SafeInstabilityMax))
		{
			TryTriggerAutoRagdoll(EAgentRagdollReason::Impact, FName(TEXT("InstabilityThreshold")));
		}
	}

	if (bShowClumsinessDebug)
	{
		ShowClumsinessDebug();
	}
}

void AAgentCharacter::AddInstability(float InstabilityDelta, EAgentRagdollReason ThresholdReason, FName SourceTag, bool bAllowThresholdRagdoll)
{
	if (!bEnableInstabilityMeter || InstabilityDelta <= 0.0f)
	{
		return;
	}

	const float SafeInstabilityMax = FMath::Max(1.0f, InstabilityMaxValue);
	InstabilityValue = FMath::Clamp(InstabilityValue + InstabilityDelta, 0.0f, SafeInstabilityMax);
	LastClumsinessEventSource = SourceTag;

	if (bLogClumsinessEvents)
	{
		UE_LOG(
			LogAgent,
			Log,
			TEXT("Instability +%.2f -> %.2f / %.2f (%s)."),
			InstabilityDelta,
			InstabilityValue,
			SafeInstabilityMax,
			*SourceTag.ToString());
	}

	if (bAllowThresholdRagdoll
		&& InstabilityValue >= FMath::Clamp(InstabilityRagdollThreshold, 0.0f, SafeInstabilityMax))
	{
		TryTriggerAutoRagdoll(ThresholdReason, FName(TEXT("InstabilityThreshold")));
	}
}

bool AAgentCharacter::TryTriggerAutoRagdoll(EAgentRagdollReason Reason, FName SourceTag)
{
	if (AutoRagdollCooldownRemaining > 0.0f || IsRagdolling())
	{
		return false;
	}

	const bool bEnteredRagdoll = RequestEnterRagdoll(Reason);
	if (bEnteredRagdoll)
	{
		AutoRagdollCooldownRemaining = FMath::Max(AutoRagdollCooldownRemaining, FMath::Max(0.0f, AutoRagdollCooldownSeconds));
		LastClumsinessEventSource = SourceTag;

		if (bLogClumsinessEvents)
		{
			UE_LOG(
				LogAgent,
				Log,
				TEXT("Auto ragdoll triggered (%s)."),
				*SourceTag.ToString());
		}
	}

	return bEnteredRagdoll;
}

float AAgentCharacter::ComputeImpactClosingSpeed(const UPrimitiveComponent* OtherComp, const FVector& HitNormal) const
{
	if (!OtherComp)
	{
		return 0.0f;
	}

	const FVector RelativeVelocity = OtherComp->GetComponentVelocity() - GetVelocity();
	if (RelativeVelocity.IsNearlyZero())
	{
		return 0.0f;
	}

	const FVector SafeHitNormal = HitNormal.GetSafeNormal();
	float ClosingSpeed = RelativeVelocity.Size();
	if (!SafeHitNormal.IsNearlyZero())
	{
		ClosingSpeed = FMath::Abs(FVector::DotProduct(RelativeVelocity, SafeHitNormal));
		if (ClosingSpeed <= KINDA_SMALL_NUMBER)
		{
			ClosingSpeed = RelativeVelocity.Size();
		}
	}

	return FMath::Max(0.0f, ClosingSpeed);
}

float AAgentCharacter::ComputeImpactMagnitude(const FVector& NormalImpulse, const UPrimitiveComponent* OtherComp, const FVector& HitNormal) const
{
	float ImpactMagnitude = NormalImpulse.Size();
	if (ImpactMagnitude > KINDA_SMALL_NUMBER)
	{
		return ImpactMagnitude;
	}

	if (!OtherComp)
	{
		return 0.0f;
	}

	const FVector RelativeVelocity = OtherComp->GetComponentVelocity() - GetVelocity();
	const FVector SafeHitNormal = HitNormal.GetSafeNormal();
	float ClosingSpeed = 0.0f;
	if (!SafeHitNormal.IsNearlyZero())
	{
		ClosingSpeed = FMath::Max(0.0f, FVector::DotProduct(RelativeVelocity, -SafeHitNormal));
	}
	else
	{
		ClosingSpeed = RelativeVelocity.Size();
	}

	if (ClosingSpeed <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const float SourceMass = OtherComp->IsSimulatingPhysics() ? FMath::Max(1.0f, OtherComp->GetMass()) : 1.0f;
	ImpactMagnitude = ClosingSpeed * SourceMass;
	return ImpactMagnitude;
}

void AAgentCharacter::ShowClumsinessDebug() const
{
	if (!GEngine)
	{
		return;
	}

	const float SafeInstabilityMax = FMath::Max(1.0f, InstabilityMaxValue);
	const FString LastSource = LastClumsinessEventSource.IsNone() ? TEXT("-") : LastClumsinessEventSource.ToString();
	const FString TripRollText = LastTripRoll < 0.0f
		? TEXT("--")
		: FString::Printf(TEXT("%.2f"), LastTripRoll);
	const FString TripFollowThroughRollText = LastTripFollowThroughRoll < 0.0f
		? TEXT("--")
		: FString::Printf(TEXT("%.2f"), LastTripFollowThroughRoll);
	const FString TripFollowThroughChanceText = LastTripFollowThroughRoll < 0.0f
		? TEXT("--")
		: FString::Printf(TEXT("%.2f%%"), LastTripFollowThroughChance * 100.0f);
	const TCHAR* SafeModeText = IsWalkModeActive() ? TEXT("ON") : TEXT("OFF");
	const FString ClumsinessDebugMessage = FString::Printf(
		TEXT("Clumsy %.2f (A %.2f)  Safe %s  Inst %.1f/%.1f  Trip %.1f%%/%s  Save %s/%s  Step %.1fcm  Speed %.0f  FallH %.0f  FallV %.0f  HitI %.0f  HitV %.0f  Src %s"),
		ClumsinessValue,
		GetMostClumsyAlpha(),
		SafeModeText,
		InstabilityValue,
		SafeInstabilityMax,
		LastComputedTripChance * 100.0f,
		*TripRollText,
		*TripFollowThroughChanceText,
		*TripFollowThroughRollText,
		LastStepUpHeightCm,
		LastTripHorizontalSpeed,
		LastFallHeightCm,
		LastFallImpactSpeed,
		LastImpactMagnitude,
		LastImpactClosingSpeed,
		*LastSource);
	GEngine->AddOnScreenDebugMessage(
		static_cast<uint64>(GetUniqueID()) + 19101ULL,
		0.0f,
		FColor::Cyan,
		ClumsinessDebugMessage);
}

void AAgentCharacter::AdjustPickupStrength(float Delta)
{
	const float NewStrength = FMath::Max(0.0f, CharacterPickupStrengthMultiplier + Delta);
	if (FMath::IsNearlyEqual(NewStrength, CharacterPickupStrengthMultiplier))
	{
		ShowPickupStrengthDebug();
		return;
	}

	CharacterPickupStrengthMultiplier = NewStrength;
	DronePickupStrengthMultiplier = CharacterPickupStrengthMultiplier * 0.1f;

	if (DroneCompanion && DroneCompanion->IsLiftAssistActive())
	{
		SyncDroneLiftAssistTuning();
	}

	if (HeldPickupComponent.IsValid())
	{
		SyncPickupHandleSettings();
	}

	ShowPickupStrengthDebug();
}

void AAgentCharacter::ShowPickupStrengthDebug() const
{
	if (!GEngine)
	{
		return;
	}

	const float CarryMassScaleKg = FMath::Max(1.0f, PickupStrengthMassScaleKg);
	const float PlayerCarryMassKg = FMath::Max(0.0f, CharacterPickupStrengthMultiplier) * CarryMassScaleKg;
	const float DroneCarryMassKg = FMath::Max(0.0f, DronePickupStrengthMultiplier) * CarryMassScaleKg;
	const FString StrengthMessage = FString::Printf(
		TEXT("Pickup Strength  Player: %.2f (%.0f kg)  Drone: %.2f (%.0f kg)  Feel x%.1f"),
		CharacterPickupStrengthMultiplier,
		PlayerCarryMassKg,
		DronePickupStrengthMultiplier,
		DroneCarryMassKg,
		PickupMassFeelMultiplier);
	GEngine->AddOnScreenDebugMessage(
		static_cast<uint64>(GetUniqueID()) + 19000ULL,
		2.5f,
		FColor::Yellow,
		StrengthMessage);
}

void AAgentCharacter::UpdateDronePilotInputs(float DeltaSeconds)
{
	if (!DroneCompanion || !IsDroneInputModeActive())
	{
		return;
	}

	const bool bSimplePilotMode = !bMapModeActive && IsSimpleDronePilotMode();
	const bool bFreeFlyPilotMode = !bMapModeActive && IsFreeFlyDronePilotMode();
	const bool bSpectatorPilotMode = !bMapModeActive && IsSpectatorDronePilotMode();
	const bool bRollPilotMode = !bMapModeActive && IsRollDronePilotMode();
	const float KeyboardForwardInput = (bDronePitchForwardHeld ? 1.0f : 0.0f) - (bDronePitchBackwardHeld ? 1.0f : 0.0f);
	float KeyboardRightInput = (bDroneRollRightHeld ? 1.0f : 0.0f) - (bDroneRollLeftHeld ? 1.0f : 0.0f);
	float KeyboardVerticalInput = 0.0f;
	float KeyboardYawInput = 0.0f;

	if (bMapModeActive)
	{
		const float KeyboardUpInput = (bDroneYawRightHeld ? 1.0f : 0.0f) + (bDroneThrottleUpHeld ? 1.0f : 0.0f);
		const float KeyboardDownInput = (bDroneYawLeftHeld ? 1.0f : 0.0f) + (bDroneThrottleDownHeld ? 1.0f : 0.0f);
		KeyboardVerticalInput = FMath::Clamp(KeyboardUpInput - KeyboardDownInput, -1.0f, 1.0f);
	}
	else if (bSimplePilotMode)
	{
		KeyboardYawInput = (bDroneYawRightHeld ? 1.0f : 0.0f) - (bDroneYawLeftHeld ? 1.0f : 0.0f);
		KeyboardVerticalInput = (bDroneThrottleUpHeld ? 1.0f : 0.0f) - (bDroneThrottleDownHeld ? 1.0f : 0.0f);
	}
	else if (bFreeFlyPilotMode || bSpectatorPilotMode)
	{
		const float KeyboardUpInput = (bDroneYawRightHeld ? 1.0f : 0.0f) + (bDroneThrottleUpHeld ? 1.0f : 0.0f);
		const float KeyboardDownInput = (bDroneYawLeftHeld ? 1.0f : 0.0f) + (bDroneThrottleDownHeld ? 1.0f : 0.0f);
		KeyboardVerticalInput = FMath::Clamp(KeyboardUpInput - KeyboardDownInput, -1.0f, 1.0f);
	}
	else if (bRollPilotMode)
	{
		KeyboardYawInput = 0.0f;
		KeyboardVerticalInput = 0.0f;
	}
	else
	{
		KeyboardYawInput = (bDroneYawRightHeld ? 1.0f : 0.0f) - (bDroneYawLeftHeld ? 1.0f : 0.0f);
		KeyboardVerticalInput = (bDroneThrottleUpHeld ? 1.0f : 0.0f) - (bDroneThrottleDownHeld ? 1.0f : 0.0f);
	}

	float GamepadVerticalInput = DroneGamepadThrottleInput;
	float GamepadYawInput = DroneGamepadYawInput;
	float GamepadRightInput = DroneGamepadRollInput;
	float GamepadForwardInput = DroneGamepadPitchInput;

	if (bMapModeActive)
	{
		GamepadVerticalInput = DroneGamepadLeftTriggerInput - DroneGamepadRightTriggerInput;
		GamepadYawInput = 0.0f;
		GamepadRightInput = DroneGamepadYawInput;
		GamepadForwardInput = DroneGamepadThrottleInput;
	}
	else if (bSimplePilotMode)
	{
		GamepadVerticalInput = DroneGamepadRightTriggerInput - DroneGamepadLeftTriggerInput;
		GamepadYawInput = DroneGamepadRollInput;
		GamepadRightInput = DroneGamepadYawInput;
		GamepadForwardInput = DroneGamepadThrottleInput;

		if (FMath::Abs(DroneGamepadPitchInput) > KINDA_SMALL_NUMBER)
		{
			DroneCompanion->AdjustCameraTilt(DroneGamepadPitchInput * DroneCompanion->SimpleCameraTiltAxisSpeed * DeltaSeconds);
		}
	}
	else if (bFreeFlyPilotMode || bSpectatorPilotMode)
	{
		GamepadVerticalInput = DroneGamepadRightTriggerInput - DroneGamepadLeftTriggerInput;
		GamepadYawInput = 0.0f;
		GamepadRightInput = DroneGamepadYawInput;
		GamepadForwardInput = DroneGamepadThrottleInput;
	}
	else if (bRollPilotMode)
	{
		GamepadVerticalInput = 0.0f;
		GamepadYawInput = 0.0f;
		GamepadRightInput = DroneGamepadYawInput;
		GamepadForwardInput = DroneGamepadThrottleInput;
	}

	const float VerticalInput = FMath::Clamp(KeyboardVerticalInput + GamepadVerticalInput, -1.0f, 1.0f);
	const float YawInput = FMath::Clamp(KeyboardYawInput + GamepadYawInput, -1.0f, 1.0f);
	const float RightInput = FMath::Clamp(KeyboardRightInput + GamepadRightInput, -1.0f, 1.0f);
	const float ForwardInput = FMath::Clamp(KeyboardForwardInput + GamepadForwardInput, -1.0f, 1.0f);

	if (bDroneEntryAssistActive
		&& IsDronePilotMode()
		&& !bFreeFlyPilotMode
		&& !bSpectatorPilotMode
		&& !bRollPilotMode
		&& FMath::Abs(VerticalInput) > FMath::Max(0.0f, DroneEntryAssistReleaseThreshold))
	{
		bDroneEntryAssistActive = false;
		ApplyDroneAssistState();
	}

	DroneCompanion->SetPilotInputs(VerticalInput, YawInput, RightInput, ForwardInput);
}

void AAgentCharacter::ResetDroneInputState()
{
	DroneGamepadThrottleInput = 0.0f;
	DroneGamepadYawInput = 0.0f;
	DroneGamepadRollInput = 0.0f;
	DroneGamepadPitchInput = 0.0f;
	DroneGamepadLeftTriggerInput = 0.0f;
	DroneGamepadRightTriggerInput = 0.0f;
	bDronePitchForwardHeld = false;
	bDronePitchBackwardHeld = false;
	bDroneRollLeftHeld = false;
	bDroneRollRightHeld = false;
	bDroneYawLeftHeld = false;
	bDroneYawRightHeld = false;
	bDroneThrottleUpHeld = false;
	bDroneThrottleDownHeld = false;
	bRollModeHoldStartedInFlight = false;
	bPendingRollReleaseToFlight = false;
	bCrashRollRecoveryActive = false;
	bRollModeJumpHeld = false;
	CrashRollRecoveryStableTime = 0.0f;
	RollToFlightAssistTimeRemaining = 0.0f;
	RollModeJumpHeldDuration = 0.0f;
	StoredRollJumpChargeAlpha = 0.0f;

	if (DroneCompanion)
	{
		DroneCompanion->ResetPilotInputs();
	}
}

void AAgentCharacter::ResetCharacterCameraRoll()
{
	if (AController* LocalController = GetController())
	{
		FRotator ResetRotation = LocalController->GetControlRotation();
		ResetRotation.Pitch = 0.0f;
		ResetRotation.Roll = 0.0f;
		LocalController->SetControlRotation(ResetRotation);
	}

	if (CameraBoom)
	{
		FRotator BoomRotation = CameraBoom->GetRelativeRotation();
		BoomRotation.Pitch = 0.0f;
		BoomRotation.Roll = 0.0f;
		CameraBoom->SetRelativeRotation(BoomRotation);
	}

	if (UCameraComponent* const ActiveFirstPersonCamera = ResolveFirstPersonCamera())
	{
		FRotator CameraRotation = ActiveFirstPersonCamera->GetRelativeRotation();
		CameraRotation.Pitch = 0.0f;
		CameraRotation.Roll = 0.0f;
		ActiveFirstPersonCamera->SetRelativeRotation(CameraRotation);
	}

	if (ThirdPersonTransitionCamera)
	{
		FRotator CameraRotation = ThirdPersonTransitionCamera->GetRelativeRotation();
		CameraRotation.Pitch = 0.0f;
		CameraRotation.Roll = 0.0f;
		ThirdPersonTransitionCamera->SetRelativeRotation(CameraRotation);
	}
}

void AAgentCharacter::UpdateDronePilotCameraLimits()
{
	APlayerController* const PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController || !PlayerController->PlayerCameraManager)
	{
		return;
	}

	APlayerCameraManager* const CameraManager = PlayerController->PlayerCameraManager;
	if (!bHasStoredDefaultViewPitchLimits)
	{
		DefaultViewPitchMin = CameraManager->ViewPitchMin;
		DefaultViewPitchMax = CameraManager->ViewPitchMax;
		bHasStoredDefaultViewPitchLimits = true;
	}

	const float FreeFlyPitchLimit = FMath::Clamp(FreeFlyUnlockedPitchLimit, 90.0f, 179.0f);
	const float CurrentPitch = FRotator::NormalizeAxis(GetControlRotation().Pitch);
	const float MinPitch = FMath::Min(DefaultViewPitchMin, DefaultViewPitchMax);
	const float MaxPitch = FMath::Max(DefaultViewPitchMin, DefaultViewPitchMax);
	const bool bNeedPitchUnwind = CurrentPitch < MinPitch || CurrentPitch > MaxPitch;
	const bool bUseUnlockedPitch = IsDronePilotMode()
		&& !bMapModeActive
		&& !bMiniMapModeActive
		&& (IsFreeFlyDronePilotMode() || bNeedPitchUnwind);

	CameraManager->ViewPitchMin = bUseUnlockedPitch ? -FreeFlyPitchLimit : DefaultViewPitchMin;
	CameraManager->ViewPitchMax = bUseUnlockedPitch ? FreeFlyPitchLimit : DefaultViewPitchMax;
}

void AAgentCharacter::UpdateRollModeJumpHold(float DeltaSeconds)
{
	RollToFlightAssistTimeRemaining = FMath::Max(0.0f, RollToFlightAssistTimeRemaining - DeltaSeconds);

	if (DroneCompanion
		&& bUseCrashRollRecovery
		&& IsDronePilotMode()
		&& !bMapModeActive
		&& !bMiniMapModeActive
		&& DroneCompanion->ConsumePendingCrashRollRecovery())
	{
		EnterRollTransitionMode(false, true);
	}

	if (bCrashRollRecoveryActive)
	{
		if (!DroneCompanion || !IsDronePilotMode() || !IsRollDronePilotMode())
		{
			bCrashRollRecoveryActive = false;
			CrashRollRecoveryStableTime = 0.0f;
		}
		else if (DroneCompanion->IsStableForRollRecovery())
		{
			CrashRollRecoveryStableTime += FMath::Max(0.0f, DeltaSeconds);
			if (CrashRollRecoveryStableTime >= FMath::Max(0.0f, CrashRollRecoveryStableDelay))
			{
				ExitRollTransitionMode(true);
				return;
			}
		}
		else
		{
			CrashRollRecoveryStableTime = 0.0f;
		}
	}

	if (!bRollModeJumpHeld)
	{
		return;
	}

	if (!IsDronePilotMode() || !IsRollDronePilotMode() || !DroneCompanion)
	{
		bRollModeJumpHeld = false;
		CrashRollRecoveryStableTime = 0.0f;
		RollModeJumpHeldDuration = 0.0f;
		return;
	}

	RollModeJumpHeldDuration += FMath::Max(0.0f, DeltaSeconds);
}

void AAgentCharacter::SyncControllerRotationToDroneCamera()
{
	if (!DroneCompanion || !GetController())
	{
		return;
	}

	FVector DroneCameraLocation = FVector::ZeroVector;
	FRotator DroneCameraRotation = FRotator::ZeroRotator;
	if (!DroneCompanion->GetDroneCameraTransform(DroneCameraLocation, DroneCameraRotation))
	{
		return;
	}

	GetController()->SetControlRotation(DroneCameraRotation);
	DroneCompanion->SetViewReferenceRotation(DroneCameraRotation);
}

void AAgentCharacter::RefreshPrimaryDroneAvailabilityFromCompanion()
{
	CleanupInvalidDroneCompanions();
	if (!DroneCompanion)
	{
		if (bPrimaryDroneAvailable)
		{
			bPrimaryDroneAvailable = false;
		}
		return;
	}

	PersistedPrimaryDroneBatteryPercent = FMath::Clamp(DroneCompanion->GetBatteryPercent(), 0.0f, 100.0f);
	// Availability is reserved for "can this drone be used at all?" (battery alive and not manually powered off).
	// Charger lock/idle behavior is handled independently inside ADroneCompanion.
	const bool bShouldBeAvailable =
		!DroneCompanion->IsBatteryDepleted()
		&& !DroneCompanion->IsManualPowerOffRequested();
	if (bShouldBeAvailable != bPrimaryDroneAvailable)
	{
		SetPrimaryDroneAvailable(bShouldBeAvailable, true);
	}
}

void AAgentCharacter::SetPrimaryDroneAvailable(bool bNewAvailable, bool bForceFirstPersonIfUnavailable)
{
	bPrimaryDroneAvailable = bNewAvailable;
	if (bPrimaryDroneAvailable)
	{
		EnsureActiveDroneSelection();
		if (!DroneCompanion && CurrentViewMode == EAgentViewMode::FirstPerson && !bMapModeActive && !bMiniMapModeActive)
		{
			SpawnDroneCompanion();
		}

		ApplyDroneFleetContext(true, false);
		return;
	}

	if (DroneCompanion)
	{
		PersistedPrimaryDroneBatteryPercent = FMath::Clamp(DroneCompanion->GetBatteryPercent(), 0.0f, 100.0f);
		DroneCompanion->ResetPilotInputs();
		DroneCompanion->SetCompanionMode(EDroneCompanionMode::Idle);
	}

	ResetDroneInputState();
	const bool bShouldForceFirstPerson =
		(bForceFirstPersonWhenDroneUnavailable || bForceFirstPersonIfUnavailable)
		&& (CurrentViewMode != EAgentViewMode::FirstPerson || bMapModeActive || bMiniMapModeActive);
	if (bShouldForceFirstPerson)
	{
		ApplyViewMode(EAgentViewMode::FirstPerson, true);
	}
}

bool AAgentCharacter::IsDronePilotMode() const
{
	return CurrentViewMode == EAgentViewMode::DronePilot;
}

bool AAgentCharacter::IsDroneInputModeActive() const
{
	return bPrimaryDroneAvailable && (IsDronePilotMode() || bMapModeActive);
}

bool AAgentCharacter::IsSimpleDronePilotMode() const
{
	return DronePilotControlMode == EAgentDronePilotControlMode::Simple;
}

bool AAgentCharacter::IsFreeFlyDronePilotMode() const
{
	return DronePilotControlMode == EAgentDronePilotControlMode::FreeFly;
}

bool AAgentCharacter::IsSpectatorDronePilotMode() const
{
	return DronePilotControlMode == EAgentDronePilotControlMode::SpectatorCam;
}

bool AAgentCharacter::IsRollDronePilotMode() const
{
	return DronePilotControlMode == EAgentDronePilotControlMode::Roll;
}

void AAgentCharacter::ToggleDronePilotControlMode()
{
	CycleDronePilotControlMode(1);
}

void AAgentCharacter::SetDronePilotControlMode(EAgentDronePilotControlMode NewMode)
{
	if (DroneCompanion && IsDroneInputModeActive())
	{
		SyncControllerRotationToDroneCamera();
	}

	if (NewMode != EAgentDronePilotControlMode::Roll)
	{
		bRollModeHoldStartedInFlight = false;
		bPendingRollReleaseToFlight = false;
		bRollModeJumpHeld = false;
		RollModeJumpHeldDuration = 0.0f;
		bCrashRollRecoveryActive = false;
		CrashRollRecoveryStableTime = 0.0f;
		StoredRollJumpChargeAlpha = 0.0f;
		LastFlightDronePilotControlMode = NewMode;
	}

	DronePilotControlMode = NewMode;

	switch (DronePilotControlMode)
	{
	case EAgentDronePilotControlMode::Roll:
		bUseSimpleDronePilotControls = false;
		bDroneStabilizerEnabled = false;
		bDroneHoverModeEnabled = false;
		break;
	case EAgentDronePilotControlMode::FreeFly:
	case EAgentDronePilotControlMode::SpectatorCam:
		bUseSimpleDronePilotControls = false;
		bDroneStabilizerEnabled = false;
		bDroneHoverModeEnabled = false;
		break;
	case EAgentDronePilotControlMode::Simple:
		bUseSimpleDronePilotControls = true;
		bDroneStabilizerEnabled = true;
		bDroneHoverModeEnabled = true;
		break;
	case EAgentDronePilotControlMode::HorizonHover:
		bUseSimpleDronePilotControls = false;
		bDroneStabilizerEnabled = true;
		bDroneHoverModeEnabled = true;
		break;
	case EAgentDronePilotControlMode::Horizon:
		bUseSimpleDronePilotControls = false;
		bDroneStabilizerEnabled = true;
		bDroneHoverModeEnabled = false;
		break;
	case EAgentDronePilotControlMode::Complex:
	default:
		bUseSimpleDronePilotControls = false;
		bDroneStabilizerEnabled = false;
		bDroneHoverModeEnabled = false;
		break;
	}

	if (DroneCompanion)
	{
		SyncDroneCompanionControlState(IsDronePilotMode() && !bMapModeActive);
	}
}

void AAgentCharacter::CycleDronePilotControlMode(int32 Direction)
{
	constexpr int32 PilotModeCount = 7;
	const int32 CurrentIndex = static_cast<int32>(DronePilotControlMode);
	const int32 WrappedIndex = (CurrentIndex + Direction + PilotModeCount) % PilotModeCount;
	SetDronePilotControlMode(static_cast<EAgentDronePilotControlMode>(WrappedIndex));

	if (DroneCompanion
		&& (DroneCompanion->GetCompanionMode() == EDroneCompanionMode::MapMode
			|| DroneCompanion->GetCompanionMode() == EDroneCompanionMode::MiniMapFollow))
	{
		SyncDroneCompanionControlState(false, true);
	}
}

void AAgentCharacter::ToggleMapMode()
{
	if (IsRagdolling() && !bAllowViewModeChangesWhileRagdoll)
	{
		return;
	}

	if (!bPrimaryDroneAvailable)
	{
		if (bForceFirstPersonWhenDroneUnavailable && CurrentViewMode != EAgentViewMode::FirstPerson)
		{
			ApplyViewMode(EAgentViewMode::FirstPerson, true);
		}

		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());

	if (bMiniMapModeActive)
	{
		ExitMiniMapMode();
		return;
	}

	if (!bMapModeActive)
	{
		SyncControllerRotationToDroneCamera();

		if (bConveyorPlacementModeActive)
		{
			ExitConveyorPlacementMode();
		}

		EnsureActiveDroneSelection();
		if (!DroneCompanion)
		{
			FVector DroneSpawnLocation = FVector::ZeroVector;
			FRotator DroneSpawnRotation = FRotator::ZeroRotator;
			if (CurrentViewMode == EAgentViewMode::ThirdPerson
				&& ThirdPersonTransitionCamera
				&& ThirdPersonTransitionCamera->IsActive())
			{
				DroneSpawnLocation = ThirdPersonTransitionCamera->GetComponentLocation();
				DroneSpawnRotation = ThirdPersonTransitionCamera->GetComponentRotation();
			}
			else
			{
				GetThirdPersonDroneTarget(DroneSpawnLocation, DroneSpawnRotation);
			}
			SpawnDroneCompanionAtTransform(DroneSpawnLocation, DroneSpawnRotation, EDroneCompanionMode::MapMode, true);
		}

		if (!DroneCompanion)
		{
			return;
		}

		SetThirdPersonProxyVisible(false);
		bMapModeActive = true;
		ApplyDroneFleetContext(true, true);

		return;
	}

	bMapModeActive = false;
	ApplyViewMode(CurrentViewMode, true);
}

void AAgentCharacter::EnterMiniMapMode()
{
	if (IsRagdolling() && !bAllowViewModeChangesWhileRagdoll)
	{
		return;
	}

	if (!bPrimaryDroneAvailable)
	{
		if (bForceFirstPersonWhenDroneUnavailable && CurrentViewMode != EAgentViewMode::FirstPerson)
		{
			ApplyViewMode(EAgentViewMode::FirstPerson, true);
		}

		return;
	}

	if (bMiniMapModeActive || bMapModeActive)
	{
		return;
	}

	if (bConveyorPlacementModeActive)
	{
		ExitConveyorPlacementMode();
	}

	MiniMapReturnViewMode = CurrentViewMode;
	EnsureActiveDroneSelection();
	if (!DroneCompanion)
	{
		return;
	}

	const bool bAssignedDifferentMiniMapDrone = MiniMapLockedDrone.Get() != DroneCompanion.Get();
	MiniMapLockedDrone = DroneCompanion;
	bMiniMapModeActive = true;
	bMiniMapViewingDroneCamera = false;
	SetThirdPersonProxyVisible(false);
	ApplyDroneFleetContext(false, false);
	if (bAssignedDifferentMiniMapDrone)
	{
		CycleActiveDrone(1);
	}
}

void AAgentCharacter::ExitMiniMapMode()
{
	if (!bMiniMapModeActive)
	{
		return;
	}

	MiniMapLockedDrone = nullptr;
	bMiniMapModeActive = false;
	bMiniMapViewingDroneCamera = false;
	ApplyDroneFleetContext(true, true);
}

void AAgentCharacter::FocusMiniMapDroneCamera()
{
	if (IsRagdolling() && !bAllowViewModeChangesWhileRagdoll)
	{
		return;
	}

	ADroneCompanion* MiniMapDrone = MiniMapLockedDrone ? MiniMapLockedDrone.Get() : DroneCompanion.Get();
	if (!bMiniMapModeActive || !MiniMapDrone)
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (!PlayerController)
	{
		return;
	}

	bMiniMapViewingDroneCamera = true;
	PlayerController->SetViewTargetWithBlend(MiniMapDrone, ViewBlendTime);
}

void AAgentCharacter::UpdateControllerMapButtonHold(float DeltaSeconds)
{
	if (IsRagdolling())
	{
		return;
	}

	if (!bControllerMapButtonHeld || bControllerMapButtonTriggeredMiniMap)
	{
		return;
	}

	ControllerMapButtonHeldDuration += FMath::Max(0.0f, DeltaSeconds);
	if (ControllerMapButtonHeldDuration >= FMath::Max(0.0f, ControllerMapButtonHoldTime))
	{
		bControllerMapButtonTriggeredMiniMap = true;
		if (bMiniMapModeActive)
		{
			ExitMiniMapMode();
		}
		else if (!bMapModeActive)
		{
			EnterMiniMapMode();
		}
	}
}

void AAgentCharacter::UpdateKeyboardMapButtonHold(float DeltaSeconds)
{
	if (IsRagdolling())
	{
		return;
	}

	if (!bKeyboardMapButtonHeld || bKeyboardMapButtonTriggeredMiniMap)
	{
		return;
	}

	KeyboardMapButtonHeldDuration += FMath::Max(0.0f, DeltaSeconds);
	if (KeyboardMapButtonHeldDuration >= FMath::Max(0.0f, ControllerMapButtonHoldTime))
	{
		bKeyboardMapButtonTriggeredMiniMap = true;
		if (bMiniMapModeActive)
		{
			ExitMiniMapMode();
		}
		else if (!bMapModeActive)
		{
			EnterMiniMapMode();
		}
	}
}

void AAgentCharacter::DoMove(float Right, float Forward)
{
	if (IsRagdolling())
	{
		return;
	}

	if (IsDroneInputModeActive() && bLockCharacterMovementDuringDronePilot)
	{
		return;
	}

	if (VehicleInteractionComponent && VehicleInteractionComponent->ApplyVehicleMoveInput(Forward, Right))
	{
		return;
	}

	if (GetController() == nullptr)
	{
		return;
	}

	const FRotator Rotation = GetController()->GetControlRotation();
	const FRotator YawRotation(0.0f, Rotation.Yaw, 0.0f);
	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(ForwardDirection, Forward);
	AddMovementInput(RightDirection, Right);
}

void AAgentCharacter::DoLook(float Yaw, float Pitch)
{
	if (IsRagdolling() && !bAllowLookInputWhileRagdoll)
	{
		return;
	}

	if (IsDroneInputModeActive()
		&& bLockCharacterMovementDuringDronePilot
		&& !(IsDronePilotMode() && (IsFreeFlyDronePilotMode() || IsSpectatorDronePilotMode() || IsRollDronePilotMode())))
	{
		return;
	}

	if (GetController() == nullptr)
	{
		return;
	}

	AddControllerYawInput(Yaw);
	AddControllerPitchInput(Pitch);
}

void AAgentCharacter::DoJumpStart()
{
	if (IsRagdolling())
	{
		RequestExitRagdoll(EAgentRagdollReason::ManualInput);
		return;
	}

	if (VehicleInteractionComponent && VehicleInteractionComponent->IsControllingVehicle())
	{
		VehicleInteractionComponent->SetVehicleHandbrakeInput(true);
		return;
	}

	if (IsDronePilotMode() && DroneCompanion && !bMapModeActive && !bMiniMapModeActive)
	{
		if (!IsRollDronePilotMode())
		{
			EnterRollTransitionMode(true, false);
		}

		if (!bRollModeJumpHeld)
		{
			bRollModeJumpHeld = true;
			RollModeJumpHeldDuration = 0.0f;
		}
		return;
	}

	if (IsDroneInputModeActive() && bLockCharacterMovementDuringDronePilot)
	{
		return;
	}

	Jump();
}

void AAgentCharacter::DoJumpEnd()
{
	if (IsRagdolling())
	{
		return;
	}

	if (VehicleInteractionComponent && VehicleInteractionComponent->IsControllingVehicle())
	{
		VehicleInteractionComponent->SetVehicleHandbrakeInput(false);
		return;
	}

	if (bRollModeJumpHeld)
	{
		const float HeldDuration = RollModeJumpHeldDuration;
		const float ChargeAlpha = RollModeJumpHoldTime > KINDA_SMALL_NUMBER
			? FMath::Clamp(HeldDuration / RollModeJumpHoldTime, 0.0f, 1.0f)
			: 1.0f;
		bRollModeJumpHeld = false;
		RollModeJumpHeldDuration = 0.0f;

		if (IsDronePilotMode() && IsRollDronePilotMode() && DroneCompanion)
		{
			if (bRollModeHoldStartedInFlight)
			{
				bRollModeHoldStartedInFlight = false;
				StoredRollJumpChargeAlpha = FMath::Max(StoredRollJumpChargeAlpha, ChargeAlpha);
				return;
			}

			const float EffectiveChargeAlpha = FMath::Max(StoredRollJumpChargeAlpha, ChargeAlpha);
			StoredRollJumpChargeAlpha = 0.0f;
			if (!DroneCompanion->TryRollJump(EffectiveChargeAlpha))
			{
				ExitRollTransitionMode(true);
			}
		}
		return;
	}

	if (IsDronePilotMode() && IsRollDronePilotMode())
	{
		return;
	}

	if (IsDroneInputModeActive() && bLockCharacterMovementDuringDronePilot)
	{
		return;
	}

	StopJumping();
}

void AAgentCharacter::OnDronePitchForwardPressed()
{
	bDronePitchForwardHeld = true;
}

void AAgentCharacter::OnDronePitchForwardReleased()
{
	bDronePitchForwardHeld = false;
}

void AAgentCharacter::OnDronePitchBackwardPressed()
{
	bDronePitchBackwardHeld = true;
}

void AAgentCharacter::OnDronePitchBackwardReleased()
{
	bDronePitchBackwardHeld = false;
}

void AAgentCharacter::OnDroneRollLeftPressed()
{
	bDroneRollLeftHeld = true;
}

void AAgentCharacter::OnDroneRollLeftReleased()
{
	bDroneRollLeftHeld = false;
}

void AAgentCharacter::OnDroneRollRightPressed()
{
	bDroneRollRightHeld = true;
}

void AAgentCharacter::OnDroneRollRightReleased()
{
	bDroneRollRightHeld = false;
}

void AAgentCharacter::OnDroneYawLeftPressed()
{
	if (bConveyorPlacementModeActive)
	{
		bPlacementRotateLeftHeld = true;
		bPlacementRotateRightHeld = false;
		FactoryPlacementRotationHoldDirection = -1;
		FactoryPlacementRotationHoldTimeRemaining = FMath::Max(0.0f, FreePlacementRotationHoldDelay);
		RotateConveyorPlacement(-1);
		return;
	}

	bDroneYawLeftHeld = true;
}

void AAgentCharacter::OnDroneYawLeftReleased()
{
	if (bConveyorPlacementModeActive)
	{
		bPlacementRotateLeftHeld = false;
		if (!bPlacementRotateRightHeld)
		{
			FactoryPlacementRotationHoldDirection = 0;
			FactoryPlacementRotationHoldTimeRemaining = 0.0f;
		}
		return;
	}

	bDroneYawLeftHeld = false;
}

void AAgentCharacter::OnDroneYawRightPressed()
{
	bDroneYawRightHeld = true;
}

void AAgentCharacter::OnDroneYawRightReleased()
{
	bDroneYawRightHeld = false;
}

void AAgentCharacter::OnDroneThrottleUpPressed()
{
	if (bConveyorPlacementModeActive)
	{
		RotateConveyorPlacement(1);
		return;
	}

	bDroneThrottleUpHeld = true;
}

void AAgentCharacter::OnDroneThrottleUpReleased()
{
	if (bConveyorPlacementModeActive)
	{
		return;
	}

	bDroneThrottleUpHeld = false;
}

void AAgentCharacter::OnDroneThrottleDownPressed()
{
	bDroneThrottleDownHeld = true;
}

void AAgentCharacter::OnDroneThrottleDownReleased()
{
	bDroneThrottleDownHeld = false;
}

void AAgentCharacter::OnDroneGamepadYawAxis(float Value)
{
	DroneGamepadYawInput = Value;
}

void AAgentCharacter::OnDroneGamepadThrottleAxis(float Value)
{
	DroneGamepadThrottleInput = Value;
}

void AAgentCharacter::OnDroneGamepadRollAxis(float Value)
{
	if (bPickupRotationModeActive && bControllerPickupHeld && HeldPickupComponent.IsValid())
	{
		const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : (1.0f / 60.0f);
		FVector ViewLocation = FVector::ZeroVector;
		FRotator ViewRotation = FRotator::ZeroRotator;
		if (GetActivePickupView(ViewLocation, ViewRotation))
		{
			ApplyHeldPickupRotationPhysicsInput(ViewRotation.Quaternion().GetUpVector(), Value, DeltaSeconds);
		}

		const float RotationGuideInputScale = GetHeldPickupRotationDriveScale();
		HeldPickupViewRelativeRotationOffset.Yaw = FRotator::NormalizeAxis(
			HeldPickupViewRelativeRotationOffset.Yaw
				+ (Value * PickupRotationYawSpeed * RotationGuideInputScale * DeltaSeconds));
		DroneGamepadRollInput = 0.0f;
		return;
	}

	DroneGamepadRollInput = Value;
}

void AAgentCharacter::OnDroneGamepadPitchAxis(float Value)
{
	if (bPickupRotationModeActive && bControllerPickupHeld && HeldPickupComponent.IsValid())
	{
		const float DeltaSeconds = GetWorld() ? GetWorld()->GetDeltaSeconds() : (1.0f / 60.0f);
		FVector ViewLocation = FVector::ZeroVector;
		FRotator ViewRotation = FRotator::ZeroRotator;
		if (GetActivePickupView(ViewLocation, ViewRotation))
		{
			ApplyHeldPickupRotationPhysicsInput(ViewRotation.Quaternion().GetRightVector(), Value, DeltaSeconds);
		}

		const float RotationGuideInputScale = GetHeldPickupRotationDriveScale();
		HeldPickupViewRelativeRotationOffset.Pitch = FRotator::NormalizeAxis(
			HeldPickupViewRelativeRotationOffset.Pitch
				+ (Value * PickupRotationPitchSpeed * RotationGuideInputScale * DeltaSeconds));
		DroneGamepadPitchInput = 0.0f;
		return;
	}

	DroneGamepadPitchInput = Value;
}

void AAgentCharacter::OnDroneGamepadLeftTriggerAxis(float Value)
{
	DroneGamepadLeftTriggerInput = FMath::Clamp(Value, 0.0f, 1.0f);
	const bool bWasPickupRotationModeActive = bPickupRotationModeActive;
	bPickupRotationModeActive = bControllerPickupHeld
		&& HeldPickupComponent.IsValid()
		&& CanUsePickupInteraction()
		&& DroneGamepadLeftTriggerInput >= FMath::Max(0.0f, PickupRotationTriggerThreshold);
	if (bWasPickupRotationModeActive != bPickupRotationModeActive)
	{
		if (bPickupRotationModeActive)
		{
			RebaseHeldPickupRotationToView();
		}
		RefreshHeldPickupConstraintMode();
	}
}

void AAgentCharacter::OnDroneGamepadRightTriggerAxis(float Value)
{
	if (bControllerPickupHeld && HeldPickupComponent.IsValid())
	{
		DroneGamepadRightTriggerInput = 0.0f;
		return;
	}

	DroneGamepadRightTriggerInput = FMath::Clamp(Value, 0.0f, 1.0f);
}

void AAgentCharacter::OnDroneCameraTiltUpPressed()
{
	if (DroneCompanion && (DroneCompanion->IsLiftAssistActive() || CanUseDroneLiftAssist()))
	{
		AdjustDroneLiftAssistStrength(0.01f);
		return;
	}

	if (DroneCompanion)
	{
		DroneCompanion->AdjustCameraTilt(DroneCompanion->DroneCameraTiltStep);
	}
}

void AAgentCharacter::OnDroneCameraTiltDownPressed()
{
	if (DroneCompanion && (DroneCompanion->IsLiftAssistActive() || CanUseDroneLiftAssist()))
	{
		AdjustDroneLiftAssistStrength(-0.01f);
		return;
	}

	if (DroneCompanion)
	{
		DroneCompanion->AdjustCameraTilt(-DroneCompanion->DroneCameraTiltStep);
	}
}

void AAgentCharacter::OnBackpackOrDroneModePressed()
{
	if (CanUseBackpackDeployInput())
	{
		if (BackAttachmentComponent && BackAttachmentComponent->HasLockedItems())
		{
			BackAttachmentComponent->DeployBackItem();
			BackAttachmentComponent->SetManualMagnetRequested(false);
			BackAttachmentComponent->SetManualMagnetChargeAlpha(0.0f);
			bBackpackKeyboardButtonHeld = false;
			BackpackKeyboardButtonHeldDuration = 0.0f;
			return;
		}

		bBackpackKeyboardButtonHeld = true;
		BackpackKeyboardButtonHeldDuration = 0.0f;
		return;
	}

	OnDroneControlModeTogglePressed();
}

void AAgentCharacter::OnBackpackOrDroneModeReleased()
{
	if (!bBackpackKeyboardButtonHeld)
	{
		return;
	}

	bBackpackKeyboardButtonHeld = false;
	BackpackKeyboardButtonHeldDuration = 0.0f;

	if (BackAttachmentComponent && !bBackpackControllerButtonHeld)
	{
		BackAttachmentComponent->SetManualMagnetRequested(false);
		BackAttachmentComponent->SetManualMagnetChargeAlpha(0.0f);
	}
}

void AAgentCharacter::OnBackpackOrDroneCameraDownPressed()
{
	if (CanUseBackpackDeployInput())
	{
		if (BackAttachmentComponent && BackAttachmentComponent->HasLockedItems())
		{
			BackAttachmentComponent->DeployBackItem();
			BackAttachmentComponent->SetManualMagnetRequested(false);
			BackAttachmentComponent->SetManualMagnetChargeAlpha(0.0f);
			bBackpackControllerButtonHeld = false;
			BackpackControllerButtonHeldDuration = 0.0f;
			return;
		}

		bBackpackControllerButtonHeld = true;
		BackpackControllerButtonHeldDuration = 0.0f;
		return;
	}

	OnDroneCameraTiltDownPressed();
}

void AAgentCharacter::OnBackpackOrDroneCameraDownReleased()
{
	if (!bBackpackControllerButtonHeld)
	{
		return;
	}

	bBackpackControllerButtonHeld = false;
	BackpackControllerButtonHeldDuration = 0.0f;

	if (BackAttachmentComponent && !bBackpackKeyboardButtonHeld)
	{
		BackAttachmentComponent->SetManualMagnetRequested(false);
		BackAttachmentComponent->SetManualMagnetChargeAlpha(0.0f);
	}
}

void AAgentCharacter::OnDroneControlModeTogglePressed()
{
	if (bConveyorPlacementModeActive)
	{
		return;
	}

	ToggleDronePilotControlMode();
}

void AAgentCharacter::OnDroneHoverModePressed()
{
	if (bConveyorPlacementModeActive)
	{
		return;
	}

	CycleDronePilotControlMode(-1);
}

void AAgentCharacter::OnDroneStabilizerTogglePressed()
{
	if (bConveyorPlacementModeActive)
	{
		return;
	}

	CycleDronePilotControlMode(1);
}

void AAgentCharacter::OnMapModePressed()
{
	if (IsRagdolling() && !bAllowViewModeChangesWhileRagdoll)
	{
		return;
	}

	if (bMiniMapModeActive)
	{
		bMiniMapViewingDroneCamera = false;
		ApplyDroneFleetContext(true, true);
		return;
	}

	ToggleMapMode();
}

void AAgentCharacter::OnKeyboardMapButtonPressed()
{
	if (IsRagdolling() && !bAllowViewModeChangesWhileRagdoll)
	{
		return;
	}

	bKeyboardMapButtonHeld = true;
	bKeyboardMapButtonTriggeredMiniMap = false;
	KeyboardMapButtonHeldDuration = 0.0f;
}

void AAgentCharacter::OnKeyboardMapButtonReleased()
{
	if (IsRagdolling() && !bAllowViewModeChangesWhileRagdoll)
	{
		return;
	}

	const bool bWasMiniMapHold = bKeyboardMapButtonTriggeredMiniMap;
	bKeyboardMapButtonHeld = false;
	bKeyboardMapButtonTriggeredMiniMap = false;
	KeyboardMapButtonHeldDuration = 0.0f;

	if (bWasMiniMapHold)
	{
		return;
	}

	if (bMiniMapModeActive)
	{
		bMiniMapViewingDroneCamera = false;
		ApplyDroneFleetContext(true, true);
		return;
	}

	ToggleMapMode();
}

void AAgentCharacter::OnControllerMapButtonPressed()
{
	if (IsRagdolling() && !bAllowViewModeChangesWhileRagdoll)
	{
		return;
	}

	bControllerMapButtonHeld = true;
	bControllerMapButtonTriggeredMiniMap = false;
	ControllerMapButtonHeldDuration = 0.0f;
}

void AAgentCharacter::OnControllerMapButtonReleased()
{
	if (IsRagdolling() && !bAllowViewModeChangesWhileRagdoll)
	{
		return;
	}

	const bool bWasMiniMapHold = bControllerMapButtonTriggeredMiniMap;
	bControllerMapButtonHeld = false;
	bControllerMapButtonTriggeredMiniMap = false;
	ControllerMapButtonHeldDuration = 0.0f;

	if (bWasMiniMapHold)
	{
		return;
	}

	if (bMiniMapModeActive)
	{
		bMiniMapViewingDroneCamera = false;
		ApplyDroneFleetContext(true, true);
		return;
	}

	ToggleMapMode();
}

void AAgentCharacter::OnConveyorPlacementModePressed()
{
	SelectFactoryPlacementType(EAgentFactoryPlacementType::Conveyor, true);
}

void AAgentCharacter::OnStorageBinPlacementModePressed()
{
	SelectFactoryPlacementType(EAgentFactoryPlacementType::StorageBin, true);
}

void AAgentCharacter::OnMachinePlacementModePressed()
{
	SelectFactoryPlacementType(EAgentFactoryPlacementType::Machine, true);
}

void AAgentCharacter::OnMinerPlacementModePressed()
{
	SelectFactoryPlacementType(EAgentFactoryPlacementType::Miner, true);
}

void AAgentCharacter::OnMiningSwarmPlacementModePressed()
{
	SelectFactoryPlacementType(EAgentFactoryPlacementType::MiningSwarm, true);
}

void AAgentCharacter::OnFactoryPlacementTogglePressed()
{
	ToggleConveyorPlacementMode();
}

void AAgentCharacter::OnFactoryFreePlacementTogglePressed()
{
	if (!bConveyorPlacementModeActive || CurrentFactoryPlacementType != EAgentFactoryPlacementType::Miner)
	{
		return;
	}

	bFactoryFreePlacementEnabled = !bFactoryFreePlacementEnabled;
	FactoryPlacementRotationHoldDirection = 0;
	FactoryPlacementRotationHoldTimeRemaining = 0.0f;

	if (!IsFreeFactoryPlacementActive())
	{
		FactoryPlacementRotationYawDegrees = AgentFactory::SnapYawToGrid(FactoryPlacementRotationYawDegrees).Yaw;
	}

	UpdateConveyorPlacementPreview();
}

void AAgentCharacter::OnConveyorPlacePressed()
{
	TryPlaceConveyor();
}

void AAgentCharacter::OnConveyorCancelPressed()
{
	CancelConveyorPlacement();
}

void AAgentCharacter::OnConveyorRotateLeftPressed()
{
	RotateConveyorPlacement(-1);
}

void AAgentCharacter::OnConveyorRotateRightPressed()
{
	RotateConveyorPlacement(1);
}

void AAgentCharacter::OnHookJobButtonPressed()
{
	if (bConveyorPlacementModeActive)
	{
		OnConveyorRotateRightPressed();
		return;
	}

	bHookJobButtonHeld = true;
	bHookJobButtonTriggeredHoldRelease = false;
	HookJobButtonHeldDuration = 0.0f;
}

void AAgentCharacter::OnHookJobButtonReleased()
{
	if (bConveyorPlacementModeActive || !bHookJobButtonHeld)
	{
		return;
	}

	const bool bWasHoldTriggered = bHookJobButtonTriggeredHoldRelease;
	bHookJobButtonHeld = false;
	bHookJobButtonTriggeredHoldRelease = false;
	HookJobButtonHeldDuration = 0.0f;

	if (!bWasHoldTriggered)
	{
		TryAssignActiveDroneToHookJob(true);
	}
}

void AAgentCharacter::OnPickupOrDroneYawRightPressed()
{
	if (IsRagdolling())
	{
		return;
	}

	if (bConveyorPlacementModeActive)
	{
		bPlacementRotateRightHeld = true;
		bPlacementRotateLeftHeld = false;
		FactoryPlacementRotationHoldDirection = 1;
		FactoryPlacementRotationHoldTimeRemaining = FMath::Max(0.0f, FreePlacementRotationHoldDelay);
		RotateConveyorPlacement(1);
		return;
	}

	if (bMapModeActive && !bConveyorPlacementModeActive)
	{
		if (CanUsePickupInteraction() && TryBeginPickup())
		{
			if (HeldPickupComponent.IsValid())
			{
				bKeyboardPickupHeld = true;
			}
			return;
		}

		if (!TryToggleDroneLiftAssist() && IsDroneInputModeActive())
		{
			OnDroneYawRightPressed();
		}
		return;
	}

	if (CanUsePickupInteraction() && TryBeginPickup())
	{
		if (HeldPickupComponent.IsValid())
		{
			bKeyboardPickupHeld = true;
		}
		return;
	}

	if (IsDroneInputModeActive())
	{
		OnDroneYawRightPressed();
	}
}

void AAgentCharacter::OnPickupOrDroneYawRightReleased()
{
	if (bConveyorPlacementModeActive)
	{
		bPlacementRotateRightHeld = false;
		if (!bPlacementRotateLeftHeld)
		{
			FactoryPlacementRotationHoldDirection = 0;
			FactoryPlacementRotationHoldTimeRemaining = 0.0f;
		}
		return;
	}

	if (bKeyboardPickupHeld)
	{
		bKeyboardPickupHeld = false;
		EndPickup();
		return;
	}

	OnDroneYawRightReleased();
}

void AAgentCharacter::OnPickupOrPlacePressed()
{
	const bool bWantsBeamFire = IsRawControllerBeamAimModifierHeld() && CanUseBeamTool();
	bControllerBeamFireHeld = bWantsBeamFire;
	if (bWantsBeamFire)
	{
		return;
	}

	if (bConveyorPlacementModeActive)
	{
		OnConveyorPlacePressed();
		return;
	}

	if (bMapModeActive)
	{
		if (CanUsePickupInteraction() && TryBeginPickup())
		{
			if (HeldPickupComponent.IsValid())
			{
				bControllerPickupHeld = true;
				const bool bWasPickupRotationModeActive = bPickupRotationModeActive;
				bPickupRotationModeActive = HeldPickupComponent.IsValid()
					&& DroneGamepadLeftTriggerInput >= FMath::Max(0.0f, PickupRotationTriggerThreshold);
				if (bWasPickupRotationModeActive != bPickupRotationModeActive)
				{
					if (bPickupRotationModeActive)
					{
						RebaseHeldPickupRotationToView();
					}
					RefreshHeldPickupConstraintMode();
				}
			}

			return;
		}

		return;
	}

	if (CanUsePickupInteraction() && TryBeginPickup())
	{
		if (HeldPickupComponent.IsValid())
		{
			bControllerPickupHeld = true;
			const bool bWasPickupRotationModeActive = bPickupRotationModeActive;
			bPickupRotationModeActive = HeldPickupComponent.IsValid()
				&& DroneGamepadLeftTriggerInput >= FMath::Max(0.0f, PickupRotationTriggerThreshold);
			if (bWasPickupRotationModeActive != bPickupRotationModeActive)
			{
				if (bPickupRotationModeActive)
				{
					RebaseHeldPickupRotationToView();
				}
				RefreshHeldPickupConstraintMode();
			}
		}

		return;
	}
}

void AAgentCharacter::OnPickupOrPlaceReleased()
{
	bControllerBeamFireHeld = false;
	if (PlayerBeamToolComponent && PlayerBeamToolComponent->IsBeamActive())
	{
		StopAllBeamTools();
		return;
	}

	if (DroneCompanion)
	{
		if (UAgentBeamToolComponent* DroneBeamToolComponent = DroneCompanion->GetBeamToolComponent())
		{
			if (DroneBeamToolComponent->IsBeamActive())
			{
				StopAllBeamTools();
				return;
			}
		}
	}

	if (!bControllerPickupHeld)
	{
		return;
	}

	bControllerPickupHeld = false;
	bPickupRotationModeActive = false;
	EndPickup();
}

void AAgentCharacter::OnPickupStrengthDecreasePressed()
{
	AdjustPickupStrength(-0.1f);
	AdjustClumsiness(-FMath::Max(0.0f, ClumsinessBracketStep));
}

void AAgentCharacter::OnPickupStrengthIncreasePressed()
{
	AdjustPickupStrength(0.1f);
	AdjustClumsiness(FMath::Max(0.0f, ClumsinessBracketStep));
}

void AAgentCharacter::OnInteractPressed()
{
	if (IsRagdolling())
	{
		bInteractKeyDrivingDroneThrottle = false;
		return;
	}

	if (CanUseCharacterInteraction())
	{
		bInteractKeyDrivingDroneThrottle = false;
		return;
	}

	bInteractKeyDrivingDroneThrottle = true;
	OnDroneThrottleDownPressed();
}

void AAgentCharacter::OnInteractReleased()
{
	if (!bInteractKeyDrivingDroneThrottle)
	{
		return;
	}

	bInteractKeyDrivingDroneThrottle = false;
	OnDroneThrottleDownReleased();
}

void AAgentCharacter::OnVehicleInteractPressed()
{
	if (!VehicleInteractionComponent)
	{
		return;
	}

	if (VehicleInteractionComponent->IsControllingVehicle())
	{
		VehicleInteractionComponent->TryExitCurrentVehicle();
		return;
	}

	if (IsRagdolling())
	{
		return;
	}

	if (!CanUseCharacterInteraction())
	{
		return;
	}

	VehicleInteractionComponent->TryEnterNearestVehicle();
}

