// Copyright Epic Games, Inc. All Rights Reserved.

#include "Material/MaterialActor.h"
#include "Components/StaticMeshComponent.h"
#include "Material/MaterialComponent.h"
#include "Materials/MaterialInterface.h"
#include "Objects/Components/ObjectFractureComponent.h"
#include "Objects/Components/ObjectHealthComponent.h"
#include "Objects/Types/ObjectHealthTypes.h"
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
	ObjectHealth = CreateDefaultSubobject<UObjectHealthComponent>(TEXT("ObjectHealth"));
	ObjectHealth->bHealthEnabled = false;
	ObjectHealth->InitializationMode = EObjectHealthInitializationMode::FromCurrentMass;
	ObjectHealth->bAutoInitializeOnBeginPlay = true;
	ObjectHealth->bDeferMassInitializationToNextTick = true;
	ObjectHealth->bDestroyOwnerWhenDepleted = true;

	ObjectFracture = CreateDefaultSubobject<UObjectFractureComponent>(TEXT("ObjectFracture"));

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
	ApplyRandomizedMaterials();
	HandlePostItemMeshConfiguration();

	if (MaterialData)
	{
		MaterialData->ResetGeneratedContents();
		if (UPrimitiveComponent* RuntimePrimitive = ResolveMaterialRuntimePrimitive())
		{
			MaterialData->InitializeRuntimeResourceState(RuntimePrimitive);
		}
	}
}

UPrimitiveComponent* AMaterialActor::ResolveMaterialRuntimePrimitive() const
{
	return ItemMesh;
}

void AMaterialActor::HandlePostItemMeshConfiguration()
{
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

void AMaterialActor::ApplyRandomizedMaterials()
{
	if (!ItemMesh || !bRandomizeMaterialOnSpawn || RandomizedMaterialSlots.Num() == 0)
	{
		return;
	}

	const int32 MaterialCount = ItemMesh->GetNumMaterials();
	if (MaterialCount <= 0)
	{
		return;
	}

	for (const FMaterialActorMaterialSlot& MaterialSlot : RandomizedMaterialSlots)
	{
		const int32 SlotIndex = MaterialSlot.MaterialSlotIndex;
		if (SlotIndex < 0 || SlotIndex >= MaterialCount)
		{
			continue;
		}

		TArray<UMaterialInterface*> ValidMaterialVariants;
		ValidMaterialVariants.Reserve(MaterialSlot.MaterialInstances.Num());
		for (UMaterialInterface* MaterialVariant : MaterialSlot.MaterialInstances)
		{
			if (IsValid(MaterialVariant))
			{
				ValidMaterialVariants.Add(MaterialVariant);
			}
		}

		if (ValidMaterialVariants.Num() == 0)
		{
			continue;
		}

		const int32 MaterialIndex = FMath::RandRange(0, ValidMaterialVariants.Num() - 1);
		ItemMesh->SetMaterial(SlotIndex, ValidMaterialVariants[MaterialIndex]);
	}
}

