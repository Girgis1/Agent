// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/ShredderMachine.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Factory/FactoryPlacementHelpers.h"
#include "Factory/MachineOutputVolumeComponent.h"
#include "Factory/ShredderVolumeComponent.h"
#include "UObject/ConstructorHelpers.h"

AShredderMachine::AShredderMachine()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SupportCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("SupportCollision"));
	SupportCollision->SetupAttachment(SceneRoot);
	SupportCollision->SetBoxExtent(FVector(60.0f, 60.0f, 60.0f));
	SupportCollision->SetRelativeLocation(FVector(0.0f, 0.0f, 60.0f));
	SupportCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SupportCollision->SetCollisionObjectType(AgentFactory::FactoryBuildableChannel);
	SupportCollision->SetCollisionResponseToAllChannels(ECR_Block);
	SupportCollision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	SupportCollision->SetGenerateOverlapEvents(false);
	SupportCollision->SetCanEverAffectNavigation(false);

	MachineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MachineMesh"));
	MachineMesh->SetupAttachment(SupportCollision);
	MachineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MachineMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	MachineMesh->SetGenerateOverlapEvents(false);
	MachineMesh->SetCanEverAffectNavigation(false);

	IntakeVolume = CreateDefaultSubobject<UShredderVolumeComponent>(TEXT("IntakeVolume"));
	IntakeVolume->SetupAttachment(SceneRoot);
	IntakeVolume->SetBoxExtent(FVector(48.0f, 48.0f, 42.0f));
	IntakeVolume->SetRelativeLocation(FVector(-55.0f, 0.0f, 55.0f));

	OutputVolume = CreateDefaultSubobject<UMachineOutputVolumeComponent>(TEXT("OutputVolume"));
	OutputVolume->SetupAttachment(SceneRoot);
	OutputVolume->SetBoxExtent(FVector(24.0f, 24.0f, 24.0f));
	OutputVolume->SetRelativeLocation(FVector(75.0f, 0.0f, 55.0f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> MachineMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (MachineMeshAsset.Succeeded())
	{
		MachineMesh->SetStaticMesh(MachineMeshAsset.Object);
	}
}
