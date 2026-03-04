// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ConveyorSurfaceVelocityComponent.generated.h"

UCLASS(ClassGroup=(Factory), meta=(BlueprintSpawnableComponent))
class UConveyorSurfaceVelocityComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UConveyorSurfaceVelocityComponent();

	UFUNCTION(BlueprintCallable, Category="Factory|Conveyor")
	void SetSurfaceVelocity(const FVector& NewVelocity);

	UFUNCTION(BlueprintCallable, Category="Factory|Conveyor")
	void ClearSurfaceVelocity();

	UFUNCTION(BlueprintPure, Category="Factory|Conveyor")
	FVector GetSurfaceVelocity() const { return SurfaceVelocity; }

	UFUNCTION(BlueprintPure, Category="Factory|Conveyor")
	bool HasSurfaceVelocity() const { return !SurfaceVelocity.IsNearlyZero(); }

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Conveyor")
	FVector SurfaceVelocity = FVector::ZeroVector;
};
