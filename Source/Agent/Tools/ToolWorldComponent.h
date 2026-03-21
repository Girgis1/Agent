// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Components/ActorComponent.h"
#include "ToolWorldComponent.generated.h"

class AAgentCharacter;
class UToolBehaviorComponent;
class UToolDefinition;
class UDirtBrushComponent;
class UPrimitiveComponent;
class USceneComponent;

UCLASS(ClassGroup=(Tools), meta=(BlueprintSpawnableComponent))
class AGENT_API UToolWorldComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UToolWorldComponent();

	virtual void BeginPlay() override;

	bool ValidateToolSetup(FString* OutErrorReason = nullptr) const;
	UFUNCTION(BlueprintPure, Category="Tool")
	FString GetToolSetupValidationError() const;

	bool CanBeEquippedBy(const AAgentCharacter* AgentCharacter) const;
	bool OnEquipped(AAgentCharacter* AgentCharacter);
	void OnDropped(const FVector& LinearVelocity, const FVector& AngularVelocity);

	bool IsEquipped() const { return EquippedByCharacter.IsValid(); }

	UPrimitiveComponent* ResolvePhysicsPrimitive() const;
	USceneComponent* ResolveGripPoint() const;
	USceneComponent* ResolveHeadPoint() const;
	UToolBehaviorComponent* ResolveBehavior() const;
	UDirtBrushComponent* ResolveDirtBrush() const;
	FVector GetGripLocation() const;

	const UToolDefinition* GetToolDefinition() const { return ToolDefinition; }

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tool")
	TObjectPtr<UToolDefinition> ToolDefinition = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tool|References", meta=(UseComponentPicker))
	FComponentReference PhysicsPrimitiveReference;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tool|References", meta=(UseComponentPicker))
	FComponentReference GripPointReference;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tool|References", meta=(UseComponentPicker))
	FComponentReference HeadPointReference;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tool|References", meta=(UseComponentPicker))
	FComponentReference BehaviorComponentReference;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tool|References", meta=(UseComponentPicker))
	FComponentReference DirtBrushReference;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tool|Debug")
	bool bLogSetupFailures = true;

protected:
	UPROPERTY(Transient)
	TWeakObjectPtr<AAgentCharacter> EquippedByCharacter;

private:
	void LogToolSetupFailure(const FString& Reason) const;

	bool bHasStoredPhysicsState = false;
	bool bStoredSimulatePhysics = false;
	bool bStoredGravityEnabled = true;
	ECollisionEnabled::Type StoredCollisionEnabled = ECollisionEnabled::QueryAndPhysics;
	FName StoredCollisionProfileName = NAME_None;
	float StoredLinearDamping = 0.0f;
	float StoredAngularDamping = 0.0f;
	mutable bool bHasLoggedSetupFailure = false;
};
