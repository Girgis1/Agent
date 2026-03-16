// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Dirty/DirtyTypes.h"
#include "DirtySurfaceComponent.generated.h"

class UMaterialInstanceDynamic;
class UPrimitiveComponent;
class UTexture2D;

UCLASS(ClassGroup=(Dirty), meta=(BlueprintSpawnableComponent))
class UDirtySurfaceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDirtySurfaceComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category="Dirty")
	void InitializeDirtySurface();

	UFUNCTION(BlueprintCallable, Category="Dirty")
	bool ApplyBrushHit(const FHitResult& Hit, const FDirtBrushStamp& Stamp, float DeltaTime);

	UFUNCTION(BlueprintCallable, Category="Dirty")
	void SetDirtyness(float NewDirtyness, bool bResetMask = true);

	UFUNCTION(BlueprintPure, Category="Dirty")
	float GetDirtyness() const { return CurrentDirtyness; }

	UFUNCTION(BlueprintPure, Category="Dirty")
	bool IsSpotless() const { return CurrentDirtyness <= SpotlessThreshold; }

	UFUNCTION(BlueprintPure, Category="Dirty")
	UPrimitiveComponent* GetTargetPrimitive() const { return TargetPrimitive.Get(); }

	UFUNCTION(BlueprintPure, Category="Dirty")
	UTexture2D* GetMaskTexture() const { return DirtMaskTexture; }

	UFUNCTION(BlueprintCallable, Category="Dirty")
	void RefreshVisualParameters();

	static UDirtySurfaceComponent* FindDirtySurfaceComponent(const AActor* Actor, const UPrimitiveComponent* PreferredPrimitive = nullptr);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty")
	bool bDirtySurfaceEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty")
	FName TargetPrimitiveComponentName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty")
	TArray<int32> MaterialIndices;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty", meta=(ClampMin="32", ClampMax="1024", UIMin="32", UIMax="1024"))
	int32 MaskResolution = 256;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float InitialDirtyness = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Dirty")
	float CurrentDirtyness = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty", meta=(ClampMin="0.0", UIMin="0.0"))
	float DirtIntensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty")
	TObjectPtr<UTexture2D> DirtTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty")
	bool bUseWorldAlignedDirt = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty", meta=(ClampMin="1.0", UIMin="1.0", Units="cm"))
	float DirtWorldScale = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float SpotlessThreshold = 0.02f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Visual")
	FLinearColor DirtyTint = FLinearColor(0.65f, 0.58f, 0.48f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Visual", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float CleanRoughness = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Visual", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float DirtyRoughness = 0.82f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Visual", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float CleanSpecular = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Visual", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float DirtySpecular = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Visual", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float CleanMetallic = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Visual", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float DirtyMetallic = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Material")
	FName DirtMaskTextureParameterName = TEXT("Dirty_MaskTexture");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Material")
	FName DirtPatternTextureParameterName = TEXT("Dirty_PatternTexture");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Material")
	FName DirtynessParameterName = TEXT("Dirty_Dirtyness");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Material")
	FName DirtIntensityParameterName = TEXT("Dirty_Intensity");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Material")
	FName DirtWorldScaleParameterName = TEXT("Dirty_WorldScale");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Material")
	FName UseWorldAlignedParameterName = TEXT("Dirty_UseWorldAligned");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Material")
	FName SpotlessParameterName = TEXT("Dirty_IsSpotless");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Material")
	FName DirtyTintParameterName = TEXT("Dirty_Tint");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Material")
	FName CleanRoughnessParameterName = TEXT("Dirty_CleanRoughness");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Material")
	FName DirtyRoughnessParameterName = TEXT("Dirty_DirtyRoughness");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Material")
	FName CleanSpecularParameterName = TEXT("Dirty_CleanSpecular");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Material")
	FName DirtySpecularParameterName = TEXT("Dirty_DirtySpecular");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Material")
	FName CleanMetallicParameterName = TEXT("Dirty_CleanMetallic");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Material")
	FName DirtyMetallicParameterName = TEXT("Dirty_DirtyMetallic");

protected:
	UPrimitiveComponent* ResolveTargetPrimitive();
	void BuildDynamicMaterials();
	void CreateMaskTexture();
	void FillMask(float NormalizedValue);
	void UploadMaskTexture();
	void RecomputeDirtyness();
	bool ResolveBrushUV(const FHitResult& Hit, FVector2D& OutUV) const;
	bool ResolveFallbackProjectionUV(const FHitResult& Hit, FVector2D& OutUV) const;
	void ComputeBrushRadiiPixels(const FHitResult& Hit, float BrushSizeCm, float& OutRadiusXPixels, float& OutRadiusYPixels) const;
	bool ApplyBrushAtUV(const FVector2D& UV, float RadiusXPixels, float RadiusYPixels, const FDirtBrushStamp& Stamp, float DeltaTime);

	UPROPERTY(Transient)
	TWeakObjectPtr<UPrimitiveComponent> TargetPrimitive;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> DynamicMaterials;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> DirtMaskTexture = nullptr;

	TArray<FColor> DirtMaskPixels;

	bool bWarnedAboutMissingUVSupport = false;
};
