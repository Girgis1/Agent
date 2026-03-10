// Copyright Epic Games, Inc. All Rights Reserved.

#include "Backpack/Actors/BlackHoleMachineActor.h"
#include "Backpack/Components/BlackHoleOutputSinkComponent.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Factory/StorageVolumeComponent.h"
#include "Factory/FactoryPlacementHelpers.h"
#include "GameFramework/Pawn.h"
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
	OutputVolume->bRouteOutputToAttachedStorage = true;
	OutputVolume->bAllowWorldSpawnWhenStorageUnavailable = false;
	OutputVolume->bAutoResolveAttachedStorage = false;

	OutputArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("OutputArrow"));
	OutputArrow->SetupAttachment(SceneRoot);
	OutputArrow->SetRelativeLocation(FVector(84.0f, 0.0f, 52.0f));
	OutputArrow->ArrowSize = 1.25f;
	OutputArrow->SetCanEverAffectNavigation(false);

	OutputSink = CreateDefaultSubobject<UBlackHoleOutputSinkComponent>(TEXT("OutputSink"));
	OutputSink->SetOutputVolume(OutputVolume);

	ActivationRangeVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("ActivationRangeVolume"));
	ActivationRangeVolume->SetupAttachment(SceneRoot);
	ActivationRangeVolume->SetBoxExtent(FVector(240.0f, 240.0f, 180.0f));
	ActivationRangeVolume->SetRelativeLocation(FVector::ZeroVector);
	ActivationRangeVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ActivationRangeVolume->SetCollisionObjectType(ECC_WorldDynamic);
	ActivationRangeVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	ActivationRangeVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	ActivationRangeVolume->SetGenerateOverlapEvents(true);
	ActivationRangeVolume->SetCanEverAffectNavigation(false);

	StorageVolume = CreateDefaultSubobject<UStorageVolumeComponent>(TEXT("StorageVolume"));
	StorageVolume->SetupAttachment(SceneRoot);
	StorageVolume->SetBoxExtent(FVector(42.0f, 42.0f, 42.0f));
	StorageVolume->SetRelativeLocation(FVector(0.0f, 0.0f, 56.0f));
	StorageVolume->MaxQuantityUnits = 50;
	StorageVolume->DebugName = TEXT("BlackHoleStorage");

	OutputVolume->SetAttachedStorageVolume(StorageVolume);

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
		OutputVolume->SetAttachedStorageVolume(StorageVolume);
	}

	if (OutputSink)
	{
		OutputSink->LinkId = LinkId;
		OutputSink->SetOutputVolume(OutputVolume);
		OutputSink->RefreshSinkRegistration();
	}

	if (ActivationRangeVolume)
	{
		ActivationRangeVolume->OnComponentBeginOverlap.AddDynamic(this, &ABlackHoleMachineActor::OnActivationRangeBeginOverlap);
		ActivationRangeVolume->OnComponentEndOverlap.AddDynamic(this, &ABlackHoleMachineActor::OnActivationRangeEndOverlap);

		TArray<AActor*> OverlappingActors;
		ActivationRangeVolume->GetOverlappingActors(OverlappingActors, APawn::StaticClass());
		for (AActor* OverlappingActor : OverlappingActors)
		{
			TryAddActivationPawn(OverlappingActor);
		}
	}

	RefreshProximityActivationState();
}

void ABlackHoleMachineActor::OnActivationRangeBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	(void)OverlappedComponent;
	(void)OtherComp;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;

	if (TryAddActivationPawn(OtherActor))
	{
		RefreshProximityActivationState();
	}
}

void ABlackHoleMachineActor::OnActivationRangeEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	(void)OverlappedComponent;
	(void)OtherComp;
	(void)OtherBodyIndex;

	RemoveActivationPawn(OtherActor);
	RefreshProximityActivationState();
}

bool ABlackHoleMachineActor::TryAddActivationPawn(AActor* OtherActor)
{
	APawn* CandidatePawn = Cast<APawn>(OtherActor);
	if (!CandidatePawn)
	{
		return false;
	}

	if (bRequirePlayerControlledPawn && !CandidatePawn->IsPlayerControlled())
	{
		return false;
	}

	ActivationPawnsInRange.Add(CandidatePawn);
	return true;
}

void ABlackHoleMachineActor::RemoveActivationPawn(AActor* OtherActor)
{
	if (APawn* ExistingPawn = Cast<APawn>(OtherActor))
	{
		ActivationPawnsInRange.Remove(ExistingPawn);
	}
}

void ABlackHoleMachineActor::RefreshProximityActivationState()
{
	if (!OutputSink || !bUsePawnProximityActivation)
	{
		return;
	}

	for (auto It = ActivationPawnsInRange.CreateIterator(); It; ++It)
	{
		APawn* TrackedPawn = It->Get();
		if (!TrackedPawn || !IsValid(TrackedPawn))
		{
			It.RemoveCurrent();
			continue;
		}

		if (bRequirePlayerControlledPawn && !TrackedPawn->IsPlayerControlled())
		{
			It.RemoveCurrent();
		}
	}

	const bool bShouldActivateMachine = ActivationPawnsInRange.Num() > 0;
	OutputSink->SetMachineActivated(bShouldActivateMachine);
}
