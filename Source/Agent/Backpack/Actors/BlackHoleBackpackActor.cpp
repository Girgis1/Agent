// Copyright Epic Games, Inc. All Rights Reserved.

#include "Backpack/Actors/BlackHoleBackpackActor.h"
#include "Battery/AgentBatteryComponent.h"
#include "Backpack/Components/BlackHoleBackpackLinkComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Machine/InputVolume.h"
#include "UObject/ConstructorHelpers.h"

ABlackHoleBackpackActor::ABlackHoleBackpackActor()
{
	PrimaryActorTick.bCanEverTick = false;

	ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
	SetRootComponent(ItemMesh);
	ItemMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ItemMesh->SetCollisionObjectType(ECC_PhysicsBody);
	ItemMesh->SetCollisionResponseToAllChannels(ECR_Block);
	ItemMesh->SetGenerateOverlapEvents(true);
	ItemMesh->SetSimulatePhysics(false);
	ItemMesh->SetEnableGravity(false);
	ItemMesh->SetCanEverAffectNavigation(false);

	SnapAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("SnapAnchor"));
	SnapAnchor->SetupAttachment(ItemMesh);
	SnapAnchor->SetRelativeLocation(FVector::ZeroVector);
	SnapAnchor->SetRelativeRotation(FRotator::ZeroRotator);
	SnapAnchor->SetRelativeScale3D(FVector::OneVector);

	InputVolume = CreateDefaultSubobject<UInputVolume>(TEXT("InputVolume"));
	InputVolume->SetupAttachment(ItemMesh);
	InputVolume->SetBoxExtent(FVector(36.0f, 26.0f, 20.0f));
	InputVolume->SetRelativeLocation(FVector(-42.0f, 0.0f, 16.0f));
	InputVolume->IntakeInterval = 0.05f;

	LinkComponent = CreateDefaultSubobject<UBlackHoleBackpackLinkComponent>(TEXT("LinkComponent"));

	BackpackBattery = CreateDefaultSubobject<UAgentBatteryComponent>(TEXT("BackpackBattery"));
	BackpackBattery->MaxCharge = 100.0f;
	BackpackBattery->CurrentCharge = 100.0f;
	BackpackBattery->FullThresholdCharge = 100.0f;
	BackpackBattery->DrainRatePerSecond = 5.0f;
	BackpackBattery->ChargeRatePerSecond = 7.5f;
	BackpackBattery->bAutoDrainEnabled = false;
	BackpackBattery->bAutoChargeEnabled = true;

	PortalStateLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("PortalStateLight"));
	PortalStateLight->SetupAttachment(ItemMesh);
	PortalStateLight->SetRelativeLocation(FVector(0.0f, 0.0f, 28.0f));
	PortalStateLight->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BackpackMeshAsset(
		TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (BackpackMeshAsset.Succeeded())
	{
		ItemMesh->SetStaticMesh(BackpackMeshAsset.Object);
	}
}

void ABlackHoleBackpackActor::BeginPlay()
{
	Super::BeginPlay();

	if (ItemMesh)
	{
		BaseItemMeshRelativeScale = ItemMesh->GetRelativeScale3D();
	}

	if (BackpackBattery)
	{
		BackpackBattery->OnBatteryDepleted.AddDynamic(this, &ABlackHoleBackpackActor::HandlePortalBatteryDepleted);
	}

	bPortalEnabled = bStartPortalEnabled;
	RefreshPortalStateVisuals();

	SetDeployedState(bStartDeployed);

	if (InputVolume)
	{
		// Teleporter backpack intake is handled by the link component; keep InputVolume sensor-only.
		InputVolume->bAutoResolveMachineComponent = false;
		InputVolume->SetMachineComponent(nullptr);
	}

	if (LinkComponent)
	{
		LinkComponent->SetSourceMachineComponent(nullptr);
		LinkComponent->SetLinkId(LinkId);
	}

	SetPortalEnabled(bPortalEnabled);
	HandleDeploymentStateChanged(IsItemDeployed());
}

void ABlackHoleBackpackActor::AttachToCarrier(USceneComponent* CarrierComponent, FName SocketName, const FTransform& FineTuneOffset)
{
	if (!ItemMesh || !CarrierComponent)
	{
		return;
	}

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	SetMeshSimulating(false);

	// Apply desired attached scale first so snap-anchor alignment is solved using final scale.
	ItemMesh->SetWorldScale3D(ResolveAttachedRelativeScale(FineTuneOffset));

	const FTransform TargetRootWorldTransform = BuildAttachedRootWorldTransform(CarrierComponent, SocketName, FineTuneOffset);
	SetActorLocationAndRotation(
		TargetRootWorldTransform.GetLocation(),
		TargetRootWorldTransform.Rotator(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);

	ItemMesh->AttachToComponent(
		CarrierComponent,
		FAttachmentTransformRules::KeepWorldTransform,
		SocketName);

	bIsDeployed = false;
	SetMeshCollisionForCurrentState();
	HandleDeploymentStateChanged(false);
}

void ABlackHoleBackpackActor::DeployToWorld(const FVector& InitialImpulse)
{
	if (!ItemMesh)
	{
		return;
	}

	DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	bIsDeployed = true;
	SetMeshCollisionForCurrentState();
	SetMeshSimulating(true);
	HandleDeploymentStateChanged(true);

	if (!InitialImpulse.IsNearlyZero())
	{
		ItemMesh->AddImpulse(InitialImpulse, NAME_None, true);
	}
}

void ABlackHoleBackpackActor::SetDeployedState(bool bInDeployed, const FVector& InitialImpulse)
{
	if (bInDeployed)
	{
		DeployToWorld(InitialImpulse);
		return;
	}

	bIsDeployed = false;
	SetMeshSimulating(false);
	SetMeshCollisionForCurrentState();
	HandleDeploymentStateChanged(false);
}

void ABlackHoleBackpackActor::SetPortalEnabled(bool bInEnabled)
{
	bool bResolvedEnable = bInEnabled;
	if (bResolvedEnable
		&& BackpackBattery
		&& BackpackBattery->IsBatteryEnabled()
		&& BackpackBattery->IsDepleted())
	{
		bResolvedEnable = false;
	}

	bPortalEnabled = bResolvedEnable;

	if (LinkComponent)
	{
		LinkComponent->SetBlackHoleEnabled(bPortalEnabled);
	}

	RefreshIntakeVolumeState();
	UpdatePortalBatteryRuntimeState();
	RefreshPortalStateVisuals();
}

void ABlackHoleBackpackActor::TogglePortalEnabled()
{
	SetPortalEnabled(!bPortalEnabled);
}

FTransform ABlackHoleBackpackActor::GetSnapAnchorLocalTransform() const
{
	return SnapAnchor ? SnapAnchor->GetRelativeTransform() : FTransform::Identity;
}

FTransform ABlackHoleBackpackActor::GetSnapAnchorWorldTransform() const
{
	return SnapAnchor ? SnapAnchor->GetComponentTransform() : GetActorTransform();
}

void ABlackHoleBackpackActor::SetSnapAnchorLocalTransform(const FTransform& NewLocalTransform)
{
	if (!SnapAnchor)
	{
		return;
	}

	SnapAnchor->SetRelativeTransform(NewLocalTransform);
}

void ABlackHoleBackpackActor::SetSnapAnchorLocalLocation(const FVector& NewLocalLocation)
{
	if (!SnapAnchor)
	{
		return;
	}

	SnapAnchor->SetRelativeLocation(NewLocalLocation);
}

void ABlackHoleBackpackActor::SetSnapAnchorLocalRotation(const FRotator& NewLocalRotation)
{
	if (!SnapAnchor)
	{
		return;
	}

	SnapAnchor->SetRelativeRotation(NewLocalRotation);
}

float ABlackHoleBackpackActor::GetPortalBatteryPercent() const
{
	return BackpackBattery ? BackpackBattery->GetChargePercent() : 0.0f;
}

bool ABlackHoleBackpackActor::IsPortalBatteryDepleted() const
{
	return BackpackBattery ? BackpackBattery->IsDepleted() : true;
}

void ABlackHoleBackpackActor::HandleDeploymentStateChanged(bool bNowDeployed)
{
	RefreshIntakeVolumeState();

	if (bNowDeployed)
	{
		if (bEnablePortalWhenDeployed)
		{
			SetPortalEnabled(true);
		}
	}
	else
	{
		if (bDisablePortalWhenAttached)
		{
			SetPortalEnabled(false);
		}
	}
}

void ABlackHoleBackpackActor::RefreshIntakeVolumeState()
{
	if (!InputVolume)
	{
		return;
	}

	const bool bAllowByDeployment = bIsDeployed ? bEnableIntakeWhenDeployed : bEnableIntakeWhenAttached;
	const bool bEnableIntake = bAllowByDeployment && bPortalEnabled;
	InputVolume->bEnabled = bEnableIntake;
	InputVolume->SetComponentTickEnabled(bEnableIntake);
	InputVolume->SetGenerateOverlapEvents(bEnableIntake);
	InputVolume->SetCollisionEnabled(bEnableIntake ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

FTransform ABlackHoleBackpackActor::ResolveAttachParentWorldTransform(const USceneComponent* CarrierComponent, FName SocketName) const
{
	if (!CarrierComponent)
	{
		return FTransform::Identity;
	}

	return SocketName.IsNone()
		? CarrierComponent->GetComponentTransform()
		: CarrierComponent->GetSocketTransform(SocketName, RTS_World);
}

FTransform ABlackHoleBackpackActor::BuildAttachedRootWorldTransform(
	USceneComponent* CarrierComponent,
	FName SocketName,
	const FTransform& FineTuneOffset) const
{
	const FTransform AttachParentWorldTransform = ResolveAttachParentWorldTransform(CarrierComponent, SocketName);

	FTransform FineTuneTransformNoScale = FineTuneOffset;
	FineTuneTransformNoScale.SetScale3D(FVector::OneVector);

	const FTransform TargetSnapAnchorWorldTransform = FineTuneTransformNoScale * AttachParentWorldTransform;
	const FTransform CurrentRootWorldTransform = GetActorTransform();
	const FTransform CurrentSnapAnchorWorldTransform = GetSnapAnchorWorldTransform();
	const FTransform SnapAnchorToRootTransform = CurrentSnapAnchorWorldTransform.GetRelativeTransform(CurrentRootWorldTransform);
	return SnapAnchorToRootTransform.Inverse() * TargetSnapAnchorWorldTransform;
}

FVector ABlackHoleBackpackActor::ResolveAttachedRelativeScale(const FTransform& FineTuneOffset) const
{
	const FVector FineTuneScale = FineTuneOffset.GetScale3D().GetAbs();
	return BaseItemMeshRelativeScale * FineTuneScale;
}

void ABlackHoleBackpackActor::SetMeshSimulating(bool bShouldSimulate) const
{
	if (!ItemMesh)
	{
		return;
	}

	ItemMesh->SetPhysicsLinearVelocity(FVector::ZeroVector);
	ItemMesh->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	ItemMesh->SetSimulatePhysics(bShouldSimulate && bSimulatePhysicsWhenDeployed);
	ItemMesh->SetEnableGravity(bShouldSimulate && bEnableGravityWhenDeployed);
}

void ABlackHoleBackpackActor::SetMeshCollisionForCurrentState() const
{
	if (!ItemMesh)
	{
		return;
	}

	const FName DesiredProfile = bIsDeployed ? DeployedCollisionProfileName : AttachedCollisionProfileName;
	if (!DesiredProfile.IsNone())
	{
		ItemMesh->SetCollisionProfileName(DesiredProfile);
	}
}

void ABlackHoleBackpackActor::UpdatePortalBatteryRuntimeState()
{
	if (!BackpackBattery || !BackpackBattery->IsBatteryEnabled())
	{
		return;
	}

	BackpackBattery->SetDrainEnabled(bPortalEnabled);
	BackpackBattery->SetChargeEnabled(!bPortalEnabled);
}

void ABlackHoleBackpackActor::HandlePortalBatteryDepleted()
{
	SetPortalEnabled(false);
}

void ABlackHoleBackpackActor::RefreshPortalStateVisuals()
{
	if (!PortalStateLight)
	{
		return;
	}

	if (!bEnablePortalStateLight)
	{
		PortalStateLight->SetVisibility(false, true);
		return;
	}

	if (bUseManualPortalLightSettings)
	{
		PortalStateLight->SetVisibility(bPortalEnabled, true);
		return;
	}

	PortalStateLight->SetVisibility(true, true);
	PortalStateLight->SetIntensity(bPortalEnabled ? FMath::Max(0.0f, PortalLightOnIntensity) : FMath::Max(0.0f, PortalLightOffIntensity));
	PortalStateLight->SetLightColor((bPortalEnabled ? PortalLightOnColor : PortalLightOffColor).ToFColor(true));
}
