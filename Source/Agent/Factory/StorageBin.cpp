// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/StorageBin.h"
#include "Factory/FactoryPlacementHelpers.h"
#include "Factory/StorageVolumeComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AStorageBin::AStorageBin()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SupportCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("SupportCollision"));
	SupportCollision->SetupAttachment(SceneRoot);
	SupportCollision->SetBoxExtent(FVector(50.0f, 50.0f, 60.0f));
	SupportCollision->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));
	SupportCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SupportCollision->SetCollisionObjectType(AgentFactory::FactoryBuildableChannel);
	SupportCollision->SetCollisionResponseToAllChannels(ECR_Block);
	SupportCollision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	SupportCollision->SetGenerateOverlapEvents(false);
	SupportCollision->SetCanEverAffectNavigation(false);

	BinMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BinMesh"));
	BinMesh->SetupAttachment(SupportCollision);
	BinMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BinMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	BinMesh->SetGenerateOverlapEvents(false);
	BinMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.2f));
	BinMesh->SetCanEverAffectNavigation(false);

	IntakeVolume = CreateDefaultSubobject<UStorageVolumeComponent>(TEXT("IntakeVolume"));
	IntakeVolume->SetupAttachment(SceneRoot);
	IntakeVolume->SetBoxExtent(FVector(46.0f, 40.0f, 40.0f));
	IntakeVolume->SetRelativeLocation(FVector(-50.0f, 0.0f, 50.0f));

	FlowArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("FlowArrow"));
	FlowArrow->SetupAttachment(SceneRoot);
	FlowArrow->SetRelativeLocation(FVector(0.0f, 0.0f, 130.0f));
	FlowArrow->SetArrowColor(FLinearColor(FColor::Cyan));
	FlowArrow->ArrowSize = 1.25f;
	FlowArrow->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BinMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (BinMeshAsset.Succeeded())
	{
		BinMesh->SetStaticMesh(BinMeshAsset.Object);
	}
}

int32 AStorageBin::GetStoredItemCount(FName PayloadType) const
{
	if (!IntakeVolume)
	{
		return 0;
	}

	if (PayloadType.IsNone())
	{
		return IntakeVolume->GetTotalStoredItemCount();
	}

	return IntakeVolume->GetStoredItemCount(PayloadType);
}

float AStorageBin::GetStoredUnits(FName PayloadType) const
{
	return IntakeVolume ? IntakeVolume->GetStoredUnits(PayloadType) : 0.0f;
}
