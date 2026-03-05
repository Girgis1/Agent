// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PawnMovementComponent.h"
#include "TrolleyMovementComponent.generated.h"

UCLASS(ClassGroup=(Vehicle), meta=(BlueprintSpawnableComponent))
class AGENT_API UTrolleyMovementComponent : public UPawnMovementComponent
{
	GENERATED_BODY()

public:
	UTrolleyMovementComponent();

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	virtual float GetMaxSpeed() const override;
	virtual void StopMovementImmediately() override;

	UFUNCTION(BlueprintCallable, Category="Vehicle|Movement")
	void SetThrottleInput(float Value);

	UFUNCTION(BlueprintCallable, Category="Vehicle|Movement")
	void SetSteeringInput(float Value);

	UFUNCTION(BlueprintCallable, Category="Vehicle|Movement")
	void SetHandbrakeInput(bool bNewHandbrakeActive);

	UFUNCTION(BlueprintPure, Category="Vehicle|Movement")
	float GetForwardSpeed() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Speed", meta=(ClampMin="0.0"))
	float MaxForwardSpeed = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Speed", meta=(ClampMin="0.0"))
	float MaxReverseSpeed = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Drive", meta=(ClampMin="0.0"))
	float DriveForce = 320000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Drive", meta=(ClampMin="0.0"))
	float ReverseDriveForce = 210000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Drive")
	float RearDriveOffset = -90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Steering", meta=(ClampMin="0.0"))
	float SteeringTorqueAtLowSpeed = 2200000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Steering", meta=(ClampMin="0.0"))
	float SteeringTorqueAtHighSpeed = 800000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Friction", meta=(ClampMin="0.0"))
	float RollingResistance = 0.85f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Friction", meta=(ClampMin="0.0"))
	float LateralGrip = 3.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Friction", meta=(ClampMin="0.0"))
	float HandbrakeLateralGrip = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Friction", meta=(ClampMin="0.0"))
	float HandbrakeDrag = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Kinematic", meta=(ClampMin="0.0"))
	float KinematicAcceleration = 1800.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Kinematic", meta=(ClampMin="0.0"))
	float KinematicDeceleration = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Kinematic", meta=(ClampMin="0.0"))
	float KinematicBrakeDeceleration = 3200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Kinematic")
	float KinematicTurnRateAtLowSpeed = 90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Kinematic")
	float KinematicTurnRateAtHighSpeed = 58.0f;

private:
	float ThrottleInput = 0.0f;
	float SteeringInput = 0.0f;
	float CurrentForwardSpeed = 0.0f;
	bool bHandbrakeActive = false;
};
