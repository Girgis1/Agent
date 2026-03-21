// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Components/ActorComponent.h"
#include "ToolSystemComponent.generated.h"

class AAgentCharacter;
class UToolBehaviorComponent;
class UToolWorldComponent;

UCLASS(ClassGroup=(Tools), meta=(BlueprintSpawnableComponent))
class AGENT_API UToolSystemComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UToolSystemComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	bool TryEquipNearestTool();
	void DropEquippedTool(bool bForceDrop = false);
	bool IsToolEquipped() const;

	void SetPushInputActive(bool bActive);
	void ClearInputs();

	UToolWorldComponent* GetEquippedTool() const { return EquippedTool.Get(); }
	UToolBehaviorComponent* GetEquippedBehavior() const { return EquippedBehavior.Get(); }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tool|Search", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float InteractionTraceDistance = 320.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tool|Search", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float InteractionTraceRadius = 28.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tool|Search", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float NearbySearchRadius = 280.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tool|Search", meta=(ClampMin="0.0", UIMin="0.0", ClampMax="1.0", UIMax="1.0"))
	float NearbyCandidateFacingWeight = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tool|Search")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Tool|Debug")
	bool bDrawDebug = false;

private:
	AAgentCharacter* ResolveAgentCharacter() const;
	bool CanMaintainEquippedTool() const;
	UToolWorldComponent* FindBestToolComponent(int32& OutCandidateCount) const;
	bool ResolveHoldView(FVector& OutLocation, FRotator& OutRotation) const;

	UPROPERTY(Transient)
	TWeakObjectPtr<UToolWorldComponent> EquippedTool;

	UPROPERTY(Transient)
	TWeakObjectPtr<UToolBehaviorComponent> EquippedBehavior;

	bool bPushInputHeld = false;
};
