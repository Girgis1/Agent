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
	bool IsSwarmEnabled() const;

	UFUNCTION(BlueprintPure, Category="Factory|MiningSwarm")
	bool IsWarmingUp() const { return bEnabled && !bSwarmOperational && WarmupTimeRemainingSeconds > 0.0f; }

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

	UFUNCTION(BlueprintCallable, Category="Factory|MiningSwarm")
	bool HandleBotDocking(
		AMiningBotActor* Bot,
		const TMap<FName, int32>& ResourcesScaled,
		int32& OutQueuedScaled,
		int32& OutBufferedScaled);

	UFUNCTION(BlueprintPure, Category="Factory|MiningSwarm")
	FVector GetBotHomeLocation(const AMiningBotActor* Bot) const;

	UFUNCTION(BlueprintPure, Category="Factory|MiningSwarm")
	FVector GetBotHomeNormal() const;

	UFUNCTION(BlueprintPure, Category="Factory|MiningSwarm")
	bool IsBotInsideDockVolume(
		const AMiningBotActor* Bot,
		float ExtraPlanarToleranceCm = 0.0f,
		float ExtraVerticalToleranceCm = 0.0f) const;

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
	TObjectPtr<UBoxComponent> BotDockVolume = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningSwarm")
	TObjectPtr<UMachineOutputVolumeComponent> OutputVolume = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningSwarm")
	TObjectPtr<UArrowComponent> OutputArrow = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Power")
	bool bUseActivationWarmup = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Power", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bUseActivationWarmup"))
	float ActivationWarmupDurationSeconds = 3.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningSwarm|Power")
	float WarmupTimeRemainingSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm")
	bool bAutoSpawnBots = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Bots")
	bool bDespawnBotsOnDock = true;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Bots", meta=(ClampMin="1.0", ClampMax="100.0", UIMin="1.0", UIMax="100.0"))
	float DefaultBotTravelSpeedCmPerSecond = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Bots", meta=(ClampMin="0.1", UIMin="0.1"))
	float DefaultBotMiningSpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Bots", meta=(ClampMin="0.05", UIMin="0.05"))
	float DefaultBotMiningDurationSeconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Bots", meta=(ClampMin="0.05", UIMin="0.05"))
	float DefaultBotSurfaceAlignmentInterpSpeed = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Bots", meta=(ClampMin="0.05", UIMin="0.05"))
	float DefaultBotGroundSnapInterpSpeed = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Bots", meta=(ClampMin="0.05", UIMin="0.05"))
	float DefaultBotSupportNormalSmoothingSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Bots", meta=(ClampMin="1.0", UIMin="1.0"))
	float DefaultBotMaxTraversalStepPerTickCm = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Bots", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float DefaultBotObstacleSlideScale = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Bots", meta=(ClampMin="0.0", UIMin="0.0"))
	float DefaultBotObstacleContactPadding = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Bots", meta=(ClampMin="1.0", UIMin="1.0"))
	float DefaultBotArrivalToleranceCm = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Bots", meta=(ClampMin="0.0", UIMin="0.0"))
	float DefaultBotSurfaceHoverOffsetCm = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Bots|Visual")
	bool bDefaultBotVisualLagEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Bots|Visual", meta=(ClampMin="0.05", UIMin="0.05", EditCondition="bDefaultBotVisualLagEnabled"))
	float DefaultBotVisualLagLocationInterpSpeed = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Bots|Visual", meta=(ClampMin="0.05", UIMin="0.05", EditCondition="bDefaultBotVisualLagEnabled"))
	float DefaultBotVisualLagRotationInterpSpeed = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Bots|Visual", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bDefaultBotVisualLagEnabled"))
	float DefaultBotVisualLagMaxDistanceCm = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Bots", meta=(ClampMin="0.05", UIMin="0.05"))
	float DefaultBotDepositRetryIntervalSeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Bots", meta=(ClampMin="0.0", UIMin="0.0"))
	float DefaultBotSurfaceProbeStartOffsetCm = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Bots", meta=(ClampMin="25.0", UIMin="25.0"))
	float DefaultBotSurfaceProbeDepthCm = 240.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Bots", meta=(ClampMin="0.0", UIMin="0.0"))
	float DefaultBotSurfaceProbeForwardLookCm = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Bots", meta=(ClampMin="0.0", UIMin="0.0"))
	float DefaultBotMaxStepUpHeightCm = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Bots", meta=(ClampMin="0.1", ClampMax="1.0", UIMin="0.1", UIMax="1.0"))
	float BotDockAreaFill = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Bots", meta=(ClampMin="0.0", UIMin="0.0"))
	float BotDockVerticalAcceptanceCm = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Bots", meta=(ClampMin="0.0", UIMin="0.0"))
	float InitialBotSpawnDelaySeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Bots", meta=(ClampMin="0.01", UIMin="0.01"))
	float BotSpawnIntervalSeconds = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningSwarm|Bots", meta=(ClampMin="1", UIMin="1"))
	int32 BotsPerSpawnBatch = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningSwarm")
	TArray<TObjectPtr<AMiningBotActor>> SpawnedBots;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningSwarm|Output")
	TMap<FName, int32> BufferedBotResourcesScaled;

protected:
	void StartActivationWarmup();
	void SetSwarmOperational(bool bNewOperational);
	void UpdateActivationState(float DeltaSeconds);
	void FlushBufferedBotResourcesToOutput();
	bool IsNodeWithinSearchRange(const AMaterialNodeActor* Node) const;
	bool IsNodeViableForMining(const AMaterialNodeActor* Node) const;
	bool IsPlacementPreviewInstance() const;
	int32 ResolveBotIndex(const AMiningBotActor* Bot) const;
	FVector ResolveBotDockPoint(int32 BotIndex) const;
	bool TrySpawnSingleBot();
	void RemoveInvalidBotReferences();
	void EnsureBotPopulation();
	bool SampleRandomSurfacePointOnNode(AMaterialNodeActor* Node, FVector& OutTargetLocation, FVector& OutTargetSurfaceNormal) const;
	AMaterialNodeActor* ResolveMaterialNodeFromActor(AActor* CandidateActor) const;

private:
	TArray<TWeakObjectPtr<AMaterialNodeActor>> NearbyResourceNodes;
	float TimeUntilNodeRefresh = 0.0f;
	float TimeUntilBotMaintenance = 0.0f;
	float TimeUntilNextBotSpawn = 0.0f;
	bool bSwarmOperational = false;
	bool bLastEnabledState = true;
};
