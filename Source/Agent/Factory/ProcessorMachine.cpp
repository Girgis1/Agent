// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/ProcessorMachine.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Factory/FactoryPlacementHelpers.h"
#include "Factory/MachineInputVolumeComponent.h"
#include "Factory/MachineOutputVolumeComponent.h"
#include "Factory/ResourceTypes.h"
#include "UObject/ConstructorHelpers.h"

AProcessorMachine::AProcessorMachine()
{
	PrimaryActorTick.bCanEverTick = true;

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

	InputVolume = CreateDefaultSubobject<UMachineInputVolumeComponent>(TEXT("InputVolume"));
	InputVolume->SetupAttachment(SceneRoot);
	InputVolume->SetBoxExtent(FVector(48.0f, 48.0f, 42.0f));
	InputVolume->SetRelativeLocation(FVector(-55.0f, 0.0f, 55.0f));

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

void AProcessorMachine::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bAutoProcess)
	{
		return;
	}

	ProcessElapsed += FMath::Max(0.0f, DeltaSeconds);
	if (ProcessElapsed < FMath::Max(KINDA_SMALL_NUMBER, ProcessInterval))
	{
		return;
	}

	if (TryProcessResources())
	{
		ProcessElapsed = 0.0f;
	}
}

bool AProcessorMachine::TryProcessResources()
{
	if (!InputVolume || !OutputVolume)
	{
		return false;
	}

	const FName ConsumedResourceId = ResolveInputResourceId();
	if (ConsumedResourceId.IsNone())
	{
		return false;
	}

	const int32 InputQuantityScaled = AgentResource::WholeUnitsToScaled(FMath::Max(1, InputQuantityUnits));
	if (!InputVolume->HasResourceQuantityScaled(ConsumedResourceId, InputQuantityScaled))
	{
		return false;
	}

	const FName ProducedResourceId = OutputResourceId.IsNone() ? ConsumedResourceId : OutputResourceId;
	const int32 OutputQuantityScaled = AgentResource::WholeUnitsToScaled(FMath::Max(1, OutputQuantityUnits));
	const int32 OutputMaxScaled = OutputVolume->GetMaxQuantityScaled();
	if (OutputMaxScaled > 0 && (OutputVolume->GetCurrentQuantityScaled() + OutputQuantityScaled) > OutputMaxScaled)
	{
		return false;
	}

	if (!InputVolume->ConsumeResourceQuantityScaled(ConsumedResourceId, InputQuantityScaled))
	{
		return false;
	}

	OutputVolume->QueueResourceScaled(ProducedResourceId, OutputQuantityScaled);
	return true;
}

FName AProcessorMachine::ResolveInputResourceId() const
{
	if (!InputResourceId.IsNone())
	{
		return InputResourceId;
	}

	if (!InputVolume)
	{
		return NAME_None;
	}

	TArray<FName> ResourceIds;
	InputVolume->InputBufferScaled.GetKeys(ResourceIds);
	ResourceIds.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});
	return ResourceIds.Num() > 0 ? ResourceIds[0] : NAME_None;
}
