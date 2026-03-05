// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AgentInteractorComponent.generated.h"

class AActor;
class UInteractVolumeComponent;
struct FHitResult;
struct FOverlapResult;

UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class AGENT_API UAgentInteractorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAgentInteractorComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	void RegisterInteractVolume(UInteractVolumeComponent* InVolume);
	void UnregisterInteractVolume(UInteractVolumeComponent* InVolume);

	UFUNCTION(BlueprintCallable, Category="Interact")
	bool ToggleInteraction();

	UFUNCTION(BlueprintCallable, Category="Interact")
	void EndCurrentInteraction();

	UFUNCTION(BlueprintPure, Category="Interact")
	bool IsInteracting() const;

	UFUNCTION(BlueprintPure, Category="Interact")
	AActor* GetActiveInteractable() const;

	UFUNCTION(BlueprintPure, Category="Interact")
	int32 GetCandidateCount() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Query")
	float MaxViewTraceDistance = 500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Query")
	float ViewTraceRadius = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Query")
	float NearbyInteractRadius = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Query")
	bool bPreferLookAtTarget = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Query")
	bool bRequireLineOfSight = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Query")
	float ViewFacingWeight = 15000.0f;

private:
	bool BeginBestInteraction();
	AActor* FindBestInteractableActor(int32& OutCandidateCount) const;
	AActor* ResolveActorFromHit(const FHitResult& HitResult) const;
	AActor* ResolveActorFromOverlap(const FOverlapResult& OverlapResult) const;
	bool CanUseInteractableActor(AActor* InteractableActor) const;

	TWeakObjectPtr<AActor> ActiveInteractableActor;
	int32 LastQueryCandidateCount = 0;
};
