// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Factory/ResourceTypes.h"
#include "ResourceComponent.generated.h"

class UPrimitiveComponent;
class UResourceCompositionAsset;
class UResourceDefinitionAsset;
class UWorld;

UENUM(BlueprintType)
enum class EResourceScaleUnitsMode : uint8
{
	None UMETA(DisplayName="None"),
	Linear UMETA(DisplayName="Linear"),
	Volume UMETA(DisplayName="Volume")
};

USTRUCT(BlueprintType)
struct FResourceMaterialEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Resource")
	TObjectPtr<UResourceDefinitionAsset> Resource = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Resource")
	bool bUseRange = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Resource", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="!bUseRange", EditConditionHides))
	float Units = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Resource", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bUseRange", EditConditionHides))
	float MinUnits = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Resource", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bUseRange", EditConditionHides))
	float MaxUnits = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Resource", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bUseRange"))
	float PickWeight = 1.0f;

	bool IsUsable() const;
	float ResolveUnits(bool bAllowRangeRoll) const;
	FName GetResourceId() const;
	float GetPickWeight() const;
	float GetMaxPossibleUnits() const;
};

UCLASS(ClassGroup=(Factory), meta=(BlueprintSpawnableComponent))
class UResourceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UResourceComponent();

	UFUNCTION(BlueprintPure, Category="Factory|Resource")
	bool HasDefinedResources() const;

	UFUNCTION(BlueprintPure, Category="Factory|Resource")
	bool HasMaterialEntries() const;

	UFUNCTION(BlueprintPure, Category="Factory|Resource")
	float ResolveEffectiveMassKg(const UPrimitiveComponent* SourcePrimitive) const;

	UFUNCTION(BlueprintCallable, Category="Factory|Resource")
	void InitializeRuntimeResourceState(UPrimitiveComponent* SourcePrimitive);

	UFUNCTION(BlueprintCallable, Category="Factory|Resource")
	bool BuildResolvedResourceQuantitiesScaled(UPrimitiveComponent* SourcePrimitive, TMap<FName, int32>& OutQuantitiesScaled);

	UFUNCTION(BlueprintCallable, Category="Factory|Resource")
	float CalculateAndApplyFinalMassKg(UPrimitiveComponent* SourcePrimitive);

	UFUNCTION(BlueprintCallable, Category="Factory|Resource")
	void ResetGeneratedContents();

	// Legacy simple path retained for backward compatibility.
	UFUNCTION(BlueprintPure, Category="Factory|Resource|Legacy")
	bool HasSimpleResourceDefinition() const;

	UFUNCTION(BlueprintPure, Category="Factory|Resource|Legacy")
	FName GetPrimaryResourceId() const;

	UFUNCTION(BlueprintPure, Category="Factory|Resource|Legacy")
	int32 ResolveSimpleResourceQuantityScaled(const UPrimitiveComponent* SourcePrimitive) const;

	UFUNCTION(BlueprintPure, Category="Factory|Resource")
	float ResolveRecoveryScalar() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Resource")
	TArray<FResourceMaterialEntry> Materials;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Resource|Random")
	bool bUseRandomizedContents = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Resource|Random", meta=(ClampMin="1", UIMin="1", EditCondition="bUseRandomizedContents"))
	int32 ChosenMaterialCountMin = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Resource|Random", meta=(ClampMin="1", UIMin="1", EditCondition="bUseRandomizedContents"))
	int32 ChosenMaterialCountMax = 3;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Resource|Random", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bUseRandomizedContents"))
	float TotalUnitsMin = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Resource|Random", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bUseRandomizedContents"))
	float TotalUnitsMax = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Resource")
	EResourceScaleUnitsMode ScaleUnitsMode = EResourceScaleUnitsMode::Linear;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Resource")
	bool bUsePhysicsMass = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Resource", meta=(ClampMin="0.0", UIMin="0.0"))
	float MassOverrideKg = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Resource", meta=(ClampMin="0.0", UIMin="0.0"))
	float MassMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Resource", meta=(ClampMin="0.0", UIMin="0.0"))
	float RecoveryMultiplier = 1.0f;

	// Legacy composition path retained for backward compatibility only.
	// Hidden from normal authoring to keep the new materials workflow clear.
	UPROPERTY()
	TObjectPtr<UResourceCompositionAsset> Composition = nullptr;

	// Legacy simple path retained for backward compatibility only.
	// Hidden from normal authoring to keep the new materials workflow clear.
	UPROPERTY()
	TObjectPtr<UResourceDefinitionAsset> PrimaryResource = nullptr;

	UPROPERTY()
	float PrimaryResourceUnitsPerKg = 1.0f;

protected:
	virtual void BeginPlay() override;

	void ResolveGeneratedContents(UPrimitiveComponent* SourcePrimitive);
	void BuildFixedContents(UPrimitiveComponent* SourcePrimitive);
	void BuildRandomizedContents(UPrimitiveComponent* SourcePrimitive);
	void AppendResolvedResource(const UResourceDefinitionAsset* ResourceAsset, float QuantityUnits);
	float ResolveScaleUnitsFactor(const UPrimitiveComponent* SourcePrimitive) const;
	float ResolveBaseMassKgForFormula(const UPrimitiveComponent* SourcePrimitive) const;
	float ResolveGlobalMassMultiplier(const UWorld* World) const;
	int32 PickWeightedEntryIndex(const TArray<int32>& CandidateIndices) const;

	UPROPERTY(Transient)
	bool bGeneratedContentsResolved = false;

	UPROPERTY(Transient)
	TArray<FResourceAmount> GeneratedResourceAmounts;

	UPROPERTY(Transient)
	float CachedTotalMaterialWeightKg = 0.0f;
};
