// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GrabVehiclePushComponent.generated.h"

class AActor;
class UPhysicsHandleComponent;
class UPrimitiveComponent;

UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class AGENT_API UGrabVehiclePushComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGrabVehiclePushComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category="Interact|GrabVehiclePush")
	bool BeginPush(AActor* InVehicleActor, UPrimitiveComponent* InPhysicsBody, const FVector& InGrabPointWorld);

	UFUNCTION(BlueprintCallable, Category="Interact|GrabVehiclePush")
	void EndPush();

	UFUNCTION(BlueprintPure, Category="Interact|GrabVehiclePush")
	bool IsPushing() const;

	UFUNCTION(BlueprintPure, Category="Interact|GrabVehiclePush")
	bool IsPushingActor(const AActor* InActor) const;

	UFUNCTION(BlueprintPure, Category="Interact|GrabVehiclePush")
	AActor* GetPushedActor() const;

	UFUNCTION(BlueprintCallable, Category="Interact|GrabVehiclePush")
	void SetSteerInput(float Value);

	UFUNCTION(BlueprintCallable, Category="Interact|GrabVehiclePush")
	void SetLiftInput(float Value);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehiclePush|Hold")
	float HoldForwardOffset = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehiclePush|Hold")
	float HoldLateralOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehiclePush|Hold")
	float HoldVerticalOffset = -30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehiclePush|Hold")
	bool bUseInitialGrabVerticalOffset = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehiclePush|Lift")
	bool bAllowLiftInput = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehiclePush|Lift", meta=(EditCondition="bAllowLiftInput"))
	float MaxLiftOffset = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehiclePush|Lift")
	float LiftInputInterpSpeed = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehiclePush|Safety")
	float BreakDistance = 320.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehiclePush|Safety")
	float MaxStartGrabDistance = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehiclePush|Handle")
	float HandleLinearStiffness = 9200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehiclePush|Handle")
	float HandleLinearDamping = 560.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehiclePush|Handle")
	float HandleAngularStiffness = 3200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehiclePush|Handle")
	float HandleAngularDamping = 280.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehiclePush|Handle")
	float HandleInterpolationSpeed = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehiclePush|Steer")
	float SteerYawRate = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehiclePush|Steer")
	float YawTorqueStrength = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehiclePush|Steer")
	float YawTorqueDamping = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehiclePush|Steer")
	float MaxYawTorque = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehiclePush|Debug")
	bool bDrawDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehiclePush|Debug")
	float DebugSphereRadius = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|GrabVehiclePush|Debug")
	float DebugLineThickness = 1.5f;

private:
	void EnsurePhysicsHandle();
	void ApplyHandleSettings() const;
	bool ComputeHoldTarget(FVector& OutTargetLocation, FRotator& OutTargetRotation) const;

	UPROPERTY()
	TObjectPtr<UPhysicsHandleComponent> PushPhysicsHandle = nullptr;

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> GrabbedBody = nullptr;

	TWeakObjectPtr<AActor> PushedActor;
	float InitialVerticalLocalOffset = 0.0f;
	float DesiredYawOffset = 0.0f;
	float SteerInput = 0.0f;
	float LiftInput = 0.0f;
	float SmoothedLiftOffset = 0.0f;
	bool bIsPushing = false;
};
