// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/ResourceSpawnerMachine.h"
#include "Factory/FactoryPayloadActor.h"
#include "Factory/FactoryPlacementHelpers.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

AResourceSpawnerMachine::AResourceSpawnerMachine()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SupportCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("SupportCollision"));
	SupportCollision->SetupAttachment(SceneRoot);
	SupportCollision->SetBoxExtent(FVector(50.0f, 50.0f, 50.0f));
	SupportCollision->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
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
	MachineMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
	MachineMesh->SetCanEverAffectNavigation(false);

	OutputArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("OutputArrow"));
	OutputArrow->SetupAttachment(SceneRoot);
	OutputArrow->SetRelativeLocation(FVector(65.0f, 0.0f, 55.0f));
	OutputArrow->SetArrowColor(FLinearColor(FColor::Orange));
	OutputArrow->ArrowSize = 1.35f;
	OutputArrow->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> MachineMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (MachineMeshAsset.Succeeded())
	{
		MachineMesh->SetStaticMesh(MachineMeshAsset.Object);
	}
}

void AResourceSpawnerMachine::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bAutoSpawnEnabled)
	{
		return;
	}

	SpawnElapsed += FMath::Max(0.0f, DeltaSeconds);
	if (SpawnElapsed < FMath::Max(KINDA_SMALL_NUMBER, SpawnInterval))
	{
		return;
	}

	if (!CanSpawnPayload())
	{
		return;
	}

	SpawnElapsed = 0.0f;
	SpawnPayload();
}

bool AResourceSpawnerMachine::CanSpawnPayload() const
{
	const UWorld* World = GetWorld();
	if (!World || !OutputArrow)
	{
		return false;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ResourceSpawnerOutputClearance), false, this);

	return !World->OverlapAnyTestByObjectType(
		OutputArrow->GetComponentLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(FMath::Max(1.0f, OutputClearanceRadius)),
		QueryParams);
}

void AResourceSpawnerMachine::SpawnPayload()
{
	UWorld* World = GetWorld();
	if (!World || !OutputArrow)
	{
		return;
	}

	UClass* PayloadClass = PayloadActorClass.Get();
	if (!PayloadClass)
	{
		PayloadClass = AFactoryPayloadActor::StaticClass();
	}

	FActorSpawnParameters SpawnParameters{};
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AFactoryPayloadActor* PayloadActor = World->SpawnActor<AFactoryPayloadActor>(
		PayloadClass,
		OutputArrow->GetComponentLocation(),
		OutputArrow->GetComponentRotation(),
		SpawnParameters);

	if (!PayloadActor)
	{
		return;
	}

	PayloadActor->SetPayloadType(SpawnedPayloadType);

	if (UPrimitiveComponent* PayloadPrimitive = Cast<UPrimitiveComponent>(PayloadActor->GetRootComponent()))
	{
		PayloadPrimitive->AddImpulse(OutputArrow->GetForwardVector() * FMath::Max(0.0f, SpawnImpulse), NAME_None, true);
	}
}
