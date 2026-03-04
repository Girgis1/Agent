// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/FactoryPayloadActor.h"
#include "Components/StaticMeshComponent.h"
#include "Factory/ResourceTypes.h"
#include "UObject/ConstructorHelpers.h"

AFactoryPayloadActor::AFactoryPayloadActor()
{
	PrimaryActorTick.bCanEverTick = false;

	PayloadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PayloadMesh"));
	SetRootComponent(PayloadMesh);

	PayloadMesh->SetSimulatePhysics(true);
	PayloadMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PayloadMesh->SetCollisionObjectType(ECC_PhysicsBody);
	PayloadMesh->SetCollisionResponseToAllChannels(ECR_Block);
	PayloadMesh->SetCanEverAffectNavigation(false);
	PayloadMesh->SetGenerateOverlapEvents(true);
	PayloadMesh->SetMassOverrideInKg(NAME_None, 12.0f, true);
	PayloadMesh->SetLinearDamping(0.35f);
	PayloadMesh->SetAngularDamping(2.0f);
	PayloadMesh->SetRelativeScale3D(FVector(0.35f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PayloadMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (PayloadMeshAsset.Succeeded())
	{
		PayloadMesh->SetStaticMesh(PayloadMeshAsset.Object);
	}
}

void AFactoryPayloadActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyRandomizedScaleAndMass();

	if (!ResourcePayload.HasQuantity())
	{
		ResourcePayload.ResourceId = PayloadType.IsNone() ? TEXT("Metal") : PayloadType;
		ResourcePayload.SetWholeUnits(DefaultPayloadUnits);
	}
	else
	{
		PayloadType = ResourcePayload.ResourceId;
	}
}

void AFactoryPayloadActor::ApplyRandomizedScaleAndMass()
{
	if (!PayloadMesh)
	{
		return;
	}

	const float SafeReferenceScale = FMath::Max(0.05f, PayloadReferenceScale);
	const float SafeMinScale = FMath::Max(0.05f, PayloadMinScale);
	const float SafeMaxScale = FMath::Max(SafeMinScale, PayloadMaxScale);
	const float SelectedScale = bRandomizeScaleOnSpawn
		? FMath::FRandRange(SafeMinScale, SafeMaxScale)
		: FMath::Clamp(SafeReferenceScale, SafeMinScale, SafeMaxScale);

	PayloadMesh->SetRelativeScale3D(FVector(SelectedScale));

	const float MassScaleRatio = SelectedScale / SafeReferenceScale;
	const float NewMassKg = FMath::Max(0.1f, PayloadReferenceMassKg * FMath::Pow(MassScaleRatio, 3.0f));
	PayloadMesh->SetMassOverrideInKg(NAME_None, NewMassKg, true);
}

void AFactoryPayloadActor::SetPayloadType(FName NewPayloadType)
{
	PayloadType = NewPayloadType.IsNone() ? TEXT("Metal") : NewPayloadType;
	ResourcePayload.ResourceId = PayloadType;
	if (!ResourcePayload.HasQuantity())
	{
		ResourcePayload.SetWholeUnits(DefaultPayloadUnits);
	}
}

void AFactoryPayloadActor::SetPayloadTypeAndWholeUnits(FName NewPayloadType, int32 QuantityUnits)
{
	PayloadType = NewPayloadType.IsNone() ? TEXT("Metal") : NewPayloadType;
	ResourcePayload.ResourceId = PayloadType;
	ResourcePayload.SetWholeUnits(QuantityUnits);
}

void AFactoryPayloadActor::SetPayloadResource(const FResourceAmount& NewPayloadResource)
{
	ResourcePayload = NewPayloadResource;
	PayloadType = ResourcePayload.ResourceId.IsNone() ? TEXT("Metal") : ResourcePayload.ResourceId;
	if (!ResourcePayload.HasQuantity())
	{
		ResourcePayload.ResourceId = PayloadType;
		ResourcePayload.SetWholeUnits(DefaultPayloadUnits);
	}
}
