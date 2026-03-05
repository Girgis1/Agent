// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interact/Interfaces/AgentInteractable.h"
#include "ShoppingCartActor.generated.h"

class UInteractVolumeComponent;
class UPhysicsDragFollowerComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;
class UTiltReleaseSafetyComponent;
class UUprightStabilizerComponent;
class USceneComponent;

UCLASS()
class AGENT_API AShoppingCartActor : public AActor, public IAgentInteractable
{
	GENERATED_BODY()

public:
	AShoppingCartActor();

	virtual void Tick(float DeltaSeconds) override;

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FVector GetInteractionLocation_Implementation(AActor* Interactor) const override;
	virtual bool BeginInteract_Implementation(AActor* Interactor) override;
	virtual void EndInteract_Implementation(AActor* Interactor) override;
	virtual bool IsInteracting_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation() const override;

protected:
	virtual void BeginPlay() override;

private:
	void RefreshPhysicsBodyBinding();
	UPrimitiveComponent* ResolvePhysicsBody() const;
	void StopInteraction(FName Reason);
	void HandleAutoReleaseRequested(FName Reason);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interact|Cart", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interact|Cart", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> CartBody = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interact|Cart", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> HandleAnchor = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interact|Cart", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInteractVolumeComponent> InteractVolume = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interact|Cart", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UPhysicsDragFollowerComponent> DragFollower = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interact|Cart", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UUprightStabilizerComponent> UprightStabilizer = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interact|Cart", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UTiltReleaseSafetyComponent> TiltReleaseSafety = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Cart", meta=(AllowPrivateAccess="true"))
	float MaxInteractDistance = 260.0f;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> ActivePhysicsBody = nullptr;

	TWeakObjectPtr<AActor> ActiveInteractor;
};
