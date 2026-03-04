// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ConveyorCharacterMovementComponent.generated.h"

UCLASS()
class UConveyorCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UConveyorCharacterMovementComponent();

	UFUNCTION(BlueprintCallable, Category="Factory|Conveyor")
	void SetPendingConveyorVelocity(const FVector& NewVelocity);

	UFUNCTION(BlueprintCallable, Category="Factory|Conveyor")
	void ClearPendingConveyorVelocity();

	UFUNCTION(BlueprintPure, Category="Factory|Conveyor")
	FVector GetPendingConveyorVelocity() const { return PendingConveyorVelocity; }

protected:
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;

	UPROPERTY(Transient)
	FVector PendingConveyorVelocity = FVector::ZeroVector;
};
