// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ToolBehaviorComponent.generated.h"

class AAgentCharacter;
class UToolSystemComponent;
class UToolWorldComponent;

UCLASS(Abstract, Blueprintable, ClassGroup=(Tools), meta=(BlueprintSpawnableComponent))
class AGENT_API UToolBehaviorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UToolBehaviorComponent();

	virtual void OnToolEquipped(UToolSystemComponent* InToolSystem, UToolWorldComponent* InToolWorld);
	virtual void OnToolDropped();
	virtual void TickEquipped(float DeltaTime);
	virtual void SetPrimaryUseActive(bool bActive);

	bool IsPrimaryUseActive() const { return bPrimaryUseActive; }
	AAgentCharacter* GetOwningCharacter() const;
	UToolSystemComponent* GetToolSystem() const { return ToolSystem.Get(); }
	UToolWorldComponent* GetToolWorld() const { return ToolWorld.Get(); }

protected:
	UPROPERTY(Transient)
	TWeakObjectPtr<UToolSystemComponent> ToolSystem;

	UPROPERTY(Transient)
	TWeakObjectPtr<UToolWorldComponent> ToolWorld;

	bool bPrimaryUseActive = false;
};

