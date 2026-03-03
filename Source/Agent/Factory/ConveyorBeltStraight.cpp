// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/ConveyorBeltStraight.h"
#include "Factory/FactoryPlacementHelpers.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

AConveyorBeltStraight::AConveyorBeltStraight()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SupportCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("SupportCollision"));
	SupportCollision->SetupAttachment(SceneRoot);
	SupportCollision->SetBoxExtent(SupportBoxExtent);
	SupportCollision->SetRelativeLocation(FVector(0.0f, 0.0f, SupportBoxExtent.Z));
	SupportCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SupportCollision->SetCollisionObjectType(AgentFactory::FactoryBuildableChannel);
	SupportCollision->SetCollisionResponseToAllChannels(ECR_Block);
	SupportCollision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	SupportCollision->SetGenerateOverlapEvents(false);
	SupportCollision->SetCanEverAffectNavigation(false);

	ConveyorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ConveyorMesh"));
	ConveyorMesh->SetupAttachment(SupportCollision);
	ConveyorMesh->SetRelativeLocation(FVector::ZeroVector);
	ConveyorMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 0.2f));
	ConveyorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ConveyorMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	ConveyorMesh->SetGenerateOverlapEvents(false);
	ConveyorMesh->SetCanEverAffectNavigation(false);

	PayloadDriveVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("PayloadDriveVolume"));
	PayloadDriveVolume->SetupAttachment(SceneRoot);
	PayloadDriveVolume->SetBoxExtent(PayloadDriveExtent);
	PayloadDriveVolume->SetRelativeLocation(FVector(0.0f, 0.0f, SupportBoxExtent.Z * 2.0f + PayloadDriveExtent.Z * 0.5f));
	PayloadDriveVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PayloadDriveVolume->SetCollisionObjectType(ECC_WorldDynamic);
	PayloadDriveVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	PayloadDriveVolume->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	PayloadDriveVolume->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	PayloadDriveVolume->SetGenerateOverlapEvents(true);
	PayloadDriveVolume->SetCanEverAffectNavigation(false);

	DirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
	DirectionArrow->SetupAttachment(SceneRoot);
	DirectionArrow->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
	DirectionArrow->SetArrowColor(FLinearColor(FColor::Yellow));
	DirectionArrow->ArrowSize = 1.2f;
	DirectionArrow->SetCanEverAffectNavigation(false);

	PayloadDriveVolume->OnComponentBeginOverlap.AddDynamic(this, &AConveyorBeltStraight::OnPayloadBeginOverlap);
	PayloadDriveVolume->OnComponentEndOverlap.AddDynamic(this, &AConveyorBeltStraight::OnPayloadEndOverlap);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConveyorMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (ConveyorMeshAsset.Succeeded())
	{
		ConveyorMesh->SetStaticMesh(ConveyorMeshAsset.Object);
	}
}

void AConveyorBeltStraight::BeginPlay()
{
	Super::BeginPlay();

	if (SupportCollision)
	{
		SupportCollision->SetBoxExtent(SupportBoxExtent);
		SupportCollision->SetRelativeLocation(FVector(0.0f, 0.0f, SupportBoxExtent.Z));
	}

	if (PayloadDriveVolume)
	{
		PayloadDriveVolume->SetBoxExtent(PayloadDriveExtent);
		PayloadDriveVolume->SetRelativeLocation(FVector(0.0f, 0.0f, SupportBoxExtent.Z * 2.0f + PayloadDriveExtent.Z * 0.5f));
	}

	UpdateTickState();
}

void AConveyorBeltStraight::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	TArray<TWeakObjectPtr<UPrimitiveComponent>> PayloadsToRemove;
	for (const TWeakObjectPtr<UPrimitiveComponent>& PayloadEntry : ActivePayloads)
	{
		UPrimitiveComponent* PrimitiveComponent = PayloadEntry.Get();
		if (!IsPayloadValid(PrimitiveComponent))
		{
			PayloadsToRemove.Add(PayloadEntry);
			continue;
		}

		ApplyBeltDrive(PrimitiveComponent, DeltaSeconds);
	}

	for (const TWeakObjectPtr<UPrimitiveComponent>& PayloadToRemove : PayloadsToRemove)
	{
		ActivePayloads.Remove(PayloadToRemove);
	}

	UpdateTickState();
}

void AConveyorBeltStraight::OnPayloadBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	(void)OverlappedComponent;
	(void)OtherActor;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;

	if (IsPayloadValid(OtherComp))
	{
		ActivePayloads.Add(OtherComp);
		UpdateTickState();
	}
}

void AConveyorBeltStraight::OnPayloadEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	(void)OverlappedComponent;
	(void)OtherActor;
	(void)OtherBodyIndex;

	if (OtherComp)
	{
		ActivePayloads.Remove(OtherComp);
		UpdateTickState();
	}
}

void AConveyorBeltStraight::UpdateTickState()
{
	SetActorTickEnabled(ActivePayloads.Num() > 0);
}

bool AConveyorBeltStraight::IsPayloadValid(const UPrimitiveComponent* PrimitiveComponent) const
{
	return IsValid(PrimitiveComponent)
		&& PrimitiveComponent->IsSimulatingPhysics()
		&& PrimitiveComponent->IsRegistered();
}

void AConveyorBeltStraight::ApplyBeltDrive(UPrimitiveComponent* PrimitiveComponent, float DeltaSeconds) const
{
	if (!PrimitiveComponent || DeltaSeconds <= 0.0f)
	{
		return;
	}

	const FVector DriveDirection = DirectionArrow ? DirectionArrow->GetForwardVector() : GetActorForwardVector();
	const FVector CurrentVelocity = PrimitiveComponent->GetPhysicsLinearVelocity();
	const float CurrentAlongBeltSpeed = FVector::DotProduct(CurrentVelocity, DriveDirection);
	const float TargetSpeed = FMath::Max(0.0f, BeltSpeed);
	const float MissingSpeed = TargetSpeed - CurrentAlongBeltSpeed;

	if (MissingSpeed <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float VelocityStep = FMath::Min(MissingSpeed, FMath::Max(0.0f, BeltAcceleration) * DeltaSeconds);
	if (VelocityStep <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	PrimitiveComponent->AddImpulse(DriveDirection * VelocityStep, NAME_None, true);
}
