// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interact/Actors/DragCubeActor.h"
#include "DragCubeDedicatedActor.generated.h"

class AActor;
class UUprightStabilizerComponent;

UCLASS()
class AGENT_API ADragCubeDedicatedActor : public ADragCubeActor
{
	GENERATED_BODY()

public:
	ADragCubeDedicatedActor();
	virtual void Tick(float DeltaSeconds) override;

	virtual bool BeginInteract_Implementation(AActor* Interactor) override;
	virtual void EndInteract_Implementation(AActor* Interactor) override;

	UFUNCTION(BlueprintCallable, Category="Interact|DragCube|Drive")
	void SetDriveInput(float ForwardValue, float RightValue = 0.0f);

	UFUNCTION(BlueprintCallable, Category="Interact|DragCube|Drive")
	void SetSteerInput(float Value);

	UFUNCTION(BlueprintCallable, Category="Interact|DragCube|Drive")
	void SetLiftInput(float Value);

	UFUNCTION(BlueprintPure, Category="Interact|DragCube|Drive")
	bool IsDrivenBy(const AActor* Interactor) const;

protected:
	virtual void BeginPlay() override;

private:
	void ApplyDriveAssist();
	void ApplyInteractorFollowAssist(float DeltaSeconds);
	void ApplyFollowerControlInput();
	void ResetControlInput();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Interact|DragCube|Dedicated", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UUprightStabilizerComponent> UprightStabilizer = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DragCube|Dedicated", meta=(AllowPrivateAccess="true"))
	bool bOverrideCubeMass = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DragCube|Dedicated", meta=(AllowPrivateAccess="true", EditCondition="bOverrideCubeMass", ClampMin="1.0"))
	float CubeMassKg = 85.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DragCube|Dedicated", meta=(AllowPrivateAccess="true"))
	bool bEnableFollowerDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DragCube|Dedicated|Drive", meta=(AllowPrivateAccess="true"))
	float MaxDriveSpeed = 430.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DragCube|Dedicated|Drive", meta=(AllowPrivateAccess="true"))
	float DriveAcceleration = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DragCube|Dedicated|Drive", meta=(AllowPrivateAccess="true"))
	float DriveBrakeAcceleration = 2600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DragCube|Dedicated|Drive", meta=(AllowPrivateAccess="true"))
	float DriveResponse = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DragCube|Dedicated|Drive", meta=(AllowPrivateAccess="true"))
	float ForwardSpeedDamping = 2.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DragCube|Dedicated|Drive", meta=(AllowPrivateAccess="true"))
	float LateralSpeedDamping = 4.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DragCube|Dedicated|Drive", meta=(AllowPrivateAccess="true"))
	bool bUseMoveRightAsSteerFallback = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DragCube|Dedicated|Follow", meta=(AllowPrivateAccess="true"))
	bool bEnableInteractorFollowAssist = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DragCube|Dedicated|Follow", meta=(AllowPrivateAccess="true", EditCondition="bEnableInteractorFollowAssist"))
	float InteractorFollowDistance = 125.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DragCube|Dedicated|Follow", meta=(AllowPrivateAccess="true", EditCondition="bEnableInteractorFollowAssist"))
	float InteractorFollowInputScale = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DragCube|Dedicated|Follow", meta=(AllowPrivateAccess="true", EditCondition="bEnableInteractorFollowAssist"))
	float InteractorFollowLateralInputScale = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DragCube|Dedicated|Follow", meta=(AllowPrivateAccess="true", EditCondition="bEnableInteractorFollowAssist"))
	float InteractorYawAlignSpeed = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DragCube|Dedicated|Stability", meta=(AllowPrivateAccess="true"))
	bool bEnableHandUprightAssist = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DragCube|Dedicated|Stability", meta=(AllowPrivateAccess="true", EditCondition="bEnableHandUprightAssist"))
	float UprightAssistTorqueStrength = 440.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DragCube|Dedicated|Stability", meta=(AllowPrivateAccess="true", EditCondition="bEnableHandUprightAssist"))
	float UprightAssistAngularDamping = 36.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|DragCube|Dedicated|Stability", meta=(AllowPrivateAccess="true", EditCondition="bEnableHandUprightAssist"))
	float UprightAssistMaxTorque = 700.0f;

	float DriveForwardInput = 0.0f;
	float MoveSteerInput = 0.0f;
	float StickSteerInput = 0.0f;
	float LiftControlInput = 0.0f;
};
