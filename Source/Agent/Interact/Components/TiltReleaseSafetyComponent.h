// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TiltReleaseSafetyComponent.generated.h"

class AActor;
class UPrimitiveComponent;
class USceneComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnInteractionAutoReleaseRequested, FName);

UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class AGENT_API UTiltReleaseSafetyComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UTiltReleaseSafetyComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category="Interact|Safety")
	void SetPhysicsBody(UPrimitiveComponent* InPhysicsBody);

	UFUNCTION(BlueprintCallable, Category="Interact|Safety")
	void BeginMonitoring(AActor* InInteractor, USceneComponent* InDistanceReferencePoint);

	UFUNCTION(BlueprintCallable, Category="Interact|Safety")
	void StopMonitoring();

	UFUNCTION(BlueprintPure, Category="Interact|Safety")
	bool IsMonitoring() const;

	FOnInteractionAutoReleaseRequested OnAutoReleaseRequested;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Safety")
	float TiltReleaseDegrees = 48.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Safety")
	float TiltReleaseDuration = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Safety")
	float HardTiltReleaseDegrees = 72.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Safety")
	bool bEnableDistanceRelease = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Safety")
	float MaxInteractorDistance = 340.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Safety")
	float DistanceReleaseDuration = 0.25f;

private:
	void RequestRelease(const FName Reason);

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> PhysicsBody = nullptr;

	UPROPERTY()
	TObjectPtr<USceneComponent> DistanceReferencePoint = nullptr;

	TWeakObjectPtr<AActor> InteractorActor;
	float TiltExceededTime = 0.0f;
	float DistanceExceededTime = 0.0f;
	bool bMonitoring = false;
};
