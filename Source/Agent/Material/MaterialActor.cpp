// Copyright Epic Games, Inc. All Rights Reserved.

#include "Material/MaterialActor.h"
#include "Components/StaticMeshComponent.h"
#include "Material/MaterialComponent.h"
#include "UObject/ConstructorHelpers.h"

AMaterialActor::AMaterialActor()
{
	PrimaryActorTick.bCanEverTick = false;

	ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
	SetRootComponent(ItemMesh);
	ItemMesh->SetSimulatePhysics(true);
	ItemMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ItemMesh->SetCollisionObjectType(ECC_PhysicsBody);
	ItemMesh->SetCollisionResponseToAllChannels(ECR_Block);
	ItemMesh->SetGenerateOverlapEvents(true);
	ItemMesh->SetCanEverAffectNavigation(false);
	ItemMesh->SetLinearDamping(0.25f);
	ItemMesh->SetAngularDamping(1.2f);

	MaterialData = CreateDefaultSubobject<UMaterialComponent>(TEXT("MaterialData"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (DefaultMesh.Succeeded())
	{
		ItemMesh->SetStaticMesh(DefaultMesh.Object);
	}
}

void AMaterialActor::BeginPlay()
{
	Super::BeginPlay();

	ApplyMeshVariant();
	ApplyRandomizedScale();

	if (MaterialData && ItemMesh)
	{
		MaterialData->ResetGeneratedContents();
		MaterialData->InitializeRuntimeResourceState(ItemMesh);
	}
}

void AMaterialActor::ApplyMeshVariant()
{
	if (!ItemMesh || !bRandomizeMeshOnSpawn || MeshVariants.Num() == 0)
	{
		return;
	}

	TArray<UStaticMesh*> ValidVariants;
	ValidVariants.Reserve(MeshVariants.Num());
	for (UStaticMesh* MeshVariant : MeshVariants)
	{
		if (IsValid(MeshVariant))
		{
			ValidVariants.Add(MeshVariant);
		}
	}

	if (ValidVariants.Num() == 0)
	{
		return;
	}

	const int32 VariantIndex = FMath::RandRange(0, ValidVariants.Num() - 1);
	ItemMesh->SetStaticMesh(ValidVariants[VariantIndex]);
}

void AMaterialActor::ApplyRandomizedScale()
{
	if (!ItemMesh || !bRandomizeScaleOnSpawn)
	{
		return;
	}

	const float SafeMinScale = FMath::Max(0.05f, ItemMinScale);
	const float SafeMaxScale = FMath::Max(SafeMinScale, ItemMaxScale);
	const float SelectedScale = FMath::FRandRange(SafeMinScale, SafeMaxScale);

	ItemMesh->SetWorldScale3D(FVector(SelectedScale));
}

