// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Vehicle/Interfaces/VehicleInteractable.h"
#include "TrolleyVehiclePawn.generated.h"

class UCameraComponent;
class USceneComponent;
class UStaticMeshComponent;
class USpringArmComponent;
class UTrolleyMovementComponent;
class UVehicleSeatComponent;

UCLASS()
class AGENT_API ATrolleyVehiclePawn : public APawn, public IVehicleInteractable
{
	GENERATED_BODY()

public:
	ATrolleyVehiclePawn();

	virtual void Tick(float DeltaSeconds) override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual UPawnMovementComponent* GetMovementComponent() const override;

	virtual bool CanEnterVehicle_Implementation(AActor* Interactor) const override;
	virtual bool EnterVehicle_Implementation(AActor* Interactor) override;
	virtual bool ExitVehicle_Implementation(AActor* Interactor) override;
	virtual bool IsVehicleOccupied_Implementation() const override;
	virtual FVector GetVehicleInteractionLocation_Implementation(AActor* Interactor) const override;
	virtual FText GetVehicleInteractionPrompt_Implementation() const override;

	UFUNCTION(BlueprintCallable, Category="Vehicle|Camera")
	void SetUseFirstPersonCamera(bool bUseFirstPerson);

	UFUNCTION(BlueprintCallable, Category="Vehicle|Camera")
	void ToggleCameraMode();

protected:
	virtual void BeginPlay() override;

private:
	void ApplyCameraMode();
	void ApplyDriveInput();
	void UpdateTipExit(float DeltaSeconds);

	void OnThrottleForwardPressed();
	void OnThrottleForwardReleased();
	void OnThrottleReversePressed();
	void OnThrottleReverseReleased();
	void OnSteerLeftPressed();
	void OnSteerLeftReleased();
	void OnSteerRightPressed();
	void OnSteerRightReleased();
	void OnHandbrakePressed();
	void OnHandbrakeReleased();
	void OnExitVehiclePressed();
	void OnToggleCameraPressed();
	void OnThrottleAxis(float Value);
	void OnSteeringAxis(float Value);
	void OnLookYawAxis(float Value);
	void OnLookPitchAxis(float Value);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> VehicleBody = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> SeatPoint = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> ExitPoint = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USpringArmComponent> CameraBoom = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCameraComponent> ThirdPersonCamera = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UCameraComponent> FirstPersonCamera = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UTrolleyMovementComponent> TrolleyMovementComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UVehicleSeatComponent> VehicleSeatComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Interaction", meta=(AllowPrivateAccess="true"))
	float MaxEnterDistance = 320.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Camera", meta=(AllowPrivateAccess="true"))
	bool bStartInFirstPerson = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Camera", meta=(AllowPrivateAccess="true"))
	bool bAllowCameraToggle = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Prompt", meta=(AllowPrivateAccess="true"))
	FText EnterPrompt = FText::FromString(TEXT("Press T to control trolley"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Prompt", meta=(AllowPrivateAccess="true"))
	FText ExitPrompt = FText::FromString(TEXT("Press T to exit trolley"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Safety", meta=(AllowPrivateAccess="true"))
	bool bExitOnTipOver = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Safety", meta=(AllowPrivateAccess="true"))
	float TipOverUpDotThreshold = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Safety", meta=(AllowPrivateAccess="true"))
	float TipOverExitDelay = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Safety", meta=(AllowPrivateAccess="true"))
	float TipOverExitRetryCooldown = 0.5f;

	bool bUseFirstPersonCamera = false;
	bool bThrottleForwardHeld = false;
	bool bThrottleReverseHeld = false;
	bool bSteerLeftHeld = false;
	bool bSteerRightHeld = false;
	bool bHandbrakeHeld = false;
	float ThrottleAxisValue = 0.0f;
	float SteeringAxisValue = 0.0f;
	float TipOverElapsed = 0.0f;
	float TipOverRetryCooldownRemaining = 0.0f;
};
