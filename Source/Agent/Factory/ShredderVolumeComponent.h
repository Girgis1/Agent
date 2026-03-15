// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factory/FactoryVolumeComponentBase.h"
#include "Material/AgentResourceTypes.h"
#include "ShredderVolumeComponent.generated.h"

class UMachineOutputVolumeComponent;
class UPrimitiveComponent;
class UMaterialComponent;

UCLASS(ClassGroup=(Factory), meta=(BlueprintSpawnableComponent))
class UShredderVolumeComponent : public UFactoryVolumeComponentBase
{
	GENERATED_BODY()

public:
	UShredderVolumeComponent();
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category="Factory|Shredder")
	float GetBufferedUnits(FName ResourceId) const;

protected:
	virtual bool TryProcessOverlappingActor(AActor* OverlappingActor) override;
	virtual int32 GetCurrentStoredQuantityScaled() const override;

	bool TryBuildRecoveredResources(const AActor* SourceActor, UMaterialComponent* ResourceData, UPrimitiveComponent* SourcePrimitive, TArray<FResourceAmount>& OutRecoveredResources) const;
	void CommitRecoveredResources(const TArray<FResourceAmount>& RecoveredResources);
	void FlushBufferedResourcesToOutput();
	UMachineOutputVolumeComponent* FindOutputVolume() const;
	bool IsActorEligibleForShredding(const AActor* OverlappingActor, const UPrimitiveComponent* SourcePrimitive) const;
	bool IsActorReadyForShredding(AActor* OverlappingActor);
	bool ShouldForceCleanupActor(AActor* OverlappingActor);
	void ClearTrackedActorState(const TWeakObjectPtr<AActor>& ActorKey);
	void CleanupTrackedActors();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Shredder", meta=(ClampMin="0.0", UIMin="0.0"))
	float BaseRecoveryScalar = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Shredder|Processing")
	bool bUseShredDelay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Shredder|Processing", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bUseShredDelay"))
	float ShredDelayMinSeconds = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Shredder|Processing", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bUseShredDelay"))
	float ShredDelayMaxSeconds = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Shredder|Processing")
	bool bOnlyShredMovableActors = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Shredder|Processing")
	bool bIgnorePawns = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Shredder|Processing")
	bool bRequireHealthDepletionBeforeShredding = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Shredder|Processing")
	bool bConsumeOverlapsImmediately = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Shredder|Processing|Pawn")
	bool bRagdollAgentCharactersOnOverlap = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Shredder|Processing|Pawn")
	bool bConsumeRagdolledAgentCharacters = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Shredder|Processing|Pawn")
	bool bUnpossessPawnsBeforeDestroy = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Shredder|Processing")
	bool bForceCleanupActorsStuckInVolume = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Shredder|Processing", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bForceCleanupActorsStuckInVolume", Units="s"))
	float ForceCleanupDelaySeconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Shredder")
	bool bDestroyWithoutResourceComponent = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Shredder")
	bool bDestroyIfNoRecoverableResources = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Shredder")
	TMap<FName, int32> InternalResourceBufferScaled;

protected:
	TMap<TWeakObjectPtr<AActor>, float> PendingReadyTimeByActor;
	TMap<TWeakObjectPtr<AActor>, float> ForceCleanupReadyTimeByActor;
};

