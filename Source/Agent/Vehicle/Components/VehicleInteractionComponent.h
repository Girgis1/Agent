// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "VehicleInteractionComponent.generated.h"

UCLASS(ClassGroup=(Vehicle), meta=(BlueprintSpawnableComponent))
class AGENT_API UVehicleInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UVehicleInteractionComponent();

	virtual void TickComponent(
		float DeltaTime,
		ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category="Vehicle|Interaction")
	bool TryEnterNearestVehicle();

	UFUNCTION(BlueprintCallable, Category="Vehicle|Interaction")
	bool TryExitCurrentVehicle();

	UFUNCTION(BlueprintPure, Category="Vehicle|Interaction")
	bool IsControllingVehicle() const;

	UFUNCTION(BlueprintPure, Category="Vehicle|Interaction")
	AActor* GetControlledVehicle() const;

	UFUNCTION(BlueprintCallable, Category="Vehicle|Interaction")
	bool ApplyVehicleMoveInput(float ForwardInput, float RightInput);

	UFUNCTION(BlueprintCallable, Category="Vehicle|Interaction")
	void SetVehicleHandbrakeInput(bool bHandbrakeActive);

	UFUNCTION(BlueprintCallable, Category="Vehicle|Interaction")
	AActor* FindBestVehicleActor(int32& OutCandidateCount) const;

	UFUNCTION(BlueprintPure, Category="Vehicle|Interaction")
	int32 GetLastCandidateCount() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Interaction")
	float InteractionTraceDistance = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Interaction")
	float InteractionTraceRadius = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Interaction")
	float NearbySearchRadius = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Interaction")
	float MaxEnterDistance = 320.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Interaction")
	float NearbyCandidateFacingWeight = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Vehicle|Interaction")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

private:
	bool IsVehicleStillControlled() const;
	bool CanUseVehicleActor(AActor* VehicleActor) const;
	AActor* ResolveVehicleActor(AActor* CandidateActor) const;

	UPROPERTY(Transient)
	int32 LastCandidateCount = 0;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> ActiveVehicleActor;
};
