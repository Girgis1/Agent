// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MinerActor.generated.h"

class AMaterialNodeActor;
class UMachineOutputVolumeComponent;
class USceneComponent;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EMaterialMinerRuntimeState : uint8
{
	Paused UMETA(DisplayName="Paused"),
	MissingNode UMETA(DisplayName="Missing Node"),
	NodeDepleted UMETA(DisplayName="Node Depleted"),
	OutputBlocked UMETA(DisplayName="Output Blocked"),
	Mining UMETA(DisplayName="Mining")
};

UCLASS()
class AMinerActor : public AActor
{
	GENERATED_BODY()

public:
	AMinerActor();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category="Factory|Miner")
	void SetTargetNode(AMaterialNodeActor* NewTargetNode);

	UFUNCTION(BlueprintPure, Category="Factory|Miner")
	AMaterialNodeActor* GetTargetNode() const { return TargetNode; }

	UFUNCTION(BlueprintCallable, Category="Factory|Miner")
	bool TryMineNow();

	UFUNCTION(BlueprintPure, Category="Factory|Miner")
	EMaterialMinerRuntimeState GetRuntimeState() const { return RuntimeState; }

	UFUNCTION(BlueprintPure, Category="Factory|Miner")
	FText GetRuntimeStatus() const { return RuntimeStatus; }

	UFUNCTION(BlueprintImplementableEvent, Category="Factory|Miner")
	void OnMinerStateChanged(EMaterialMinerRuntimeState NewState, const FText& NewStatus);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Miner")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Miner")
	TObjectPtr<UStaticMeshComponent> MinerMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Miner")
	TObjectPtr<UMachineOutputVolumeComponent> OutputVolume = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Miner")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Miner")
	TObjectPtr<AMaterialNodeActor> TargetNode = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Miner")
	bool bAttachToTargetNode = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Miner", meta=(ClampMin="0.01", UIMin="0.01"))
	float MiningIntervalSeconds = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Miner", meta=(ClampMin="1", UIMin="1"))
	int32 ExtractionUnitsPerCycle = 1;

protected:
	int32 ResolveOutputRequestBudgetScaled(int32 DesiredScaled) const;
	void SetRuntimeState(EMaterialMinerRuntimeState NewState, const TCHAR* NewStatusMessage);
	void SetRuntimeStateFormatted(EMaterialMinerRuntimeState NewState, const FString& NewStatusMessage);
	bool QueueExtractedResources(const TMap<FName, int32>& ExtractedResourcesScaled, TMap<FName, int32>& OutRejectedResourcesScaled, int32& OutQueuedScaled) const;

	UPROPERTY(Transient)
	float TimeUntilNextMine = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Miner")
	EMaterialMinerRuntimeState RuntimeState = EMaterialMinerRuntimeState::MissingNode;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Miner")
	FText RuntimeStatus;
};
