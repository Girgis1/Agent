// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentScannerReadoutActor.h"
#include "Engine/EngineTypes.h"
#include "MachineUISystemTypes.h"
#include "MachineUIReadoutActor.generated.h"

class AMachineUIButtonActor;
class UMachineComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS(Blueprintable)
class AMachineUIReadoutActor : public AAgentScannerReadoutActor
{
	GENERATED_BODY()

public:
	AMachineUIReadoutActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category="Machine|UI")
	void SetMachineComponentOverride(UMachineComponent* InMachineComponent);

	UFUNCTION(BlueprintCallable, Category="Machine|UI")
	bool RefreshMachineUIState();

	UFUNCTION(BlueprintCallable, Category="Machine|UI")
	bool ApplyMachineCommand(const FMachineUICommand& Command);

	UFUNCTION(BlueprintPure, Category="Machine|UI")
	UMachineComponent* GetResolvedMachineComponent() const { return CachedMachineComponent.Get(); }

	UFUNCTION(BlueprintPure, Category="Machine|UI")
	AActor* GetResolvedMachineActor() const { return CachedMachineActor.Get(); }

	UFUNCTION(BlueprintPure, Category="Machine|UI")
	const FMachineUIState& GetMachineUIState() const { return CachedMachineState; }

	UFUNCTION(BlueprintPure, Category="Machine|UI")
	bool IsMachineUIActive() const { return bIsMachineUIActive; }

	UFUNCTION(BlueprintImplementableEvent, Category="Machine|UI")
	void HandleMachineUIStateUpdated(const FMachineUIState& NewState);

	UFUNCTION(BlueprintImplementableEvent, Category="Machine|UI")
	void HandleMachineUIActivationChanged(bool bIsActive);

protected:
	virtual void HandlePresentationApplied_Implementation() override;
	virtual bool ResolveTargetFacingRotation(
		const FVector& InPanelLocation,
		const FVector& InViewerLocation,
		FRotator& OutTargetRotation) const override;
	virtual float ResolveRotationInterpSpeed(const FRotator& CurrentRotation, const FRotator& TargetRotation) const override;

	bool ResolveMachineContext();
	bool ResolveScannerModeActive() const;
	bool ResolvePrimaryViewerPose(FVector& OutViewLocation, FVector& OutViewDirection) const;
	bool ComputeMachineVisibilityForViewer(
		const FVector& ViewLocation,
		const FVector& ViewDirection,
		bool& OutIsAimedAtMachine,
		bool& OutIsWithinRange) const;
	void UpdateButtonRaycastInteraction(const FVector& ViewLocation, const FVector& ViewDirection, bool bScannerModeActive);
	void ResetButtonRaycastInteractionState();
	void RefreshButtonCache(bool bForceRefresh);
	AMachineUIButtonActor* ResolveButtonFromHitActor(AActor* HitActor) const;
	UFUNCTION()
	void HandleRaycastButtonClicked(const FMachineUICommand& Command);
	FAgentScannerPresentation BuildPresentationFromState(const FMachineUIState& State) const;
	void UpdateFrameVisuals();
	void ApplyFrameMaterialParams();
	void SetMachineUIActive(bool bNewIsActive);

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Source")
	bool bAutoResolveMachineFromOwnership = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Source")
	TObjectPtr<UMachineComponent> MachineComponentOverride = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Activation")
	bool bAutoVisibilityFromPlayerView = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Activation")
	bool bRequireScannerModeForUI = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Activation")
	bool bShowWhenAimed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Activation")
	bool bShowWhenInRange = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Activation", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float ActivationRange = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Activation", meta=(ClampMin="100.0", UIMin="100.0", Units="cm"))
	float AimTraceDistance = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Activation", meta=(ClampMin="-1.0", ClampMax="1.0", UIMin="-1.0", UIMax="1.0"))
	float AimDotThreshold = 0.72f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Activation")
	bool bRequireLineOfSightInRange = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Activation")
	bool bHideWhenNoViewer = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Refresh", meta=(ClampMin="0.01", UIMin="0.01"))
	float StateRefreshInterval = 0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Buttons")
	bool bUseRaycastButtonInteraction = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Buttons")
	bool bRequireScannerModeForButtonInteraction = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Buttons", meta=(ClampMin="100.0", UIMin="100.0", Units="cm"))
	float ButtonTraceDistance = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Buttons")
	TEnumAsByte<ECollisionChannel> ButtonTraceChannel = ECC_Camera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Buttons")
	bool bIgnoreMachineActorDuringButtonTrace = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Buttons", meta=(ClampMin="0.01", UIMin="0.01"))
	float ButtonCacheRefreshInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Rows")
	bool bShowPowerRows = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Rows")
	bool bShowRecipeRows = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Rows")
	bool bShowStorageRows = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Rows")
	bool bShowBatteryRows = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Rows")
	bool bShowPowerUsageRows = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Rows", meta=(ClampMin="1", UIMin="1"))
	int32 MaxDisplayRows = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Motion", meta=(ClampMin="0.0", UIMin="0.0", ClampMax="45.0", UIMax="45.0"))
	float MaxTiltDegrees = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Motion", meta=(ClampMin="0.01", UIMin="0.01", ClampMax="1.0", UIMax="1.0"))
	float MinTiltInterpScaleAtLimit = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Motion", meta=(ClampMin="0.2", UIMin="0.2"))
	float TiltEdgeSlowdownExponent = 2.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Machine|UI|Frame")
	TObjectPtr<USceneComponent> FrameRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Machine|UI|Frame")
	TObjectPtr<UStaticMeshComponent> FrameMeshComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Frame")
	bool bShowFrame = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Frame")
	bool bAutoSizeFrameToContent = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Frame")
	FVector FramePadding = FVector(0.0f, 14.0f, 10.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Frame")
	FVector FrameLocalOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Frame")
	FRotator FrameRotationOffset = FRotator(90.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Frame")
	FLinearColor FrameColor = FLinearColor(0.12f, 0.86f, 1.0f, 0.42f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Frame", meta=(ClampMin="0.0", UIMin="0.0"))
	float FrameEmissiveIntensity = 3.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Frame")
	TObjectPtr<UStaticMesh> FrameMeshAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|UI|Frame")
	TObjectPtr<UMaterialInterface> FrameMaterial = nullptr;

protected:
	UPROPERTY(Transient)
	TWeakObjectPtr<UMachineComponent> CachedMachineComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CachedMachineActor;

	UPROPERTY(Transient)
	FMachineUIState CachedMachineState;

	UPROPERTY(Transient)
	float TimeUntilStateRefresh = 0.0f;

	UPROPERTY(Transient)
	bool bIsMachineUIActive = false;

	UPROPERTY(Transient)
	bool bHasCapturedNeutralRotation = false;

	UPROPERTY(Transient)
	FRotator NeutralFacingRotation = FRotator::ZeroRotator;

	UPROPERTY(Transient)
	mutable float TiltLimitAlpha = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> FrameMaterialInstance = nullptr;

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<AMachineUIButtonActor>> CachedButtons;

	UPROPERTY(Transient)
	TWeakObjectPtr<AMachineUIButtonActor> HoveredButton;

	UPROPERTY(Transient)
	bool bButtonClickHeldLastTick = false;

	UPROPERTY(Transient)
	float TimeUntilButtonCacheRefresh = 0.0f;
};
