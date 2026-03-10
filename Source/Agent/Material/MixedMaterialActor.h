// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MixedMaterialActor.generated.h"

class UMaterialComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class EMixedMaterialVisualPattern : uint8
{
	WeightedColor UMETA(DisplayName="Weighted Color"),
	Voronoi UMETA(DisplayName="Voronoi"),
	LinearGradient UMETA(DisplayName="Linear Gradient")
};

UCLASS()
class AMixedMaterialActor : public AActor
{
	GENERATED_BODY()

public:
	AMixedMaterialActor();

	UFUNCTION(BlueprintCallable, Category="Factory|MixedMaterial")
	bool ConfigureMixedMaterialPayload(
		const TMap<FName, int32>& InCompositionScaled,
		float InMinUnitsForScale = 1.0f,
		float InMaxUnitsForScale = 10.0f,
		float InMinVisualScale = 0.5f,
		float InMaxVisualScale = 1.5f);

	UFUNCTION(BlueprintPure, Category="Factory|MixedMaterial")
	float GetTotalMaterialUnits() const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MixedMaterial")
	TObjectPtr<UStaticMeshComponent> ItemMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MixedMaterial")
	TObjectPtr<UMaterialComponent> MaterialData = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	TSoftObjectPtr<UMaterialInterface> MixedVisualMaterialOverride;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	EMixedMaterialVisualPattern VisualPattern = EMixedMaterialVisualPattern::Voronoi;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual", meta=(ClampMin="0.0", UIMin="0.0"))
	float PatternSeedOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual", meta=(ClampMin="0.01", UIMin="0.01"))
	float VoronoiScale = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual", meta=(ClampMin="0.0", UIMin="0.0"))
	float VoronoiEdgeWidth = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	float GradientAngleDegrees = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual", meta=(ClampMin="0.01", UIMin="0.01"))
	float GradientSharpness = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	bool bApplyWeightedColorMixToMaterial = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	bool bPushSourceMaterialTextures = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName TintColorParameterName = TEXT("TintColor");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName BaseColorParameterName = TEXT("BaseColor");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName MixModeParameterName = TEXT("MixMode");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName MixLayerCountParameterName = TEXT("MixLayerCount");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName MixSeedParameterName = TEXT("MixSeed");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName VoronoiScaleParameterName = TEXT("VoronoiScale");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName VoronoiEdgeWidthParameterName = TEXT("VoronoiEdgeWidth");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName GradientDirectionParameterName = TEXT("GradientDirection");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName GradientSharpnessParameterName = TEXT("GradientSharpness");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName MixColorAParameterName = TEXT("MixColorA");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName MixColorBParameterName = TEXT("MixColorB");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName MixColorCParameterName = TEXT("MixColorC");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName MixColorDParameterName = TEXT("MixColorD");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName MixWeightAParameterName = TEXT("MixWeightA");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName MixWeightBParameterName = TEXT("MixWeightB");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName MixWeightCParameterName = TEXT("MixWeightC");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName MixWeightDParameterName = TEXT("MixWeightD");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName MixColorIdAParameterName = TEXT("MixColorIdA");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName MixColorIdBParameterName = TEXT("MixColorIdB");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName MixColorIdCParameterName = TEXT("MixColorIdC");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName MixColorIdDParameterName = TEXT("MixColorIdD");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName MixBaseTextureAParameterName = TEXT("MixBaseTexA");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName MixBaseTextureBParameterName = TEXT("MixBaseTexB");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName MixBaseTextureCParameterName = TEXT("MixBaseTexC");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName MixBaseTextureDParameterName = TEXT("MixBaseTexD");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName MixNormalTextureAParameterName = TEXT("MixNormalTexA");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName MixNormalTextureBParameterName = TEXT("MixNormalTexB");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName MixNormalTextureCParameterName = TEXT("MixNormalTexC");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName MixNormalTextureDParameterName = TEXT("MixNormalTexD");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName MixPackedTextureAParameterName = TEXT("MixPackedTexA");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName MixPackedTextureBParameterName = TEXT("MixPackedTexB");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName MixPackedTextureCParameterName = TEXT("MixPackedTexC");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MixedMaterial|Visual")
	FName MixPackedTextureDParameterName = TEXT("MixPackedTexD");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MixedMaterial")
	TMap<FName, int32> CompositionScaled;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MixedMaterial")
	float TotalCompositionUnits = 0.0f;

protected:
	virtual void BeginPlay() override;

	void ReapplyMixedMaterialPresentation();
	void ApplyScaleFromCurrentComposition();
	void ApplyVisualFromCurrentComposition();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicVisualMaterial = nullptr;

	UPROPERTY(Transient)
	float MinUnitsForScale = 1.0f;

	UPROPERTY(Transient)
	float MaxUnitsForScale = 10.0f;

	UPROPERTY(Transient)
	float MinVisualScale = 0.5f;

	UPROPERTY(Transient)
	float MaxVisualScale = 1.5f;
};
