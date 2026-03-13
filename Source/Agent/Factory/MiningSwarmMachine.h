// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MiningSwarmMachine.generated.h"

class AMaterialNodeActor;
class AMiningBotActor;
class UArrowComponent;
class UBoxComponent;
class UMachineOutputVolumeComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class AMiningSwarmMachine : public AActor
{
	GENERATED_BODY()

public:
	AMiningSwarmMachine();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category="Factory|MiningSwarm")
	void RefreshNearbyResourceNodes();

	UFUNCTION(BlueprintPure, Category="Factory|MiningSwarm")
	int32 GetNearbyNodeCount() const;

	UFUNCTION(BlueprintPure, Category="Factory|MiningSwarm")
	int32 GetActiveBotCount() const;

	UFUNCTION(BlueprintPure, Category="Factory|MiningSwarm")
	bool IsSwarmEnabled() const { return bEnabled; }

	UFUNCTION(BlueprintCallable, Category="Factory|MiningSwarm")
	bool RequestMiningAssignment(
		AMiningBotActor* RequestingBot,
		AMaterialNodeActor*& OutTargetNode,
		FVector& OutTargetLocation,
		FVector& OutTargetSurfaceNormal);

	UFUNCTION(BlueprintCallable, Category="Factory|MiningSwarm")
	bool QueueBotResources(
		const TMap<FName, int32>& ResourcesScaled,
		TMap<FName, int32>& OutRejectedResourcesScaled,
		int32& OutQueuedScaled);

	UFUNCTION(BlueprintPure, Category="Factory|MiningSwarm")
	FVector GetBotHomeLocation(const AMiningBotActor* Bot) const;

	UFUNCTION(BlueprintPure, Category="Factory|MiningSwarm")
	FVector GetBotHomeNormal() const;

	UFUNCTION(BlueprintImplementableEvent, Category="Factory|MiningSwarm")
	void OnMiningBotSpawned(AMiningBotActor* SpawnedBot);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningSwarm")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningSwarm")
	TObjectPtr<UBoxComponent> SupportCollision = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningSwarm")
	TObjectPtr<UStaticMeshComponent> MachineMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningSwarm")
	TObjectPtr<USceneComponent> BotSpawnRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningSwarm")
	TObjectPtr<UMachineOutputVolumeComponent> OutputVolume = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningSwarm")
	TObjectPtr<UArrowComponent> OutputArrow = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm")
	TSubclassOf<AMiningBotActor> MiningBotClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm", meta=(ClampMin="1", UIMin="1"))
	int32 MaxBotCount = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm", meta=(ClampMin="0.0", UIMin="0.0"))
	float ResourceSearchRadiusCm = 10000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm", meta=(ClampMin="0.05", UIMin="0.05"))
	float NodeRefreshIntervalSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm", meta=(ClampMin="0.05", UIMin="0.05"))
	float BotMaintenanceIntervalSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm", meta=(ClampMin="1", UIMin="1"))
	int32 SurfaceSampleAttemptsPerNode = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm", meta=(ClampMin="0.0", UIMin="0.0"))
	float SurfaceSampleTracePaddingCm = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm", meta=(ClampMin="0.0", UIMin="0.0"))
	float SurfaceSampleOffsetCm = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm", meta=(ClampMin="0.0", UIMin="0.0"))
	float BotHomeRingRadiusCm = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Bots", meta=(ClampMin="1", UIMin="1"))
	int32 DefaultBotMiningCapacityUnits = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Bots", meta=(ClampMin="1.0", UIMin="1.0"))
	float DefaultBotTravelSpeedCmPerSecond = 450.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Bots", meta=(ClampMin="0.1", UIMin="0.1"))
	float DefaultBotMiningSpeedMultiplier = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningSwarm")
	TArray<TObjectPtr<AMiningBotActor>> SpawnedBots;

protected:
	bool IsNodeWithinSearchRange(const AMaterialNodeActor* Node) const;
	bool IsNodeViableForMining(const AMaterialNodeActor* Node) const;
	void RemoveInvalidBotReferences();
	void EnsureBotPopulation();
	bool SampleRandomSurfacePointOnNode(AMaterialNodeActor* Node, FVector& OutTargetLocation, FVector& OutTargetSurfaceNormal) const;
	AMaterialNodeActor* ResolveMaterialNodeFromActor(AActor* CandidateActor) const;

private:
	TArray<TWeakObjectPtr<AMaterialNodeActor>> NearbyResourceNodes;
	float TimeUntilNodeRefresh = 0.0f;
	float TimeUntilBotMaintenance = 0.0f;
};
