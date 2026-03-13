// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Material/AgentResourceTypes.h"
#include "MaterialNodeActor.generated.h"

class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EMaterialNodeDepletionVisualMode : uint8
{
	None UMETA(DisplayName="None"),
	Scale UMETA(DisplayName="Scale"),
	StagedMeshes UMETA(DisplayName="Staged Meshes"),
	BlueprintDriven UMETA(DisplayName="Blueprint Driven")
};

USTRUCT(BlueprintType)
struct FMaterialNodeEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialNode|Output")
	TSoftClassPtr<AActor> BlueprintClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialNode|Output", meta=(ClampMin="0.0", UIMin="0.0"))
	float Chance = 1.0f;

	float ResolveChance() const;
};

USTRUCT(BlueprintType)
struct FMaterialNodeResourcePreview
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|MaterialNode")
	FName ResourceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|MaterialNode", meta=(ClampMin="0"))
	int32 InitialQuantityScaled = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|MaterialNode", meta=(ClampMin="0"))
	int32 RemainingQuantityScaled = 0;
};

UCLASS()
class AMaterialNodeActor : public AActor
{
	GENERATED_BODY()

public:
	AMaterialNodeActor();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category="Factory|MaterialNode")
	void InitializeNodeState();

	UFUNCTION(BlueprintPure, Category="Factory|MaterialNode")
	bool IsNodeInitialized() const { return bNodeInitialized; }

	UFUNCTION(BlueprintPure, Category="Factory|MaterialNode")
	bool IsInfiniteNode() const { return bInfiniteNode; }

	UFUNCTION(BlueprintPure, Category="Factory|MaterialNode")
	bool IsDepleted() const;

	UFUNCTION(BlueprintPure, Category="Factory|MaterialNode")
	float GetRemainingRatio01() const;

	UFUNCTION(BlueprintPure, Category="Factory|MaterialNode")
	float GetRemainingUnits() const;

	UFUNCTION(BlueprintPure, Category="Factory|MaterialNode")
	int32 GetInitialScaledTotal() const { return InitialTotalQuantityScaled; }

	UFUNCTION(BlueprintPure, Category="Factory|MaterialNode")
	int32 GetRemainingScaledTotal() const { return RemainingTotalQuantityScaled; }

	UFUNCTION(BlueprintCallable, Category="Factory|MaterialNode")
	bool ExtractResourcesScaled(int32 MaxRequestScaled, TMap<FName, int32>& OutExtractedResourcesScaled);

	UFUNCTION(BlueprintCallable, Category="Factory|MaterialNode")
	void AddResourcesBack(const TMap<FName, int32>& ReturnedResourcesScaled);

	UFUNCTION(BlueprintPure, Category="Factory|MaterialNode")
	void GetResourcePreview(TArray<FMaterialNodeResourcePreview>& OutPreview) const;

	UFUNCTION(BlueprintCallable, Category="Factory|MaterialNode")
	void RefreshDepletionVisuals();

	UFUNCTION(BlueprintImplementableEvent, Category="Factory|MaterialNode")
	void OnNodeVisualsUpdated(float RemainingRatio01, float QuantizedRemainingRatio01, float FinalScaleMultiplier);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MaterialNode")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialNode|Output")
	TArray<FMaterialNodeEntry> MaterialEntries;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialNode|Capacity", meta=(ClampMin="0.0", UIMin="0.0"))
	float CapacityMinUnits = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialNode|Capacity", meta=(ClampMin="0.0", UIMin="0.0"))
	float CapacityMaxUnits = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialNode")
	bool bInfiniteNode = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialNode|Scale")
	bool bScaleWithTotalMaterialQty = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialNode|Scale", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bScaleWithTotalMaterialQty"))
	float QuantityScaleReferenceMinUnits = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialNode|Scale", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bScaleWithTotalMaterialQty"))
	float QuantityScaleReferenceMaxUnits = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialNode|Scale", meta=(ClampMin="0.05", UIMin="0.05", EditCondition="bScaleWithTotalMaterialQty"))
	float QuantityScaleAtMinUnits = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialNode|Scale", meta=(ClampMin="0.05", UIMin="0.05", EditCondition="bScaleWithTotalMaterialQty"))
	float QuantityScaleAtMaxUnits = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialNode|Depletion")
	EMaterialNodeDepletionVisualMode DepletionVisualMode = EMaterialNodeDepletionVisualMode::Scale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialNode|Depletion|Scale", meta=(EditCondition="DepletionVisualMode==EMaterialNodeDepletionVisualMode::Scale"))
	bool bQuantizeDepletionScale = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialNode|Depletion|Scale", meta=(ClampMin="0.1", ClampMax="100.0", UIMin="0.1", UIMax="100.0", EditCondition="DepletionVisualMode==EMaterialNodeDepletionVisualMode::Scale&&bQuantizeDepletionScale"))
	float DepletionScaleStepPercent = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialNode|Depletion|Scale", meta=(ClampMin="0.01", ClampMax="1.0", UIMin="0.01", UIMax="1.0", EditCondition="DepletionVisualMode==EMaterialNodeDepletionVisualMode::Scale"))
	float MinDepletionScale = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialNode|Depletion|Stages", meta=(EditCondition="DepletionVisualMode==EMaterialNodeDepletionVisualMode::StagedMeshes"))
	TObjectPtr<UStaticMeshComponent> StageMeshComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialNode|Depletion|Stages", meta=(EditCondition="DepletionVisualMode==EMaterialNodeDepletionVisualMode::StagedMeshes"))
	TArray<TObjectPtr<UStaticMesh>> DepletionStageMeshes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MaterialNode")
	int32 InitialTotalQuantityScaled = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MaterialNode")
	int32 RemainingTotalQuantityScaled = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MaterialNode")
	float QuantityScaleMultiplier = 1.0f;

protected:
	struct FResolvedNodeEntry
	{
		FName ResourceId = NAME_None;
		int32 InitialQuantityScaled = 0;
		int32 RemainingQuantityScaled = 0;
		float SelectionChance = 1.0f;
	};

	FName ResolveResourceIdFromEntry(const FMaterialNodeEntry& Entry) const;
	FName ResolveResourceIdFromClass(UClass* EntryClass) const;
	int32 ResolveRolledCapacityScaled() const;
	float ResolveQuantityScaleMultiplier() const;
	float ResolveQuantizedRemainingRatio(float RemainingRatio01) const;
	void UpdateStageMeshFromRatio(float QuantizedRemainingRatio01);
	void SetNodeScaleFromMultiplier(float FinalScaleMultiplier);

	TArray<FResolvedNodeEntry> ResolvedEntries;

	UPROPERTY(Transient)
	bool bNodeInitialized = false;

	UPROPERTY(Transient)
	FVector InitialActorScale = FVector::OneVector;
};
