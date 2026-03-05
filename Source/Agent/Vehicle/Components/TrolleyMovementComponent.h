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

	UFUNCTION(BlueprintCallable, Category="Vehicle|Movement|Load")
	void SetPayloadMassKg(float NewPayloadMassKg);

	UFUNCTION(BlueprintCallable, Category="Vehicle|Movement|Strength")
	void SetDriverStrengthSpeedMultiplier(float NewDriverStrengthSpeedMultiplier);

	UFUNCTION(BlueprintPure, Category="Vehicle|Movement|Load")
	float GetPayloadMassKg() const { return PayloadMassKg; }

	UFUNCTION(BlueprintPure, Category="Vehicle|Movement|Strength")
	float GetDriverStrengthSpeedMultiplier() const { return StrengthSpeedMultiplier; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Strength", meta=(ClampMin="0.0"))
	float StrengthSpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Strength", meta=(ClampMin="0.0"))
	float DefaultStrengthSpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Strength", meta=(ClampMin="0.0"))
	float StrengthInputMin = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Strength", meta=(ClampMin="0.0"))
	float StrengthInputMax = 10.0f;

	// Ease-out response: bigger values ramp faster early and flatten toward max.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Strength", meta=(ClampMin="0.01"))
	float StrengthCurveExponent = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Strength", meta=(ClampMin="0.0"))
	float MaxStrengthSpeedMultiplier = 5.0f;

	// Controls how strongly driver strength offsets cargo load drag:
	// 0 = no strength help against load, 1 = full strength effect.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Strength", meta=(ClampMin="0.0", ClampMax="1.0"))
	float StrengthToCargoScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Speed", meta=(ClampMin="0.0"))
	float MaxForwardSpeed = 700.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Speed", meta=(ClampMin="0.0"))
	float MaxReverseSpeed = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Drive", meta=(ClampMin="0.0"))
	float DriveForce = 140000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Drive", meta=(ClampMin="0.0"))
	float ReverseDriveForce = 90000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Drive")
	float RearDriveOffset = -90.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Steering", meta=(ClampMin="0.0"))
	float SteeringForceAtLowSpeed = 70000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Steering", meta=(ClampMin="0.0"))
	float SteeringForceAtHighSpeed = 28000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Steering")
	float FrontSteerOffset = 95.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Steering", meta=(ClampMin="0.0"))
	float MinSteerSpeed = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Stability", meta=(ClampMin="0.0"))
	float MaxYawAngularSpeedDeg = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Stability", meta=(ClampMin="0.0"))
	float MaxPitchRollAngularSpeedDeg = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Friction", meta=(ClampMin="0.0"))
	float RollingResistance = 0.55f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Friction", meta=(ClampMin="0.0"))
	float LateralGrip = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Friction", meta=(ClampMin="0.0"))
	float HandbrakeLateralGrip = 0.9f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Friction", meta=(ClampMin="0.0"))
	float HandbrakeDrag = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Conveyor", meta=(ClampMin="0.0"))
	float ConveyorCarryForceScale = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Conveyor", meta=(ClampMin="0.0"))
	float ConveyorVelocityAssist = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Load", meta=(ClampMin="0.0"))
	float PayloadMassInfluence = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Load", meta=(ClampMin="0.0"))
	float LoadDriveExponent = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Load", meta=(ClampMin="0.0", ClampMax="1.0"))
	float MinLoadDriveScale = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Movement|Load", meta=(ClampMin="0.0", ClampMax="1.0"))
	float LoadAffectsSteering = 0.6f;

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
	float PayloadMassKg = 0.0f;
};
