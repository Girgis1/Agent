// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/TextRenderComponent.h"
#include "AgentScannerTypes.h"
#include "GameFramework/Actor.h"
#include "AgentScannerReadoutActor.generated.h"

class UFont;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
UCLASS(Blueprintable)
class AAgentScannerReadoutActor : public AActor
{
	GENERATED_BODY()

public:
	AAgentScannerReadoutActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category="Scanner")
	void RefreshVisualLayout();

	UFUNCTION(BlueprintCallable, Category="Scanner")
	void PreparePlacementMeasurement(const FAgentScannerPresentation& InPresentation, const FVector& InViewerLocation);

	UFUNCTION(BlueprintCallable, Category="Scanner")
	bool GetPlacementLocalBounds(FVector& OutLocalCenter, FVector& OutLocalExtent) const;

	UFUNCTION(BlueprintCallable, Category="Scanner")
	void UpdateReadout(
		const FAgentScannerPresentation& InPresentation,
		const FVector& InDesiredPanelLocation,
		const FVector& InAnchorLocation,
		const FVector& InViewerLocation,
		float InDisplayAlpha,
		bool bInShouldDisplay);

	UFUNCTION(BlueprintCallable, Category="Scanner")
	void HideReadout();

	UFUNCTION(BlueprintPure, Category="Scanner")
	const FAgentScannerPresentation& GetPresentation() const { return Presentation; }

	UFUNCTION(BlueprintCallable, Category="Scanner")
	void GetGeneratedTextComponents(TArray<UTextRenderComponent*>& OutRowTextComponents, TArray<UTextRenderComponent*>& OutProgressTextComponents) const;

	UFUNCTION(BlueprintNativeEvent, Category="Scanner")
	void HandlePresentationApplied();
	virtual void HandlePresentationApplied_Implementation();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scanner")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scanner")
	TObjectPtr<USceneComponent> PanelRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scanner")
	TObjectPtr<USceneComponent> DecorRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scanner")
	TObjectPtr<USceneComponent> TextRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scanner")
	TObjectPtr<USceneComponent> TitleAnchor = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scanner")
	TObjectPtr<USceneComponent> SubtitleAnchor = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scanner")
	TObjectPtr<USceneComponent> RowsAnchor = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scanner")
	TObjectPtr<USceneComponent> ProgressAnchor = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scanner")
	TObjectPtr<USceneComponent> ProgressBarRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scanner")
	TObjectPtr<USceneComponent> LeaderEnd = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scanner")
	TObjectPtr<UTextRenderComponent> TitleText = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scanner")
	TObjectPtr<UTextRenderComponent> SubtitleText = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scanner")
	TObjectPtr<UStaticMeshComponent> LeaderLineMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scanner")
	TObjectPtr<UStaticMeshComponent> LeaderArmMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scanner")
	TObjectPtr<UStaticMeshComponent> LeaderStartSphereMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scanner")
	TObjectPtr<UStaticMeshComponent> ProgressBackgroundMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scanner")
	TObjectPtr<UStaticMeshComponent> ProgressFillMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Display")
	bool bUseDefaultRowText = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Display", meta=(DisplayName="Use Default Progress Bar"))
	bool bUseDefaultProgressReticle = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Layout")
	bool bApplyPanelTransformSettings = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Layout")
	FVector PanelOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Layout")
	FRotator PanelRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Layout")
	FVector PanelScale = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Layout")
	FVector ContentOffset = FVector(0.0f, 50.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Motion", meta=(ClampMin="0.0", UIMin="0.0"))
	float PositionInterpSpeed = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Motion", meta=(ClampMin="0.0", UIMin="0.0"))
	float RotationInterpSpeed = 3.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Motion", meta=(ClampMin="0.0", UIMin="0.0"))
	float FloatAmplitude = 1.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Motion", meta=(ClampMin="0.0", UIMin="0.0"))
	float FloatSpeed = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Text", meta=(ClampMin="1.0", UIMin="1.0"))
	float TitleTextWorldSize = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Text", meta=(ClampMin="1.0", UIMin="1.0"))
	float SubtitleTextWorldSize = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Text", meta=(ClampMin="1.0", UIMin="1.0"))
	float RowTextWorldSize = 7.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Text", meta=(ClampMin="1.0", UIMin="1.0"))
	float RowSpacing = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Text")
	FLinearColor TitleColor = FLinearColor(0.82f, 0.97f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Text")
	FLinearColor SubtitleColor = FLinearColor(0.6f, 0.82f, 0.9f, 0.78f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Text")
	FLinearColor NeutralRowColor = FLinearColor(0.84f, 0.93f, 0.98f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Text")
	FRotator TextRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Text")
	TEnumAsByte<EHorizTextAligment> TitleHorizontalAlignment = EHTA_Left;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Text")
	TEnumAsByte<EHorizTextAligment> SubtitleHorizontalAlignment = EHTA_Left;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Text")
	TEnumAsByte<EHorizTextAligment> RowHorizontalAlignment = EHTA_Left;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Text")
	int32 GeneratedTextSortPriority = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Progress", meta=(ClampMin="1.0", UIMin="1.0", Units="cm"))
	float ProgressBarWidth = 26.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Progress", meta=(ClampMin="0.5", UIMin="0.5", Units="cm"))
	float ProgressBarHeight = 3.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Progress", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float ProgressBarFillPadding = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Progress")
	FRotator ProgressBarRotationOffset = FRotator(90.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Progress")
	FLinearColor ProgressBarBackgroundColor = FLinearColor(0.07f, 0.15f, 0.2f, 0.22f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Progress", meta=(ClampMin="0.0", UIMin="0.0"))
	float ProgressBarBackgroundEmissiveMultiplier = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Progress", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float ProgressBarFillDepthOffset = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Leader", meta=(ClampMin="0.1", UIMin="0.1"))
	float LeaderThickness = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Leader")
	bool bShowLeader = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Leader", meta=(ClampMin="0.1", UIMin="0.1", Units="cm"))
	float LeaderSphereDiameter = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Leader")
	FLinearColor LeaderColor = FLinearColor(0.16f, 0.86f, 1.0f, 0.7f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Assets")
	TObjectPtr<UMaterialInterface> ScannerTextMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Assets")
	TObjectPtr<UFont> ScannerTextFont = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Assets")
	TObjectPtr<UStaticMesh> LeaderMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Assets")
	TObjectPtr<UStaticMesh> LeaderSphereMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Assets")
	TObjectPtr<UStaticMesh> ProgressBarMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Assets")
	TObjectPtr<UMaterialInterface> ScannerMasterMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Assets", meta=(ClampMin="0.0", UIMin="0.0"))
	float ScannerEmissiveIntensity = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Preview")
	bool bShowPreviewInEditor = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Preview")
	FText PreviewTitle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Preview")
	FText PreviewSubtitle;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Preview")
	TArray<FAgentScannerRowPresentation> PreviewRows;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Preview", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float PreviewScanProgress01 = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Preview")
	bool bPreviewScanComplete = true;

protected:
	virtual void BeginPlay() override;

	void ApplyLayoutSettings();
	void ApplyPresentationToVisuals();
	void EnsureRowTextCount(int32 DesiredCount);
	bool GetPanelContentLocalBounds(FVector& OutLocalCenter, FVector& OutLocalExtent) const;
	void ConfigureTitleText();
	void ConfigureSubtitleText();
	void ConfigureRowText(UTextRenderComponent* TextComponent, const FAgentScannerRowPresentation& Row, int32 RowIndex) const;
	void ConfigureProgressBarVisuals(const FLinearColor& AccentColor, bool bVisible);
	void SetTextComponentEnabled(UTextRenderComponent* TextComponent, bool bEnabled) const;
	UTextRenderComponent* CreateTextComponent(const FName ComponentName, USceneComponent* AttachParent);
	FText FormatRowText(const FAgentScannerRowPresentation& Row) const;
	FAgentScannerPresentation BuildPreviewPresentation() const;
	virtual bool ResolveTargetFacingRotation(
		const FVector& InPanelLocation,
		const FVector& InViewerLocation,
		FRotator& OutTargetRotation) const;
	virtual float ResolveRotationInterpSpeed(const FRotator& CurrentRotation, const FRotator& TargetRotation) const;
	void UpdateLeaderLine(float Alpha);
	void UpdateLeaderSegment(UStaticMeshComponent* LineMesh, const FVector& StartLocation, const FVector& EndLocation, float Alpha, bool bCanShowLine) const;
	void ApplyLeaderMaterialColor(const FLinearColor& InColor);
	void ApplyScannerMaterialParams(
		UStaticMeshComponent* MeshComponent,
		TObjectPtr<UMaterialInstanceDynamic>& MaterialInstance,
		const FLinearColor& InColor,
		float EmissiveIntensity);

	UPROPERTY(Transient)
	FAgentScannerPresentation Presentation;

	UPROPERTY(Transient)
	FVector DesiredPanelLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector SmoothedPanelLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector DesiredAnchorLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector SmoothedAnchorLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector ViewerLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	float DisplayAlpha = 0.0f;

	UPROPERTY(Transient)
	bool bShouldDisplay = false;

	UPROPERTY(Transient)
	bool bHasInitializedTransforms = false;

	UPROPERTY(Transient)
	bool bSnapToNextReadoutLocation = true;

	UPROPERTY(Transient)
	float FloatTime = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LeaderLineMaterial = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LeaderArmMaterial = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> LeaderSphereMaterial = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ProgressBackgroundMaterial = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ProgressFillMaterial = nullptr;

	UPROPERTY(Transient, BlueprintReadOnly, Category="Scanner|State", meta=(AllowPrivateAccess="true"))
	FLinearColor RuntimeAccentColor = FLinearColor::White;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextRenderComponent>> RowTextComponents;
};
