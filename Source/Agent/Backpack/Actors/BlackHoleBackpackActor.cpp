// Copyright Epic Games, Inc. All Rights Reserved.

#include "Backpack/Actors/BlackHoleBackpackActor.h"
#include "Backpack/Components/BlackHoleBackpackLinkComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Machine/InputVolume.h"
#include "Machine/MachineComponent.h"
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

	MachineComponent = CreateDefaultSubobject<UMachineComponent>(TEXT("MachineComponent"));
	MachineComponent->bRecipeAny = true;
	MachineComponent->Recipes.Reset();
	MachineComponent->bOutputPureMaterials = false;

	LinkComponent = CreateDefaultSubobject<UBlackHoleBackpackLinkComponent>(TEXT("LinkComponent"));

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

	SetDeployedState(bStartDeployed);

	if (MachineComponent)
	{
		MachineComponent->SetInputVolume(InputVolume);
	}

	if (LinkComponent)
	{
		LinkComponent->SetSourceMachineComponent(MachineComponent);
		LinkComponent->SetLinkId(LinkId);
	}

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

void ABlackHoleBackpackActor::HandleDeploymentStateChanged(bool bNowDeployed)
{
	if (InputVolume)
	{
		const bool bEnableIntake = bNowDeployed ? bEnableIntakeWhenDeployed : bEnableIntakeWhenAttached;
		InputVolume->bEnabled = bEnableIntake;
		InputVolume->SetComponentTickEnabled(bEnableIntake);
		InputVolume->SetGenerateOverlapEvents(bEnableIntake);
		InputVolume->SetCollisionEnabled(bEnableIntake ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
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
