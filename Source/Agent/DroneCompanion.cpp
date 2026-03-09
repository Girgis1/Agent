// Copyright Epic Games, Inc. All Rights Reserved.

#include "DroneCompanion.h"
#include "Agent.h"
#include "Camera/CameraComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/SphereComponent.h"
#include "Factory/ConveyorSurfaceVelocityComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "DroneBatteryComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"
#include "Materials/MaterialInstanceDynamic.h"
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
	DroneBody->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
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

	DroneBody->SetRelativeScale3D(FVector(DroneBodyVisualScale));

	DronePickupProxySphere = CreateDefaultSubobject<USphereComponent>(TEXT("DronePickupProxySphere"));
	DronePickupProxySphere->SetupAttachment(DroneBody);
	DronePickupProxySphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	DronePickupProxySphere->SetCollisionObjectType(ECC_WorldDynamic);
	DronePickupProxySphere->SetCollisionResponseToAllChannels(ECR_Block);
	DronePickupProxySphere->SetGenerateOverlapEvents(false);
	DronePickupProxySphere->SetCanEverAffectNavigation(false);
	DronePickupProxySphere->SetSphereRadius(PickupProxySphereRadius);
	ApplyPickupProxyVisualizationSettings();

	CameraMount = CreateDefaultSubobject<USceneComponent>(TEXT("CameraMount"));
	CameraMount->SetupAttachment(DroneBody);

	DroneCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("DroneCamera"));
	DroneCamera->SetupAttachment(CameraMount);
	DroneCamera->bUsePawnControlRotation = false;
#if WITH_EDITORONLY_DATA
	DroneCamera->bCameraMeshHiddenInGame = true;
	DroneCamera->bDrawFrustumAllowed = false;
	DroneCamera->SetCameraMesh(nullptr);
#endif
	DroneCamera->FieldOfView = PilotCameraFieldOfView;

	DroneStatusLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("DroneStatusLight"));
	DroneStatusLight->SetupAttachment(DroneBody);
	DroneStatusLight->SetIntensity(StatusLightBaseIntensity);
	DroneStatusLight->SetAttenuationRadius(StatusLightAttenuationRadius);
	DroneStatusLight->SetLightColor(StatusLightNormalColor);
	DroneStatusLight->SetCastShadows(false);
	DroneStatusLight->SetCanEverAffectNavigation(false);

	BatteryComponent = CreateDefaultSubobject<UDroneBatteryComponent>(TEXT("BatteryComponent"));

	ConveyorSurfaceVelocity = CreateDefaultSubobject<UConveyorSurfaceVelocityComponent>(TEXT("ConveyorSurfaceVelocity"));
}

void ADroneCompanion::BeginPlay()
{
	Super::BeginPlay();

	if (DroneBody)
	{
		DroneBody->SetRelativeScale3D(FVector(FMath::Max(DroneBodyVisualScale, 0.01f)));
		DroneBody->SetCollisionObjectType(ECC_PhysicsBody);
		DroneBody->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		DroneBody->SetCollisionResponseToAllChannels(ECR_Block);
		DroneBody->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
		DroneBody->SetMassOverrideInKg(NAME_None, FMath::Max(0.1f, DroneMassKg), true);
		DroneBody->SetLinearDamping(DroneBodyLinearDamping);
		DroneBody->SetAngularDamping(DroneBodyAngularDamping);
		DroneBody->OnComponentHit.AddDynamic(this, &ADroneCompanion::OnDroneBodyHit);
	}

	if (DronePickupProxySphere)
	{
		DronePickupProxySphere->SetCollisionEnabled(bUsePickupProxySphere ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		DronePickupProxySphere->SetCollisionObjectType(ECC_WorldDynamic);
		DronePickupProxySphere->SetCollisionResponseToAllChannels(ECR_Block);
		ApplyPickupProxyVisualizationSettings();
	}

	if (DroneCamera)
	{
		ApplyEditorCameraVisualizationSettings();
	}

	ApplyRuntimePhysicalMaterial();
	AdjustCameraTilt(0.0f);
	UpdateBuddyDrift(BuddyDriftUpdateInterval);
	UpdateBatteryState(0.0f);
	RefreshStateDomains(TEXT("BeginPlay"));
	InitializeDynamicEmissiveMaterials();
	ApplyStaticMeshOwnerVisibility();
	UpdateSpotlightVisuals();
	UpdateStatusLight(0.0f);
}

void ADroneCompanion::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!DroneBody)
	{
		return;
	}

	ApplyEditorCameraVisualizationSettings();
	ApplyPickupProxyVisualizationSettings();

	PreviousLinearVelocity = DroneBody->GetPhysicsLinearVelocity();
	RemoveAppliedConveyorSurfaceVelocity();
	RollJumpCooldownRemaining = FMath::Max(0.0f, RollJumpCooldownRemaining - DeltaSeconds);
	RollModeLogTimeRemaining = FMath::Max(0.0f, RollModeLogTimeRemaining - DeltaSeconds);
	UpdateBatteryState(DeltaSeconds);
	const bool bWasChargingLockActive = bChargingLockActive;
	const bool bShouldApplyChargingLock =
		CompanionMode != EDroneCompanionMode::PilotControlled;
	const bool bBatteryEnabled = BatteryComponent && BatteryComponent->IsBatteryEnabled();
	bChargingLockActive = bBatteryEnabled
		&& bIsInChargerVolume
		&& BatteryComponent
		&& bShouldApplyChargingLock
		&& !IsBatteryFullyCharged();
	if (bChargingLockActive && !bWasChargingLockActive)
	{
		// Entering charger-idle should behave like dead-mode physics:
		// stop active control/lift, but leave rigid-body simulation free.
		StopLiftAssist();
		ResetPilotInputs();
	}

	RefreshStateDomains(TEXT("Tick"));
	UpdateAnchoredTransform();

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
	const bool bCanApplyMovementForces = CanApplyMovementForces();
	const bool bRollPilotActive =
		bCanApplyMovementForces
		&& bUseRollPilotControls
		&& CompanionMode == EDroneCompanionMode::PilotControlled;
	if (!bCanApplyMovementForces)
	{
		bCrashed = false;
		CrashRecoveryTimeRemaining = 0.0f;
		bPendingCrashRollRecovery = false;
	}
	else if (bRollPilotActive)
	{
		bCrashed = false;
		CrashRecoveryTimeRemaining = 0.0f;
	}
	else
	{
		UpdateCrashRecovery(DeltaSeconds);
	}

	if (bCanApplyMovementForces && !bCrashed)
	{
		if (bLiftAssistActive
			&& (CompanionMode == EDroneCompanionMode::BuddyFollow
				|| CompanionMode == EDroneCompanionMode::MapMode
				|| CompanionMode == EDroneCompanionMode::PilotControlled))
		{
			UpdateLiftAssistFlight(DeltaSeconds);
		}
		else
		{
			if (CompanionMode == EDroneCompanionMode::PilotControlled)
			{
				if (bUseRollPilotControls)
				{
					UpdateRollFlight(DeltaSeconds);
				}
				else if (bUseSpectatorPilotControls)
				{
					UpdateSpectatorFlight(DeltaSeconds);
				}
				else if (bUseFreeFlyPilotControls)
				{
					UpdateFreeFlyFlight(DeltaSeconds);
				}
				else if (bUseSimplePilotControls)
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
			else if (CompanionMode == EDroneCompanionMode::MiniMapFollow)
			{
				UpdateMiniMapFlight(DeltaSeconds);
			}
			else if (CompanionMode == EDroneCompanionMode::Idle)
			{
				// Idle drones stay as free physics bodies. We intentionally do not
				// apply autopilot forces here so inactive drones remain in-world.
			}
			else
			{
				if (bLiftAssistActive && CompanionMode == EDroneCompanionMode::BuddyFollow)
				{
					UpdateLiftAssistFlight(DeltaSeconds);
				}
				else
				{
					UpdateAutonomousFlight(DeltaSeconds);
				}
			}
		}
	}

	if (!bRollPilotActive && ShouldClampVelocityForCurrentState())
	{
		ClampVelocity();
	}

	ApplyConveyorSurfaceVelocity();
	UpdateCameraTransition(DeltaSeconds);
	if (bCanApplyMovementForces && bRollPilotActive && !bCameraTransitionActive && CameraMount)
	{
		UpdateRollCamera(DeltaSeconds);
	}

	UpdateStatusLight(DeltaSeconds);
	UpdateDebugOutput();
}

float ADroneCompanion::GetBatteryPercent() const
{
	return BatteryComponent ? BatteryComponent->GetBatteryPercent() : 0.0f;
}

void ADroneCompanion::SetBatteryPercent(float NewBatteryPercent)
{
	if (!BatteryComponent)
	{
		return;
	}

	BatteryComponent->SetBatteryPercent(NewBatteryPercent);
	if (BatteryComponent->IsDepleted())
	{
		if (CurrentPowerState != EDronePowerState::Depleted)
		{
			HandleDronePowerLoss();
		}

		return;
	}

	if (CurrentPowerState == EDronePowerState::Depleted)
	{
		HandleDronePowerRestore();
	}

	RefreshStateDomains(TEXT("SetBatteryPercent"));
}

float ADroneCompanion::ChargeBatteryPercent(float PercentToCharge)
{
	if (!BatteryComponent)
	{
		return 0.0f;
	}

	const float ChargedAmount = BatteryComponent->ChargePercent(PercentToCharge);
	if (ChargedAmount > KINDA_SMALL_NUMBER
		&& CurrentPowerState == EDronePowerState::Depleted
		&& !BatteryComponent->IsDepleted())
	{
		HandleDronePowerRestore();
	}
	else if (ChargedAmount > KINDA_SMALL_NUMBER)
	{
		RefreshStateDomains(TEXT("ChargeBatteryPercent"));
	}

	return ChargedAmount;
}

bool ADroneCompanion::IsBatteryDepleted() const
{
	return BatteryComponent ? BatteryComponent->IsDepleted() : (CurrentPowerState == EDronePowerState::Depleted);
}

bool ADroneCompanion::IsBatteryFullyCharged() const
{
	if (!BatteryComponent)
	{
		return false;
	}

	return BatteryComponent->IsFullyCharged();
}

bool ADroneCompanion::SetManualPowerOff(bool bPowerOff)
{
	if (bManualPowerOffRequested == bPowerOff)
	{
		return false;
	}

	bManualPowerOffRequested = bPowerOff;
	RefreshStateDomains(bPowerOff ? FName(TEXT("ManualPowerOff")) : FName(TEXT("ManualPowerOn")));
	return true;
}

void ADroneCompanion::ToggleManualPowerOff()
{
	SetManualPowerOff(!bManualPowerOffRequested);
}

void ADroneCompanion::NotifyEnteredChargerVolume()
{
	ChargerVolumeOverlapCount = FMath::Max(0, ChargerVolumeOverlapCount + 1);
	bIsInChargerVolume = ChargerVolumeOverlapCount > 0;
	const bool bBatteryEnabled = BatteryComponent && BatteryComponent->IsBatteryEnabled();
	bChargingLockActive = bBatteryEnabled
		&& bIsInChargerVolume
		&& CompanionMode != EDroneCompanionMode::PilotControlled
		&& !IsBatteryFullyCharged();
	RefreshStateDomains(TEXT("ChargerEntered"));
}

void ADroneCompanion::NotifyExitedChargerVolume()
{
	ChargerVolumeOverlapCount = FMath::Max(0, ChargerVolumeOverlapCount - 1);
	bIsInChargerVolume = ChargerVolumeOverlapCount > 0;
	const bool bBatteryEnabled = BatteryComponent && BatteryComponent->IsBatteryEnabled();
	bChargingLockActive = bBatteryEnabled
		&& bIsInChargerVolume
		&& CompanionMode != EDroneCompanionMode::PilotControlled
		&& !IsBatteryFullyCharged();
	RefreshStateDomains(TEXT("ChargerExited"));
}

void ADroneCompanion::UpdateBatteryState(float DeltaSeconds)
{
	if (!BatteryComponent)
	{
		LowBatteryFlickerAlpha = 0.0f;
		RefreshStateDomains(TEXT("NoBatteryComponent"));
		return;
	}

	if (!BatteryComponent->IsBatteryEnabled())
	{
		if (CurrentPowerState == EDronePowerState::Depleted)
		{
			HandleDronePowerRestore();
		}

		LowBatteryFlickerAlpha = 0.0f;
		RefreshStateDomains(TEXT("BatteryDisabled"));
		return;
	}

	const float SafeDeltaSeconds = FMath::Max(0.0f, DeltaSeconds);
	const float DrainRatePercentPerSecond = BatteryComponent->GetDrainRatePercentPerSecond();
	const bool bTorchDrainActive = bTorchEnabled
		&& CurrentPowerState == EDronePowerState::Active;
	float DrainScale = CurrentPowerState == EDronePowerState::PoweredOff
		? FMath::Clamp(PoweredOffDrainMultiplier, 0.0f, 1.0f)
		: 1.0f;
	if (bTorchDrainActive)
	{
		DrainScale *= FMath::Max(1.0f, TorchDrainMultiplier);
	}
	if (DrainRatePercentPerSecond > KINDA_SMALL_NUMBER && !BatteryComponent->IsDepleted() && DrainScale > KINDA_SMALL_NUMBER)
	{
		BatteryComponent->ConsumePercent(DrainRatePercentPerSecond * SafeDeltaSeconds * DrainScale);
	}

	const float PassiveChargeRatePercentPerSecond = BatteryComponent->GetPassiveChargeRatePercentPerSecond();
	if (bIsInChargerVolume && PassiveChargeRatePercentPerSecond > KINDA_SMALL_NUMBER)
	{
		BatteryComponent->ChargePercent(PassiveChargeRatePercentPerSecond * SafeDeltaSeconds);
	}

	if (BatteryComponent->IsDepleted())
	{
		if (CurrentPowerState != EDronePowerState::Depleted)
		{
			HandleDronePowerLoss();
		}
	}
	else if (CurrentPowerState == EDronePowerState::Depleted)
	{
		HandleDronePowerRestore();
	}

	RefreshStateDomains(TEXT("UpdateBatteryState"));
}

void ADroneCompanion::HandleDronePowerLoss()
{
	bCrashed = false;
	CrashRecoveryTimeRemaining = 0.0f;
	bPendingCrashRollRecovery = false;
	StopLiftAssist();
	ResetPilotInputs();
	RequestPowerState(EDronePowerState::Depleted, TEXT("BatteryDepleted"));
}

void ADroneCompanion::HandleDronePowerRestore()
{
	const EDronePowerState RestoredState = bManualPowerOffRequested
		? EDronePowerState::PoweredOff
		: EDronePowerState::Active;
	RequestPowerState(RestoredState, TEXT("BatteryRestored"));
}

void ADroneCompanion::RefreshStateDomains(FName Reason)
{
	const EDronePowerState DesiredPowerState = ResolveDesiredPowerState();
	RequestPowerState(DesiredPowerState, Reason);

	const EDroneRoleState DesiredRoleState = ResolveRoleStateFromCompanionMode(CompanionMode);
	RequestRoleState(DesiredRoleState, Reason);

	const EDroneControlState DesiredControlState = ResolveControlStateFromCompanionMode(CompanionMode);
	RequestControlState(DesiredControlState, Reason);

	const EDroneMobilityState DesiredMobilityState = ResolveDesiredMobilityState();
	if (!RequestMobilityState(DesiredMobilityState, Reason)
		&& DesiredMobilityState != EDroneMobilityState::PhysicsBody)
	{
		RequestMobilityState(EDroneMobilityState::PhysicsBody, TEXT("MobilityFallback"));
	}

	RefreshTorchRuntimeState();
}

EDronePowerState ADroneCompanion::ResolveDesiredPowerState() const
{
	const bool bBatteryEnabled = BatteryComponent && BatteryComponent->IsBatteryEnabled();
	const bool bBatteryDepleted = bBatteryEnabled && BatteryComponent->IsDepleted();
	if (bBatteryDepleted)
	{
		return EDronePowerState::Depleted;
	}

	return bManualPowerOffRequested
		? EDronePowerState::PoweredOff
		: EDronePowerState::Active;
}

EDroneRoleState ADroneCompanion::ResolveRoleStateFromCompanionMode(EDroneCompanionMode Mode) const
{
	switch (Mode)
	{
	case EDroneCompanionMode::PilotControlled:
		return EDroneRoleState::Pilot;
	case EDroneCompanionMode::Idle:
		return EDroneRoleState::Idle;
	case EDroneCompanionMode::HoldPosition:
		return EDroneRoleState::Hold;
	case EDroneCompanionMode::BuddyFollow:
		return EDroneRoleState::Buddy;
	case EDroneCompanionMode::MapMode:
		return EDroneRoleState::Map;
	case EDroneCompanionMode::MiniMapFollow:
		return EDroneRoleState::MiniMap;
	case EDroneCompanionMode::ThirdPersonFollow:
	default:
		return EDroneRoleState::ThirdPersonCamera;
	}
}

EDroneControlState ADroneCompanion::ResolveControlStateFromCompanionMode(EDroneCompanionMode Mode) const
{
	return Mode == EDroneCompanionMode::PilotControlled
		? EDroneControlState::PlayerControlled
		: EDroneControlState::Autonomous;
}

EDroneMobilityState ADroneCompanion::ResolveDesiredMobilityState() const
{
	if (CurrentPowerState != EDronePowerState::Active)
	{
		return EDroneMobilityState::PhysicsBody;
	}

	const bool bChargeLocked =
		bChargingLockActive
		&& CompanionMode != EDroneCompanionMode::PilotControlled;
	if (bChargeLocked)
	{
		return EDroneMobilityState::PhysicsBody;
	}

	if (CurrentRoleState == EDroneRoleState::ThirdPersonCamera && bAnchorThirdPersonRoleToCamera)
	{
		return EDroneMobilityState::Anchored;
	}

	return EDroneMobilityState::Flight;
}

bool ADroneCompanion::RequestPowerState(EDronePowerState NewState, FName Reason)
{
	if (CurrentPowerState == NewState)
	{
		return true;
	}

	const bool bCanTransition = CanTransitionPowerState(CurrentPowerState, NewState);
	LogStateTransition(
		TEXT("Power"),
		static_cast<int32>(CurrentPowerState),
		static_cast<int32>(NewState),
		Reason,
		bCanTransition);
	if (!bCanTransition)
	{
		return false;
	}

	CurrentPowerState = NewState;
	bDronePoweredOff = CurrentPowerState != EDronePowerState::Active;
	if (bDronePoweredOff)
	{
		bCrashed = false;
		CrashRecoveryTimeRemaining = 0.0f;
		bPendingCrashRollRecovery = false;
		StopLiftAssist();
		ResetPilotInputs();
	}

	return true;
}

bool ADroneCompanion::RequestMobilityState(EDroneMobilityState NewState, FName Reason)
{
	if (CurrentMobilityState == NewState)
	{
		return true;
	}

	const bool bCanTransition = CanTransitionMobilityState(CurrentMobilityState, NewState);
	LogStateTransition(
		TEXT("Mobility"),
		static_cast<int32>(CurrentMobilityState),
		static_cast<int32>(NewState),
		Reason,
		bCanTransition);
	if (!bCanTransition)
	{
		return false;
	}

	const bool bResetKinematics =
		CurrentMobilityState == EDroneMobilityState::Anchored
		|| NewState == EDroneMobilityState::Anchored;
	CurrentMobilityState = NewState;
	ApplyPhysicsProfile(bResetKinematics);
	if (CurrentMobilityState == EDroneMobilityState::Anchored)
	{
		UpdateAnchoredTransform();
	}

	return true;
}

bool ADroneCompanion::RequestRoleState(EDroneRoleState NewState, FName Reason)
{
	if (CurrentRoleState == NewState)
	{
		return true;
	}

	const bool bCanTransition = CanTransitionRoleState(CurrentRoleState, NewState);
	LogStateTransition(
		TEXT("Role"),
		static_cast<int32>(CurrentRoleState),
		static_cast<int32>(NewState),
		Reason,
		bCanTransition);
	if (!bCanTransition)
	{
		return false;
	}

	CurrentRoleState = NewState;
	return true;
}

bool ADroneCompanion::RequestControlState(EDroneControlState NewState, FName Reason)
{
	if (CurrentControlState == NewState)
	{
		return true;
	}

	const bool bCanTransition = CanTransitionControlState(CurrentControlState, NewState);
	LogStateTransition(
		TEXT("Control"),
		static_cast<int32>(CurrentControlState),
		static_cast<int32>(NewState),
		Reason,
		bCanTransition);
	if (!bCanTransition)
	{
		return false;
	}

	CurrentControlState = NewState;
	return true;
}

bool ADroneCompanion::CanTransitionPowerState(EDronePowerState FromState, EDronePowerState ToState) const
{
	if (FromState == ToState)
	{
		return true;
	}

	if (FromState == EDronePowerState::Depleted && ToState == EDronePowerState::Active)
	{
		return BatteryComponent && !BatteryComponent->IsDepleted();
	}

	if (ToState == EDronePowerState::Active && BatteryComponent && BatteryComponent->IsBatteryEnabled() && BatteryComponent->IsDepleted())
	{
		return false;
	}

	return true;
}

bool ADroneCompanion::CanTransitionMobilityState(EDroneMobilityState FromState, EDroneMobilityState ToState) const
{
	if (FromState == ToState)
	{
		return true;
	}

	if (ToState == EDroneMobilityState::Flight)
	{
		return CurrentPowerState == EDronePowerState::Active
			&& (!bChargingLockActive || CompanionMode == EDroneCompanionMode::PilotControlled);
	}

	if (ToState == EDroneMobilityState::Anchored)
	{
		return CurrentPowerState == EDronePowerState::Active
			&& CurrentRoleState == EDroneRoleState::ThirdPersonCamera
			&& bAnchorThirdPersonRoleToCamera;
	}

	return true;
}

bool ADroneCompanion::CanTransitionRoleState(EDroneRoleState FromState, EDroneRoleState ToState) const
{
	if (FromState == ToState)
	{
		return true;
	}

	if (ToState == EDroneRoleState::Pilot)
	{
		return CurrentPowerState == EDronePowerState::Active;
	}

	return true;
}

bool ADroneCompanion::CanTransitionControlState(EDroneControlState FromState, EDroneControlState ToState) const
{
	if (FromState == ToState)
	{
		return true;
	}

	if (ToState == EDroneControlState::PlayerControlled)
	{
		return CurrentPowerState == EDronePowerState::Active;
	}

	return true;
}

void ADroneCompanion::ApplyPhysicsProfile(bool bResetKinematics)
{
	if (!DroneBody)
	{
		return;
	}

	ApplyEditorCameraVisualizationSettings();

	DroneBody->SetCollisionObjectType(ECC_PhysicsBody);
	DroneBody->SetCollisionResponseToAllChannels(ECR_Block);
	DroneBody->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

	if (DronePickupProxySphere)
	{
		const bool bAllowPickupProxy = bUsePickupProxySphere && CurrentMobilityState != EDroneMobilityState::Anchored;
		DronePickupProxySphere->SetCollisionObjectType(ECC_WorldDynamic);
		DronePickupProxySphere->SetCollisionResponseToAllChannels(ECR_Block);
		DronePickupProxySphere->SetCollisionEnabled(
			bAllowPickupProxy
				? ECollisionEnabled::QueryOnly
				: ECollisionEnabled::NoCollision);
		ApplyPickupProxyVisualizationSettings();
	}

	if (CurrentMobilityState == EDroneMobilityState::Anchored)
	{
		DroneBody->SetSimulatePhysics(false);
		DroneBody->SetEnableGravity(false);
		DroneBody->SetCollisionEnabled(
			bDisableCollisionWhenAnchored
				? ECollisionEnabled::NoCollision
				: FlightCollisionEnabled.GetValue());
		DroneBody->SetLinearDamping(FMath::Max(0.0f, DroneBodyLinearDamping));
		DroneBody->SetAngularDamping(FMath::Max(0.0f, DroneBodyAngularDamping));
		AppliedConveyorSurfaceVelocity = FVector::ZeroVector;
	}
	else if (CurrentMobilityState == EDroneMobilityState::PhysicsBody)
	{
		DroneBody->SetSimulatePhysics(true);
		DroneBody->SetEnableGravity(true);
		DroneBody->SetCollisionEnabled(PassiveCollisionEnabled.GetValue());
		DroneBody->SetLinearDamping(FMath::Max(0.0f, PassivePhysicsLinearDamping));
		DroneBody->SetAngularDamping(FMath::Max(0.0f, PassivePhysicsAngularDamping));
	}
	else
	{
		DroneBody->SetSimulatePhysics(true);
		DroneBody->SetEnableGravity(true);
		DroneBody->SetCollisionEnabled(FlightCollisionEnabled.GetValue());
		DroneBody->SetLinearDamping(FMath::Max(0.0f, DroneBodyLinearDamping));
		DroneBody->SetAngularDamping(FMath::Max(0.0f, DroneBodyAngularDamping));
	}

	if (bResetKinematics && DroneBody->IsSimulatingPhysics())
	{
		DroneBody->SetPhysicsLinearVelocity(FVector::ZeroVector);
		DroneBody->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		PreviousLinearVelocity = FVector::ZeroVector;
	}
}

bool ADroneCompanion::CanApplyMovementForces() const
{
	if (!DroneBody || !DroneBody->IsSimulatingPhysics())
	{
		return false;
	}

	if (CurrentPowerState != EDronePowerState::Active)
	{
		return false;
	}

	if (CurrentMobilityState != EDroneMobilityState::Flight)
	{
		return false;
	}

	return !bChargingLockActive || CompanionMode == EDroneCompanionMode::PilotControlled;
}

bool ADroneCompanion::ShouldProcessConveyorAssist() const
{
	return CanApplyMovementForces();
}

bool ADroneCompanion::ShouldClampVelocityForCurrentState() const
{
	return CanApplyMovementForces();
}

void ADroneCompanion::UpdateAnchoredTransform()
{
	if (!DroneBody || CurrentMobilityState != EDroneMobilityState::Anchored)
	{
		return;
	}

	if (!bHasThirdPersonCameraTarget)
	{
		return;
	}

	DroneBody->SetWorldLocationAndRotation(
		ThirdPersonCameraTargetLocation,
		ThirdPersonCameraTargetRotation,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
}

void ADroneCompanion::LogStateTransition(
	const TCHAR* Domain,
	int32 PreviousValue,
	int32 NewValue,
	FName Reason,
	bool bAccepted) const
{
	if (!bLogStateTransitions)
	{
		return;
	}

	UE_LOG(
		LogAgent,
		Log,
		TEXT("[DroneState] %s %d -> %d | Reason=%s | %s"),
		Domain,
		PreviousValue,
		NewValue,
		Reason.IsNone() ? TEXT("None") : *Reason.ToString(),
		bAccepted ? TEXT("Accepted") : TEXT("Blocked"));
}

void ADroneCompanion::RefreshTorchRuntimeState()
{
	const bool bBatteryEnabled = BatteryComponent && BatteryComponent->IsBatteryEnabled();
	const bool bBatteryDepleted = bBatteryEnabled && BatteryComponent && BatteryComponent->IsDepleted();
	bTorchRuntimeActive = bTorchEnabled
		&& CurrentPowerState == EDronePowerState::Active
		&& !bBatteryDepleted;
}

void ADroneCompanion::InitializeDynamicEmissiveMaterials()
{
	AccessoryEmissiveMaterialInstances.Reset();
	EyeEmissiveMaterialInstances.Reset();

	if (!bUseDynamicEmissiveMaterials)
	{
		return;
	}

	TInlineComponentArray<UMeshComponent*> MeshComponents(this);
	for (UMeshComponent* MeshComponent : MeshComponents)
	{
		if (!IsValid(MeshComponent))
		{
			continue;
		}

		const TArray<FName> MaterialSlotNames = MeshComponent->GetMaterialSlotNames();
		const int32 MaterialCount = MeshComponent->GetNumMaterials();
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
		{
			const FName SlotName = MaterialSlotNames.IsValidIndex(MaterialIndex)
				? MaterialSlotNames[MaterialIndex]
				: NAME_None;
			const bool bAccessorySlot =
				SlotName == AccessoryEmissiveSlotName
				|| (AccessoryEmissiveLegacySlotName != NAME_None && SlotName == AccessoryEmissiveLegacySlotName);
			const bool bEyeSlot =
				SlotName == EyeEmissiveSlotName
				|| (EyeEmissiveAltSlotName != NAME_None && SlotName == EyeEmissiveAltSlotName);
			if (!bAccessorySlot && !bEyeSlot)
			{
				continue;
			}

			UMaterialInstanceDynamic* DynamicMaterial = MeshComponent->CreateAndSetMaterialInstanceDynamic(MaterialIndex);
			if (!DynamicMaterial)
			{
				continue;
			}

			if (bAccessorySlot)
			{
				AccessoryEmissiveMaterialInstances.AddUnique(DynamicMaterial);
			}

			if (bEyeSlot)
			{
				EyeEmissiveMaterialInstances.AddUnique(DynamicMaterial);
			}
		}
	}
}

void ADroneCompanion::UpdateDynamicEmissiveMaterials(
	const FLinearColor& StatusColor,
	float StatusBrightnessAlpha,
	float BatteryAlpha,
	bool bBatteryFlat)
{
	if (!bUseDynamicEmissiveMaterials)
	{
		return;
	}

	const float ClampedStatusBrightnessAlpha = FMath::Clamp(StatusBrightnessAlpha, 0.0f, 1.0f);
	const float AccessoryMinIntensity = FMath::Max(0.0f, AccessoryEmissiveMinIntensity);
	const float AccessoryMaxIntensity = FMath::Max(AccessoryMinIntensity, AccessoryEmissiveMaxIntensity);
	const float AccessoryIntensity = FMath::Lerp(
		AccessoryMinIntensity,
		AccessoryMaxIntensity,
		ClampedStatusBrightnessAlpha);
	for (UMaterialInstanceDynamic* DynamicMaterial : AccessoryEmissiveMaterialInstances)
	{
		if (!IsValid(DynamicMaterial))
		{
			continue;
		}

		DynamicMaterial->SetVectorParameterValue(AccessoryEmissiveColorParameterName, StatusColor);
		DynamicMaterial->SetScalarParameterValue(AccessoryEmissiveIntensityParameterName, AccessoryIntensity);
	}

	const float EyeMinIntensity = FMath::Max(0.0f, EyeEmissiveMinIntensity);
	const float EyeMaxIntensity = FMath::Max(EyeMinIntensity, EyeEmissiveMaxIntensity);
	const bool bAtVisualFullCharge = BatteryAlpha >= 0.999f;
	const bool bFullChargeBlinkActive = bEyeBlinkGreenWhenFullyCharged
		&& bAtVisualFullCharge
		&& (!bEyeFullChargeBlinkRequiresChargerVolume || bIsInChargerVolume);

	float EyeIntensity = 0.0f;
	FLinearColor EyeColor = EyeEmissiveColor;
	if (bFullChargeBlinkActive)
	{
		const UWorld* World = GetWorld();
		const float TimeSeconds = World ? World->GetTimeSeconds() : 0.0f;
		const float BlinkWave = 0.5f + 0.5f * FMath::Sin(
			TimeSeconds * FMath::Max(0.01f, EyeFullChargeBlinkFrequencyHz) * 2.0f * PI);
		const float MinBlinkAlpha = FMath::Clamp(EyeFullChargeBlinkMinAlpha, 0.0f, 1.0f);
		const float MaxBlinkAlpha = FMath::Max(MinBlinkAlpha, FMath::Clamp(EyeFullChargeBlinkMaxAlpha, 0.0f, 1.0f));
		const float BlinkAlpha = FMath::Lerp(MinBlinkAlpha, MaxBlinkAlpha, BlinkWave);
		EyeIntensity = FMath::Lerp(EyeMinIntensity, EyeMaxIntensity, BlinkAlpha);
		EyeColor = EyeFullyChargedBlinkColor;
	}
	else
	{
		const float EyeRampAlpha = FMath::Pow(
			FMath::Clamp(BatteryAlpha, 0.0f, 1.0f),
			FMath::Max(0.1f, EyeEmissiveBatteryRampExponent));
		EyeIntensity = FMath::Lerp(EyeMinIntensity, EyeMaxIntensity, EyeRampAlpha);
		if (CurrentPowerState == EDronePowerState::PoweredOff || bBatteryFlat)
		{
			EyeIntensity = 0.0f;
		}
	}

	for (UMaterialInstanceDynamic* DynamicMaterial : EyeEmissiveMaterialInstances)
	{
		if (!IsValid(DynamicMaterial))
		{
			continue;
		}

		DynamicMaterial->SetVectorParameterValue(EyeEmissiveColorParameterName, EyeColor);
		DynamicMaterial->SetScalarParameterValue(EyeEmissiveIntensityParameterName, EyeIntensity);
	}
}

void ADroneCompanion::UpdateSpotlightVisuals(float DeltaSeconds)
{
	const float SafeDeltaSeconds = FMath::Max(0.0f, DeltaSeconds);
	const bool bBatteryEnabled = BatteryComponent && BatteryComponent->IsBatteryEnabled();
	const float BatteryPercent = BatteryComponent ? BatteryComponent->GetBatteryPercent() : 0.0f;
	const float MaxBatteryPercent = BatteryComponent ? FMath::Max(0.01f, BatteryComponent->GetMaxBatteryPercent()) : 100.0f;
	const float TorchCutoffPercent = FMath::Clamp(TorchDisableAtLowBatteryPercent, 0.0f, MaxBatteryPercent);
	const float TorchFlickerStartClamped = FMath::Clamp(
		FMath::Max(TorchCutoffPercent, TorchFlickerStartPercent),
		0.0f,
		MaxBatteryPercent);
	const bool bCanUseTorchFlickerRange =
		bBatteryEnabled
		&& TorchFlickerStartClamped > KINDA_SMALL_NUMBER
		&& BatteryPercent > 0.0f
		&& BatteryPercent <= TorchFlickerStartClamped;
	const bool bTorchInLowBatteryRedMode =
		bBatteryEnabled
		&& BatteryPercent > 0.0f
		&& BatteryPercent <= TorchCutoffPercent + KINDA_SMALL_NUMBER;

	if (!bTorchRuntimeActive || !bCanUseTorchFlickerRange)
	{
		bTorchFlickerOn = true;
		TorchFlickerTimeRemaining = 0.0f;
	}
	else
	{
		const float RawFlickerAlpha = 1.0f - FMath::Clamp(
			BatteryPercent / FMath::Max(KINDA_SMALL_NUMBER, TorchFlickerStartClamped),
			0.0f,
			1.0f);
		const float FlickerAlpha = FMath::Pow(
			RawFlickerAlpha,
			FMath::Max(0.1f, TorchFlickerRampExponent));

		const float GapMin = FMath::Lerp(
			FMath::Max(0.01f, TorchFlickerGapMinAtStartSeconds),
			FMath::Max(0.01f, TorchFlickerGapMinAtCutoffSeconds),
			FlickerAlpha);
		const float GapMax = FMath::Lerp(
			FMath::Max(GapMin, TorchFlickerGapMaxAtStartSeconds),
			FMath::Max(0.01f, TorchFlickerGapMaxAtCutoffSeconds),
			FlickerAlpha);
		const float OffMin = FMath::Lerp(
			FMath::Max(0.005f, TorchFlickerOffMinAtStartSeconds),
			FMath::Max(0.005f, TorchFlickerOffMinAtCutoffSeconds),
			FlickerAlpha);
		const float OffMax = FMath::Lerp(
			FMath::Max(OffMin, TorchFlickerOffMaxAtStartSeconds),
			FMath::Max(0.005f, TorchFlickerOffMaxAtCutoffSeconds),
			FlickerAlpha);
		const float OutageChance = FMath::Lerp(
			FMath::Clamp(TorchFlickerOutageChanceAtStart, 0.0f, 1.0f),
			FMath::Clamp(TorchFlickerOutageChanceAtCutoff, 0.0f, 1.0f),
			FlickerAlpha);

		TorchFlickerTimeRemaining = FMath::Max(0.0f, TorchFlickerTimeRemaining - SafeDeltaSeconds);
		if (TorchFlickerTimeRemaining <= KINDA_SMALL_NUMBER)
		{
			if (bTorchFlickerOn)
			{
				const bool bTriggerOutage = FMath::FRand() <= OutageChance;
				bTorchFlickerOn = !bTriggerOutage;
				TorchFlickerTimeRemaining = bTriggerOutage
					? FMath::FRandRange(FMath::Min(OffMin, OffMax), FMath::Max(OffMin, OffMax))
					: FMath::FRandRange(FMath::Min(GapMin, GapMax), FMath::Max(GapMin, GapMax));
			}
			else
			{
				bTorchFlickerOn = true;
				TorchFlickerTimeRemaining = FMath::FRandRange(FMath::Min(GapMin, GapMax), FMath::Max(GapMin, GapMax));
			}
		}
	}

	const bool bTorchVisible = bTorchRuntimeActive && bTorchFlickerOn;
	TInlineComponentArray<USpotLightComponent*> SpotLightComponents(this);
	for (USpotLightComponent* SpotLightComponent : SpotLightComponents)
	{
		if (!IsValid(SpotLightComponent))
		{
			continue;
		}

		TWeakObjectPtr<USpotLightComponent> SpotLightKey(SpotLightComponent);
		float* ExistingBaseIntensity = TorchSpotlightBaseIntensityByComponent.Find(SpotLightKey);
		if (!ExistingBaseIntensity)
		{
			TorchSpotlightBaseIntensityByComponent.Add(SpotLightKey, SpotLightComponent->Intensity);
			ExistingBaseIntensity = TorchSpotlightBaseIntensityByComponent.Find(SpotLightKey);
		}
		const float BaseIntensity = ExistingBaseIntensity ? FMath::Max(0.0f, *ExistingBaseIntensity) : FMath::Max(0.0f, SpotLightComponent->Intensity);
		const float CriticalLowBrightnessAlpha = FMath::Clamp(TorchLowBatteryBrightnessAlpha, 0.0f, 1.0f);
		const float TargetIntensity = bTorchInLowBatteryRedMode
			? BaseIntensity * CriticalLowBrightnessAlpha
			: BaseIntensity;
		SpotLightComponent->SetIntensity(TargetIntensity);

		SpotLightComponent->SetVisibility(bTorchVisible, true);
		if (!bTorchVisible)
		{
			continue;
		}

		if (bTorchInLowBatteryRedMode)
		{
			SpotLightComponent->SetLightColor(TorchLowBatteryColor);
		}
		else if (bForceTorchSpotlightsWhite)
		{
			SpotLightComponent->SetLightColor(TorchSpotlightColor);
		}

		if (bTorchHasAimTarget)
		{
			const FVector AimDirection = (TorchAimTargetLocation - SpotLightComponent->GetComponentLocation()).GetSafeNormal();
			if (!AimDirection.IsNearlyZero())
			{
				const FRotator DesiredRotation = AimDirection.Rotation();
				const float InterpSpeed = FMath::Max(0.0f, TorchAimInterpSpeed);
				const bool bUseInterp = DeltaSeconds > KINDA_SMALL_NUMBER && InterpSpeed > KINDA_SMALL_NUMBER;
				const FRotator NextRotation = bUseInterp
					? FMath::RInterpTo(SpotLightComponent->GetComponentRotation(), DesiredRotation, DeltaSeconds, InterpSpeed)
					: DesiredRotation;
				SpotLightComponent->SetWorldRotation(NextRotation);
			}
		}
	}
}

void ADroneCompanion::UpdateStatusLight(float DeltaSeconds)
{
	if (!DroneStatusLight)
	{
		UpdateSpotlightVisuals(FMath::Max(0.0f, DeltaSeconds));
		return;
	}

	const float SafeDeltaSeconds = FMath::Max(0.0f, DeltaSeconds);
	const float BatteryPercent = BatteryComponent ? BatteryComponent->GetBatteryPercent() : 0.0f;
	const float MaxBatteryPercent = BatteryComponent ? FMath::Max(0.01f, BatteryComponent->GetMaxBatteryPercent()) : 100.0f;
	const float BatteryAlpha = FMath::Clamp(BatteryPercent / MaxBatteryPercent, 0.0f, 1.0f);
	const float FlickerStartPercent = FMath::Clamp(BatteryFlickerStartPercent, 0.0f, MaxBatteryPercent);
	const float RedLightStartPercent = FMath::Clamp(BatteryRedLightStartPercent, 0.0f, MaxBatteryPercent);
	const bool bManuallyPoweredOff = CurrentPowerState == EDronePowerState::PoweredOff;
	const bool bBatteryFlat = BatteryComponent && BatteryComponent->IsBatteryEnabled() && BatteryPercent <= KINDA_SMALL_NUMBER;
	const bool bInLowBatteryRange = FlickerStartPercent > KINDA_SMALL_NUMBER
		&& BatteryPercent > 0.0f
		&& BatteryPercent <= FlickerStartPercent;
	const bool bInRedLightRange = RedLightStartPercent > KINDA_SMALL_NUMBER
		&& BatteryPercent > 0.0f
		&& BatteryPercent <= RedLightStartPercent;

	float FlickerMultiplier = 1.0f;
	LowBatteryFlickerAlpha = 0.0f;
	if (bInLowBatteryRange)
	{
		LowBatteryFlickerAlpha = 1.0f - FMath::Clamp(BatteryPercent / FlickerStartPercent, 0.0f, 1.0f);
		const float FlickerRampAlpha = FMath::Pow(
			LowBatteryFlickerAlpha,
			FMath::Max(0.1f, LowBatteryFlickerRampExponent));

		const float FlickerFrequency = FMath::Lerp(
			FMath::Max(0.0f, LowBatteryFlickerMinFrequencyHz),
			FMath::Max(0.0f, LowBatteryFlickerMaxFrequencyHz),
			FlickerRampAlpha);
		const float FlickerOffChance = FMath::Lerp(
			FMath::Clamp(LowBatteryFlickerMinDepth, 0.0f, 1.0f),
			FMath::Clamp(LowBatteryFlickerMaxDepth, 0.0f, 1.0f),
			FlickerRampAlpha);
		StatusLightFlickerTime = FMath::Max(0.0f, StatusLightFlickerTime - SafeDeltaSeconds);
		if (StatusLightFlickerTime <= KINDA_SMALL_NUMBER)
		{
			bStatusLightFlickerOn = FMath::FRand() > FlickerOffChance;

			const float SafeFrequency = FMath::Max(0.1f, FlickerFrequency);
			const float MeanIntervalSeconds = 1.0f / SafeFrequency;
			const float MinIntervalSeconds = FMath::Max(0.05f, MeanIntervalSeconds * 0.35f);
			const float MaxIntervalSeconds = FMath::Max(MinIntervalSeconds, MeanIntervalSeconds * 1.65f);
			StatusLightFlickerTime = FMath::FRandRange(MinIntervalSeconds, MaxIntervalSeconds);
		}

		FlickerMultiplier = bStatusLightFlickerOn ? 1.0f : 0.0f;
	}
	else
	{
		bStatusLightFlickerOn = true;
		StatusLightFlickerTime = 0.0f;
	}

	const float MinIntensity = FMath::Max(0.0f, StatusLightMinIntensity);
	const float MaxIntensity = FMath::Max(MinIntensity, StatusLightBaseIntensity);
	float BaseIntensity = FMath::Lerp(MinIntensity, MaxIntensity, BatteryAlpha);
	if (bInRedLightRange)
	{
		const float RedStartBatteryAlpha = FMath::Clamp(RedLightStartPercent / MaxBatteryPercent, 0.0f, 1.0f);
		const float IntensityAtRedStart = FMath::Lerp(MinIntensity, MaxIntensity, RedStartBatteryAlpha);
		const float MinRedBrightnessAlpha = FMath::Clamp(StatusLightLowBatteryMinBrightnessAlpha, 0.0f, 1.0f);
		const float RedRangeAlpha = RedLightStartPercent > KINDA_SMALL_NUMBER
			? FMath::Clamp(BatteryPercent / RedLightStartPercent, 0.0f, 1.0f)
			: 0.0f;
		const float RedBrightnessAlpha = FMath::Lerp(MinRedBrightnessAlpha, 1.0f, RedRangeAlpha);
		BaseIntensity = IntensityAtRedStart * RedBrightnessAlpha;
	}

	float FinalIntensity = 0.0f;
	if (bBatteryFlat)
	{
		FlatBatteryPulseTime += SafeDeltaSeconds;
		const float PulseFrequencyHz = FMath::Max(0.01f, FlatBatteryPulseFrequencyHz);
		const float PulseWave = 0.5f + 0.5f * FMath::Sin(FlatBatteryPulseTime * PulseFrequencyHz * 2.0f * PI);
		const float PulseMinAlpha = FMath::Clamp(FlatBatteryPulseMinBrightnessAlpha, 0.0f, 1.0f);
		const float PulseMaxAlpha = FMath::Max(PulseMinAlpha, FMath::Clamp(FlatBatteryPulseMaxBrightnessAlpha, 0.0f, 1.0f));
		const float PulseAlpha = FMath::Lerp(PulseMinAlpha, PulseMaxAlpha, PulseWave);
		FinalIntensity = MaxIntensity * PulseAlpha;
	}
	else
	{
		FlatBatteryPulseTime = 0.0f;
		if (bManuallyPoweredOff)
		{
			if (bUseManualOffHeartbeat)
			{
				ManualOffHeartbeatTime += SafeDeltaSeconds;
				const float HeartbeatWave = 0.5f + 0.5f * FMath::Sin(
					ManualOffHeartbeatTime * FMath::Max(0.01f, ManualOffHeartbeatFrequencyHz) * 2.0f * PI);
				const float HeartbeatMinAlpha = FMath::Clamp(ManualOffHeartbeatMinBrightnessAlpha, 0.0f, 1.0f);
				const float HeartbeatMaxAlpha = FMath::Max(
					HeartbeatMinAlpha,
					FMath::Clamp(ManualOffHeartbeatMaxBrightnessAlpha, 0.0f, 1.0f));
				const float HeartbeatAlpha = FMath::Lerp(HeartbeatMinAlpha, HeartbeatMaxAlpha, HeartbeatWave);
				FinalIntensity = MaxIntensity * HeartbeatAlpha;
			}
			else
			{
				FinalIntensity = 0.0f;
			}
		}
		else
		{
			ManualOffHeartbeatTime = 0.0f;
			FinalIntensity = BaseIntensity * FlickerMultiplier;
		}
	}

	DroneStatusLight->SetIntensity(FinalIntensity);
	DroneStatusLight->SetAttenuationRadius(FMath::Max(0.0f, StatusLightAttenuationRadius));

	const bool bIsFullyChargedInCharger = bIsInChargerVolume && BatteryAlpha >= 0.999f;
	FLinearColor StatusColor = StatusLightNormalColor;
	if (bIsFullyChargedInCharger)
	{
		StatusColor = StatusLightChargingFullColor;
	}
	else if (bBatteryFlat || bInRedLightRange)
	{
		StatusColor = StatusLightLowBatteryColor;
	}
	else if (bInLowBatteryRange)
	{
		const float WarningBlendAlpha = FMath::Pow(FMath::Clamp(LowBatteryFlickerAlpha, 0.0f, 1.0f), 0.7f);
		StatusColor = FLinearColor::LerpUsingHSV(StatusLightNormalColor, StatusLightLowBatteryWarningColor, WarningBlendAlpha);
	}

	DroneStatusLight->SetLightColor(StatusColor);
	const float StatusBrightnessAlpha = MaxIntensity > KINDA_SMALL_NUMBER
		? FMath::Clamp(FinalIntensity / MaxIntensity, 0.0f, 1.0f)
		: 0.0f;
	UpdateDynamicEmissiveMaterials(StatusColor, StatusBrightnessAlpha, BatteryAlpha, bBatteryFlat);
	UpdateSpotlightVisuals(SafeDeltaSeconds);
}

void ADroneCompanion::RemoveAppliedConveyorSurfaceVelocity()
{
	if (!DroneBody)
	{
		AppliedConveyorSurfaceVelocity = FVector::ZeroVector;
		return;
	}

	if (!DroneBody->IsSimulatingPhysics())
	{
		AppliedConveyorSurfaceVelocity = FVector::ZeroVector;
		return;
	}

	if (AppliedConveyorSurfaceVelocity.IsNearlyZero())
	{
		return;
	}

	const FVector CurrentVelocity = DroneBody->GetPhysicsLinearVelocity();
	DroneBody->SetPhysicsLinearVelocity(CurrentVelocity - AppliedConveyorSurfaceVelocity, false);
	AppliedConveyorSurfaceVelocity = FVector::ZeroVector;
}

void ADroneCompanion::ApplyConveyorSurfaceVelocity()
{
	if (!DroneBody || !ConveyorSurfaceVelocity || !ShouldProcessConveyorAssist())
	{
		AppliedConveyorSurfaceVelocity = FVector::ZeroVector;
		return;
	}

	if (!DroneBody->IsSimulatingPhysics())
	{
		AppliedConveyorSurfaceVelocity = FVector::ZeroVector;
		return;
	}

	const FVector SurfaceVelocity = ConveyorSurfaceVelocity->GetConveyorSurfaceVelocity();
	if (SurfaceVelocity.IsNearlyZero())
	{
		AppliedConveyorSurfaceVelocity = FVector::ZeroVector;
		return;
	}

	const FVector CurrentVelocity = DroneBody->GetPhysicsLinearVelocity();
	DroneBody->SetPhysicsLinearVelocity(CurrentVelocity + SurfaceVelocity, false);
	AppliedConveyorSurfaceVelocity = SurfaceVelocity;
}

void ADroneCompanion::SetFollowTarget(AActor* NewFollowTarget)
{
	FollowTarget = NewFollowTarget;
}

void ADroneCompanion::SetHideStaticMeshesFromOwnerCamera(bool bHide)
{
	if (bHideStaticMeshesFromOwnerCamera == bHide)
	{
		return;
	}

	bHideStaticMeshesFromOwnerCamera = bHide;
	ApplyStaticMeshOwnerVisibility();
}

void ADroneCompanion::ApplyStaticMeshOwnerVisibility()
{
	if (!DroneBody)
	{
		return;
	}

	// Only hide the primary drone body mesh in camera-driven modes.
	// Lights and non-body components remain active/visible.
	const bool bHideDroneBodyMesh = bHideStaticMeshesFromOwnerCamera;
	DroneBody->SetOwnerNoSee(false);
	DroneBody->SetHiddenInGame(bHideDroneBodyMesh, true);
	DroneBody->SetVisibility(!bHideDroneBodyMesh, true);
	if (bHideDroneBodyMesh)
	{
		DroneBody->SetCastShadow(true);
	}
	DroneBody->bCastHiddenShadow = bHideDroneBodyMesh;
}

void ADroneCompanion::ApplyEditorCameraVisualizationSettings()
{
#if WITH_EDITORONLY_DATA
	if (!DroneCamera)
	{
		return;
	}

	DroneCamera->bCameraMeshHiddenInGame = true;
	DroneCamera->bDrawFrustumAllowed = false;
	DroneCamera->SetCameraMesh(nullptr);
	DroneCamera->RefreshVisualRepresentation();

	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(this);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!IsValid(PrimitiveComponent)
			|| !PrimitiveComponent->IsVisualizationComponent()
			|| PrimitiveComponent->GetAttachParent() != DroneCamera)
		{
			continue;
		}

		PrimitiveComponent->SetHiddenInGame(true, true);
		PrimitiveComponent->SetVisibility(false, true);
	}
#endif
}

void ADroneCompanion::ApplyPickupProxyVisualizationSettings()
{
	if (!DronePickupProxySphere)
	{
		return;
	}

	// Pickup proxy should always be non-rendering; it exists only as an interaction collider.
	DronePickupProxySphere->SetHiddenInGame(true, true);
	DronePickupProxySphere->SetVisibility(false, true);
	DronePickupProxySphere->SetCastShadow(false);
	DronePickupProxySphere->bCastHiddenShadow = false;
}

void ADroneCompanion::SetTorchEnabled(bool bEnabled)
{
	if (bTorchEnabled == bEnabled)
	{
		return;
	}

	bTorchEnabled = bEnabled;
	RefreshTorchRuntimeState();
}

void ADroneCompanion::ToggleTorchEnabled()
{
	SetTorchEnabled(!bTorchEnabled);
}

void ADroneCompanion::SetTorchAimTarget(const FVector& NewAimTargetLocation)
{
	TorchAimTargetLocation = NewAimTargetLocation;
	bTorchHasAimTarget = true;
}

void ADroneCompanion::ClearTorchAimTarget()
{
	bTorchHasAimTarget = false;
	TorchAimTargetLocation = FVector::ZeroVector;
}

void ADroneCompanion::SetCompanionMode(EDroneCompanionMode NewMode)
{
	const EDroneCompanionMode PreviousMode = CompanionMode;
	CompanionMode = NewMode;
	const bool bModeSupportsLiftAssist =
		CompanionMode == EDroneCompanionMode::BuddyFollow
		|| CompanionMode == EDroneCompanionMode::MapMode
		|| CompanionMode == EDroneCompanionMode::PilotControlled;
	if (!bModeSupportsLiftAssist)
	{
		StopLiftAssist();
	}

	if (CompanionMode != EDroneCompanionMode::PilotControlled)
	{
		bCrashed = false;
		CrashRecoveryTimeRemaining = 0.0f;
		bPendingCrashRollRecovery = false;
	}

	if (CompanionMode != EDroneCompanionMode::PilotControlled)
	{
		ResetPilotInputs();
	}

	const bool bEnteringTopDownMode =
		(CompanionMode == EDroneCompanionMode::MapMode || CompanionMode == EDroneCompanionMode::MiniMapFollow)
		&& (PreviousMode != EDroneCompanionMode::MapMode && PreviousMode != EDroneCompanionMode::MiniMapFollow);
	if (bEnteringTopDownMode)
	{
		MapTargetLocation = DroneBody ? DroneBody->GetComponentLocation() : GetActorLocation();
		bMapModeUsesHeightLimits = bNextMapModeUsesEntryLift;

		if (CompanionMode == EDroneCompanionMode::MiniMapFollow)
		{
			if (FollowTarget)
			{
				MapTargetLocation = FollowTarget->GetActorLocation();
			}
			MapTargetLocation.Z += MiniMapHeightAboveTarget;
		}
		else if (bNextMapModeUsesEntryLift)
		{
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
	}

	const bool bWasTopDownMode = PreviousMode == EDroneCompanionMode::MapMode || PreviousMode == EDroneCompanionMode::MiniMapFollow;
	const bool bIsTopDownMode = CompanionMode == EDroneCompanionMode::MapMode || CompanionMode == EDroneCompanionMode::MiniMapFollow;
	if (bWasTopDownMode != bIsTopDownMode)
	{
		StartCameraTransition(
			GetDesiredCameraMountRotationForMode(CompanionMode),
			GetDesiredCameraFieldOfViewForMode(CompanionMode),
			false);
	}
	else
	{
		RefreshCameraMountRotation();
	}

	RefreshStateDomains(TEXT("SetCompanionMode"));
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
	UpdateAnchoredTransform();
}

void ADroneCompanion::SetHoldTransform(const FVector& NewLocation, const FRotator& NewRotation)
{
	HoldTargetLocation = NewLocation;
	HoldTargetRotation = NewRotation;
}

void ADroneCompanion::SetPilotInputs(float InThrottleInput, float InYawInput, float InRollInput, float InPitchInput)
{
	if (bDronePoweredOff || bChargingLockActive)
	{
		ResetPilotInputs();
		return;
	}

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
	FreeFlyCurrentVelocity = FVector::ZeroVector;
	RollJumpCooldownRemaining = 0.0f;
	CurrentRollCameraLean = 0.0f;
	bPilotHoverAssistTemporarilySuppressed = false;
}

void ADroneCompanion::SetUseSimplePilotControls(bool bEnable)
{
	const bool bWasRollControls = bUseRollPilotControls;
	const bool bNeedsModeChange = bUseSimplePilotControls != bEnable
		|| (bEnable && (bUseFreeFlyPilotControls || bUseSpectatorPilotControls || bUseRollPilotControls));
	if (!bNeedsModeChange)
	{
		return;
	}

	bUseSimplePilotControls = bEnable;
	if (bEnable)
	{
		bUseFreeFlyPilotControls = false;
		bUseSpectatorPilotControls = false;
		bUseRollPilotControls = false;
	}
	FreeFlyCurrentVelocity = FVector::ZeroVector;

	if (!bSuppressCameraMountRefresh)
	{
		RefreshCameraForPilotControlModeChange(bWasRollControls);
	}
}

void ADroneCompanion::SetUseFreeFlyPilotControls(bool bEnable)
{
	const bool bWasRollControls = bUseRollPilotControls;
	const bool bNeedsModeChange = bUseFreeFlyPilotControls != bEnable
		|| (bEnable && (bUseSimplePilotControls || bUseSpectatorPilotControls || bUseRollPilotControls));
	if (!bNeedsModeChange)
	{
		return;
	}

	bUseFreeFlyPilotControls = bEnable;
	if (bEnable)
	{
		bUseSimplePilotControls = false;
		bUseSpectatorPilotControls = false;
		bUseRollPilotControls = false;
	}
	FreeFlyCurrentVelocity = FVector::ZeroVector;

	if (!bSuppressCameraMountRefresh)
	{
		RefreshCameraForPilotControlModeChange(bWasRollControls);
	}
}

void ADroneCompanion::SetUseSpectatorPilotControls(bool bEnable)
{
	const bool bWasRollControls = bUseRollPilotControls;
	const bool bNeedsModeChange = bUseSpectatorPilotControls != bEnable
		|| (bEnable && (bUseSimplePilotControls || bUseFreeFlyPilotControls || bUseRollPilotControls));
	if (!bNeedsModeChange)
	{
		return;
	}

	bUseSpectatorPilotControls = bEnable;
	if (bEnable)
	{
		bUseSimplePilotControls = false;
		bUseFreeFlyPilotControls = false;
		bUseRollPilotControls = false;
	}
	FreeFlyCurrentVelocity = FVector::ZeroVector;

	if (!bSuppressCameraMountRefresh)
	{
		RefreshCameraForPilotControlModeChange(bWasRollControls);
	}
}

void ADroneCompanion::SetUseRollPilotControls(bool bEnable)
{
	const bool bWasRollControls = bUseRollPilotControls;
	const bool bNeedsModeChange = bUseRollPilotControls != bEnable
		|| (bEnable && (bUseSimplePilotControls || bUseFreeFlyPilotControls || bUseSpectatorPilotControls));
	if (!bNeedsModeChange)
	{
		return;
	}

	bUseRollPilotControls = bEnable;
	if (bEnable)
	{
		LogRollModeState(TEXT("EnterRollMode_BeforeReset"), true);
		bUseSimplePilotControls = false;
		bUseFreeFlyPilotControls = false;
		bUseSpectatorPilotControls = false;
		bCrashed = false;
		CrashRecoveryTimeRemaining = 0.0f;
		bImpactDetected = false;
		bImpactWouldCrash = false;
		ImpactDebugTimeRemaining = 0.0f;
		CurrentRollCameraPitchOffset = 0.0f;
		CurrentRollCameraLean = 0.0f;
		RollModeLogTimeRemaining = 0.0f;
		LogRollModeState(TEXT("EnterRollMode_AfterReset"), true);
	}
	else
	{
		CurrentRollCameraPitchOffset = 0.0f;
		CurrentRollCameraLean = 0.0f;
		LogRollModeState(TEXT("ExitRollMode"), false);
	}
	FreeFlyCurrentVelocity = FVector::ZeroVector;

	if (!bSuppressCameraMountRefresh)
	{
		RefreshCameraForPilotControlModeChange(bWasRollControls);
	}
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
	if (DroneBody->IsSimulatingPhysics())
	{
		DroneBody->SetPhysicsLinearVelocity(FVector::ZeroVector);
		DroneBody->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	}
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
	bPilotHoverAssistTemporarilySuppressed = false;
}

bool ADroneCompanion::TryRollJump(float ChargeAlpha)
{
	if (!DroneBody
		|| bDronePoweredOff
		|| bChargingLockActive
		|| !bUseRollPilotControls
		|| CompanionMode != EDroneCompanionMode::PilotControlled
		|| RollJumpCooldownRemaining > 0.0f)
	{
		return false;
	}

	float GroundClearance = -1.0f;
	if (!QueryRollGroundContact(GroundClearance)
		|| GroundClearance > FMath::Max(0.0f, RollGroundedMaxClearance))
	{
		return false;
	}

	const float MinJumpVelocity = FMath::Max(0.0f, RollJumpMinLaunchVelocity);
	const float MaxJumpVelocity = FMath::Max(MinJumpVelocity, RollJumpMaxLaunchVelocity);
	if (MaxJumpVelocity <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float ClampedChargeAlpha = FMath::Clamp(ChargeAlpha, 0.0f, 1.0f);
	const float ShapedChargeAlpha = FMath::Pow(
		ClampedChargeAlpha,
		FMath::Max(KINDA_SMALL_NUMBER, RollJumpChargeExponent));
	const float TargetUpwardSpeed = FMath::Lerp(MinJumpVelocity, MaxJumpVelocity, ShapedChargeAlpha);
	FVector LaunchVelocity = DroneBody->GetPhysicsLinearVelocity();
	if (LaunchVelocity.Z >= TargetUpwardSpeed - KINDA_SMALL_NUMBER)
	{
		return false;
	}

	LaunchVelocity.Z = TargetUpwardSpeed;
	DroneBody->SetPhysicsLinearVelocity(LaunchVelocity);
	RollJumpCooldownRemaining = FMath::Max(0.0f, RollJumpCooldown);
	return true;
}

void ADroneCompanion::SetUseCrashRollRecoveryMode(bool bEnable)
{
	bUseCrashRollRecoveryMode = bEnable;
	if (!bUseCrashRollRecoveryMode)
	{
		bPendingCrashRollRecovery = false;
	}
}

bool ADroneCompanion::ConsumePendingCrashRollRecovery()
{
	const bool bWasPending = bPendingCrashRollRecovery;
	bPendingCrashRollRecovery = false;
	return bWasPending;
}

bool ADroneCompanion::IsStableForRollRecovery() const
{
	if (!DroneBody || !bUseRollPilotControls || CompanionMode != EDroneCompanionMode::PilotControlled)
	{
		return false;
	}

	float GroundClearance = -1.0f;
	return QueryRollGroundContact(GroundClearance)
		&& GroundClearance <= FMath::Max(0.0f, RollGroundedMaxClearance)
		&& DroneBody->GetPhysicsLinearVelocity().Size() <= FMath::Max(0.0f, DroneCrashSettleLinearSpeed)
		&& DroneBody->GetPhysicsAngularVelocityInDegrees().Size() <= FMath::Max(0.0f, DroneCrashSettleAngularSpeed);
}

void ADroneCompanion::SetNextMapModeUsesEntryLift(bool bEnable)
{
	bNextMapModeUsesEntryLift = bEnable;
}

void ADroneCompanion::SetSuppressCameraMountRefresh(bool bSuppress)
{
	bSuppressCameraMountRefresh = bSuppress;
}

bool ADroneCompanion::StartLiftAssist(
	UPrimitiveComponent* InTargetComponent,
	const FVector& InGrabLocation)
{
	const bool bSupportsLiftAssistMode =
		CompanionMode == EDroneCompanionMode::BuddyFollow
		|| CompanionMode == EDroneCompanionMode::MapMode;

	if (!DroneBody
		|| bDronePoweredOff
		|| bChargingLockActive
		|| !InTargetComponent
		|| !InTargetComponent->IsSimulatingPhysics()
		|| !bSupportsLiftAssistMode)
	{
		return false;
	}

	LiftAssistTargetComponent = InTargetComponent;
	LiftAssistLocalGrabOffset = InTargetComponent->GetComponentTransform().InverseTransformPosition(InGrabLocation);
	bLiftAssistActive = true;
	bLiftAssistLatched = false;
	bLiftAssistRopeVisible = false;
	bLiftAssistLiftEngaged = false;
	bLiftAssistFollowEngaged = false;
	LiftAssistStageTimeRemaining = 0.0f;
	LiftAssistForceRampTime = 0.0f;
	LiftAssistSmoothedVerticalAcceleration = 0.0f;
	return true;
}

void ADroneCompanion::StopLiftAssist()
{
	bLiftAssistActive = false;
	bLiftAssistLatched = false;
	bLiftAssistRopeVisible = false;
	bLiftAssistLiftEngaged = false;
	bLiftAssistFollowEngaged = false;
	LiftAssistStageTimeRemaining = 0.0f;
	LiftAssistForceRampTime = 0.0f;
	LiftAssistSmoothedVerticalAcceleration = 0.0f;
	LiftAssistTargetComponent.Reset();
	LiftAssistLocalGrabOffset = FVector::ZeroVector;
}

void ADroneCompanion::SetLiftAssistForceTuning(const FDroneLiftAssistForceTuning& InTuning)
{
	LiftAssistForceTuning = InTuning;
}

void ADroneCompanion::StartPilotCameraTransitionFromThirdPerson()
{
	if (!CameraMount || !DroneCamera)
	{
		return;
	}

	CameraMount->SetRelativeRotation(FRotator::ZeroRotator);
	DroneCamera->SetFieldOfView(GetDesiredCameraFieldOfViewForMode(EDroneCompanionMode::ThirdPersonFollow));

	StartCameraTransition(
		GetDesiredCameraMountRotationForMode(EDroneCompanionMode::PilotControlled),
		GetDesiredCameraFieldOfViewForMode(EDroneCompanionMode::PilotControlled),
		false,
		PilotEntryCameraTransitionDuration);
}

void ADroneCompanion::AdjustCameraTilt(float DeltaDegrees)
{
	CameraTiltDegrees = FMath::Clamp(CameraTiltDegrees + DeltaDegrees, DroneMinCameraTilt, DroneMaxCameraTilt);
	RefreshCameraMountRotation();
}

bool ADroneCompanion::GetDroneCameraTransform(FVector& OutLocation, FRotator& OutRotation) const
{
	if (DroneCamera)
	{
		OutLocation = DroneCamera->GetComponentLocation();
		OutRotation = DroneCamera->GetComponentRotation();
		return true;
	}

	if (DroneBody)
	{
		OutLocation = DroneBody->GetComponentLocation();
		OutRotation = DroneBody->GetComponentRotation();
		return true;
	}

	OutLocation = GetActorLocation();
	OutRotation = GetActorRotation();
	return true;
}

void ADroneCompanion::OnDroneBodyHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (!DroneBody || !Hit.bBlockingHit || bDronePoweredOff || bChargingLockActive)
	{
		return;
	}

	if (bUseRollPilotControls && CompanionMode == EDroneCompanionMode::PilotControlled)
	{
		if (bLogRollModeDiagnostics)
		{
			UE_LOG(
				LogAgent,
				Warning,
				TEXT("[RollDiag] Roll hit ignored. ImpactPoint=(%.1f, %.1f, %.1f) NormalImpulse=%.1f"),
				Hit.ImpactPoint.X,
				Hit.ImpactPoint.Y,
				Hit.ImpactPoint.Z,
				NormalImpulse.Size());
			LogRollModeState(TEXT("RollHitIgnored"), true);
		}
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
		if (bUseCrashRollRecoveryMode && !bUseRollPilotControls)
		{
			bPendingCrashRollRecovery = true;
			bCrashed = false;
			CrashRecoveryTimeRemaining = 0.0f;
			ResetPilotInputs();
			return;
		}

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
	if (bUseRollPilotControls && CompanionMode == EDroneCompanionMode::PilotControlled)
	{
		bCrashed = false;
		CrashRecoveryTimeRemaining = 0.0f;
		return;
	}

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
	const bool bLinearSettled = DroneBody->GetPhysicsLinearVelocity().Size() <= DroneCrashSettleLinearSpeed;
	const bool bAngularSettled = DroneBody->GetPhysicsAngularVelocityInDegrees().Size() <= DroneCrashSettleAngularSpeed;
	const bool bRecoveredByVelocity = bLinearSettled && bAngularSettled;
	const bool bRecoveredByTime = CrashRecoveryTimeRemaining <= 0.0f;

	if (bRecoveredByVelocity || bRecoveredByTime)
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
	const FVector CurrentUpVector = DroneBody->GetUpVector().GetSafeNormal();
	const float LiftDot = FVector::DotProduct(CurrentUpVector, FVector::UpVector);
	const float CurrentSpeed = DroneBody->GetPhysicsLinearVelocity().Size();

	if (bPilotHoverModeEnabled)
	{
		if (!bPilotHoverAssistTemporarilySuppressed
			&& LiftDot <= PilotHoverSelfRightMinUpDot
			&& CurrentSpeed <= FMath::Max(0.0f, PilotHoverSelfRightActivationSpeed))
		{
			bPilotHoverAssistTemporarilySuppressed = true;
		}
		else if (bPilotHoverAssistTemporarilySuppressed
			&& LiftDot >= PilotHoverSelfRightRestoreUpDot)
		{
			bPilotHoverAssistTemporarilySuppressed = false;
		}
	}
	else
	{
		bPilotHoverAssistTemporarilySuppressed = false;
	}

	const bool bHoverAssistActive = bPilotHoverModeEnabled && !bPilotHoverAssistTemporarilySuppressed;
	const bool bShouldHoverSelfRight = bPilotHoverAssistTemporarilySuppressed && bPilotStabilizerEnabled;

	if (bPilotHoverModeEnabled)
	{
		CurrentHoverCommandInput = bHoverAssistActive ? AppliedThrottleInput : 0.0f;
		CurrentHoverLiftDot = LiftDot;
		if (!bHoverAssistActive)
		{
			CurrentHoverBaseAcceleration = 0.0f;
			CurrentHoverVerticalAcceleration = 0.0f;
		}
		else
		{
			const float GravityAcceleration = GetWorld() ? FMath::Abs(GetWorld()->GetGravityZ()) : 980.0f;
			const float EffectiveLiftDot = LiftDot > 0.0f
				? FMath::Max(LiftDot, FMath::Max(KINDA_SMALL_NUMBER, PilotHoverMinUpDot))
				: FMath::Max(KINDA_SMALL_NUMBER, PilotHoverMinUpDot);
			const float BaseAcceleration = GravityAcceleration / EffectiveLiftDot;
			const float CommandAcceleration = AppliedThrottleInput * PilotHoverCollectiveRange;
			const float TotalAcceleration = FMath::Max(0.0f, BaseAcceleration + CommandAcceleration);

			CurrentHoverBaseAcceleration = BaseAcceleration;
			CurrentHoverVerticalAcceleration = TotalAcceleration;

			DroneBody->AddForce(
				CurrentUpVector * TotalAcceleration,
				NAME_None,
				true);
		}
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
		const float UprightAssist = bShouldHoverSelfRight
			? FMath::Max(PilotUprightAssist, PilotHoverSelfRightAssist)
			: PilotUprightAssist;
		const float MaxCorrectionRate = bShouldHoverSelfRight
			? FMath::Max(PilotStabilizerMaxCorrectionRate, PilotHoverSelfRightMaxCorrectionRate)
			: PilotStabilizerMaxCorrectionRate;
		DesiredLocalAngularVelocity += BuildUprightAssistAngularVelocity(
			UprightAssist,
			MaxCorrectionRate);
	}

	ApplyDesiredAngularVelocity(DesiredLocalAngularVelocity, DeltaSeconds, PilotAngularResponse);
}

void ADroneCompanion::UpdateSimpleFlight(float DeltaSeconds)
{
	if (!DroneBody)
	{
		return;
	}

	auto ApplyExpo = [this](float Value)
	{
		const float ExpoAmount = FMath::Clamp(SimpleInputExpo, 0.0f, 0.95f);
		const float Magnitude = FMath::Clamp(FMath::Abs(Value), 0.0f, 1.0f);
		const float LinearValue = Magnitude;
		const float CurvedValue = Magnitude * Magnitude * Magnitude;
		return FMath::Sign(Value) * FMath::Lerp(LinearValue, CurvedValue, ExpoAmount);
	};

	const float ShapedThrottleInput = ApplyExpo(PilotThrottleInput);
	const float ShapedYawInput = ApplyExpo(PilotYawInput);
	const float ShapedRollInput = ApplyExpo(PilotRollInput);
	const float ShapedPitchInput = ApplyExpo(PilotPitchInput);
	AppliedThrottleInput = FMath::FInterpTo(
		AppliedThrottleInput,
		ShapedThrottleInput,
		DeltaSeconds,
		PilotThrottleResponse);

	const FVector CurrentVelocity = DroneBody->GetPhysicsLinearVelocity();
	const float GravityAcceleration = GetWorld() ? FMath::Abs(GetWorld()->GetGravityZ()) : 980.0f;
	const FVector CurrentUpVector = DroneBody->GetUpVector().GetSafeNormal();
	const float LiftDot = FVector::DotProduct(CurrentUpVector, FVector::UpVector);
	const float DesiredVerticalSpeed = AppliedThrottleInput * SimpleMaxVerticalSpeed;
	const float VerticalSpeedError = DesiredVerticalSpeed - CurrentVelocity.Z;
	const float VerticalAcceleration = FMath::Clamp(
		VerticalSpeedError * FMath::Max(SimpleBrakingResponse, 1.0f),
		-FMath::Max(0.0f, SimpleCollectiveRange),
		FMath::Max(0.0f, SimpleCollectiveRange));

	CurrentHoverBaseAcceleration = GravityAcceleration;
	CurrentHoverCommandInput = AppliedThrottleInput;
	CurrentHoverVerticalAcceleration = VerticalAcceleration;
	CurrentHoverLiftDot = LiftDot;

	DroneBody->AddForce(
		FVector::UpVector * (GravityAcceleration + VerticalAcceleration),
		NAME_None,
		true);

	// Simple mode must move in the drone's yaw frame because character look is locked while piloting.
	const FRotator ReferenceYawRotation(0.0f, DroneBody->GetComponentRotation().Yaw, 0.0f);
	const FVector DesiredHorizontalVelocity = (
		(FRotationMatrix(ReferenceYawRotation).GetUnitAxis(EAxis::X) * ShapedPitchInput) +
		(FRotationMatrix(ReferenceYawRotation).GetUnitAxis(EAxis::Y) * ShapedRollInput)).GetClampedToMaxSize(1.0f) * SimpleMaxHorizontalSpeed;
	FVector HorizontalVelocity = CurrentVelocity;
	HorizontalVelocity.Z = 0.0f;

	const bool bHasHorizontalInput = !DesiredHorizontalVelocity.IsNearlyZero(1.0f);
	const float HorizontalResponse = bHasHorizontalInput
		? FMath::Max(SimpleVelocityResponse, 0.0f)
		: FMath::Max(SimpleVelocityResponse, SimpleBrakingResponse);
	const FVector HorizontalAcceleration = (DesiredHorizontalVelocity - HorizontalVelocity) * HorizontalResponse;

	DroneBody->AddForce(
		FVector(HorizontalAcceleration.X, HorizontalAcceleration.Y, 0.0f),
		NAME_None,
		true);

	const float HorizontalSpeed = HorizontalVelocity.Size();
	if (HorizontalSpeed > SimpleMaxHorizontalSpeed)
	{
		const float Overspeed = HorizontalSpeed - SimpleMaxHorizontalSpeed;
		DroneBody->AddForce(
			-HorizontalVelocity.GetSafeNormal() * (Overspeed * FMath::Max(SimpleBrakingResponse, 1.0f)),
			NAME_None,
			true);
	}

	if (SimpleMaxDistanceFromTarget > KINDA_SMALL_NUMBER && FollowTarget)
	{
		const FVector ToDrone = DroneBody->GetComponentLocation() - FollowTarget->GetActorLocation();
		const FVector HorizontalOffset(ToDrone.X, ToDrone.Y, 0.0f);
		const float MaxDistanceSquared = FMath::Square(SimpleMaxDistanceFromTarget);
		if (HorizontalOffset.SizeSquared() >= MaxDistanceSquared)
		{
			const FVector ReturnDirection = -HorizontalOffset.GetSafeNormal();
			DroneBody->AddForce(
				ReturnDirection * (HorizontalOffset.Size() - SimpleMaxDistanceFromTarget) * SimpleVelocityResponse,
				NAME_None,
				true);
		}
	}

	const FRotator CurrentRotation = DroneBody->GetComponentRotation();
	const float DesiredPitchDegrees = -ShapedPitchInput * SimpleMaxTiltDegrees;
	const float DesiredRollDegrees = -ShapedRollInput * SimpleMaxTiltDegrees;
	const float PitchError = FMath::FindDeltaAngleDegrees(CurrentRotation.Pitch, DesiredPitchDegrees);
	const float RollError = FMath::FindDeltaAngleDegrees(CurrentRotation.Roll, DesiredRollDegrees);
	const FVector DesiredLocalAngularVelocity(
		-RollError * SimpleAttitudeHoldRate,
		-PitchError * SimpleAttitudeHoldRate,
		ShapedYawInput * PilotYawRate);

	ApplyDesiredAngularVelocity(
		DesiredLocalAngularVelocity,
		DeltaSeconds,
		FMath::Max(SimpleRotationResponse, FMath::Max(SimpleYawFollowSpeed, SimpleAttitudeHoldRate)));
}

void ADroneCompanion::UpdateFreeFlyFlight(float DeltaSeconds)
{
	if (!DroneBody)
	{
		return;
	}

	const FVector ForwardDirection = ViewReferenceRotation.Vector();
	const FVector RightDirection = FRotationMatrix(ViewReferenceRotation).GetUnitAxis(EAxis::Y);
	const FVector UpDirection = FRotationMatrix(ViewReferenceRotation).GetUnitAxis(EAxis::Z);
	const FVector DesiredVelocity = (
		(ForwardDirection * PilotPitchInput) +
		(RightDirection * PilotRollInput) +
		(UpDirection * PilotThrottleInput)).GetClampedToMaxSize(1.0f) * FreeFlyMaxSpeed;

	const bool bHasInput = !DesiredVelocity.IsNearlyZero(1.0f);
	const float VelocityChangeRate = bHasInput
		? FMath::Max(0.0f, FreeFlyAcceleration)
		: FMath::Max(0.0f, FreeFlyDeceleration);
	FreeFlyCurrentVelocity = FMath::VInterpConstantTo(
		FreeFlyCurrentVelocity,
		DesiredVelocity,
		DeltaSeconds,
		VelocityChangeRate);

	DroneBody->SetPhysicsLinearVelocity(FreeFlyCurrentVelocity);

	const FRotator DesiredRotation = ViewReferenceRotation.GetNormalized();
	const FRotator DeltaRotation = (DesiredRotation - DroneBody->GetComponentRotation()).GetNormalized();
	const float RotationResponse = FMath::Max(0.0f, FreeFlyRotationResponse);
	const FVector DesiredLocalAngularVelocity(
		-DeltaRotation.Roll * RotationResponse,
		-DeltaRotation.Pitch * RotationResponse,
		DeltaRotation.Yaw * RotationResponse);
	ApplyDesiredAngularVelocity(DesiredLocalAngularVelocity, DeltaSeconds, RotationResponse);
	PreviousLinearVelocity = FreeFlyCurrentVelocity;

	CurrentHoverBaseAcceleration = 0.0f;
	CurrentHoverCommandInput = PilotThrottleInput;
	CurrentHoverVerticalAcceleration = FreeFlyCurrentVelocity.Z;
	CurrentHoverLiftDot = 1.0f;
	AppliedThrottleInput = PilotThrottleInput;
}

void ADroneCompanion::UpdateSpectatorFlight(float /*DeltaSeconds*/)
{
	if (!DroneBody)
	{
		return;
	}

	const FVector ForwardDirection = ViewReferenceRotation.Vector();
	const FVector RightDirection = FRotationMatrix(ViewReferenceRotation).GetUnitAxis(EAxis::Y);
	const FVector UpDirection = FVector::UpVector;
	const FVector DesiredVelocity = (
		(ForwardDirection * PilotPitchInput) +
		(RightDirection * PilotRollInput) +
		(UpDirection * PilotThrottleInput)).GetClampedToMaxSize(1.0f) * FreeFlyMaxSpeed;

	FreeFlyCurrentVelocity = DesiredVelocity;
	DroneBody->SetPhysicsLinearVelocity(FreeFlyCurrentVelocity);
	DroneBody->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);

	if (CameraMount)
	{
		bCameraTransitionActive = false;
		CameraMount->SetAbsolute(false, true, false);
		CameraMount->SetWorldRotation(FRotator(
			ViewReferenceRotation.Pitch,
			ViewReferenceRotation.Yaw,
			0.0f));
	}

	PreviousLinearVelocity = FreeFlyCurrentVelocity;
	CurrentHoverBaseAcceleration = 0.0f;
	CurrentHoverCommandInput = PilotThrottleInput;
	CurrentHoverVerticalAcceleration = FreeFlyCurrentVelocity.Z;
	CurrentHoverLiftDot = 1.0f;
	AppliedThrottleInput = PilotThrottleInput;
}

void ADroneCompanion::UpdateRollFlight(float DeltaSeconds)
{
	if (!DroneBody)
	{
		return;
	}

	CurrentHoverBaseAcceleration = 0.0f;
	CurrentHoverCommandInput = 0.0f;
	CurrentHoverVerticalAcceleration = 0.0f;
	CurrentHoverLiftDot = 1.0f;
	AppliedThrottleInput = 0.0f;

	const FRotator ReferenceYawRotation(0.0f, ViewReferenceRotation.Yaw, 0.0f);
	const FVector ForwardDirection = FRotationMatrix(ReferenceYawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(ReferenceYawRotation).GetUnitAxis(EAxis::Y);
	const FVector DriveInput = (
		(ForwardDirection * PilotPitchInput) +
		(RightDirection * PilotRollInput)).GetClampedToMaxSize(1.0f);

	if (!DriveInput.IsNearlyZero())
	{
		DroneBody->AddForce(DriveInput * FMath::Max(0.0f, RollDriveAcceleration), NAME_None, true);
	}

	const float MaxRollSpeed = FMath::Max(0.0f, RollMaxSpeed);
	if (MaxRollSpeed > 0.0f)
	{
		const FVector CurrentVelocity = DroneBody->GetPhysicsLinearVelocity();
		const float VerticalVelocity = CurrentVelocity.Z;
		FVector HorizontalVelocity = CurrentVelocity;
		HorizontalVelocity.Z = 0.0f;
		if (HorizontalVelocity.SizeSquared() > FMath::Square(MaxRollSpeed))
		{
			const FVector ClampedHorizontalVelocity = HorizontalVelocity.GetClampedToMaxSize(MaxRollSpeed);
			DroneBody->SetPhysicsLinearVelocity(FVector(
				ClampedHorizontalVelocity.X,
				ClampedHorizontalVelocity.Y,
				VerticalVelocity));
		}
	}

	if (bLogRollModeDiagnostics && RollModeLogTimeRemaining <= 0.0f)
	{
		RollModeLogTimeRemaining = FMath::Max(0.05f, RollModeLogInterval);
		LogRollModeState(TEXT("RollTick"), true);
	}
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

	if (bMapModeUsesHeightLimits && FollowTarget)
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

void ADroneCompanion::UpdateMiniMapFlight(float DeltaSeconds)
{
	if (!DroneBody)
	{
		return;
	}

	CurrentHoverCommandInput = 0.0f;
	CurrentHoverVerticalAcceleration = 0.0f;
	CurrentHoverBaseAcceleration = 0.0f;
	CurrentHoverLiftDot = 1.0f;

	FVector DesiredLocation = DroneBody->GetComponentLocation();
	if (FollowTarget)
	{
		DesiredLocation = FollowTarget->GetActorLocation() + FVector(0.0f, 0.0f, MiniMapHeightAboveTarget);
	}

	const FVector CurrentVelocity = DroneBody->GetPhysicsLinearVelocity();
	FVector PositionCorrection = ((DesiredLocation - DroneBody->GetComponentLocation()) * MiniMapPositionGain) -
		(CurrentVelocity * MiniMapVelocityDamping);

	if (UWorld* World = GetWorld())
	{
		PositionCorrection.Z += FMath::Abs(World->GetGravityZ());
	}

	DroneBody->AddForce(PositionCorrection, NAME_None, true);

	const FRotator DesiredRotation(0.0f, ViewReferenceRotation.Yaw, 0.0f);
	const FRotator DeltaRotation = (DesiredRotation - DroneBody->GetComponentRotation()).GetNormalized();
	const FVector DesiredLocalAngularVelocity(
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

void ADroneCompanion::UpdateLiftAssistFlight(float DeltaSeconds)
{
	if (!DroneBody)
	{
		return;
	}

	UPrimitiveComponent* TargetComponent = LiftAssistTargetComponent.Get();
	if (!bLiftAssistActive || !TargetComponent || !TargetComponent->IsSimulatingPhysics())
	{
		StopLiftAssist();
		UpdateAutonomousFlight(DeltaSeconds);
		return;
	}

	CurrentHoverCommandInput = 0.0f;
	CurrentHoverVerticalAcceleration = 0.0f;
	CurrentHoverBaseAcceleration = 0.0f;
	CurrentHoverLiftDot = 1.0f;
	AppliedThrottleInput = 0.0f;

	const FVector AnchorLocation = TargetComponent->GetComponentTransform().TransformPosition(LiftAssistLocalGrabOffset);
	FVector DesiredAnchorLocation = AnchorLocation;
	auto ResolveClearanceTargetLocation = [&](const FVector& BaseLocation) -> FVector
	{
		FVector ResolvedAnchorLocation = BaseLocation;

		float GroundZ = ResolvedAnchorLocation.Z;
		if (UWorld* World = GetWorld())
		{
			const float TraceHeight = FMath::Max(2000.0f, FMath::Max(0.0f, LiftAssistDesiredClearance) + 1000.0f);
			const float TraceDepth = FMath::Max(5000.0f, FMath::Max(0.0f, LiftAssistDesiredClearance) + 3000.0f);
			FCollisionQueryParams QueryParams(FName(TEXT("DroneLiftAssistCarryTrace")), false, this);
			QueryParams.AddIgnoredActor(this);
			if (AActor* TargetOwner = TargetComponent->GetOwner())
			{
				QueryParams.AddIgnoredActor(TargetOwner);
			}

			FHitResult GroundHit;
			if (World->LineTraceSingleByChannel(
				GroundHit,
				ResolvedAnchorLocation + FVector(0.0f, 0.0f, TraceHeight),
				ResolvedAnchorLocation - FVector(0.0f, 0.0f, TraceDepth),
				ECC_Visibility,
				QueryParams))
			{
				GroundZ = GroundHit.ImpactPoint.Z;
			}
		}

		ResolvedAnchorLocation.Z = GroundZ + FMath::Max(0.0f, LiftAssistDesiredClearance);
		return ResolvedAnchorLocation;
	};

	const float RopeRestLength = FMath::Max(1.0f, LiftAssistRopeLength);
	const float RopeTautLength = RopeRestLength + FMath::Max(0.0f, LiftAssistRopeSlack);
	FVector DesiredDroneLocation = AnchorLocation + FVector(0.0f, 0.0f, FMath::Max(0.0f, LiftAssistApproachHeight));
	FVector DroneLocation = DroneBody->GetComponentLocation();
	FVector DroneVelocity = DroneBody->GetPhysicsLinearVelocity();
	float LiftAssistGravityAcceleration = 980.0f;
	float LiftAssistMaxPayloadForce = 0.0f;

	if (!bLiftAssistLatched)
	{
		const float LatchDistance = FMath::Max(0.0f, LiftAssistLatchDistance);
		if (FVector::DistSquared(DroneLocation, DesiredDroneLocation) <= FMath::Square(LatchDistance))
		{
			bLiftAssistLatched = true;
			bLiftAssistRopeVisible = false;
			bLiftAssistLiftEngaged = false;
			bLiftAssistFollowEngaged = false;
			LiftAssistStageTimeRemaining = FMath::Max(0.0f, LiftAssistPreRopeDelay);
		}
	}
	else if (!bLiftAssistRopeVisible)
	{
		LiftAssistStageTimeRemaining = FMath::Max(0.0f, LiftAssistStageTimeRemaining - FMath::Max(0.0f, DeltaSeconds));
		LiftAssistForceRampTime = 0.0f;
		LiftAssistSmoothedVerticalAcceleration = 0.0f;
		if (LiftAssistStageTimeRemaining <= KINDA_SMALL_NUMBER)
		{
			bLiftAssistRopeVisible = true;
			bLiftAssistLiftEngaged = false;
			LiftAssistStageTimeRemaining = FMath::Max(0.0f, LiftAssistPreLiftDelay);
		}
	}
	else if (!bLiftAssistLiftEngaged)
	{
		DesiredDroneLocation = AnchorLocation + FVector(0.0f, 0.0f, RopeRestLength);
		LiftAssistForceRampTime = 0.0f;
		LiftAssistSmoothedVerticalAcceleration = 0.0f;

		LiftAssistStageTimeRemaining = FMath::Max(0.0f, LiftAssistStageTimeRemaining - FMath::Max(0.0f, DeltaSeconds));
		if (LiftAssistStageTimeRemaining <= KINDA_SMALL_NUMBER)
		{
			bLiftAssistLiftEngaged = true;
			bLiftAssistFollowEngaged = false;
			LiftAssistStageTimeRemaining = FMath::Max(0.0f, LiftAssistPreFollowDelay);
			LiftAssistForceRampTime = 0.0f;
		}
	}
	else
	{
		if (!bLiftAssistFollowEngaged)
		{
			DesiredAnchorLocation = ResolveClearanceTargetLocation(AnchorLocation);
			LiftAssistStageTimeRemaining = FMath::Max(0.0f, LiftAssistStageTimeRemaining - FMath::Max(0.0f, DeltaSeconds));
			if (LiftAssistStageTimeRemaining <= KINDA_SMALL_NUMBER)
			{
				bLiftAssistFollowEngaged = true;
				LiftAssistStageTimeRemaining = 0.0f;
			}
		}
		else if (FollowTarget)
		{
			const FVector FollowLocation = FollowTarget->GetActorLocation();
			const FVector HorizontalOffsetToPlayer(
				FollowLocation.X - AnchorLocation.X,
				FollowLocation.Y - AnchorLocation.Y,
				0.0f);
			const float FollowDistance = HorizontalOffsetToPlayer.Size();
			const float AllowedRange = FMath::Max(0.0f, LiftAssistFollowRange);
			FVector FollowAnchorLocation = AnchorLocation;
			if (FollowDistance > AllowedRange + KINDA_SMALL_NUMBER)
			{
				const FVector PullDirection = HorizontalOffsetToPlayer / FollowDistance;
				FollowAnchorLocation += PullDirection * (FollowDistance - AllowedRange);
			}

			DesiredAnchorLocation = ResolveClearanceTargetLocation(FollowAnchorLocation);
		}
		else
		{
			DesiredAnchorLocation = ResolveClearanceTargetLocation(AnchorLocation);
		}

		DesiredDroneLocation = DesiredAnchorLocation + FVector(0.0f, 0.0f, RopeRestLength);
	}

	if (bLiftAssistRopeVisible)
	{
		const FVector RopeVector = DroneLocation - AnchorLocation;
		const float RopeDistance = RopeVector.Size();
		if (RopeDistance >= RopeTautLength - 1.0f)
		{
			const FVector RopeDirection = RopeDistance > KINDA_SMALL_NUMBER
				? (RopeVector / RopeDistance)
				: FVector::UpVector;
			if (RopeDistance > RopeTautLength + KINDA_SMALL_NUMBER)
			{
				const FVector ClampedDroneLocation = AnchorLocation + (RopeDirection * RopeTautLength);
				DroneBody->SetWorldLocation(ClampedDroneLocation, false, nullptr, ETeleportType::TeleportPhysics);
				DroneLocation = ClampedDroneLocation;
			}

			const FVector AnchorVelocity = TargetComponent->GetPhysicsLinearVelocityAtPoint(AnchorLocation);
			const FVector RelativeVelocity = DroneVelocity - AnchorVelocity;
			const float OutwardSpeed = FVector::DotProduct(RelativeVelocity, RopeDirection);
			if (OutwardSpeed > 0.0f)
			{
				const float OutwardDampingAlpha = FMath::Clamp(
					DeltaSeconds * FMath::Max(0.0f, LiftAssistRopeOutwardDamping),
					0.0f,
					1.0f);
				DroneVelocity -= RopeDirection * (OutwardSpeed * OutwardDampingAlpha);
				DroneBody->SetPhysicsLinearVelocity(DroneVelocity, false, NAME_None);
			}
		}
	}

	const FVector CurrentVelocity = DroneVelocity;
	if (bLiftAssistLiftEngaged)
	{
		constexpr float LiftRampStartScale = 0.0f;
		constexpr float LiftRampEndScale = 1.0f;
		constexpr float LiftRampDurationSeconds = 3.0f;

		const UWorld* World = GetWorld();
		LiftAssistGravityAcceleration = World ? FMath::Abs(World->GetGravityZ()) : 980.0f;
		const float CarryMassScaleKg = FMath::Max(1.0f, LiftAssistForceTuning.StrengthMassScaleKg);
		const float MaxLiftMassKg = FMath::Max(0.0f, LiftAssistForceTuning.StrengthValue)
			* CarryMassScaleKg
			* FMath::Max(0.0f, LiftAssistStrengthScale);
		LiftAssistMaxPayloadForce = MaxLiftMassKg * LiftAssistGravityAcceleration;

		const float DroneBodyMassKg = FMath::Max(0.01f, DroneBody->GetMass());
		const FVector HorizontalOffset(
			DesiredDroneLocation.X - DroneLocation.X,
			DesiredDroneLocation.Y - DroneLocation.Y,
			0.0f);
		FVector HorizontalVelocity = CurrentVelocity;
		HorizontalVelocity.Z = 0.0f;
		FVector HorizontalDriveForce = ((HorizontalOffset * BuddyPositionGain) - (HorizontalVelocity * BuddyVelocityDamping))
			* DroneBodyMassKg;
		const float MaxHorizontalForce = DroneBodyMassKg * FMath::Max(0.0f, LiftAssistMaxRaiseAcceleration);
		if (MaxHorizontalForce > KINDA_SMALL_NUMBER)
		{
			HorizontalDriveForce = HorizontalDriveForce.GetClampedToMaxSize(MaxHorizontalForce);
		}

		const float HoverForce = DroneBodyMassKg * LiftAssistGravityAcceleration;
		const float HeightError = DesiredAnchorLocation.Z - AnchorLocation.Z;
		const float ClearanceDeadband = FMath::Max(0.0f, LiftAssistClearanceDeadband);
		float HeightErrorWithDeadband = 0.0f;
		if (FMath::Abs(HeightError) > ClearanceDeadband)
		{
			HeightErrorWithDeadband = FMath::Sign(HeightError) * (FMath::Abs(HeightError) - ClearanceDeadband);
		}

		const float PositionResponse = FMath::Max(0.0f, LiftAssistClearanceResponse);
		const float MaxControllerSpeed = FMath::Max(0.0f, LiftAssistPDMaxVerticalSpeed);
		const float DesiredAnchorVerticalSpeed = FMath::Clamp(
			HeightErrorWithDeadband * PositionResponse,
			-MaxControllerSpeed,
			MaxControllerSpeed);

		const float AnchorVerticalSpeed = TargetComponent->GetPhysicsLinearVelocityAtPoint(AnchorLocation).Z;
		const float VerticalSpeedError = DesiredAnchorVerticalSpeed - AnchorVerticalSpeed;
		const float VelocityResponse = FMath::Max(0.0f, LiftAssistPDVelocityResponse);
		const float MaxControllerAcceleration = FMath::Max(0.0f, LiftAssistPDMaxVerticalAcceleration);
		float RawControllerVerticalAcceleration = VerticalSpeedError * VelocityResponse;
		if (MaxControllerAcceleration > KINDA_SMALL_NUMBER)
		{
			RawControllerVerticalAcceleration = FMath::Clamp(
				RawControllerVerticalAcceleration,
				-MaxControllerAcceleration,
				MaxControllerAcceleration);
		}

		const float LiftDemandResponse = FMath::Max(0.0f, LiftAssistDemandResponse);
		LiftAssistSmoothedVerticalAcceleration = LiftDemandResponse > KINDA_SMALL_NUMBER
			? FMath::FInterpTo(
				LiftAssistSmoothedVerticalAcceleration,
				RawControllerVerticalAcceleration,
				DeltaSeconds,
				LiftDemandResponse)
			: RawControllerVerticalAcceleration;
		const float ControllerVerticalAcceleration = LiftAssistSmoothedVerticalAcceleration;
		CurrentHoverBaseAcceleration = LiftAssistGravityAcceleration;
		CurrentHoverCommandInput = DesiredAnchorVerticalSpeed;
		CurrentHoverVerticalAcceleration = ControllerVerticalAcceleration;
		CurrentHoverLiftDot = 1.0f;

		const float PayloadMassKg = FMath::Max(0.01f, TargetComponent->GetMass());
		const float HoverSupportForce = PayloadMassKg * LiftAssistGravityAcceleration;
		const float RawDesiredPayloadSupportForce = PayloadMassKg * FMath::Max(
			0.0f,
			LiftAssistGravityAcceleration + ControllerVerticalAcceleration);
		const float MaxControllerSupportForce = HoverSupportForce * (1.0f + FMath::Max(0.0f, LiftAssistControlLiftOverhead));
		const float DesiredPayloadSupportForce = FMath::Clamp(
			RawDesiredPayloadSupportForce,
			0.0f,
			FMath::Max(HoverSupportForce, MaxControllerSupportForce));
		LiftAssistForceRampTime = FMath::Min(
			LiftAssistForceRampTime + FMath::Max(0.0f, DeltaSeconds),
			LiftRampDurationSeconds);
		const float LiftRampAlpha = LiftRampDurationSeconds > KINDA_SMALL_NUMBER
			? FMath::Clamp(LiftAssistForceRampTime / LiftRampDurationSeconds, 0.0f, 1.0f)
			: 1.0f;
		const float SmoothedLiftRampAlpha = LiftRampAlpha * LiftRampAlpha * (3.0f - (2.0f * LiftRampAlpha));
		const float LiftForceScale = FMath::Lerp(LiftRampStartScale, LiftRampEndScale, SmoothedLiftRampAlpha);
		const float MaxAvailablePayloadForce = LiftAssistMaxPayloadForce * LiftForceScale;
		const float ClampedPayloadTensionForce = FMath::Min(MaxAvailablePayloadForce, DesiredPayloadSupportForce);
		const FVector PayloadSupportAssistForce = FVector::UpVector * ClampedPayloadTensionForce;
		const float VerticalDampingAcceleration = -CurrentVelocity.Z * FMath::Max(0.0f, LiftAssistVerticalDamping);
		const FVector VerticalDampingForce = FVector::UpVector * (DroneBodyMassKg * VerticalDampingAcceleration);
		FVector DroneDriveForce = HorizontalDriveForce
			+ FVector::UpVector * HoverForce
			+ PayloadSupportAssistForce
			+ VerticalDampingForce;

		if (bLiftAssistRopeVisible)
		{
			const FVector RopeVector = DroneLocation - AnchorLocation;
			const float RopeDistance = RopeVector.Size();
			if (RopeDistance >= RopeTautLength - 1.0f)
			{
				const FVector RopeDirection = RopeDistance > KINDA_SMALL_NUMBER
					? (RopeVector / RopeDistance)
					: FVector::UpVector;
				if (ClampedPayloadTensionForce > KINDA_SMALL_NUMBER)
				{
					const FVector RopePullForce = RopeDirection * ClampedPayloadTensionForce;
					TargetComponent->WakeAllRigidBodies();
					TargetComponent->AddForceAtLocation(RopePullForce, AnchorLocation, NAME_None);
					DroneDriveForce += -RopePullForce * FMath::Max(0.0f, LiftAssistReactionScale);
				}
			}
		}

		DroneBody->AddForce(DroneDriveForce, NAME_None, false);
	}
	else
	{
		DroneBody->AddForce(
			((DesiredDroneLocation - DroneLocation) * BuddyPositionGain) - (CurrentVelocity * BuddyVelocityDamping),
			NAME_None,
			true);
	}

	FRotator DesiredRotation = DroneBody->GetComponentRotation();
	const FVector DesiredForward = bLiftAssistRopeVisible
		? (AnchorLocation - DroneLocation)
		: (DesiredDroneLocation - DroneLocation);
	if (!DesiredForward.IsNearlyZero())
	{
		DesiredRotation = DesiredForward.Rotation();
	}

	const FRotator DeltaRotation = (DesiredRotation - DroneBody->GetComponentRotation()).GetNormalized();
	const float RotationResponse = FMath::Max(0.0f, AutopilotAngularResponse);
	const FVector DesiredLocalAngularVelocity(
		-DeltaRotation.Roll * RotationResponse,
		-DeltaRotation.Pitch * RotationResponse,
		DeltaRotation.Yaw * RotationResponse);
	ApplyDesiredAngularVelocity(DesiredLocalAngularVelocity, DeltaSeconds, RotationResponse);

	if (bLiftAssistRopeVisible)
	{
		if (UWorld* World = GetWorld())
		{
			const float RopeDistance = FVector::Dist(DroneLocation, AnchorLocation);
			const FColor RopeColor = bLiftAssistLiftEngaged
				? (RopeDistance > RopeTautLength ? FColor::Orange : FColor::Yellow)
				: FColor::Cyan;
			DrawDebugLine(World, DroneLocation, AnchorLocation, RopeColor, false, 0.0f, 0, 2.0f);
		}
	}
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
	if (!DroneBody || !DroneBody->IsSimulatingPhysics() || !ShouldClampVelocityForCurrentState())
	{
		return;
	}

	float MaxAllowedLinearSpeed = DroneMaxLinearSpeed;
	if (CompanionMode == EDroneCompanionMode::BuddyFollow && BuddyMaxLinearSpeed > 0.0f)
	{
		MaxAllowedLinearSpeed = FMath::Min(MaxAllowedLinearSpeed, BuddyMaxLinearSpeed);
	}

	const FVector CurrentLinearVelocity = DroneBody->GetPhysicsLinearVelocity();
	if (CurrentLinearVelocity.SizeSquared() > FMath::Square(MaxAllowedLinearSpeed))
	{
		DroneBody->SetPhysicsLinearVelocity(CurrentLinearVelocity.GetClampedToMaxSize(MaxAllowedLinearSpeed));
	}

	float MaxAllowedAngularSpeed = DroneMaxAngularSpeed;
	if (bUseRollPilotControls
		&& CompanionMode == EDroneCompanionMode::PilotControlled
		&& RollMaxAngularSpeed > 0.0f)
	{
		MaxAllowedAngularSpeed = FMath::Min(MaxAllowedAngularSpeed, RollMaxAngularSpeed);
	}

	const FVector CurrentAngularVelocity = DroneBody->GetPhysicsAngularVelocityInDegrees();
	if (CurrentAngularVelocity.SizeSquared() > FMath::Square(MaxAllowedAngularSpeed))
	{
		DroneBody->SetPhysicsAngularVelocityInDegrees(CurrentAngularVelocity.GetClampedToMaxSize(MaxAllowedAngularSpeed));
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

void ADroneCompanion::UpdateRollCamera(float DeltaSeconds)
{
	if (!CameraMount || !DroneBody)
	{
		return;
	}

	const FVector CurrentVelocity = DroneBody->GetPhysicsLinearVelocity();
	const FVector HorizontalVelocity(CurrentVelocity.X, CurrentVelocity.Y, 0.0f);
	const FRotator ReferenceYawRotation(0.0f, ViewReferenceRotation.Yaw, 0.0f);
	const FVector ForwardDirection = FRotationMatrix(ReferenceYawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(ReferenceYawRotation).GetUnitAxis(EAxis::Y);
	const float SpeedForMaxInfluence = FMath::Max(1.0f, RollCameraInfluenceVelocity);
	const float ForwardSpeedRatio = FMath::Clamp(
		FVector::DotProduct(HorizontalVelocity, ForwardDirection) / SpeedForMaxInfluence,
		-1.0f,
		1.0f);
	const float RightSpeedRatio = FMath::Clamp(
		FVector::DotProduct(HorizontalVelocity, RightDirection) / SpeedForMaxInfluence,
		-1.0f,
		1.0f);
	const float ResponseSpeed = FMath::Max(0.0f, RollCameraLeanResponse);
	const float TargetPitchOffset = -ForwardSpeedRatio * RollCameraPitchInfluenceDegrees;
	const float TargetRollOffset = -RightSpeedRatio * RollCameraLeanDegrees;

	CurrentRollCameraPitchOffset = FMath::FInterpTo(
		CurrentRollCameraPitchOffset,
		TargetPitchOffset,
		DeltaSeconds,
		ResponseSpeed);
	CurrentRollCameraLean = FMath::FInterpTo(
		CurrentRollCameraLean,
		TargetRollOffset,
		DeltaSeconds,
		ResponseSpeed);

	CameraMount->SetAbsolute(false, true, false);
	CameraMount->SetWorldRotation(FRotator(
		ViewReferenceRotation.Pitch + CurrentRollCameraPitchOffset,
		ViewReferenceRotation.Yaw,
		CurrentRollCameraLean));
}

bool ADroneCompanion::QueryRollGroundContact(float& OutGroundClearance) const
{
	OutGroundClearance = -1.0f;
	if (!DroneBody || !GetWorld())
	{
		return false;
	}

	const float SphereRadius = FMath::Max(DroneBody->Bounds.SphereRadius, 1.0f);
	const float TraceDistance = FMath::Max(RollGroundCheckDistance + SphereRadius, SphereRadius);
	const FVector TraceStart = DroneBody->GetComponentLocation();
	const FVector TraceEnd = TraceStart - FVector(0.0f, 0.0f, TraceDistance);
	FCollisionQueryParams QueryParams(FName(TEXT("DroneRollGroundTrace")), false, this);
	QueryParams.AddIgnoredActor(this);

	FHitResult GroundHit;
	if (!GetWorld()->LineTraceSingleByChannel(
		GroundHit,
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		QueryParams))
	{
		return false;
	}

	OutGroundClearance = TraceStart.Z - GroundHit.ImpactPoint.Z - SphereRadius;
	return true;
}

void ADroneCompanion::LogRollModeState(const TCHAR* Context, bool bProbeGround)
{
	if (!bLogRollModeDiagnostics || !DroneBody)
	{
		return;
	}

	bool bGrounded = false;
	float GroundClearance = -1.0f;
	if (bProbeGround)
	{
		bGrounded = QueryRollGroundContact(GroundClearance)
			&& GroundClearance <= FMath::Max(0.0f, RollGroundedMaxClearance);
	}

	const FVector LinearVelocity = DroneBody->GetPhysicsLinearVelocity();
	const FVector AngularVelocity = DroneBody->GetPhysicsAngularVelocityInDegrees();
	const FRotator BodyRotation = DroneBody->GetComponentRotation();
	const FRotator CameraRotation = CameraMount ? CameraMount->GetComponentRotation() : FRotator::ZeroRotator;
	UE_LOG(
		LogAgent,
		Warning,
		TEXT("[RollDiag] %s Mode=%d Roll=%d CamTransition=%d Grounded=%d Clearance=%.1f Pos=(%.1f, %.1f, %.1f) BodyRot=(P%.1f Y%.1f R%.1f) CamRot=(P%.1f Y%.1f R%.1f) LinVel=(%.1f, %.1f, %.1f)|%.1f AngVel=(%.1f, %.1f, %.1f)|%.1f Inputs=(Thr %.2f Yaw %.2f Roll %.2f Pitch %.2f) Lean=%.1f Crashed=%d"),
		Context,
		static_cast<int32>(CompanionMode),
		bUseRollPilotControls ? 1 : 0,
		bCameraTransitionActive ? 1 : 0,
		bGrounded ? 1 : 0,
		GroundClearance,
		DroneBody->GetComponentLocation().X,
		DroneBody->GetComponentLocation().Y,
		DroneBody->GetComponentLocation().Z,
		BodyRotation.Pitch,
		BodyRotation.Yaw,
		BodyRotation.Roll,
		CameraRotation.Pitch,
		CameraRotation.Yaw,
		CameraRotation.Roll,
		LinearVelocity.X,
		LinearVelocity.Y,
		LinearVelocity.Z,
		LinearVelocity.Size(),
		AngularVelocity.X,
		AngularVelocity.Y,
		AngularVelocity.Z,
		AngularVelocity.Size(),
		PilotThrottleInput,
		PilotYawInput,
		PilotRollInput,
		PilotPitchInput,
		CurrentRollCameraLean,
		bCrashed ? 1 : 0);
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
		if (bUseRollPilotControls)
		{
			ModeLabel = TEXT("PilotRoll");
		}
		else if (bUseSpectatorPilotControls)
		{
			ModeLabel = TEXT("PilotSpectator");
		}
		else if (bUseFreeFlyPilotControls)
		{
			ModeLabel = TEXT("PilotFreeFly");
		}
		else if (bUseSimplePilotControls)
		{
			ModeLabel = TEXT("PilotSimple");
		}
		else if (bPilotStabilizerEnabled && bPilotHoverModeEnabled)
		{
			ModeLabel = TEXT("PilotHorizonHover");
		}
		else if (bPilotStabilizerEnabled)
		{
			ModeLabel = TEXT("PilotHorizon");
		}
		else
		{
			ModeLabel = TEXT("PilotComplex");
		}
	}
	else if (CompanionMode == EDroneCompanionMode::HoldPosition)
	{
		ModeLabel = TEXT("Hold");
	}
	else if (CompanionMode == EDroneCompanionMode::Idle)
	{
		ModeLabel = TEXT("Idle");
	}
	else if (CompanionMode == EDroneCompanionMode::BuddyFollow)
	{
		ModeLabel = bLiftAssistActive ? TEXT("BuddyLift") : TEXT("Buddy");
	}
	else if (CompanionMode == EDroneCompanionMode::MapMode)
	{
		ModeLabel = TEXT("Map");
	}
	else if (CompanionMode == EDroneCompanionMode::MiniMapFollow)
	{
		ModeLabel = TEXT("MiniMap");
	}

	const FVector Velocity = DroneBody->GetPhysicsLinearVelocity();
	const FVector AngularVelocity = DroneBody->GetPhysicsAngularVelocityInDegrees();
	const TCHAR* AssistLabel = TEXT("ACRO / RATE");
	if (bUseRollPilotControls)
	{
		AssistLabel = TEXT("ROLL");
	}
	else if (bUseSpectatorPilotControls)
	{
		AssistLabel = TEXT("SPECTATOR");
	}
	else if (bUseFreeFlyPilotControls)
	{
		AssistLabel = TEXT("FREE FLY");
	}
	else if (bUseSimplePilotControls)
	{
		AssistLabel = TEXT("SIMPLE");
	}
	else if (bPilotStabilizerEnabled)
	{
		AssistLabel = TEXT("ANGLE / HORIZON");
	}
	const FString AssistDebugText = FString::Printf(
		TEXT("Flight Assist: %s"),
		AssistLabel);
	const FColor AssistDebugColor = (bPilotStabilizerEnabled || bUseSimplePilotControls || bUseFreeFlyPilotControls || bUseSpectatorPilotControls || bUseRollPilotControls)
		? FColor::Green
		: FColor::Red;
	const FString HoverDebugText = FString::Printf(
		TEXT("Hover Assist: %s"),
		bPilotHoverAssistTemporarilySuppressed
			? TEXT("AUTO OFF")
			: (bPilotHoverModeEnabled ? TEXT("ON") : TEXT("OFF")));
	const FColor HoverDebugColor = bPilotHoverAssistTemporarilySuppressed
		? FColor::Yellow
		: (bPilotHoverModeEnabled ? FColor::Green : FColor::Red);
	const float BatteryPercent = GetBatteryPercent();
	const FString DebugText = FString::Printf(
		TEXT("Drone Companion\n")
		TEXT("Mode: %s  Stab: %s  Hover: %s  Crashed: %s  Recovery: %.2fs\n")
		TEXT("State: Power %d  Mobility %d  Role %d  Control %d  ManualOff: %s\n")
		TEXT("Battery: %.2f%%  Dead: %s  Charger: %s  ChargeLock: %s  Flicker: %.2f\n")
		TEXT("Speed: %.0f / %.0f uu/s  Angular: %.0f deg/s\n")
		TEXT("Impact: %s  Would Crash: %s  Last Impact: %.0f\n")
		TEXT("Inputs: V %.2f  Y %.2f  X %.2f  F %.2f  Cam Tilt: %.1f  FOV: %.1f  Lean: %.1f\n")
		TEXT("Hover: Base %.1f  Total %.1f  Cmd %.2f  Lift %.2f  VZ %.1f"),
		ModeLabel,
		bPilotStabilizerEnabled ? TEXT("ON") : TEXT("OFF"),
		bPilotHoverModeEnabled ? TEXT("ON") : TEXT("OFF"),
		bCrashed ? TEXT("YES") : TEXT("NO"),
		CrashRecoveryTimeRemaining,
		static_cast<int32>(CurrentPowerState),
		static_cast<int32>(CurrentMobilityState),
		static_cast<int32>(CurrentRoleState),
		static_cast<int32>(CurrentControlState),
		bManualPowerOffRequested ? TEXT("YES") : TEXT("NO"),
		BatteryPercent,
		bDronePoweredOff ? TEXT("YES") : TEXT("NO"),
		bIsInChargerVolume ? TEXT("YES") : TEXT("NO"),
		bChargingLockActive ? TEXT("YES") : TEXT("NO"),
		LowBatteryFlickerAlpha,
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
		DroneCamera ? DroneCamera->FieldOfView : GetDesiredCameraFieldOfViewForMode(CompanionMode),
		CurrentRollCameraLean,
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

void ADroneCompanion::RefreshCameraForPilotControlModeChange(bool bWasRollControls)
{
	const bool bLeavingRoll = bWasRollControls && !bUseRollPilotControls;
	if (bLeavingRoll && CompanionMode == EDroneCompanionMode::PilotControlled && !bUseSpectatorPilotControls)
	{
		StartCameraTransition(
			GetDesiredCameraMountRotationForMode(CompanionMode),
			GetDesiredCameraFieldOfViewForMode(CompanionMode),
			false,
			FMath::Max(1.0f, PilotModeCameraTransitionDuration));
		return;
	}

	RefreshCameraMountRotation();
}

void ADroneCompanion::UpdateCameraTransition(float DeltaSeconds)
{
	if (!bCameraTransitionActive || !CameraMount || !DroneCamera)
	{
		return;
	}

	CameraTransitionElapsed = FMath::Min(
		CameraTransitionElapsed + FMath::Max(0.0f, DeltaSeconds),
		CameraTransitionTotalDuration);
	const float Duration = FMath::Max(KINDA_SMALL_NUMBER, CameraTransitionTotalDuration);
	const float RawAlpha = CameraTransitionElapsed / Duration;
	const float Alpha = FMath::InterpEaseInOut(0.0f, 1.0f, RawAlpha, 2.0f);
	const FQuat NewRotation = FQuat::Slerp(
		CameraTransitionStartRotation.Quaternion(),
		CameraTransitionTargetRotation.Quaternion(),
		Alpha);

	CameraMount->SetRelativeRotation(NewRotation);
	DroneCamera->SetFieldOfView(FMath::Lerp(
		CameraTransitionStartFieldOfView,
		CameraTransitionTargetFieldOfView,
		Alpha));

	if (RawAlpha >= 1.0f)
	{
		bCameraTransitionActive = false;
		CameraMount->SetRelativeRotation(CameraTransitionTargetRotation);
		DroneCamera->SetFieldOfView(CameraTransitionTargetFieldOfView);
	}
}

void ADroneCompanion::StartCameraTransition(
	const FRotator& TargetRotation,
	float TargetFieldOfView,
	bool bInstant,
	float TransitionDuration)
{
	if (!CameraMount || !DroneCamera)
	{
		return;
	}

	FRotator CurrentStartRotation = CameraMount->GetRelativeRotation();
	if (USceneComponent* CameraParent = CameraMount->GetAttachParent())
	{
		const FQuat ParentWorldRotation = CameraParent->GetComponentQuat();
		const FQuat CameraWorldRotation = CameraMount->GetComponentQuat();
		CurrentStartRotation = (ParentWorldRotation.Inverse() * CameraWorldRotation).Rotator();
	}
	else
	{
		CurrentStartRotation = CameraMount->GetComponentRotation();
	}

	CameraMount->SetAbsolute(false, false, false);
	CameraMount->SetRelativeRotation(CurrentStartRotation);
	CameraTransitionStartRotation = CurrentStartRotation;
	CameraTransitionTargetRotation = TargetRotation;
	CameraTransitionStartFieldOfView = DroneCamera->FieldOfView;
	CameraTransitionTargetFieldOfView = TargetFieldOfView;
	CameraTransitionElapsed = 0.0f;
	const float RequestedDuration = TransitionDuration >= 0.0f ? TransitionDuration : MapCameraTransitionDuration;
	CameraTransitionTotalDuration = FMath::Max(0.0f, RequestedDuration);

	if (bInstant || CameraTransitionTotalDuration <= KINDA_SMALL_NUMBER)
	{
		bCameraTransitionActive = false;
		CameraMount->SetRelativeRotation(CameraTransitionTargetRotation);
		DroneCamera->SetFieldOfView(CameraTransitionTargetFieldOfView);
		return;
	}

	bCameraTransitionActive = true;
}

FRotator ADroneCompanion::GetDesiredCameraMountRotationForMode(EDroneCompanionMode ForMode) const
{
	if (ForMode == EDroneCompanionMode::ThirdPersonFollow
		|| ForMode == EDroneCompanionMode::Idle
		|| ForMode == EDroneCompanionMode::HoldPosition)
	{
		return FRotator::ZeroRotator;
	}

	if (ForMode == EDroneCompanionMode::MapMode || ForMode == EDroneCompanionMode::MiniMapFollow)
	{
		return FRotator(MapCameraPitchDegrees, 0.0f, 0.0f);
	}

	if (ForMode == EDroneCompanionMode::PilotControlled && bUseFreeFlyPilotControls)
	{
		return FRotator::ZeroRotator;
	}

	if (ForMode == EDroneCompanionMode::PilotControlled && bUseSpectatorPilotControls)
	{
		return FRotator::ZeroRotator;
	}

	if (ForMode == EDroneCompanionMode::PilotControlled && bUseRollPilotControls)
	{
		return FRotator::ZeroRotator;
	}

	return FRotator(CameraTiltDegrees, 0.0f, 0.0f);
}

float ADroneCompanion::GetDesiredCameraFieldOfViewForMode(EDroneCompanionMode ForMode) const
{
	float BaseFieldOfView = PilotCameraFieldOfView;

	if (ForMode == EDroneCompanionMode::ThirdPersonFollow || ForMode == EDroneCompanionMode::Idle)
	{
		BaseFieldOfView = ThirdPersonCameraFieldOfView;
	}
	else if (ForMode == EDroneCompanionMode::MapMode || ForMode == EDroneCompanionMode::MiniMapFollow)
	{
		BaseFieldOfView = MapCameraFieldOfView;
	}

	return FMath::Clamp(
		BaseFieldOfView,
		DroneMinCameraFieldOfView,
		DroneMaxCameraFieldOfView);
}

void ADroneCompanion::RefreshCameraMountRotation()
{
	if (!CameraMount)
	{
		return;
	}

	bCameraTransitionActive = false;
	CameraMount->SetAbsolute(false, bUseRollPilotControls || bUseSpectatorPilotControls, false);
	CameraMount->SetRelativeRotation(GetDesiredCameraMountRotationForMode(CompanionMode));
	if (bUseRollPilotControls)
	{
		CameraMount->SetWorldRotation(FRotator(
			ViewReferenceRotation.Pitch + CurrentRollCameraPitchOffset,
			ViewReferenceRotation.Yaw,
			CurrentRollCameraLean));
	}
	else if (bUseSpectatorPilotControls)
	{
		CameraMount->SetWorldRotation(FRotator(
			ViewReferenceRotation.Pitch,
			ViewReferenceRotation.Yaw,
			0.0f));
	}

	if (DroneCamera)
	{
		DroneCamera->SetFieldOfView(GetDesiredCameraFieldOfViewForMode(CompanionMode));
	}
}
