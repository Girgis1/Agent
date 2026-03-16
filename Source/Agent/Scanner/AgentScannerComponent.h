// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentScannerTypes.h"
#include "Components/ActorComponent.h"
#include "AgentScannerComponent.generated.h"

class AAgentScannerReadoutActor;
class UPrimitiveComponent;
struct FAgentBeamTraceState;

UCLASS(ClassGroup=(Agent), meta=(BlueprintSpawnableComponent))
class UAgentScannerComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAgentScannerComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	void UpdateScanner(
		float DeltaTime,
		bool bAimActive,
		const FVector& ViewOrigin,
		const FVector& ViewDirection,
		const FAgentBeamTraceState* BeamTraceState = nullptr);

	UFUNCTION(BlueprintCallable, Category="Scanner")
	void ClearScanner(bool bImmediate = true);

	UFUNCTION(BlueprintPure, Category="Scanner")
	bool HasLockedScan() const { return bHasLockedScan; }

	UFUNCTION(BlueprintPure, Category="Scanner")
	const FAgentScannerPresentation& GetPresentation() const { return CurrentPresentation; }

	UFUNCTION(BlueprintPure, Category="Scanner")
	AAgentScannerReadoutActor* GetReadoutActor() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner")
	bool bScannerEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Timing", meta=(ClampMin="0.05", UIMin="0.05"))
	float ScanLockDuration = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Timing", meta=(ClampMin="0.0", UIMin="0.0"))
	float ScanLingerDuration = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Timing", meta=(ClampMin="0.0", UIMin="0.0"))
	float ScanProgressResetDelay = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Timing", meta=(ClampMin="0.0", UIMin="0.0"))
	float NodeReadoutRefreshInterval = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Trace", meta=(ClampMin="100.0", UIMin="100.0", Units="cm"))
	float ScannerTraceDistance = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Placement", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float SurfaceAnchorOffset = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Placement", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float TowardViewerOffset = 28.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Placement", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float VerticalPlacementOffset = 22.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Placement", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float SidePlacementOffset = 34.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Placement", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float SecondaryVerticalPlacementOffset = 44.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Placement", meta=(ClampMin="0.1", UIMin="0.1", Units="cm"))
	FVector PlacementClearanceExtents = FVector(20.0f, 8.0f, 16.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Placement")
	bool bUseDynamicReadoutBoundsForPlacement = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Placement", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float PlacementLiftStepHeight = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Placement", meta=(ClampMin="0", UIMin="0"))
	int32 MaxPlacementLiftSteps = 10;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Display", meta=(ClampMin="1", UIMin="1"))
	int32 MaxMaterialRows = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Display", meta=(ClampMin="1", UIMin="1"))
	int32 MaxDwellingReadouts = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Display")
	bool bShowHealthInScan = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Display")
	bool bShowMassInScan = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Display")
	bool bShowSalvageYieldInScan = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Display")
	bool bShowFractureRiskInScan = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Display")
	FLinearColor DefaultAccentColor = FLinearColor(0.14f, 0.86f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner|Display")
	TSubclassOf<AAgentScannerReadoutActor> ReadoutActorClass;

protected:
	struct FScannerTraceResult
	{
		bool bHasHit = false;
		FVector TraceEnd = FVector::ZeroVector;
		FVector ImpactPoint = FVector::ZeroVector;
		FVector ImpactNormal = FVector::ZeroVector;
		TWeakObjectPtr<AActor> HitActor;
		TWeakObjectPtr<UPrimitiveComponent> HitComponent;
	};

	struct FScannerReadoutState
	{
		TWeakObjectPtr<AAgentScannerReadoutActor> Actor;
		TWeakObjectPtr<AActor> TargetActor;
		TWeakObjectPtr<UPrimitiveComponent> TargetComponent;
		FAgentScannerPresentation Presentation;
		FVector PanelLocation = FVector::ZeroVector;
		FVector AnchorLocation = FVector::ZeroVector;
		FVector ViewerLocation = FVector::ZeroVector;
		FVector LocalAnchorLocation = FVector::ZeroVector;
		FVector LocalAnchorNormal = FVector::UpVector;
		float LingerTimeRemaining = 0.0f;
		float PresentationRefreshTimeRemaining = 0.0f;
		bool bLocked = false;
		bool bFollowTargetMotion = false;
		bool bLiveRefreshPresentation = false;
	};

	bool ResolveTraceResult(
		const FVector& ViewOrigin,
		const FVector& ViewDirection,
		const FAgentBeamTraceState* BeamTraceState,
		FScannerTraceResult& OutTraceResult) const;

	bool BuildPresentationForTrace(
		const FScannerTraceResult& TraceResult,
		FAgentScannerPresentation& OutPresentation,
		FLinearColor& OutDominantColor) const;

	bool BuildMaterialRows(
		AActor* TargetActor,
		UPrimitiveComponent* HitPrimitive,
		TArray<FAgentScannerRowPresentation>& OutRows,
		FLinearColor& OutDominantColor) const;

	bool BuildNodeRows(
		AActor* TargetActor,
		TArray<FAgentScannerRowPresentation>& OutRows,
		FLinearColor& OutDominantColor) const;

	void AddHealthRow(AActor* TargetActor, TArray<FAgentScannerRowPresentation>& OutRows, FLinearColor& OutAccentColor) const;
	void AddMassRow(AActor* TargetActor, UPrimitiveComponent* HitPrimitive, TArray<FAgentScannerRowPresentation>& OutRows) const;
	void AddSalvageRow(AActor* TargetActor, TArray<FAgentScannerRowPresentation>& OutRows) const;
	void AddFractureRiskRow(AActor* TargetActor, TArray<FAgentScannerRowPresentation>& OutRows) const;

	void ResetCandidate();
	void DestroyReadoutState(FScannerReadoutState& ReadoutState);
	void ClearActiveReadout(bool bDestroyActor);
	void DetachActiveReadoutToDwelling();
	bool IsReadoutTargetValid(const FScannerReadoutState& ReadoutState) const;
	void ConfigureReadoutLiveRefresh(FScannerReadoutState& ReadoutState) const;
	void CacheReadoutAnchor(FScannerReadoutState& ReadoutState, const FScannerTraceResult& TraceResult) const;
	void RefreshReadoutAnchorFromTarget(FScannerReadoutState& ReadoutState, const FVector& ViewOrigin, const FVector& ViewDirection, bool bHasViewerLocation);
	bool RefreshReadoutPresentation(FScannerReadoutState& ReadoutState) const;
	void UpdateDwellingReadouts(float DeltaTime, const FVector& ViewOrigin, const FVector& ViewDirection, bool bHasViewerLocation);
	void EnforceDwellingLimit();
	AAgentScannerReadoutActor* SpawnReadoutActor();
	bool ActivateReadoutForTarget(AActor* TargetActor, bool& bOutWasAlreadyLocked);
	int32 FindDwellingReadoutIndexByTarget(AActor* TargetActor) const;
	void UpdateReadoutActor(FScannerReadoutState& ReadoutState, float Alpha, bool bShouldDisplay);

	bool SolvePanelPlacement(
		const FVector& ViewOrigin,
		const FVector& ViewDirection,
		AAgentScannerReadoutActor* PlacementReadoutActor,
		const FAgentScannerPresentation& Presentation,
		const FScannerTraceResult& TraceResult,
		FVector& OutPanelLocation,
		FVector& OutAnchorLocation);

	bool IsSameCandidateActor(const FScannerTraceResult& TraceResult) const;
	bool TryHoldCandidateProgress(
		float DeltaTime,
		const FVector& ViewOrigin,
		const FVector& ViewDirection,
		bool bHasViewerLocation,
		bool bAllowHoldDisplay);

	TWeakObjectPtr<AActor> CandidateActor;
	TWeakObjectPtr<UPrimitiveComponent> CandidateComponent;
	FVector CandidateImpactPoint = FVector::ZeroVector;
	FVector CandidateImpactNormal = FVector::ZeroVector;
	float CandidateDwellTime = 0.0f;
	float CandidateResetGraceRemaining = 0.0f;
	bool bHasLockedScan = false;
	bool bWasAimActiveLastTick = false;
	FAgentScannerPresentation CurrentPresentation;
	FScannerReadoutState ActiveReadout;
	bool bHasActiveReadout = false;
	TArray<FScannerReadoutState> DwellingReadouts;
};
