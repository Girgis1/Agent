// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ToolActor.generated.h"

class UDirtBrushComponent;
class UStaticMeshComponent;
class USceneComponent;
class UToolDefinition;
class UToolWorldComponent;

UCLASS(Abstract, Blueprintable)
class AGENT_API AToolActor : public AActor
{
	GENERATED_BODY()

public:
	AToolActor();

	virtual void OnConstruction(const FTransform& Transform) override;

	UFUNCTION(BlueprintCallable, Category="Tool")
	void RefreshToolComponentReferences();

protected:
	virtual UActorComponent* ResolvePrimaryBehaviorComponent() const;
	virtual UToolDefinition* ResolveToolDefinitionForWorldComponent() const;
	void SyncToolComponentReferences();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tool", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> ToolRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tool", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> ToolMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tool", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> GripPoint = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tool", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> HeadPoint = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tool", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UDirtBrushComponent> DirtBrushComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Tool", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UToolWorldComponent> ToolWorldComponent = nullptr;
};

