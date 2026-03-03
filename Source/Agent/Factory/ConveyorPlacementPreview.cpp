// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/ConveyorPlacementPreview.h"
#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
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

	SetPlacementState(true);
}

void AConveyorPlacementPreview::SetPlacementState(bool bIsValid)
{
	bPlacementValid = bIsValid;

	if (DirectionArrow)
	{
		DirectionArrow->SetArrowColor(FLinearColor(bPlacementValid ? FColor::Green : FColor::Red));
	}

	if (PreviewMesh)
	{
		PreviewMesh->SetRenderCustomDepth(!bPlacementValid);
	}
}
