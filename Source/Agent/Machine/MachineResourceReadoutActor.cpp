// Copyright Epic Games, Inc. All Rights Reserved.

#include "Machine/MachineResourceReadoutActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Factory/FactoryResourceBankSubsystem.h"
#include "Factory/MachineInputVolumeComponent.h"
#include "Factory/MachineOutputVolumeComponent.h"
#include "Factory/ShredderVolumeComponent.h"
#include "Factory/StorageVolumeComponent.h"
#include "Machine/OutputVolume.h"
#include "Machine/MachineComponent.h"
#include "Machine/AtomiserVolume.h"
#include "Material/AgentResourceTypes.h"
#include "Material/MaterialDefinitionAsset.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectIterator.h"

namespace
{
void AccumulateScaledResourceMap(const TMap<FName, int32>& SourceMap, TMap<FName, int32>& InOutCombinedMap)
{
	for (const TPair<FName, int32>& Pair : SourceMap)
	{
		const FName ResourceId = Pair.Key;
		const int32 QuantityScaled = FMath::Max(0, Pair.Value);
		if (ResourceId.IsNone() || QuantityScaled <= 0)
		{
			continue;
		}

		InOutCombinedMap.FindOrAdd(ResourceId) += QuantityScaled;
	}
}

FText FormatReadoutUnits(const double Value, const int32 DecimalPlaces)
{
	FNumberFormattingOptions FormattingOptions;
	FormattingOptions.MinimumFractionalDigits = FMath::Clamp(DecimalPlaces, 0, 4);
	FormattingOptions.MaximumFractionalDigits = FMath::Clamp(DecimalPlaces, 0, 4);
	return FText::AsNumber(Value, &FormattingOptions);
}
}

AMachineResourceReadoutActor::AMachineResourceReadoutActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultBarMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (DefaultBarMesh.Succeeded())
	{
		BarMesh = DefaultBarMesh.Object;
	}
}

void AMachineResourceReadoutActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshReadout();
}

void AMachineResourceReadoutActor::BeginPlay()
{
	Super::BeginPlay();
	BindToActiveSource();
	RefreshReadout();
}

void AMachineResourceReadoutActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromAtomiserSource();
	UnbindFromAtomBankSource();
	Super::EndPlay(EndPlayReason);
}

void AMachineResourceReadoutActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bAutoRefresh)
	{
		return;
	}

	TimeUntilNextRefresh = FMath::Max(0.0f, TimeUntilNextRefresh - FMath::Max(0.0f, DeltaSeconds));
	if (TimeUntilNextRefresh > 0.0f)
	{
		return;
	}

	RefreshReadout();
	TimeUntilNextRefresh = FMath::Max(0.05f, RefreshIntervalSeconds);
}

void AMachineResourceReadoutActor::SetAtomiserSource(UAtomiserVolume* InAtomiserVolume)
{
	AtomiserVolume = InAtomiserVolume;
	SourceMode = EMachineReadoutSourceMode::Atomiser;
	BindToAtomiserSource();
	RefreshReadout();
}

void AMachineResourceReadoutActor::SetManualEntries(const TArray<FMachineReadoutEntry>& InEntries)
{
	ManualEntries = InEntries;
	SourceMode = EMachineReadoutSourceMode::Manual;
	RefreshReadout();
}

void AMachineResourceReadoutActor::RefreshReadout()
{
	BindToActiveSource();
	RebuildEntriesFromSource();
	SyncVisualRows();
}

void AMachineResourceReadoutActor::HandleAtomiserStorageChanged()
{
	RefreshReadout();
}

void AMachineResourceReadoutActor::HandleAtomBankStorageChanged()
{
	RefreshReadout();
}

void AMachineResourceReadoutActor::BindToActiveSource()
{
	if (SourceMode == EMachineReadoutSourceMode::Atomiser)
	{
		BindToAtomiserSource();
		UnbindFromAtomBankSource();
		return;
	}

	if (SourceMode == EMachineReadoutSourceMode::AtomBank)
	{
		UnbindFromAtomiserSource();
		BindToAtomBankSource();
		return;
	}

	UnbindFromAtomiserSource();
	UnbindFromAtomBankSource();
}

void AMachineResourceReadoutActor::BindToAtomiserSource()
{
	if (SourceMode != EMachineReadoutSourceMode::Atomiser)
	{
		UnbindFromAtomiserSource();
		return;
	}

	if (!AtomiserVolume)
	{
		AtomiserVolume = ResolveAtomiserSourceCandidate();
	}

	if (BoundAtomiserVolume == AtomiserVolume)
	{
		return;
	}

	if (BoundAtomiserVolume)
	{
		BoundAtomiserVolume->OnStorageChanged.RemoveDynamic(this, &AMachineResourceReadoutActor::HandleAtomiserStorageChanged);
	}

	BoundAtomiserVolume = AtomiserVolume;
	if (BoundAtomiserVolume)
	{
		BoundAtomiserVolume->OnStorageChanged.AddUniqueDynamic(this, &AMachineResourceReadoutActor::HandleAtomiserStorageChanged);
	}
}

void AMachineResourceReadoutActor::UnbindFromAtomiserSource()
{
	if (BoundAtomiserVolume)
	{
		BoundAtomiserVolume->OnStorageChanged.RemoveDynamic(this, &AMachineResourceReadoutActor::HandleAtomiserStorageChanged);
		BoundAtomiserVolume = nullptr;
	}
}

void AMachineResourceReadoutActor::BindToAtomBankSource()
{
	UFactoryResourceBankSubsystem* ResolvedBank = ResolveAtomBankSource();
	if (BoundAtomBank == ResolvedBank)
	{
		return;
	}

	if (BoundAtomBank)
	{
		BoundAtomBank->OnStorageChanged.RemoveDynamic(this, &AMachineResourceReadoutActor::HandleAtomBankStorageChanged);
	}

	BoundAtomBank = ResolvedBank;
	if (BoundAtomBank)
	{
		BoundAtomBank->OnStorageChanged.AddUniqueDynamic(this, &AMachineResourceReadoutActor::HandleAtomBankStorageChanged);
	}
}

void AMachineResourceReadoutActor::UnbindFromAtomBankSource()
{
	if (BoundAtomBank)
	{
		BoundAtomBank->OnStorageChanged.RemoveDynamic(this, &AMachineResourceReadoutActor::HandleAtomBankStorageChanged);
		BoundAtomBank = nullptr;
	}
}

UAtomiserVolume* AMachineResourceReadoutActor::ResolveAtomiserSourceCandidate() const
{
	if (AActor* AttachedParentActor = GetAttachParentActor())
	{
		if (UAtomiserVolume* FoundVolume = AttachedParentActor->FindComponentByClass<UAtomiserVolume>())
		{
			return FoundVolume;
		}
	}

	if (AActor* OwnerActor = GetOwner())
	{
		if (UAtomiserVolume* FoundVolume = OwnerActor->FindComponentByClass<UAtomiserVolume>())
		{
			return FoundVolume;
		}
	}

	return nullptr;
}

UFactoryResourceBankSubsystem* AMachineResourceReadoutActor::ResolveAtomBankSource() const
{
	if (UWorld* World = GetWorld())
	{
		return World->GetSubsystem<UFactoryResourceBankSubsystem>();
	}

	return nullptr;
}

void AMachineResourceReadoutActor::RebuildEntriesFromSource()
{
	CurrentEntries.Reset();

	if (SourceMode == EMachineReadoutSourceMode::Atomiser)
	{
		BuildEntriesFromAtomiser(CurrentEntries);
	}
	else if (SourceMode == EMachineReadoutSourceMode::AtomBank)
	{
		BuildEntriesFromAtomBank(CurrentEntries);
	}
	else if (SourceMode == EMachineReadoutSourceMode::CurrentMachine)
	{
		BuildEntriesFromCurrentMachine(CurrentEntries);
	}
	else
	{
		CurrentEntries = ManualEntries;
		const float SafeReferenceUnits = FMath::Max(1.0f, ReferenceUnits);
		for (FMachineReadoutEntry& Entry : CurrentEntries)
		{
			if (Entry.Label.IsEmpty() && !Entry.ResourceId.IsNone())
			{
				Entry.Label = ResolveResourceLabel(Entry.ResourceId);
			}

			if (Entry.Percent01 <= KINDA_SMALL_NUMBER && Entry.Units > KINDA_SMALL_NUMBER)
			{
				Entry.Percent01 = FMath::Clamp(Entry.Units / SafeReferenceUnits, 0.0f, 1.0f);
			}
			else
			{
				Entry.Percent01 = FMath::Clamp(Entry.Percent01, 0.0f, 1.0f);
			}
		}
	}

	if (CurrentEntries.Num() == 0 && ShouldShowPlaceholderEntries())
	{
		BuildPlaceholderEntries(CurrentEntries);
	}

	CurrentEntries.Sort([](const FMachineReadoutEntry& Left, const FMachineReadoutEntry& Right)
	{
		if (!FMath::IsNearlyEqual(Left.Units, Right.Units))
		{
			return Left.Units > Right.Units;
		}

		return Left.ResourceId.LexicalLess(Right.ResourceId);
	});

	const int32 SafeMaxRows = FMath::Max(1, MaxRows);
	if (CurrentEntries.Num() > SafeMaxRows)
	{
		CurrentEntries.SetNum(SafeMaxRows);
	}
}

void AMachineResourceReadoutActor::BuildPlaceholderEntries(TArray<FMachineReadoutEntry>& OutEntries) const
{
	OutEntries.Reset();

	const int32 SafeRowCount = FMath::Clamp(PlaceholderRowCount, 1, 32);
	const float SafeReferenceUnits = FMath::Max(1.0f, ReferenceUnits);
	const float StepUnits = FMath::Max(0.0f, PlaceholderUnitsStep);

	for (int32 RowIndex = 0; RowIndex < SafeRowCount; ++RowIndex)
	{
		const float Units = FMath::Max(0.0f, SafeReferenceUnits - (StepUnits * static_cast<float>(RowIndex)));

		FMachineReadoutEntry NewEntry;
		NewEntry.ResourceId = FName(*FString::Printf(TEXT("Preview_%02d"), RowIndex + 1));
		NewEntry.Label = FText::FromString(FString::Printf(TEXT("Current Output %02d"), RowIndex + 1));
		NewEntry.Units = Units;
		NewEntry.Percent01 = FMath::Clamp(Units / SafeReferenceUnits, 0.0f, 1.0f);
		OutEntries.Add(NewEntry);
	}
}

bool AMachineResourceReadoutActor::ShouldShowPlaceholderEntries() const
{
#if WITH_EDITOR
	if (!bShowPlaceholderWhenNoData)
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	return World->WorldType == EWorldType::EditorPreview;
#else
	return false;
#endif
}

FVector AMachineResourceReadoutActor::GetRowDirectionVector() const
{
	if (RowLayout == EMachineReadoutRowLayout::Vertical)
	{
		if (HorizontalRowDirection.IsNearlyZero())
		{
			return FVector(0.0f, -1.0f, 0.0f);
		}

		return HorizontalRowDirection.GetSafeNormal();
	}

	return FVector(0.0f, 0.0f, -1.0f);
}

FVector AMachineResourceReadoutActor::GetBarFillDirectionVector() const
{
	if (RowLayout == EMachineReadoutRowLayout::Vertical)
	{
		return FVector::UpVector;
	}

	if (BarFillDirection.IsNearlyZero())
	{
		return FVector(1.0f, 0.0f, 0.0f);
	}

	return BarFillDirection.GetSafeNormal();
}

void AMachineResourceReadoutActor::BuildEntriesFromAtomiser(TArray<FMachineReadoutEntry>& OutEntries) const
{
	OutEntries.Reset();

	const UAtomiserVolume* SourceAtomiser = BoundAtomiserVolume ? BoundAtomiserVolume : AtomiserVolume;
	if (!SourceAtomiser)
	{
		return;
	}

	TArray<FResourceStorageEntry> SnapshotEntries;
	SourceAtomiser->GetStorageSnapshot(SnapshotEntries);

	const float SafeReferenceUnits = FMath::Max(1.0f, ReferenceUnits);
	for (const FResourceStorageEntry& SnapshotEntry : SnapshotEntries)
	{
		if (SnapshotEntry.ResourceId.IsNone() || SnapshotEntry.Units <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		FMachineReadoutEntry NewEntry;
		NewEntry.ResourceId = SnapshotEntry.ResourceId;
		NewEntry.Label = ResolveResourceLabel(SnapshotEntry.ResourceId);
		NewEntry.Units = SnapshotEntry.Units;
		NewEntry.Percent01 = FMath::Clamp(SnapshotEntry.Units / SafeReferenceUnits, 0.0f, 1.0f);
		OutEntries.Add(NewEntry);
	}
}

void AMachineResourceReadoutActor::BuildEntriesFromAtomBank(TArray<FMachineReadoutEntry>& OutEntries) const
{
	OutEntries.Reset();

	const UFactoryResourceBankSubsystem* ResourceBank = BoundAtomBank.Get() ? BoundAtomBank.Get() : ResolveAtomBankSource();
	if (!ResourceBank)
	{
		return;
	}

	TArray<FResourceStorageEntry> SnapshotEntries;
	ResourceBank->GetStorageSnapshot(SnapshotEntries);

	const float SafeReferenceUnits = FMath::Max(1.0f, ReferenceUnits);
	for (const FResourceStorageEntry& SnapshotEntry : SnapshotEntries)
	{
		if (SnapshotEntry.ResourceId.IsNone() || SnapshotEntry.Units <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		FMachineReadoutEntry NewEntry;
		NewEntry.ResourceId = SnapshotEntry.ResourceId;
		NewEntry.Label = ResolveResourceLabel(SnapshotEntry.ResourceId);
		NewEntry.Units = SnapshotEntry.Units;
		NewEntry.Percent01 = FMath::Clamp(SnapshotEntry.Units / SafeReferenceUnits, 0.0f, 1.0f);
		OutEntries.Add(NewEntry);
	}
}

void AMachineResourceReadoutActor::BuildEntriesFromCurrentMachine(TArray<FMachineReadoutEntry>& OutEntries) const
{
	OutEntries.Reset();

	TArray<const AActor*> Candidates;
	Candidates.Reserve(3);
	if (const AActor* AttachedParentActor = GetAttachParentActor())
	{
		Candidates.Add(AttachedParentActor);
	}

	if (const AActor* OwnerActor = GetOwner())
	{
		Candidates.Add(OwnerActor);
	}

	Candidates.Add(this);

	for (const AActor* CandidateActor : Candidates)
	{
		if (!CandidateActor)
		{
			continue;
		}

		TMap<FName, int32> CombinedResourceQuantitiesScaled;

		if (const UMachineComponent* MachineComponent = CandidateActor->FindComponentByClass<UMachineComponent>())
		{
			AccumulateScaledResourceMap(MachineComponent->StoredResourcesScaled, CombinedResourceQuantitiesScaled);
		}

		if (const UMachineInputVolumeComponent* MachineInputVolume = CandidateActor->FindComponentByClass<UMachineInputVolumeComponent>())
		{
			AccumulateScaledResourceMap(MachineInputVolume->InputBufferScaled, CombinedResourceQuantitiesScaled);
		}

		if (const UShredderVolumeComponent* ShredderVolume = CandidateActor->FindComponentByClass<UShredderVolumeComponent>())
		{
			AccumulateScaledResourceMap(ShredderVolume->InternalResourceBufferScaled, CombinedResourceQuantitiesScaled);
		}

		if (const UMachineOutputVolumeComponent* MachineOutputVolume = CandidateActor->FindComponentByClass<UMachineOutputVolumeComponent>())
		{
			AccumulateScaledResourceMap(MachineOutputVolume->PendingOutputQuantitiesScaled, CombinedResourceQuantitiesScaled);
		}

		if (const UOutputVolume* OutputVolume = CandidateActor->FindComponentByClass<UOutputVolume>())
		{
			AccumulateScaledResourceMap(OutputVolume->PendingOutputScaled, CombinedResourceQuantitiesScaled);
		}

		if (const UStorageVolumeComponent* StorageVolume = CandidateActor->FindComponentByClass<UStorageVolumeComponent>())
		{
			AccumulateScaledResourceMap(StorageVolume->StoredResourceQuantitiesScaled, CombinedResourceQuantitiesScaled);
		}

		BuildEntriesFromScaledResourceMap(CombinedResourceQuantitiesScaled, OutEntries);
		if (OutEntries.Num() > 0)
		{
			return;
		}
	}
}

void AMachineResourceReadoutActor::BuildEntriesFromScaledResourceMap(const TMap<FName, int32>& ResourceQuantitiesScaled, TArray<FMachineReadoutEntry>& OutEntries) const
{
	OutEntries.Reset();

	TArray<FName> ResourceIds;
	ResourceQuantitiesScaled.GetKeys(ResourceIds);
	ResourceIds.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});

	const float SafeReferenceUnits = FMath::Max(1.0f, ReferenceUnits);
	for (const FName ResourceId : ResourceIds)
	{
		const int32 QuantityScaled = FMath::Max(0, ResourceQuantitiesScaled.FindRef(ResourceId));
		if (ResourceId.IsNone() || QuantityScaled <= 0)
		{
			continue;
		}

		const float Units = AgentResource::ScaledToUnits(QuantityScaled);

		FMachineReadoutEntry NewEntry;
		NewEntry.ResourceId = ResourceId;
		NewEntry.Label = ResolveResourceLabel(ResourceId);
		NewEntry.Units = Units;
		NewEntry.Percent01 = FMath::Clamp(Units / SafeReferenceUnits, 0.0f, 1.0f);
		OutEntries.Add(NewEntry);
	}
}

FText AMachineResourceReadoutActor::ResolveResourceLabel(FName ResourceId) const
{
	if (ResourceId.IsNone())
	{
		return FText::FromString(TEXT("Unknown"));
	}

	for (TObjectIterator<UMaterialDefinitionAsset> It; It; ++It)
	{
		const UMaterialDefinitionAsset* MaterialDefinition = *It;
		if (!MaterialDefinition || MaterialDefinition->GetResolvedMaterialId() != ResourceId)
		{
			continue;
		}

		if (!MaterialDefinition->DisplayName.IsEmpty())
		{
			return MaterialDefinition->DisplayName;
		}

		break;
	}

	return FText::FromName(ResourceId);
}

void AMachineResourceReadoutActor::SyncVisualRows()
{
	const int32 DesiredCount = FMath::Max(0, CurrentEntries.Num());
	EnsureRowVisualCount(DesiredCount);

	for (int32 RowIndex = 0; RowIndex < RowVisuals.Num(); ++RowIndex)
	{
		if (RowIndex < DesiredCount)
		{
			ConfigureRowVisual(RowIndex, CurrentEntries[RowIndex]);
			SetRowVisualEnabled(RowIndex, true);
		}
		else
		{
			SetRowVisualEnabled(RowIndex, false);
		}
	}
}

void AMachineResourceReadoutActor::EnsureRowVisualCount(int32 DesiredCount)
{
	RowVisuals.RemoveAll([](const FMachineReadoutRowVisual& RowVisual)
	{
		return !IsValid(RowVisual.RowRoot) || !IsValid(RowVisual.LabelText) || !IsValid(RowVisual.FillBar);
	});

	while (RowVisuals.Num() < DesiredCount)
	{
		FMachineReadoutRowVisual NewVisual;

		NewVisual.RowRoot = NewObject<USceneComponent>(this);
		if (NewVisual.RowRoot)
		{
			AddInstanceComponent(NewVisual.RowRoot);
			NewVisual.RowRoot->SetupAttachment(SceneRoot);
			NewVisual.RowRoot->RegisterComponent();
		}

		NewVisual.LabelText = NewObject<UTextRenderComponent>(this);
		if (NewVisual.LabelText)
		{
			AddInstanceComponent(NewVisual.LabelText);
			NewVisual.LabelText->SetupAttachment(NewVisual.RowRoot);
			NewVisual.LabelText->SetHorizontalAlignment(EHTA_Left);
			NewVisual.LabelText->SetVerticalAlignment(EVRTA_TextCenter);
			NewVisual.LabelText->SetMobility(EComponentMobility::Movable);
			NewVisual.LabelText->SetCanEverAffectNavigation(false);
			NewVisual.LabelText->RegisterComponent();
		}

		NewVisual.FillBar = NewObject<UStaticMeshComponent>(this);
		if (NewVisual.FillBar)
		{
			AddInstanceComponent(NewVisual.FillBar);
			NewVisual.FillBar->SetupAttachment(NewVisual.RowRoot);
			NewVisual.FillBar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			NewVisual.FillBar->SetCollisionResponseToAllChannels(ECR_Ignore);
			NewVisual.FillBar->SetGenerateOverlapEvents(false);
			NewVisual.FillBar->SetCanEverAffectNavigation(false);
			NewVisual.FillBar->SetMobility(EComponentMobility::Movable);
			if (BarMesh)
			{
				NewVisual.FillBar->SetStaticMesh(BarMesh);
			}
			NewVisual.FillBar->RegisterComponent();
		}

		RowVisuals.Add(NewVisual);
	}
}

void AMachineResourceReadoutActor::ConfigureRowVisual(int32 RowIndex, const FMachineReadoutEntry& Entry)
{
	if (!RowVisuals.IsValidIndex(RowIndex))
	{
		return;
	}

	FMachineReadoutRowVisual& RowVisual = RowVisuals[RowIndex];
	if (!RowVisual.RowRoot || !RowVisual.LabelText || !RowVisual.FillBar)
	{
		return;
	}

	const FVector RowDirection = GetRowDirectionVector();
	RowVisual.RowRoot->SetRelativeLocation(RowDirection * (FMath::Max(1.0f, RowSpacing) * static_cast<float>(RowIndex)));

	const float SafeReferenceUnits = FMath::Max(1.0f, ReferenceUnits);
	const int32 SafeDecimalPlaces = FMath::Clamp(UnitDisplayDecimalPlaces, 0, 4);
	const FText DisplayLabel = Entry.Label.IsEmpty()
		? FText::FromString(Entry.ResourceId.IsNone() ? TEXT("Unknown") : Entry.ResourceId.ToString())
		: Entry.Label;
	RowVisual.LabelText->SetWorldSize(FMath::Max(1.0f, TextWorldSize));
	RowVisual.LabelText->SetTextRenderColor(TextColor);
	RowVisual.LabelText->SetText(FText::Format(
		NSLOCTEXT("MachineReadout", "RowLabelFormat", "{0}  {1}/{2}"),
		DisplayLabel,
		FormatReadoutUnits(Entry.Units, SafeDecimalPlaces),
		FormatReadoutUnits(SafeReferenceUnits, SafeDecimalPlaces)));

	if (BarMesh && RowVisual.FillBar->GetStaticMesh() != BarMesh)
	{
		RowVisual.FillBar->SetStaticMesh(BarMesh);
	}

	const float Percent01 = FMath::Clamp(Entry.Percent01, 0.0f, 1.0f);
	const float LengthUnits = FMath::Max(0.0f, BarWidth) * Percent01;
	const float WidthScale = FMath::Max(KINDA_SMALL_NUMBER, LengthUnits / 100.0f);
	const float DepthScale = FMath::Max(KINDA_SMALL_NUMBER, FMath::Max(0.1f, BarDepth) / 100.0f);
	const float HeightScale = FMath::Max(KINDA_SMALL_NUMBER, FMath::Max(0.1f, BarHeight) / 100.0f);
	const FVector BarDirection = GetBarFillDirectionVector();
	const FRotator FinalBarRotation = BarDirection.Rotation() + BarRotationOffset;
	const FVector FinalBarDirection = FinalBarRotation.Vector();
	const FVector EffectiveBarStartOffset = (RowLayout == EMachineReadoutRowLayout::Vertical) ? FVector::ZeroVector : BarStartOffset;

	RowVisual.FillBar->SetRelativeScale3D(FVector(WidthScale, DepthScale, HeightScale));
	RowVisual.FillBar->SetRelativeRotation(FinalBarRotation);
	RowVisual.FillBar->SetRelativeLocation(EffectiveBarStartOffset + (FinalBarDirection * (LengthUnits * 0.5f)));
	RowVisual.FillBar->SetVisibility(LengthUnits > KINDA_SMALL_NUMBER, true);

	if (RowLayout == EMachineReadoutRowLayout::Vertical)
	{
		RowVisual.LabelText->SetHorizontalAlignment(EHTA_Center);
		RowVisual.LabelText->SetVerticalAlignment(EVRTA_TextCenter);

		const float MaxBarLengthUnits = FMath::Max(0.0f, BarWidth);
		const float SafeVerticalTextGap = FMath::Max(0.0f, VerticalTextGap);
		const FVector VerticalTextLocation = EffectiveBarStartOffset - (FinalBarDirection * ((MaxBarLengthUnits * 0.5f) + SafeVerticalTextGap));
		RowVisual.LabelText->SetRelativeLocation(VerticalTextLocation + TextOffset);
	}
	else
	{
		RowVisual.LabelText->SetHorizontalAlignment(EHTA_Left);
		RowVisual.LabelText->SetVerticalAlignment(EVRTA_TextCenter);
		RowVisual.LabelText->SetRelativeLocation(TextOffset);
	}
}

void AMachineResourceReadoutActor::SetRowVisualEnabled(int32 RowIndex, bool bEnabled)
{
	if (!RowVisuals.IsValidIndex(RowIndex))
	{
		return;
	}

	FMachineReadoutRowVisual& RowVisual = RowVisuals[RowIndex];
	if (RowVisual.RowRoot)
	{
		RowVisual.RowRoot->SetHiddenInGame(!bEnabled, true);
		if (!bEnabled)
		{
			RowVisual.RowRoot->SetVisibility(false, true);
		}
		else
		{
			RowVisual.RowRoot->SetVisibility(true, false);
		}
	}

	if (RowVisual.LabelText)
	{
		RowVisual.LabelText->SetHiddenInGame(!bEnabled);
		RowVisual.LabelText->SetVisibility(bEnabled, true);
	}

	if (RowVisual.FillBar)
	{
		RowVisual.FillBar->SetHiddenInGame(!bEnabled);
		if (!bEnabled)
		{
			RowVisual.FillBar->SetVisibility(false, true);
		}
	}
}
