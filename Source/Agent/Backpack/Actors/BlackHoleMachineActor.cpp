// Copyright Epic Games, Inc. All Rights Reserved.

#include "Backpack/Actors/BlackHoleMachineActor.h"
#include "Backpack/Components/BlackHoleOutputSinkComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Factory/FactoryPlacementHelpers.h"
#include "Machine/OutputVolume.h"
#include "UObject/ConstructorHelpers.h"

ABlackHoleMachineActor::ABlackHoleMachineActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SupportCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("SupportCollision"));
	SupportCollision->SetupAttachment(SceneRoot);
	SupportCollision->SetBoxExtent(FVector(62.0f, 62.0f, 64.0f));
	SupportCollision->SetRelativeLocation(FVector(0.0f, 0.0f, 64.0f));
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

	OutputVolume = CreateDefaultSubobject<UOutputVolume>(TEXT("OutputVolume"));
	OutputVolume->SetupAttachment(SceneRoot);
	OutputVolume->SetBoxExtent(FVector(26.0f, 26.0f, 24.0f));
	OutputVolume->SetRelativeLocation(FVector(84.0f, 0.0f, 52.0f));

	OutputArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("OutputArrow"));
	OutputArrow->SetupAttachment(SceneRoot);
	OutputArrow->SetRelativeLocation(FVector(84.0f, 0.0f, 52.0f));
	OutputArrow->ArrowSize = 1.25f;
	OutputArrow->SetCanEverAffectNavigation(false);

	OutputSink = CreateDefaultSubobject<UBlackHoleOutputSinkComponent>(TEXT("OutputSink"));
	OutputSink->SetOutputVolume(OutputVolume);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> MachineMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (MachineMeshAsset.Succeeded())
	{
		MachineMesh->SetStaticMesh(MachineMeshAsset.Object);
	}
}

void ABlackHoleMachineActor::BeginPlay()
{
	Super::BeginPlay();

	if (OutputVolume)
	{
		OutputVolume->SetOutputArrow(OutputArrow);
	}

	if (OutputSink)
	{
		OutputSink->LinkId = LinkId;
		OutputSink->SetOutputVolume(OutputVolume);
		OutputSink->RefreshSinkRegistration();
	}
}
