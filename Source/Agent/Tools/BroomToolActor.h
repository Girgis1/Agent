// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Tools/ToolActor.h"
#include "BroomToolActor.generated.h"

class UBroomSweepBehaviorComponent;
class UBroomToolDefinition;
class UToolDefinition;

UCLASS(Blueprintable)
class AGENT_API ABroomToolActor : public AToolActor
{
	GENERATED_BODY()

public:
	ABroomToolActor();

	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual UActorComponent* ResolvePrimaryBehaviorComponent() const override;
	virtual UToolDefinition* ResolveToolDefinitionForWorldComponent() const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tool|Broom", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBroomSweepBehaviorComponent> BroomSweepBehaviorComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tool|Broom")
	TObjectPtr<UBroomToolDefinition> ToolDefinitionAsset = nullptr;

	UPROPERTY(VisibleAnywhere, Instanced, BlueprintReadOnly, Category="Tool|Broom", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBroomToolDefinition> DefaultInlineToolDefinition = nullptr;
};

