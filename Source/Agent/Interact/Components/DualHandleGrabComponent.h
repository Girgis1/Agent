// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DualHandleGrabComponent.generated.h"

class AActor;
class UPhysicsHandleComponent;
class UPrimitiveComponent;
class USceneComponent;

UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class AGENT_API UDualHandleGrabComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDualHandleGrabComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category="Interact|DualHandle")
	void SetHandPivots(USceneComponent* InLeftHandPivot, USceneComponent* InRightHandPivot);

	UFUNCTION(BlueprintCallable, Category="Interact|DualHandle")
	bool BeginGrab(AActor* InGrabbedActor, UPrimitiveComponent* InGrabbedBody);

	UFUNCTION(BlueprintCallable, Category="Interact|DualHandle")
	bool BeginGrabAtLocations(
		AActor* InGrabbedActor,
		UPrimitiveComponent* InGrabbedBody,
		const FVector& InLeftGrabLocation,
		const FVector& InRightGrabLocation);

	UFUNCTION(BlueprintCallable, Category="Interact|DualHandle")
	void EndGrab();

	UFUNCTION(BlueprintPure, Category="Interact|DualHandle")
	bool IsGrabbing() const;

	UFUNCTION(BlueprintPure, Category="Interact|DualHandle")
	bool IsGrabbingActor(const AActor* InActor) const;

	UFUNCTION(BlueprintPure, Category="Interact|DualHandle")
	AActor* GetGrabbedActor() const;

	UFUNCTION(BlueprintCallable, Category="Interact|DualHandle")
	void SetSteeringHorizontal(float Value);

	UFUNCTION(BlueprintCallable, Category="Interact|DualHandle")
	void SetSteeringVertical(float Value);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DualHandle")
	float HandleLinearStiffness = 9000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DualHandle")
	float HandleLinearDamping = 420.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DualHandle")
	float HandleAngularStiffness = 3400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DualHandle")
	float HandleAngularDamping = 240.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DualHandle")
	float HandleInterpolationSpeed = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DualHandle|Steering")
	float MaxVerticalSteeringOffset = 22.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DualHandle|Steering")
	float MaxForwardSteeringOffset = 28.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DualHandle|Steering")
	float SteeringInputInterpSpeed = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DualHandle|Grab")
	bool bPreserveInitialHandAnchorOffset = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DualHandle|Grab")
	float MaxInitialHandToAnchorDistance = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DualHandle")
	float BreakDistance = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DualHandle|Debug")
	bool bDrawDebugHandSpheres = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DualHandle|Debug")
	float DebugSphereRadius = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DualHandle|Debug")
	float DebugSphereThickness = 1.5f;

private:
	void EnsurePhysicsHandles();
	void ApplyHandleSettings(UPhysicsHandleComponent* PhysicsHandle) const;
	bool ComputeHandTargets(FVector& OutLeftTarget, FVector& OutRightTarget) const;
	void DrawDebugTargets(const FVector& LeftTarget, const FVector& RightTarget) const;

	UPROPERTY()
	TObjectPtr<UPhysicsHandleComponent> LeftPhysicsHandle = nullptr;

	UPROPERTY()
	TObjectPtr<UPhysicsHandleComponent> RightPhysicsHandle = nullptr;

	UPROPERTY()
	TObjectPtr<USceneComponent> LeftHandPivot = nullptr;

	UPROPERTY()
	TObjectPtr<USceneComponent> RightHandPivot = nullptr;

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> GrabbedBody = nullptr;

	TWeakObjectPtr<AActor> GrabbedActor;
	float SteeringHorizontalInput = 0.0f;
	float SteeringVerticalInput = 0.0f;
	float SmoothedSteeringHorizontal = 0.0f;
	float SmoothedSteeringVertical = 0.0f;
	FVector LeftInitialOffsetLocal = FVector::ZeroVector;
	FVector RightInitialOffsetLocal = FVector::ZeroVector;
	bool bIsGrabbing = false;
};
