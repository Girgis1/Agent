// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PhysicsDragFollowerComponent.generated.h"

class AActor;
class UPrimitiveComponent;
class USceneComponent;

UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class AGENT_API UPhysicsDragFollowerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPhysicsDragFollowerComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category="Interact|Drag")
	void SetPhysicsBody(UPrimitiveComponent* InPhysicsBody);

	UFUNCTION(BlueprintCallable, Category="Interact|Drag")
	void SetDrivePoint(USceneComponent* InDrivePoint);

	UFUNCTION(BlueprintCallable, Category="Interact|Drag")
	bool BeginFollowing(AActor* InInteractor);

	UFUNCTION(BlueprintCallable, Category="Interact|Drag")
	void StopFollowing();

	UFUNCTION(BlueprintCallable, Category="Interact|Drag")
	void SetSteerInput(float Value);

	UFUNCTION(BlueprintCallable, Category="Interact|Drag")
	void SetLiftInput(float Value);

	UFUNCTION(BlueprintCallable, Category="Interact|Drag")
	void ResetControlInputs();

	UFUNCTION(BlueprintPure, Category="Interact|Drag")
	bool IsFollowing() const;

	UFUNCTION(BlueprintPure, Category="Interact|Drag")
	AActor* GetInteractorActor() const;

	UFUNCTION(BlueprintPure, Category="Interact|Drag")
	FVector GetCurrentTargetLocation() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag")
	FVector InteractorHandleOffset = FVector(105.0f, 0.0f, 8.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag")
	float MaxPositionError = 280.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag")
	float MaxLateralError = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag")
	float MaxVerticalError = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag")
	float HorizontalPositionGain = 34.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag")
	float ForwardPositionGainMultiplier = 1.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag")
	float LateralPositionGainMultiplier = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag")
	float HorizontalVelocityDamping = 13.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag")
	float ForwardVelocityDampingMultiplier = 1.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag")
	float LateralVelocityDampingMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag")
	float VerticalPositionGain = 6.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag")
	float VerticalVelocityDamping = 11.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag")
	float MaxHorizontalAcceleration = 2400.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag")
	float MaxForwardPushAccelerationMultiplier = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag")
	float MaxForwardPullAccelerationMultiplier = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag")
	float MaxLateralAccelerationMultiplier = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag")
	float MaxVerticalAcceleration = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag")
	float MaxRaiseAccelerationMultiplier = 0.18f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag")
	float MaxDropAccelerationMultiplier = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag")
	float TargetLeadTime = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag")
	float ExtraLinearDragAcceleration = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag")
	float ForwardDragFraction = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag")
	bool bEnableYawAlignment = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag")
	float YawAlignmentStrength = 135.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag")
	float YawAlignmentDamping = 26.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag")
	float MaxYawAlignmentTorque = 160.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag|Control")
	bool bAllowSteerInput = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag|Control", meta=(EditCondition="bAllowSteerInput"))
	float SteerYawRate = 95.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag|Control", meta=(EditCondition="bAllowSteerInput"))
	float MaxSteerYawOffset = 70.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag|Control", meta=(EditCondition="bAllowSteerInput"))
	float SteerCenteringSpeed = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag|Control")
	bool bAllowLiftInput = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag|Control", meta=(EditCondition="bAllowLiftInput"))
	float MaxLiftOffset = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag|Control", meta=(EditCondition="bAllowLiftInput"))
	float LiftInputInterpSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag|Debug")
	bool bDrawDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag|Debug")
	float DebugSphereRadius = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Drag|Debug")
	float DebugLineThickness = 1.5f;

private:
	FVector ComputeTargetLocation() const;

	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> PhysicsBody = nullptr;

	UPROPERTY()
	TObjectPtr<USceneComponent> DrivePoint = nullptr;

	TWeakObjectPtr<AActor> InteractorActor;
	FVector CurrentTargetLocation = FVector::ZeroVector;
	float SteeringInput = 0.0f;
	float LiftInput = 0.0f;
	float DesiredSteerYawOffset = 0.0f;
	float SmoothedLiftOffset = 0.0f;
	bool bIsFollowing = false;
};
