// Copyright Epic Games, Inc. All Rights Reserved.

#include "Machine/AtomiserOutputMachine.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Factory/FactoryPlacementHelpers.h"
#include "Machine/AtomiserOutputVolume.h"
#include "UObject/ConstructorHelpers.h"

AAtomiserOutputMachine::AAtomiserOutputMachine()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SupportCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("SupportCollision"));
	SupportCollision->SetupAttachment(SceneRoot);
	SupportCollision->SetBoxExtent(FVector(50.0f, 50.0f, 55.0f));
	SupportCollision->SetRelativeLocation(FVector(0.0f, 0.0f, 55.0f));
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

	OutputVolume = CreateDefaultSubobject<UAtomiserOutputVolume>(TEXT("OutputVolume"));
	OutputVolume->SetupAttachment(SceneRoot);
	OutputVolume->SetBoxExtent(FVector(24.0f, 24.0f, 24.0f));
	OutputVolume->SetRelativeLocation(FVector(75.0f, 0.0f, 55.0f));

	OutputArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("OutputArrow"));
	OutputArrow->SetupAttachment(SceneRoot);
	OutputArrow->SetRelativeLocation(FVector(75.0f, 0.0f, 55.0f));
	OutputArrow->ArrowSize = 1.35f;
	OutputArrow->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> MachineMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (MachineMeshAsset.Succeeded())
	{
		MachineMesh->SetStaticMesh(MachineMeshAsset.Object);
	}
}

void AAtomiserOutputMachine::BeginPlay()
{
	Super::BeginPlay();

	if (OutputVolume)
	{
		OutputVolume->SetOutputArrow(OutputArrow);
	}
}
