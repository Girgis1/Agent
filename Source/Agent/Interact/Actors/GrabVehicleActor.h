// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interact/Interfaces/AgentInteractable.h"
#include "GrabVehicleActor.generated.h"

class UArrowComponent;
class UGrabVehicleComponent;
class UGrabVehiclePushComponent;
class UInteractVolumeComponent;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class AGENT_API AGrabVehicleActor : public AActor, public IAgentInteractable
{
	GENERATED_BODY()

public:
	AGrabVehicleActor();

	virtual void Tick(float DeltaSeconds) override;

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FVector GetInteractionLocation_Implementation(AActor* Interactor) const override;
	virtual bool BeginInteract_Implementation(AActor* Interactor) override;
	virtual void EndInteract_Implementation(AActor* Interactor) override;
	virtual bool IsInteracting_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation() const override;

protected:
	virtual void BeginPlay() override;
	virtual void OnConstruction(const FTransform& Transform) override;

private:
	void RefreshPhysicsBodyBinding();
	UPrimitiveComponent* ResolvePhysicsBody() const;
	void StopInteraction(FName Reason);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interact|GrabVehicle", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interact|GrabVehicle", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> VehicleBody = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interact|GrabVehicle", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInteractVolumeComponent> InteractVolume = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interact|GrabVehicle", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UGrabVehicleComponent> GrabVehicleComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interact|GrabVehicle|Anchors", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> GrabPoint = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interact|GrabVehicle|Anchors", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UArrowComponent> ForwardArrow = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> ActivePhysicsBody = nullptr;

	TWeakObjectPtr<AActor> ActiveInteractor;
};
