// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MiningBotActor.generated.h"

class AMaterialNodeActor;
class AMiningSwarmMachine;
class USceneComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EMiningBotRuntimeState : uint8
{
	Disabled UMETA(DisplayName="Disabled"),
	MissingSwarm UMETA(DisplayName="Missing Swarm"),
	Idle UMETA(DisplayName="Idle"),
	NoNodeAvailable UMETA(DisplayName="No Node Available"),
	TravelingToNode UMETA(DisplayName="Traveling To Node"),
	Mining UMETA(DisplayName="Mining"),
	ReturningToSwarm UMETA(DisplayName="Returning To Swarm"),
	WaitingForOutput UMETA(DisplayName="Waiting For Output")
};

UCLASS()
class AMiningBotActor : public AActor
{
	GENERATED_BODY()

public:
	AMiningBotActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category="Factory|MiningBot")
	void SetOwningSwarmMachine(AMiningSwarmMachine* NewOwningSwarmMachine);

	UFUNCTION(BlueprintPure, Category="Factory|MiningBot")
	AMiningSwarmMachine* GetOwningSwarmMachine() const { return OwningSwarmMachine; }

	UFUNCTION(BlueprintCallable, Category="Factory|MiningBot")
	void SetMiningTuning(int32 InCapacityUnits, float InTravelSpeedCmPerSecond, float InMiningSpeedMultiplier);

	UFUNCTION(BlueprintPure, Category="Factory|MiningBot")
	EMiningBotRuntimeState GetRuntimeState() const { return RuntimeState; }

	UFUNCTION(BlueprintPure, Category="Factory|MiningBot")
	FText GetRuntimeStatus() const { return RuntimeStatus; }

	UFUNCTION(BlueprintPure, Category="Factory|MiningBot")
	int32 GetCarriedTotalQuantityScaled() const;

	UFUNCTION(BlueprintImplementableEvent, Category="Factory|MiningBot")
	void OnMiningBotStateChanged(EMiningBotRuntimeState NewState, const FText& NewStatus);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningBot")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningBot")
	TObjectPtr<UStaticMeshComponent> BotMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot")
	bool bEnabled = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningBot")
	TObjectPtr<AMiningSwarmMachine> OwningSwarmMachine = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Mining", meta=(ClampMin="1", UIMin="1"))
	int32 MiningCapacityUnits = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Movement", meta=(ClampMin="1.0", UIMin="1.0"))
	float TravelSpeedCmPerSecond = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Mining", meta=(ClampMin="0.05", UIMin="0.05"))
	float MiningDurationSeconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Mining", meta=(ClampMin="0.1", UIMin="0.1"))
	float MiningSpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Movement", meta=(ClampMin="0.05", UIMin="0.05"))
	float SurfaceAlignmentInterpSpeed = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Movement", meta=(ClampMin="1.0", UIMin="1.0"))
	float ArrivalToleranceCm = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Movement", meta=(ClampMin="0.0", UIMin="0.0"))
	float SurfaceHoverOffsetCm = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Output", meta=(ClampMin="0.05", UIMin="0.05"))
	float DepositRetryIntervalSeconds = 0.25f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningBot")
	EMiningBotRuntimeState RuntimeState = EMiningBotRuntimeState::MissingSwarm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningBot")
	FText RuntimeStatus;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningBot")
	TObjectPtr<AMaterialNodeActor> CurrentTargetNode = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningBot")
	FVector CurrentTargetLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningBot")
	FVector CurrentTargetSurfaceNormal = FVector::UpVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningBot")
	TMap<FName, int32> CarriedResourcesScaled;

protected:
	void SetRuntimeState(EMiningBotRuntimeState NewState, const FString& NewStatusMessage);
	bool RequestAssignmentFromSwarm();
	void StartMiningCycle();
	void CompleteMiningCycle();
	void HandleTravelToNode(float DeltaSeconds);
	void HandleMining(float DeltaSeconds);
	void HandleReturnToSwarm(float DeltaSeconds);
	bool MoveTowardsPoint(const FVector& TargetLocation, const FVector& SurfaceNormal, float DeltaSeconds);
	void UpdateSurfaceAlignment(const FVector& DesiredDirection, const FVector& SurfaceNormal, float DeltaSeconds);
	float GetEffectiveMiningDurationSeconds() const;

	float MiningTimeRemaining = 0.0f;
	float TimeUntilNextDepositAttempt = 0.0f;
	FVector LastMovementDirection = FVector::ForwardVector;
};
