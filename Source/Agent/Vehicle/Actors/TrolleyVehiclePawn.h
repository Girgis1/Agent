// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Vehicle/Interfaces/VehicleInteractable.h"
#include "TrolleyVehiclePawn.generated.h"

class UConveyorSurfaceVelocityComponent;
class UBoxComponent;
class UPhysicalMaterial;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMeshComponent;
class UTrolleyMovementComponent;
class UVehicleSeatComponent;
struct FHitResult;

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
	virtual bool IsVehicleControlledBy_Implementation(AActor* Interactor) const override;
	virtual FVector GetVehicleInteractionLocation_Implementation(AActor* Interactor) const override;
	virtual FText GetVehicleInteractionPrompt_Implementation() const override;
	virtual void SetVehicleMoveInput_Implementation(AActor* Interactor, float ForwardInput, float RightInput) override;
	virtual void SetVehicleHandbrakeInput_Implementation(AActor* Interactor, bool bHandbrakeActive) override;

protected:
	virtual void BeginPlay() override;

private:
	void ApplyDriveInput();
	void ResetDriveInput();
	void UpdateTipExit(float DeltaSeconds);
	void UpdatePhysicsMaterial();
	void UpdateCargoMassTracking();
	float ResolveDriverStrengthSpeedMultiplier(AActor* Interactor) const;
	bool IsTrackedCargoComponent(const UPrimitiveComponent* PrimitiveComponent) const;

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
	void OnThrottleAxis(float Value);
	void OnSteeringAxis(float Value);
	UFUNCTION()
	void OnCargoBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);
	UFUNCTION()
	void OnCargoEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> PhysicsBody = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> VehicleBody = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> SeatPoint = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> ExitPoint = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USceneComponent> DriverAttachPoint = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Cargo", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UBoxComponent> CargoMassVolume = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UTrolleyMovementComponent> TrolleyMovementComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UConveyorSurfaceVelocityComponent> ConveyorSurfaceVelocityComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UVehicleSeatComponent> VehicleSeatComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Interaction", meta=(AllowPrivateAccess="true"))
	float MaxEnterDistance = 320.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Prompt", meta=(AllowPrivateAccess="true"))
	FText EnterPrompt = FText::FromString(TEXT("Press T to control trolley"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Prompt", meta=(AllowPrivateAccess="true"))
	FText ExitPrompt = FText::FromString(TEXT("Press T to exit trolley"));

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Feel", meta=(AllowPrivateAccess="true", ClampMin="0.0"))
	float PhysicsSurfaceFriction = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Feel", meta=(AllowPrivateAccess="true", ClampMin="0.0", ClampMax="1.0"))
	float PhysicsSurfaceRestitution = 0.01f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Safety", meta=(AllowPrivateAccess="true"))
	bool bExitOnTipOver = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Safety", meta=(AllowPrivateAccess="true"))
	float TipOverUpDotThreshold = 0.45f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Safety", meta=(AllowPrivateAccess="true"))
	float TipOverExitDelay = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Safety", meta=(AllowPrivateAccess="true"))
	float TipOverExitRetryCooldown = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Cargo", meta=(AllowPrivateAccess="true"))
	FVector CargoVolumeExtent = FVector(110.0f, 80.0f, 70.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Cargo", meta=(AllowPrivateAccess="true"))
	FVector CargoVolumeOffset = FVector(20.0f, 0.0f, 68.0f);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Vehicle|Cargo", meta=(AllowPrivateAccess="true"))
	float CurrentCargoMassKg = 0.0f;

	bool bThrottleForwardHeld = false;
	bool bThrottleReverseHeld = false;
	bool bSteerLeftHeld = false;
	bool bSteerRightHeld = false;
	bool bHandbrakeHeld = false;
	float ThrottleAxisValue = 0.0f;
	float SteeringAxisValue = 0.0f;
	bool bUsingExternalInput = false;
	float ExternalThrottleInput = 0.0f;
	float ExternalSteeringInput = 0.0f;
	bool bExternalHandbrakeHeld = false;
	float TipOverElapsed = 0.0f;
	float TipOverRetryCooldownRemaining = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UPhysicalMaterial> RuntimePhysicsMaterial = nullptr;

	TSet<TWeakObjectPtr<UPrimitiveComponent>> TrackedCargoComponents;
};
