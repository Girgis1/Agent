// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MachineResourceReadoutActor.generated.h"

class UAtomiserVolume;
class UFactoryResourceBankSubsystem;
class UMachineComponent;
class UMachineInputVolumeComponent;
class UStorageVolumeComponent;
class USceneComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UTextRenderComponent;

UENUM(BlueprintType)
enum class EMachineReadoutSourceMode : uint8
{
	Atomiser,
	Manual,
	CurrentMachine,
	AtomBank
};

UENUM(BlueprintType)
enum class EMachineReadoutRowLayout : uint8
{
	Vertical,
	Horizontal
};

USTRUCT(BlueprintType)
struct FMachineReadoutEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Readout")
	FName ResourceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Readout")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Readout", meta=(ClampMin="0.0", UIMin="0.0"))
	float Units = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Readout", meta=(ClampMin="0.0", UIMin="0.0", ClampMax="1.0", UIMax="1.0"))
	float Percent01 = 0.0f;
};

USTRUCT()
struct FMachineReadoutRowVisual
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<USceneComponent> RowRoot = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UTextRenderComponent> LabelText = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> FillBar = nullptr;
};

UCLASS()
class AMachineResourceReadoutActor : public AActor
{
	GENERATED_BODY()

public:
	AMachineResourceReadoutActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category="Machine|Readout")
	void SetAtomiserSource(UAtomiserVolume* InAtomiserVolume);

	UFUNCTION(BlueprintCallable, Category="Machine|Readout")
	void SetManualEntries(const TArray<FMachineReadoutEntry>& InEntries);

	UFUNCTION(BlueprintCallable, Category="Machine|Readout")
	void RefreshReadout();

protected:
	UFUNCTION()
	void HandleAtomiserStorageChanged();

	UFUNCTION()
	void HandleAtomBankStorageChanged();

	void BindToActiveSource();
	void BindToAtomiserSource();
	void UnbindFromAtomiserSource();
	void BindToAtomBankSource();
	void UnbindFromAtomBankSource();
	UAtomiserVolume* ResolveAtomiserSourceCandidate() const;
	UFactoryResourceBankSubsystem* ResolveAtomBankSource() const;
	void RebuildEntriesFromSource();
	void BuildEntriesFromAtomiser(TArray<FMachineReadoutEntry>& OutEntries) const;
	void BuildEntriesFromAtomBank(TArray<FMachineReadoutEntry>& OutEntries) const;
	void BuildEntriesFromCurrentMachine(TArray<FMachineReadoutEntry>& OutEntries) const;
	void BuildEntriesFromScaledResourceMap(const TMap<FName, int32>& ResourceQuantitiesScaled, TArray<FMachineReadoutEntry>& OutEntries) const;
	void BuildPlaceholderEntries(TArray<FMachineReadoutEntry>& OutEntries) const;
	bool ShouldShowPlaceholderEntries() const;
	FVector GetRowDirectionVector() const;
	FVector GetBarFillDirectionVector() const;
	FText ResolveResourceLabel(FName ResourceId) const;
	void SyncVisualRows();
	void EnsureRowVisualCount(int32 DesiredCount);
	void ConfigureRowVisual(int32 RowIndex, const FMachineReadoutEntry& Entry);
	void SetRowVisualEnabled(int32 RowIndex, bool bEnabled);

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Machine|Readout")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Readout")
	EMachineReadoutSourceMode SourceMode = EMachineReadoutSourceMode::Atomiser;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Readout", meta=(EditCondition="SourceMode==EMachineReadoutSourceMode::Atomiser"))
	TObjectPtr<UAtomiserVolume> AtomiserVolume = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Readout", meta=(EditCondition="SourceMode==EMachineReadoutSourceMode::Manual"))
	TArray<FMachineReadoutEntry> ManualEntries;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Readout", meta=(ClampMin="1"))
	int32 MaxRows = 12;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Readout", meta=(ClampMin="0.1", UIMin="0.1"))
	float ReferenceUnits = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Readout|Layout", meta=(ClampMin="1.0", UIMin="1.0"))
	float RowSpacing = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Readout|Layout")
	EMachineReadoutRowLayout RowLayout = EMachineReadoutRowLayout::Horizontal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Readout|Layout")
	FVector HorizontalRowDirection = FVector(0.0f, -1.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Readout|Layout")
	FVector TextOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Readout|Layout")
	FVector BarStartOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Readout|Layout")
	FVector BarFillDirection = FVector(1.0f, 0.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Readout|Layout")
	FRotator BarRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Readout|Layout", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="RowLayout==EMachineReadoutRowLayout::Vertical"))
	float VerticalTextGap = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Readout|Layout", meta=(ClampMin="1.0", UIMin="1.0"))
	float BarWidth = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Readout|Layout", meta=(ClampMin="0.1", UIMin="0.1"))
	float BarHeight = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Readout|Layout", meta=(ClampMin="0.1", UIMin="0.1"))
	float BarDepth = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Readout|Text", meta=(ClampMin="1.0", UIMin="1.0"))
	float TextWorldSize = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Readout|Text")
	FColor TextColor = FColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Readout|Refresh")
	bool bAutoRefresh = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Readout|Refresh", meta=(ClampMin="0.05", UIMin="0.05", EditCondition="bAutoRefresh"))
	float RefreshIntervalSeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Readout|Preview")
	bool bShowPlaceholderWhenNoData = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Readout|Preview", meta=(ClampMin="1", UIMin="1", EditCondition="bShowPlaceholderWhenNoData"))
	int32 PlaceholderRowCount = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Readout|Preview", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bShowPlaceholderWhenNoData"))
	float PlaceholderUnitsStep = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Readout|Visual")
	TObjectPtr<UStaticMesh> BarMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Machine|Readout")
	TArray<FMachineReadoutEntry> CurrentEntries;

protected:
	UPROPERTY(Transient)
	TObjectPtr<UAtomiserVolume> BoundAtomiserVolume = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UFactoryResourceBankSubsystem> BoundAtomBank = nullptr;

	UPROPERTY(Transient)
	TArray<FMachineReadoutRowVisual> RowVisuals;

	UPROPERTY(Transient)
	float TimeUntilNextRefresh = 0.0f;
};
