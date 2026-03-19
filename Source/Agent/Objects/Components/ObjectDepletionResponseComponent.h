// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ObjectDepletionResponseComponent.generated.h"

class AActor;
class AFactoryPayloadActor;
class UMaterialComponent;
class UMaterialDefinitionAsset;
class UMaterialInterface;
class UObjectHealthComponent;
class UParticleSystem;
class UPrimitiveComponent;

USTRUCT(BlueprintType)
struct FObjectDepletionBlueprintDropEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|BlueprintDrops")
	TSubclassOf<AActor> DropActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|BlueprintDrops")
	bool bUseRange = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|BlueprintDrops", meta=(ClampMin="0", UIMin="0", EditCondition="!bUseRange", EditConditionHides))
	int32 Amount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|BlueprintDrops", meta=(ClampMin="0", UIMin="0", EditCondition="bUseRange", EditConditionHides))
	int32 MinAmount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|BlueprintDrops", meta=(ClampMin="0", UIMin="0", EditCondition="bUseRange", EditConditionHides))
	int32 MaxAmount = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|BlueprintDrops", meta=(ClampMin="0.0", UIMin="0.0"))
	float SelectionWeight = 1.0f;

	bool IsUsable() const;
	int32 ResolveAmount(bool bAllowRangeRoll) const;
};

USTRUCT(BlueprintType)
struct FObjectDepletionRawDropClassOverride
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|RawDrops")
	FName MaterialId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|RawDrops")
	TSubclassOf<AActor> DropActorClass;

	bool IsUsable() const;
};

UCLASS(ClassGroup=(Objects), meta=(BlueprintSpawnableComponent))
class UObjectDepletionResponseComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UObjectDepletionResponseComponent();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category="Objects|Depletion")
	void TriggerDepletionResponses();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion")
	bool bResponseEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion")
	bool bHandleOnlyFirstDepletion = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|Lifecycle")
	bool bTakeOwnershipOfOwnerDepletionLifecycle = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|Lifecycle", meta=(EditCondition="bTakeOwnershipOfOwnerDepletionLifecycle", EditConditionHides))
	bool bDisableOwnerCollisionAfterResponses = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|Lifecycle", meta=(EditCondition="bTakeOwnershipOfOwnerDepletionLifecycle", EditConditionHides))
	bool bHideOwnerAfterResponses = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|Lifecycle", meta=(EditCondition="bTakeOwnershipOfOwnerDepletionLifecycle", EditConditionHides))
	bool bDestroyOwnerAfterResponses = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|Emitter")
	bool bSpawnEmitterOnDepleted = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|Emitter", meta=(EditCondition="bSpawnEmitterOnDepleted", EditConditionHides))
	TObjectPtr<UParticleSystem> DepletionEmitterTemplate = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|Emitter", meta=(EditCondition="bSpawnEmitterOnDepleted", EditConditionHides))
	FVector EmitterWorldOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|Emitter", meta=(EditCondition="bSpawnEmitterOnDepleted", EditConditionHides))
	bool bAlignEmitterToGroundNormal = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|Emitter", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bSpawnEmitterOnDepleted && bAlignEmitterToGroundNormal", EditConditionHides, Units="cm"))
	float EmitterGroundOffsetCm = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|Decal")
	bool bSpawnGroundDecalOnDepleted = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|Decal", meta=(EditCondition="bSpawnGroundDecalOnDepleted", EditConditionHides))
	TSubclassOf<AActor> GroundDecalActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|Decal", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bSpawnGroundDecalOnDepleted", EditConditionHides, Units="cm"))
	float GroundDecalNormalOffsetCm = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|Decal", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bSpawnGroundDecalOnDepleted", EditConditionHides, Units="deg"))
	float GroundDecalRandomYawDegrees = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|Decal", meta=(EditCondition="bSpawnGroundDecalOnDepleted", EditConditionHides))
	bool bRandomizeGroundDecalScaleOnSpawn = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|Decal", meta=(ClampMin="0.01", UIMin="0.01", EditCondition="bSpawnGroundDecalOnDepleted && bRandomizeGroundDecalScaleOnSpawn", EditConditionHides))
	float GroundDecalScaleMultiplierMin = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|Decal", meta=(ClampMin="0.01", UIMin="0.01", EditCondition="bSpawnGroundDecalOnDepleted && bRandomizeGroundDecalScaleOnSpawn", EditConditionHides))
	float GroundDecalScaleMultiplierMax = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|Decal", meta=(EditCondition="bSpawnGroundDecalOnDepleted", EditConditionHides, DisplayName="Ground Decal Material Variants"))
	TArray<TObjectPtr<UMaterialInterface>> GroundDecalMaterialVariants;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|GroundTrace", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float GroundTraceUpDistanceCm = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|GroundTrace", meta=(ClampMin="1.0", UIMin="1.0", Units="cm"))
	float GroundTraceDownDistanceCm = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|RawDrops")
	bool bDropRawMaterialsOnDepleted = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|RawDrops", meta=(EditCondition="bDropRawMaterialsOnDepleted", EditConditionHides))
	bool bApplyDamagePenaltyToRawDrops = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|RawDrops", meta=(ClampMin="1", UIMin="1", EditCondition="bDropRawMaterialsOnDepleted", EditConditionHides))
	int32 RawDropChunkUnits = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|RawDrops", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bDropRawMaterialsOnDepleted", EditConditionHides, Units="cm"))
	float RawDropScatterRadiusCm = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|RawDrops", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bDropRawMaterialsOnDepleted", EditConditionHides, Units="cm"))
	float RawDropHeightOffsetCm = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|RawDrops", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bDropRawMaterialsOnDepleted", EditConditionHides))
	float RawDropImpulseMin = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|RawDrops", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bDropRawMaterialsOnDepleted", EditConditionHides))
	float RawDropImpulseMax = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|RawDrops", meta=(ClampMin="1", UIMin="1", EditCondition="bDropRawMaterialsOnDepleted", EditConditionHides))
	int32 MaxRawDropActorSpawns = 128;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|RawDrops", meta=(EditCondition="bDropRawMaterialsOnDepleted", EditConditionHides))
	TArray<FObjectDepletionRawDropClassOverride> RawDropClassOverrides;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|RawDrops", meta=(EditCondition="bDropRawMaterialsOnDepleted", EditConditionHides))
	TSubclassOf<AFactoryPayloadActor> RawDropFallbackPayloadActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|BlueprintDrops")
	bool bSpawnBlueprintDropsOnDepleted = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|BlueprintDrops", meta=(EditCondition="bSpawnBlueprintDropsOnDepleted", EditConditionHides))
	TArray<FObjectDepletionBlueprintDropEntry> BlueprintDrops;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|BlueprintDrops", meta=(EditCondition="bSpawnBlueprintDropsOnDepleted", EditConditionHides))
	bool bRandomizeBlueprintDrops = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|BlueprintDrops", meta=(ClampMin="1", UIMin="1", EditCondition="bSpawnBlueprintDropsOnDepleted && bRandomizeBlueprintDrops", EditConditionHides))
	int32 ChosenBlueprintDropCountMin = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|BlueprintDrops", meta=(ClampMin="1", UIMin="1", EditCondition="bSpawnBlueprintDropsOnDepleted && bRandomizeBlueprintDrops", EditConditionHides))
	int32 ChosenBlueprintDropCountMax = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|BlueprintDrops", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bSpawnBlueprintDropsOnDepleted", EditConditionHides, Units="cm"))
	float BlueprintDropScatterRadiusCm = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|BlueprintDrops", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bSpawnBlueprintDropsOnDepleted", EditConditionHides, Units="cm"))
	float BlueprintDropHeightOffsetCm = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|BlueprintDrops", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bSpawnBlueprintDropsOnDepleted", EditConditionHides))
	float BlueprintDropImpulseMin = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|BlueprintDrops", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bSpawnBlueprintDropsOnDepleted", EditConditionHides))
	float BlueprintDropImpulseMax = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Depletion|BlueprintDrops", meta=(ClampMin="1", UIMin="1", EditCondition="bSpawnBlueprintDropsOnDepleted", EditConditionHides))
	int32 MaxBlueprintDropActorSpawns = 128;

protected:
	UFUNCTION()
	void HandleOwnerDepleted();

	void ExecuteDepletionResponses();
	void SpawnEmitterResponse(const FHitResult* GroundHit);
	void SpawnGroundDecalResponse(const FHitResult& GroundHit);
	void ApplyGroundDecalSpawnOverrides(AActor* SpawnedDecalActor) const;
	void SpawnRawMaterialDrops();
	void SpawnBlueprintDrops();
	void ApplyOwnerPostDepletionState();

	UObjectHealthComponent* ResolveHealthComponent() const;
	UMaterialComponent* ResolveMaterialComponent() const;
	UPrimitiveComponent* ResolveOwnerPrimitive() const;
	bool ResolveGroundHit(FHitResult& OutGroundHit) const;
	TSubclassOf<AActor> ResolveRawDropActorClass(FName ResourceId) const;
	const UMaterialDefinitionAsset* FindMaterialDefinitionById(FName ResourceId) const;

	AActor* SpawnActorAtWorldTransform(TSubclassOf<AActor> ActorClass, const FTransform& WorldTransform) const;
	FTransform BuildGroundAlignedTransform(const FHitResult& GroundHit, float OffsetAlongNormalCm, float RandomYawDegrees) const;
	FVector BuildRandomHorizontalOffset(float RadiusCm) const;
	void ApplyImpulseToActor(AActor* SpawnedActor, float ImpulseMin, float ImpulseMax) const;

	UPROPERTY(Transient)
	bool bHasHandledDepletion = false;

	UPROPERTY(Transient)
	TObjectPtr<UObjectHealthComponent> CachedHealthComponent = nullptr;
};
