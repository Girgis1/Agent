// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Scanner/AgentScannerReadoutActor.h"
#include "WorldUISystemTypes.h"
#include "WorldUIActor.generated.h"

class AWorldUIButtonActor;
class UTextRenderComponent;
class UWorldUIHostComponent;
class USceneComponent;

UCLASS(Blueprintable)
class AWorldUIActor : public AAgentScannerReadoutActor
{
	GENERATED_BODY()

public:
	AWorldUIActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category="World UI")
	void SetHostComponent(UWorldUIHostComponent* InHostComponent);

	UFUNCTION(BlueprintCallable, Category="World UI")
	void SetFacingMode(EWorldUIFacingMode InFacingMode);

	UFUNCTION(BlueprintCallable, Category="World UI")
	void SetNeutralFacingRotation(const FRotator& InRotation);

	UFUNCTION(BlueprintCallable, Category="World UI")
	void ApplyWorldUIModel(const FWorldUIModel& InModel);

	UFUNCTION(BlueprintCallable, Category="World UI")
	void UpdateWorldUI(
		const FWorldUIModel& InModel,
		const FVector& InPanelLocation,
		const FVector& InAnchorLocation,
		const FVector& InViewerLocation,
		float InDisplayAlpha,
		bool bInShouldDisplay);

	UFUNCTION(BlueprintCallable, Category="World UI|Button")
	void UpdateButtonInteractionFromTrace(AActor* HitActor, bool bCanInteract, bool bClickHeld);

	UFUNCTION(BlueprintCallable, Category="World UI|Button")
	void ResetButtonInteractionState();

	UFUNCTION(BlueprintPure, Category="World UI")
	bool FindTextBinding(FName FieldTag, FText& OutValue) const;

	UFUNCTION(BlueprintPure, Category="World UI")
	bool FindScalarBinding(FName FieldTag, float& OutValue) const;

	UFUNCTION(BlueprintPure, Category="World UI")
	bool FindBoolBinding(FName FieldTag, bool& OutValue) const;

	UFUNCTION(BlueprintPure, Category="World UI")
	bool FindColorBinding(FName FieldTag, FLinearColor& OutValue) const;

	UFUNCTION(BlueprintPure, Category="World UI")
	bool FindVisibilityBinding(FName FieldTag, bool& bOutVisible) const;

	UFUNCTION(BlueprintCallable, Category="World UI|Button")
	AWorldUIButtonActor* ResolveButtonFromHitActor(AActor* HitActor) const;

	UFUNCTION(BlueprintImplementableEvent, Category="World UI")
	void HandleWorldUIModelUpdated(const FWorldUIModel& NewModel);

protected:
	virtual void HandlePresentationApplied_Implementation() override;
	virtual bool ResolveTargetFacingRotation(
		const FVector& InPanelLocation,
		const FVector& InViewerLocation,
		FRotator& OutTargetRotation) const override;
	virtual float ResolveRotationInterpSpeed(const FRotator& CurrentRotation, const FRotator& TargetRotation) const override;

	void CacheTaggedComponents(bool bForceRefresh);
	void RefreshButtonCache();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Binding")
	bool bHideUnboundTaggedTextFields = true;

protected:
	TWeakObjectPtr<UWorldUIHostComponent> HostComponent;

	FWorldUIModel CurrentModel;

	TMultiMap<FName, TWeakObjectPtr<UTextRenderComponent>> TaggedTextFields;

	TMultiMap<FName, TWeakObjectPtr<USceneComponent>> TaggedSceneComponents;

	TArray<TWeakObjectPtr<AWorldUIButtonActor>> CachedButtons;

	TWeakObjectPtr<AWorldUIButtonActor> HoveredButton;

	bool bButtonClickHeldLastTick = false;

	bool bHasCapturedNeutralRotation = false;

	FRotator NeutralFacingRotation = FRotator::ZeroRotator;

	mutable float TiltLimitAlpha = 0.0f;

	EWorldUIFacingMode FacingMode = EWorldUIFacingMode::LimitedFaceCamera;

	float RuntimeMaxTiltDegrees = 15.0f;

	float RuntimeMinTiltInterpScaleAtLimit = 0.25f;

	float RuntimeTiltEdgeSlowdownExponent = 2.0f;
};
