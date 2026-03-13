// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/ConveyorPlacementPreview.h"
#include "Components/ArrowComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AConveyorPlacementPreview::AConveyorPlacementPreview()
{
	PrimaryActorTick.bCanEverTick = false;
	SetActorEnableCollision(false);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PreviewMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewMesh"));
	PreviewMesh->SetupAttachment(SceneRoot);
	PreviewMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 10.0f));
	PreviewMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 0.2f));
	PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PreviewMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	PreviewMesh->SetGenerateOverlapEvents(false);
	PreviewMesh->SetCanEverAffectNavigation(false);

	PreviewBuildable = CreateDefaultSubobject<UChildActorComponent>(TEXT("PreviewBuildable"));
	PreviewBuildable->SetupAttachment(SceneRoot);
	PreviewBuildable->SetCanEverAffectNavigation(false);

	DirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
	DirectionArrow->SetupAttachment(SceneRoot);
	DirectionArrow->SetRelativeLocation(FVector(0.0f, 0.0f, 40.0f));
	DirectionArrow->ArrowSize = 1.5f;
	DirectionArrow->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PreviewMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (PreviewMeshAsset.Succeeded())
	{
		PreviewMesh->SetStaticMesh(PreviewMeshAsset.Object);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GhostGreenMaterial(
		TEXT("/Game/Material/BuildMaterials/MI_Ghost_Green.MI_Ghost_Green"));
	if (GhostGreenMaterial.Succeeded())
	{
		ValidPlacementMaterial = GhostGreenMaterial.Object;
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GhostRedMaterial(
		TEXT("/Game/Material/BuildMaterials/MI_Ghost_Red.MI_Ghost_Red"));
	if (GhostRedMaterial.Succeeded())
	{
		InvalidPlacementMaterial = GhostRedMaterial.Object;
	}

	SetPlacementState(true);
}

void AConveyorPlacementPreview::SetPlacementState(bool bIsValid)
{
	bPlacementValid = bIsValid;
	RefreshPreviewVisuals();
}

void AConveyorPlacementPreview::SetPreviewActorClass(TSubclassOf<AActor> InPreviewActorClass)
{
	PreviewActorClass = InPreviewActorClass;

	if (PreviewBuildable)
	{
		PreviewBuildable->SetChildActorClass(PreviewActorClass.Get());
		ConfigurePreviewBuildableActor(PreviewBuildable->GetChildActor());
	}

	RefreshPreviewVisuals();
}

void AConveyorPlacementPreview::SetGhostMaterials(
	UMaterialInterface* InValidPlacementMaterial,
	UMaterialInterface* InInvalidPlacementMaterial)
{
	ValidPlacementMaterial = InValidPlacementMaterial;
	InvalidPlacementMaterial = InInvalidPlacementMaterial;
	RefreshPreviewVisuals();
}

void AConveyorPlacementPreview::RefreshPreviewVisuals()
{
	if (DirectionArrow)
	{
		DirectionArrow->SetArrowColor(FLinearColor(bPlacementValid ? FColor::Green : FColor::Red));
	}

	UMaterialInterface* GhostMaterial = bPlacementValid ? ValidPlacementMaterial.Get() : InvalidPlacementMaterial.Get();
	AActor* PreviewActor = PreviewBuildable ? PreviewBuildable->GetChildActor() : nullptr;

	if (PreviewMesh)
	{
		const bool bUseFallbackMesh = PreviewActor == nullptr;
		PreviewMesh->SetVisibility(bUseFallbackMesh, true);
		PreviewMesh->SetRenderCustomDepth(!bPlacementValid);

		if (bUseFallbackMesh && GhostMaterial)
		{
			const int32 MaterialCount = FMath::Max(1, PreviewMesh->GetNumMaterials());
			for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
			{
				PreviewMesh->SetMaterial(MaterialIndex, GhostMaterial);
			}
		}
	}

	if (!PreviewActor)
	{
		return;
	}

	ConfigurePreviewBuildableActor(PreviewActor);
	ApplyGhostMaterialToActor(PreviewActor, GhostMaterial);
}

void AConveyorPlacementPreview::ConfigurePreviewBuildableActor(AActor* PreviewActor) const
{
	if (!PreviewActor)
	{
		return;
	}

	PreviewActor->SetActorTickEnabled(false);
	PreviewActor->SetActorEnableCollision(false);

	TInlineComponentArray<UActorComponent*> ActorComponents;
	PreviewActor->GetComponents(ActorComponents);
	for (UActorComponent* ActorComponent : ActorComponents)
	{
		if (!ActorComponent)
		{
			continue;
		}

		ActorComponent->SetComponentTickEnabled(false);

		UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(ActorComponent);
		if (!PrimitiveComponent)
		{
			continue;
		}

		PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PrimitiveComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
		PrimitiveComponent->SetGenerateOverlapEvents(false);
		PrimitiveComponent->SetCanEverAffectNavigation(false);
	}
}

void AConveyorPlacementPreview::ApplyGhostMaterialToActor(AActor* PreviewActor, UMaterialInterface* GhostMaterial) const
{
	if (!PreviewActor || !GhostMaterial)
	{
		return;
	}

	TInlineComponentArray<UMeshComponent*> MeshComponents;
	PreviewActor->GetComponents(MeshComponents);
	for (UMeshComponent* MeshComponent : MeshComponents)
	{
		if (!MeshComponent)
		{
			continue;
		}

		const int32 MaterialCount = MeshComponent->GetNumMaterials();
		for (int32 MaterialIndex = 0; MaterialIndex < MaterialCount; ++MaterialIndex)
		{
			MeshComponent->SetMaterial(MaterialIndex, GhostMaterial);
		}
	}
}
