// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/StorageBin.h"
#include "Factory/FactoryPayloadActor.h"
#include "Factory/FactoryPlacementHelpers.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
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

	IntakeVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("IntakeVolume"));
	IntakeVolume->SetupAttachment(SceneRoot);
	IntakeVolume->SetBoxExtent(FVector(46.0f, 40.0f, 40.0f));
	IntakeVolume->SetRelativeLocation(FVector(-50.0f, 0.0f, 50.0f));
	IntakeVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	IntakeVolume->SetCollisionObjectType(ECC_WorldDynamic);
	IntakeVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	IntakeVolume->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	IntakeVolume->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	IntakeVolume->SetGenerateOverlapEvents(true);
	IntakeVolume->SetCanEverAffectNavigation(false);

	FlowArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("FlowArrow"));
	FlowArrow->SetupAttachment(SceneRoot);
	FlowArrow->SetRelativeLocation(FVector(0.0f, 0.0f, 130.0f));
	FlowArrow->SetArrowColor(FLinearColor(FColor::Cyan));
	FlowArrow->ArrowSize = 1.25f;
	FlowArrow->SetCanEverAffectNavigation(false);

	IntakeVolume->OnComponentBeginOverlap.AddDynamic(this, &AStorageBin::OnIntakeBeginOverlap);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BinMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (BinMeshAsset.Succeeded())
	{
		BinMesh->SetStaticMesh(BinMeshAsset.Object);
	}
}

int32 AStorageBin::GetStoredItemCount(FName PayloadType) const
{
	if (PayloadType.IsNone())
	{
		return TotalStoredItemCount;
	}

	if (const int32* FoundCount = StoredItemCounts.Find(PayloadType))
	{
		return *FoundCount;
	}

	return 0;
}

void AStorageBin::OnIntakeBeginOverlap(
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

	AFactoryPayloadActor* PayloadActor = Cast<AFactoryPayloadActor>(OtherActor);
	if (!PayloadActor)
	{
		return;
	}

	const FName PayloadType = PayloadActor->GetPayloadType().IsNone() ? TEXT("RawOre") : PayloadActor->GetPayloadType();
	if (!AcceptedPayloadType.IsNone() && PayloadType != AcceptedPayloadType)
	{
		return;
	}

	StoredItemCounts.FindOrAdd(PayloadType) += 1;
	++TotalStoredItemCount;

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			static_cast<uint64>(GetUniqueID()) + 19000ULL,
			1.25f,
			FColor::Cyan,
			FString::Printf(TEXT("Bin Stored %s: %d (Total %d)"), *PayloadType.ToString(), GetStoredItemCount(PayloadType), TotalStoredItemCount));
	}

	PayloadActor->Destroy();
}
