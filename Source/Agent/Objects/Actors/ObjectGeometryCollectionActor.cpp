// Copyright Epic Games, Inc. All Rights Reserved.

#include "Objects/Actors/ObjectGeometryCollectionActor.h"
#include "Engine/World.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "Material/MaterialComponent.h"
#include "Objects/Components/ObjectFractureComponent.h"
#include "Objects/Components/ObjectHealthComponent.h"
#include "Objects/Types/ObjectHealthTypes.h"
#include "TimerManager.h"

AObjectGeometryCollectionActor::AObjectGeometryCollectionActor()
{
	PrimaryActorTick.bCanEverTick = false;

	GeometryCollectionComponent = CreateDefaultSubobject<UGeometryCollectionComponent>(TEXT("GeometryCollection"));
	SetRootComponent(GeometryCollectionComponent);
	GeometryCollectionComponent->SetSimulatePhysics(true);
	GeometryCollectionComponent->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GeometryCollectionComponent->SetCollisionObjectType(ECC_PhysicsBody);
	GeometryCollectionComponent->SetCollisionResponseToAllChannels(ECR_Block);
	GeometryCollectionComponent->SetGenerateOverlapEvents(true);
	GeometryCollectionComponent->SetCanEverAffectNavigation(false);
	GeometryCollectionComponent->SetNotifyBreaks(true);

	MaterialData = CreateDefaultSubobject<UMaterialComponent>(TEXT("MaterialData"));
	ObjectHealth = CreateDefaultSubobject<UObjectHealthComponent>(TEXT("ObjectHealth"));
	ObjectHealth->bHealthEnabled = false;
	ObjectHealth->InitializationMode = EObjectHealthInitializationMode::FromCurrentMass;
	ObjectHealth->bAutoInitializeOnBeginPlay = true;
	ObjectHealth->bDeferMassInitializationToNextTick = true;
	ObjectHealth->bDestroyOwnerWhenDepleted = true;

	ObjectFracture = CreateDefaultSubobject<UObjectFractureComponent>(TEXT("ObjectFracture"));
}

void AObjectGeometryCollectionActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	ApplyConfiguredGeometryCollection();
	ApplyRuntimeCollectionSettings();
}

void AObjectGeometryCollectionActor::SetGeometryCollectionAsset(UGeometryCollection* NewGeometryCollectionAsset)
{
	GeometryCollectionAsset = NewGeometryCollectionAsset;
	ApplyConfiguredGeometryCollection();
}

void AObjectGeometryCollectionActor::BeginPlay()
{
	Super::BeginPlay();

	ApplyConfiguredGeometryCollection();
	ApplyRuntimeCollectionSettings();

	if (MaterialData && GeometryCollectionComponent)
	{
		MaterialData->ResetGeneratedContents();
		MaterialData->InitializeRuntimeResourceState(GeometryCollectionComponent);
	}

	if (bCrumbleImmediatelyOnBeginPlay && GeometryCollectionComponent && GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick(this, &AObjectGeometryCollectionActor::HandleDeferredInitialCrumble);
	}
}

void AObjectGeometryCollectionActor::ApplyConfiguredGeometryCollection()
{
	if (!GeometryCollectionComponent || !GeometryCollectionAsset)
	{
		return;
	}

	GeometryCollectionComponent->SetRestCollection(GeometryCollectionAsset, true);
}

void AObjectGeometryCollectionActor::ApplyRuntimeCollectionSettings()
{
	if (!GeometryCollectionComponent)
	{
		return;
	}

	GeometryCollectionComponent->bAllowRemovalOnSleep = !bDisableRemovalOnSleep;
	GeometryCollectionComponent->bAllowRemovalOnBreak = !bDisableRemovalOnBreak;
}

void AObjectGeometryCollectionActor::HandleDeferredInitialCrumble()
{
	if (!GeometryCollectionComponent || !GeometryCollectionAsset)
	{
		return;
	}

	GeometryCollectionComponent->CrumbleActiveClusters();
}
