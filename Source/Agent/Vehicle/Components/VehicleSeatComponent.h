// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
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
	float ExitForwardDistance = 140.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Seat")
	float ExitSideDistance = 110.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Seat")
	float ExitVerticalOffset = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Seat")
	bool bHideDriverWhileSeated = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Seat")
	bool bDisableDriverCollisionWhileSeated = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Seat|Emergency")
	float EmergencyExitDistance = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Seat|Emergency")
	float EmergencyExitVerticalOffset = 65.0f;

protected:
	virtual void BeginPlay() override;

private:
	bool TryExitInternal(AActor* Interactor, bool bAllowUnsafeFallback);
	bool FindSafeExitTransform(APawn* DriverPawn, FTransform& OutExitTransform) const;

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
};
