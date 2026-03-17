// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dirty/DirtyTypes.h"
#include "GameFramework/Actor.h"
#include "DirtDecalActor.generated.h"

class UDecalComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UTexture2D;
class UArrowComponent;
class UBillboardComponent;

UCLASS(Blueprintable)
class ADirtDecalActor : public AActor
{
	GENERATED_BODY()

public:
	ADirtDecalActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category="Dirty|Decal")
	void InitializeDirtDecal();

	UFUNCTION(BlueprintCallable, Category="Dirty|Decal")
	bool ApplyBrushAtWorldHit(const FHitResult& Hit, const FDirtBrushStamp& Stamp, float DeltaTime);

	UFUNCTION(BlueprintCallable, Category="Dirty|Decal")
	bool ApplyBrushAtWorldPoint(const FVector& WorldPoint, const FDirtBrushStamp& Stamp, float DeltaTime);

	UFUNCTION(BlueprintCallable, Category="Dirty|Decal")
	void SetDirtyness(float NewDirtyness, bool bResetMask = true);

	UFUNCTION(BlueprintCallable, Category="Dirty|Decal")
	void RefreshVisualParameters();

	UFUNCTION(BlueprintCallable, Category="Dirty|Decal")
	void SetDecalHalfExtent(const FVector& NewHalfExtentCm);

	UFUNCTION(BlueprintPure, Category="Dirty|Decal")
	bool CanBrushAffectWorldPoint(const FVector& WorldPoint, float BrushRadiusCm = 0.0f) const;

	UFUNCTION(BlueprintPure, Category="Dirty|Decal")
	bool IsSpotless() const { return CurrentDirtyness <= SpotlessThreshold; }

	UFUNCTION(BlueprintPure, Category="Dirty|Decal")
	float GetDirtyness() const { return CurrentDirtyness; }

	UFUNCTION(BlueprintPure, Category="Dirty|Decal")
	float GetCleanProgressPercent() const { return CurrentCleanProgressPercent; }

	UFUNCTION(BlueprintPure, Category="Dirty|Decal")
	UDecalComponent* GetDirtDecalComponent() const { return DirtDecalComponent; }

	UFUNCTION(BlueprintPure, Category="Dirty|Decal")
	UTexture2D* GetMaskTexture() const { return DirtMaskTexture; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dirty|Decal")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dirty|Decal")
	TObjectPtr<UDecalComponent> DirtDecalComponent = nullptr;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<UArrowComponent> ArrowComponent = nullptr;

	UPROPERTY()
	TObjectPtr<UBillboardComponent> SpriteComponent = nullptr;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal")
	bool bDirtyDecalEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal")
	TObjectPtr<UMaterialInterface> BaseDecalMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal|Material")
	bool bUseMaterialVisualDefaults = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal|Debug")
	bool bDebugBrushLogging = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal", meta=(ClampMin="32", ClampMax="1024", UIMin="32", UIMax="1024"))
	int32 MaskResolution = 256;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float InitialDirtyness = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dirty|Decal")
	float CurrentDirtyness = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dirty|Decal")
	float CurrentCleanProgressPercent = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal", meta=(ClampMin="0.0", UIMin="0.0"))
	float DirtIntensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal")
	TObjectPtr<UTexture2D> DirtPatternTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal")
	TObjectPtr<UTexture2D> DirtCoverageTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal|Visual", meta=(ClampMin="0.0", UIMin="0.0"))
	float DirtPatternIntensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal|Visual", meta=(ClampMin="0.0", UIMin="0.0"))
	float DirtMaskIntensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal|Visual")
	bool bUseVisibilityWeightedDirtyness = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float SpotlessThreshold = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal|Spotless")
	bool bAutoDestroyWhenSpotless = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal|Spotless", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bAutoDestroyWhenSpotless"))
	float DestroyDelayAfterSpotlessSeconds = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal|Visual")
	FLinearColor DirtyTint = FLinearColor(0.65f, 0.58f, 0.48f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal|Visual", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float CleanRoughness = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal|Visual", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float DirtyRoughness = 0.82f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal|Visual", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float CleanSpecular = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal|Visual", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float DirtySpecular = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal|Visual", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float CleanMetallic = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal|Visual", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float DirtyMetallic = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal|Material")
	FName DirtMaskTextureParameterName = TEXT("Dirty_MaskTexture");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal|Material")
	FName DirtPatternTextureParameterName = TEXT("Dirty_PatternTexture");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal|Material")
	FName DirtCoverageTextureParameterName = TEXT("Mask");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal|Material")
	FName DirtynessParameterName = TEXT("Dirty_Dirtyness");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal|Material")
	FName DirtIntensityParameterName = TEXT("Dirty_Intensity");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal|Material")
	FName DirtPatternIntensityParameterName = TEXT("Dirty_PatternIntensity");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal|Material")
	FName DirtMaskIntensityParameterName = TEXT("Dirty_MaskIntensity");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal|Material")
	FName CleanProgressParameterName = TEXT("Dirty_CleanProgressPercent");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal|Material")
	FName SpotlessParameterName = TEXT("Dirty_IsSpotless");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal|Material")
	FName DirtyTintParameterName = TEXT("Dirty_Tint");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal|Material")
	FName CleanRoughnessParameterName = TEXT("Dirty_CleanRoughness");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal|Material")
	FName DirtyRoughnessParameterName = TEXT("Dirty_DirtyRoughness");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal|Material")
	FName CleanSpecularParameterName = TEXT("Dirty_CleanSpecular");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal|Material")
	FName DirtySpecularParameterName = TEXT("Dirty_DirtySpecular");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal|Material")
	FName CleanMetallicParameterName = TEXT("Dirty_CleanMetallic");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Decal|Material")
	FName DirtyMetallicParameterName = TEXT("Dirty_DirtyMetallic");

protected:
	void CreateMaskTexture();
	void FillMask(float NormalizedValue);
	void UploadMaskTexture();
	void RecomputeDirtyness();
	void ResolveMaterialDrivenDefaults(UMaterialInterface* SourceMaterial);
	void ComputeBrushRadiiPixels(float BrushSizeCm, float& OutRadiusXPixels, float& OutRadiusYPixels) const;
	float SampleVisibleCoverageWeight(float U, float V) const;
	bool ResolveWorldPointToUV(const FVector& WorldPoint, FVector2D& OutUV, FVector& OutLocalPoint) const;
	bool ApplyBrushAtUV(const FVector2D& UV, float RadiusXPixels, float RadiusYPixels, const FDirtBrushStamp& Stamp, float DeltaTime);
	void UpdateSpotlessDestroyState();
	void HandleSpotlessDestroy();

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> DynamicMaterial = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> DirtMaskTexture = nullptr;

	FTimerHandle SpotlessDestroyTimerHandle;

	TArray<FColor> DirtMaskPixels;
};
