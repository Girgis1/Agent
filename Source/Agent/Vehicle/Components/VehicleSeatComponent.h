// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "VehicleSeatComponent.generated.h"

class APlayerController;
class APawn;
class USceneComponent;

UCLASS(ClassGroup=(Vehicle), meta=(BlueprintSpawnableComponent))
class AGENT_API UVehicleSeatComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVehicleSeatComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category="Vehicle|Seat")
	bool TryEnter(AActor* Interactor);

	UFUNCTION(BlueprintCallable, Category="Vehicle|Seat")
	bool TryExit(AActor* Interactor);

	UFUNCTION(BlueprintCallable, Category="Vehicle|Seat")
	bool ForceExit(AActor* Interactor);

	UFUNCTION(BlueprintPure, Category="Vehicle|Seat")
	bool IsOccupied() const;

	UFUNCTION(BlueprintPure, Category="Vehicle|Seat")
	APawn* GetDriverPawn() const;

	UFUNCTION(BlueprintPure, Category="Vehicle|Seat")
	APlayerController* GetDriverController() const;

	UFUNCTION(BlueprintPure, Category="Vehicle|Seat")
	FVector GetInteractionLocation() const;

	UFUNCTION(BlueprintPure, Category="Vehicle|Seat")
	FVector GetExitLocation() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Seat")
	TObjectPtr<USceneComponent> SeatPoint = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Seat")
	TObjectPtr<USceneComponent> ExitPoint = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Seat")
	TObjectPtr<USceneComponent> DriverAttachPoint = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Seat")
	float ExitForwardDistance = 140.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Seat")
	float ExitSideDistance = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Seat")
	float ExitVerticalOffset = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Seat")
	bool bUsePossessionFlow = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Seat")
	bool bAttachDriverToVehicle = true;

	// When false, the seat sync only follows XY and keeps the driver's current world Z.
	// Useful for grounded vehicle-driving where the driver should still respond to gravity/falling.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Seat")
	bool bFollowSeatVerticalAxis = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Seat")
	bool bKeepDriverUprightWhileSeated = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Seat")
	bool bDisableDriverMovementWhileSeated = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Seat")
	bool bHideDriverWhileSeated = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Seat")
	bool bDisableDriverCollisionWhileSeated = true;

	// When enabled, the seated driver follows the attach point with swept capsule moves
	// instead of a hard attachment, so world collision still matters while driving.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Seat|Collision")
	bool bUseCollisionAwareDriverSync = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Seat|Collision", meta=(EditCondition="bUseCollisionAwareDriverSync"))
	bool bForceExitWhenDriverBlocked = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Seat|Collision", meta=(EditCondition="bUseCollisionAwareDriverSync", ClampMin="0.0"))
	float DriverBlockedExitDistance = 35.0f;

	// Keeps exit seamless by default: detach exactly where the driver currently is.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Seat|Exit")
	bool bExitAtCurrentLocation = true;

	// Optional safety mode: if current location is blocked, try to relocate to a safe candidate.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Seat|Exit")
	bool bUseSafeFallbackIfCurrentExitBlocked = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Seat|Emergency")
	float EmergencyExitDistance = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Seat|Emergency")
	float EmergencyExitVerticalOffset = 65.0f;

protected:
	virtual void BeginPlay() override;

private:
	bool TryExitInternal(AActor* Interactor, bool bAllowUnsafeFallback);
	bool FindSafeExitTransform(APawn* DriverPawn, FTransform& OutExitTransform) const;
	void UpdateSeatedDriverTransform(float DeltaTime);
	FTransform GetDriverSeatTransform(const APawn* DriverPawn, const APlayerController* DriverController) const;
	void SetDriverVehicleCollisionIgnored(APawn* DriverPawn, bool bShouldIgnore) const;

	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> CachedVehiclePawn;

	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> ActiveDriverPawn;

	UPROPERTY(Transient)
	TWeakObjectPtr<APlayerController> ActiveDriverController;

	UPROPERTY(Transient)
	bool bDriverWasHiddenInGame = false;

	UPROPERTY(Transient)
	bool bDriverCollisionWasEnabled = true;

	UPROPERTY(Transient)
	bool bDriverMovementWasEnabled = true;

	UPROPERTY(Transient)
	TEnumAsByte<EMovementMode> DriverPreviousMovementMode = MOVE_Walking;

	UPROPERTY(Transient)
	bool bDriverAttachedToVehicle = false;

	UPROPERTY(Transient)
	bool bDriverRootUsedAbsoluteRotation = false;

	UPROPERTY(Transient)
	bool bDriverCollisionAwareSyncActive = false;
};
