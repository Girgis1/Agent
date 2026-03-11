// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ConveyorCharacterMovementComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FConveyorCharacterStepUpSignature, float);

UCLASS()
class UConveyorCharacterMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UConveyorCharacterMovementComponent();

	FConveyorCharacterStepUpSignature OnCharacterStepUp;

	UFUNCTION(BlueprintCallable, Category="Factory|Conveyor")
	void SetPendingConveyorVelocity(const FVector& NewVelocity);

	UFUNCTION(BlueprintCallable, Category="Factory|Conveyor")
	void ClearPendingConveyorVelocity();

	UFUNCTION(BlueprintPure, Category="Factory|Conveyor")
	FVector GetPendingConveyorVelocity() const { return PendingConveyorVelocity; }

protected:
	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Conveyor|StepUp")
	bool bEmitStepUpEvents = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Conveyor|StepUp", meta=(ClampMin="0.0", UIMin="0.0"))
	float StepUpEventMinHeightCm = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Conveyor|StepUp", meta=(ClampMin="0.0", UIMin="0.0"))
	float StepUpEventMaxHeightCm = 75.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Conveyor|StepUp", meta=(ClampMin="0.0", UIMin="0.0"))
	float StepUpEventMaxVerticalSpeedCmPerSecond = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Conveyor|StepUp", meta=(ClampMin="0.0", UIMin="0.0"))
	float StepUpEventMinHorizontalMoveCm = 1.0f;

	UPROPERTY(Transient)
	FVector PendingConveyorVelocity = FVector::ZeroVector;
};
