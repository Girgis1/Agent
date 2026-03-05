// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interact/Interfaces/AgentInteractable.h"
#include "DragCubeActor.generated.h"

class UInteractVolumeComponent;
class UPhysicsDragFollowerComponent;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class AGENT_API ADragCubeActor : public AActor, public IAgentInteractable
{
	GENERATED_BODY()

public:
	ADragCubeActor();

	virtual void Tick(float DeltaSeconds) override;

	virtual bool CanInteract_Implementation(AActor* Interactor) const override;
	virtual FVector GetInteractionLocation_Implementation(AActor* Interactor) const override;
	virtual bool BeginInteract_Implementation(AActor* Interactor) override;
	virtual void EndInteract_Implementation(AActor* Interactor) override;
	virtual bool IsInteracting_Implementation(AActor* Interactor) const override;
	virtual FText GetInteractPrompt_Implementation() const override;

	UFUNCTION(BlueprintPure, Category="Interact|DragCube")
	UStaticMeshComponent* GetCubeBodyComponent() const;

	UFUNCTION(BlueprintPure, Category="Interact|DragCube")
	USceneComponent* GetHandleAnchorComponent() const;

	UFUNCTION(BlueprintPure, Category="Interact|DragCube")
	UPhysicsDragFollowerComponent* GetDragFollowerComponent() const;

	UFUNCTION(BlueprintPure, Category="Interact|DragCube")
	AActor* GetActiveInteractorActor() const;

	UFUNCTION(BlueprintPure, Category="Interact|DragCube")
	UPrimitiveComponent* GetActivePhysicsBodyComponent() const;

protected:
	virtual void BeginPlay() override;
	void SetInteractionDistances(float InMaxInteractDistance, float InBreakDistance);

private:
	void RefreshPhysicsBodyBinding();
	UPrimitiveComponent* ResolvePhysicsBody() const;
	void StopInteraction(FName Reason);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interact|DragCube", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interact|DragCube", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> CubeBody = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interact|DragCube", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> HandleAnchor = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interact|DragCube", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UInteractVolumeComponent> InteractVolume = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interact|DragCube", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UPhysicsDragFollowerComponent> DragFollower = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DragCube", meta=(AllowPrivateAccess="true"))
	float MaxInteractDistance = 280.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DragCube", meta=(AllowPrivateAccess="true"))
	float BreakDistance = 420.0f;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> ActivePhysicsBody = nullptr;

	TWeakObjectPtr<AActor> ActiveInteractor;
};
