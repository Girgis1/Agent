// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UprightStabilizerComponent.generated.h"

class UPrimitiveComponent;

UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class AGENT_API UUprightStabilizerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UUprightStabilizerComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category="Interact|Upright")
	void SetPhysicsBody(UPrimitiveComponent* InPhysicsBody);

	UFUNCTION(BlueprintCallable, Category="Interact|Upright")
	void SetStabilizationEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category="Interact|Upright")
	bool IsStabilizationEnabled() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Upright")
	float UprightTorqueStrength = 360.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Upright")
	float UprightAngularDamping = 32.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Upright")
	float MaxUprightTorque = 480.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Interact|Upright")
	float UprightDeadZoneDegrees = 1.0f;

private:
	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> PhysicsBody = nullptr;

	bool bStabilizationEnabled = true;
};

