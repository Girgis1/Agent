// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Material/AgentResourceTypes.h"
#include "MaterialComponent.generated.h"

class UPrimitiveComponent;
class UMaterialDefinitionAsset;
class UWorld;

USTRUCT(BlueprintType)
struct FMaterialEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Materials")
	TObjectPtr<UMaterialDefinitionAsset> Material = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Materials")
	bool bUseRange = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Materials", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="!bUseRange", EditConditionHides))
	float Units = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Materials", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bUseRange", EditConditionHides))
	float MinUnits = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Materials", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bUseRange", EditConditionHides))
	float MaxUnits = 1.0f;

	bool IsUsable() const;
	float ResolveUnits(bool bAllowRangeRoll) const;
	float GetMaxPossibleUnits() const;
};

UCLASS(ClassGroup=(Factory), meta=(BlueprintSpawnableComponent))
class UMaterialComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMaterialComponent();

	UFUNCTION(BlueprintPure, Category="Factory|Materials")
	bool HasDefinedResources() const;

	UFUNCTION(BlueprintPure, Category="Factory|Materials")
	bool HasMaterialEntries() const;

	UFUNCTION(BlueprintPure, Category="Factory|Materials")
	float ResolveEffectiveMassKg(const UPrimitiveComponent* SourcePrimitive) const;

	UFUNCTION(BlueprintPure, Category="Factory|Materials")
	float GetResolvedMaterialWeightKg(UPrimitiveComponent* SourcePrimitive);

	UFUNCTION(BlueprintPure, Category="Factory|Materials")
	float GetResolvedGlobalMassMultiplier() const;

	UFUNCTION(BlueprintCallable, Category="Factory|Materials")
	void InitializeRuntimeResourceState(UPrimitiveComponent* SourcePrimitive);

	UFUNCTION(BlueprintCallable, Category="Factory|Materials")
	bool BuildResolvedResourceQuantitiesScaled(UPrimitiveComponent* SourcePrimitive, TMap<FName, int32>& OutQuantitiesScaled);

	UFUNCTION(BlueprintCallable, Category="Factory|Materials")
	bool ConfigureResourcesById(const TMap<FName, int32>& ResourceQuantitiesScaled);

	UFUNCTION(BlueprintCallable, Category="Factory|Materials")
	bool ConfigureSingleResourceById(FName ResourceId, int32 QuantityScaled);

	UFUNCTION(BlueprintCallable, Category="Factory|Materials")
	void ClearConfiguredResources();

	UFUNCTION(BlueprintCallable, Category="Factory|Materials")
	float CalculateAndApplyFinalMassKg(UPrimitiveComponent* SourcePrimitive);

	UFUNCTION(BlueprintCallable, Category="Factory|Materials")
	void ResetGeneratedContents();

	UFUNCTION(BlueprintCallable, Category="Factory|Materials")
	void SetExplicitBaseMassKgOverride(float NewBaseMassKg);

	UFUNCTION(BlueprintCallable, Category="Factory|Materials")
	void ClearExplicitBaseMassKgOverride();

	UFUNCTION(BlueprintPure, Category="Factory|Materials")
	bool HasExplicitBaseMassKgOverride() const { return bUseExplicitBaseMassKgOverride; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Materials")
	TArray<FMaterialEntry> Materials;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Materials|Random")
	bool bUseRandomizedContents = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Materials|Random", meta=(ClampMin="1", UIMin="1", EditCondition="bUseRandomizedContents"))
	int32 ChosenMaterialCountMin = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Materials|Random", meta=(ClampMin="1", UIMin="1", EditCondition="bUseRandomizedContents"))
	int32 ChosenMaterialCountMax = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Materials|Random", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bUseRandomizedContents"))
	float TotalUnitsMin = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Materials|Random", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bUseRandomizedContents"))
	float TotalUnitsMax = 0.0f;

protected:
	virtual void BeginPlay() override;

	void ResolveGeneratedContents(UPrimitiveComponent* SourcePrimitive);
	void BuildFixedContents(UPrimitiveComponent* SourcePrimitive);
	void BuildRandomizedContents(UPrimitiveComponent* SourcePrimitive);
	void AppendResolvedResource(const UMaterialDefinitionAsset* ResourceAsset, float QuantityUnits);
	float ResolveLinearScaleFactor(const UPrimitiveComponent* SourcePrimitive) const;
	float ResolveBaseMassKgForFormula(UPrimitiveComponent* SourcePrimitive);
	float ResolveGlobalMassMultiplier(const UWorld* World) const;

	UPROPERTY(Transient)
	bool bGeneratedContentsResolved = false;

	UPROPERTY(Transient)
	TArray<FResourceAmount> GeneratedResourceAmounts;

	UPROPERTY(Transient)
	float CachedTotalMaterialWeightKg = 0.0f;

	UPROPERTY(Transient)
	bool bBaseMassResolved = false;

	UPROPERTY(Transient)
	float CachedBaseMassKg = 0.0f;

	UPROPERTY(Transient)
	bool bUseExplicitBaseMassKgOverride = false;

	UPROPERTY(Transient)
	float ExplicitBaseMassKgOverride = 0.0f;
};

