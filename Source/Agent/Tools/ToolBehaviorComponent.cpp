// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/ToolBehaviorComponent.h"

#include "AgentCharacter.h"
#include "Tools/ToolSystemComponent.h"
#include "Tools/ToolWorldComponent.h"

UToolBehaviorComponent::UToolBehaviorComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UToolBehaviorComponent::OnToolEquipped(UToolSystemComponent* InToolSystem, UToolWorldComponent* InToolWorld)
{
	ToolSystem = InToolSystem;
	ToolWorld = InToolWorld;
	bPrimaryUseActive = false;
}

void UToolBehaviorComponent::OnToolDropped()
{
	bPrimaryUseActive = false;
	ToolSystem.Reset();
	ToolWorld.Reset();
}

void UToolBehaviorComponent::TickEquipped(float DeltaTime)
{
	(void)DeltaTime;
}

void UToolBehaviorComponent::SetPrimaryUseActive(bool bActive)
{
	bPrimaryUseActive = bActive;
}

AAgentCharacter* UToolBehaviorComponent::GetOwningCharacter() const
{
	UToolSystemComponent* ToolSystemComponent = ToolSystem.Get();
	return ToolSystemComponent ? Cast<AAgentCharacter>(ToolSystemComponent->GetOwner()) : nullptr;
}

