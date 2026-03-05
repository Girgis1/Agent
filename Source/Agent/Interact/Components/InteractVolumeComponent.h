// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "Engine/EngineTypes.h"
#include "InteractVolumeComponent.generated.h"

class UPrimitiveComponent;
struct FHitResult;

UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class AGENT_API UInteractVolumeComponent : public UBoxComponent
{
	GENERATED_BODY()

public:
	UInteractVolumeComponent();

	UFUNCTION(BlueprintCallable, Category="Interact")
	AActor* GetInteractableActor() const;

protected:
	virtual void OnRegister() override;
	virtual void BeginPlay() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact")
	TObjectPtr<AActor> InteractableActorOverride = nullptr;

	void ApplyVolumeDefaults();

private:
	UFUNCTION()
	void HandleBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);
};
