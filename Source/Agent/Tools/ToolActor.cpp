// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/ToolActor.h"

#include "Dirty/DirtBrushComponent.h"
#include "Tools/ToolDefinition.h"
#include "Tools/ToolWorldComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"

namespace
{
	void AssignComponentReference(FComponentReference& InOutReference, UActorComponent* InComponent)
	{
		InOutReference.OtherActor = nullptr;
		InOutReference.ComponentProperty = NAME_None;
		InOutReference.PathToComponent.Reset();
		InOutReference.OverrideComponent = InComponent;
	}
}

AToolActor::AToolActor()
{
	PrimaryActorTick.bCanEverTick = false;

	ToolRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ToolRoot"));
	SetRootComponent(ToolRoot);

	ToolMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ToolMesh"));
	ToolMesh->SetupAttachment(ToolRoot);
	ToolMesh->SetMobility(EComponentMobility::Movable);
	ToolMesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	ToolMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ToolMesh->SetSimulatePhysics(true);
	ToolMesh->SetEnableGravity(true);
	ToolMesh->SetCanEverAffectNavigation(false);
	ToolMesh->BodyInstance.bUseCCD = true;

	GripPoint = CreateDefaultSubobject<USceneComponent>(TEXT("GripPoint"));
	GripPoint->SetupAttachment(ToolMesh);
	GripPoint->SetRelativeLocation(FVector(-15.0f, 0.0f, 65.0f));

	HeadPoint = CreateDefaultSubobject<USceneComponent>(TEXT("HeadPoint"));
	HeadPoint->SetupAttachment(ToolMesh);
	HeadPoint->SetRelativeLocation(FVector(130.0f, 0.0f, 6.0f));

	DirtBrushComponent = CreateDefaultSubobject<UDirtBrushComponent>(TEXT("DirtBrushComponent"));
	DirtBrushComponent->BrushMode = EDirtBrushMode::Clean;
	DirtBrushComponent->ApplicationType = EDirtBrushApplicationType::Trail;

	ToolWorldComponent = CreateDefaultSubobject<UToolWorldComponent>(TEXT("ToolWorldComponent"));
	ToolWorldComponent->bLogSetupFailures = true;

	SyncToolComponentReferences();
}

void AToolActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	SyncToolComponentReferences();
}

void AToolActor::RefreshToolComponentReferences()
{
	SyncToolComponentReferences();
}

UActorComponent* AToolActor::ResolvePrimaryBehaviorComponent() const
{
	return nullptr;
}

UToolDefinition* AToolActor::ResolveToolDefinitionForWorldComponent() const
{
	return ToolWorldComponent ? ToolWorldComponent->ToolDefinition : nullptr;
}

void AToolActor::SyncToolComponentReferences()
{
	if (!ToolWorldComponent)
	{
		return;
	}

	AssignComponentReference(ToolWorldComponent->PhysicsPrimitiveReference, ToolMesh);
	AssignComponentReference(ToolWorldComponent->GripPointReference, GripPoint);
	AssignComponentReference(ToolWorldComponent->HeadPointReference, HeadPoint);
	AssignComponentReference(ToolWorldComponent->DirtBrushReference, DirtBrushComponent);
	AssignComponentReference(ToolWorldComponent->BehaviorComponentReference, ResolvePrimaryBehaviorComponent());

	if (UToolDefinition* ToolDefinition = ResolveToolDefinitionForWorldComponent())
	{
		ToolWorldComponent->ToolDefinition = ToolDefinition;
	}
}

