// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/BroomToolActor.h"

#include "Tools/BroomSweepBehaviorComponent.h"
#include "Tools/BroomToolDefinition.h"
#include "Tools/ToolWorldComponent.h"
#include "UObject/ConstructorHelpers.h"

ABroomToolActor::ABroomToolActor()
{
	BroomSweepBehaviorComponent = CreateDefaultSubobject<UBroomSweepBehaviorComponent>(TEXT("BroomSweepBehaviorComponent"));
	DefaultInlineToolDefinition = CreateDefaultSubobject<UBroomToolDefinition>(TEXT("DefaultInlineToolDefinition"));

	static ConstructorHelpers::FObjectFinder<UBroomToolDefinition> DefaultToolDefinitionAsset(
		TEXT("/Game/Tools/Data/DA_BroomToolDefinition_Default.DA_BroomToolDefinition_Default"));
	if (DefaultToolDefinitionAsset.Succeeded())
	{
		ToolDefinitionAsset = DefaultToolDefinitionAsset.Object;
	}

	RefreshToolComponentReferences();
}

void ABroomToolActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshToolComponentReferences();
}

UActorComponent* ABroomToolActor::ResolvePrimaryBehaviorComponent() const
{
	return BroomSweepBehaviorComponent;
}

UToolDefinition* ABroomToolActor::ResolveToolDefinitionForWorldComponent() const
{
	if (ToolDefinitionAsset)
	{
		return ToolDefinitionAsset;
	}

	return DefaultInlineToolDefinition;
}

