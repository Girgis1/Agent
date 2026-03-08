// Copyright Epic Games, Inc. All Rights Reserved.

#include "Machine/MachineResourceReadoutActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Machine/AtomiserVolume.h"
#include "Material/MaterialDefinitionAsset.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectIterator.h"

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
	BindToAtomiserSource();
	RefreshReadout();
}

void AMachineResourceReadoutActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromAtomiserSource();
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
	BindToAtomiserSource();
	RebuildEntriesFromSource();
	SyncVisualRows();
}

void AMachineResourceReadoutActor::HandleAtomiserStorageChanged()
{
	RefreshReadout();
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

void AMachineResourceReadoutActor::RebuildEntriesFromSource()
{
	CurrentEntries.Reset();

	if (SourceMode == EMachineReadoutSourceMode::Atomiser)
	{
		BuildEntriesFromAtomiser(CurrentEntries);
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

void AMachineResourceReadoutActor::BuildEntriesFromAtomiser(TArray<FMachineReadoutEntry>& OutEntries) const
{
	OutEntries.Reset();

	const UAtomiserVolume* SourceAtomiser = BoundAtomiserVolume ? BoundAtomiserVolume : AtomiserVolume;
	if (!SourceAtomiser)
	{
		return;
	}

	TArray<FAtomiserStoredResourceEntry> SnapshotEntries;
	SourceAtomiser->GetStorageSnapshot(SnapshotEntries);

	const float SafeReferenceUnits = FMath::Max(1.0f, ReferenceUnits);
	for (const FAtomiserStoredResourceEntry& SnapshotEntry : SnapshotEntries)
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

FText AMachineResourceReadoutActor::ResolveResourceLabel(FName ResourceId) const
{
	if (ResourceId.IsNone())
	{
		return FText::FromString(TEXT("Unknown"));
	}

	for (TObjectIterator<UMaterialDefinitionAsset> It; It; ++It)
	{
		const UMaterialDefinitionAsset* MaterialDefinition = *It;
		if (!MaterialDefinition || MaterialDefinition->GetResolvedResourceId() != ResourceId)
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

	RowVisual.RowRoot->SetRelativeLocation(FVector(0.0f, 0.0f, -RowSpacing * static_cast<float>(RowIndex)));

	const FString LabelString = Entry.Label.IsEmpty()
		? (Entry.ResourceId.IsNone() ? TEXT("Unknown") : Entry.ResourceId.ToString())
		: Entry.Label.ToString();
	const float SafeReferenceUnits = FMath::Max(1.0f, ReferenceUnits);
	RowVisual.LabelText->SetRelativeLocation(TextOffset);
	RowVisual.LabelText->SetWorldSize(FMath::Max(1.0f, TextWorldSize));
	RowVisual.LabelText->SetTextRenderColor(TextColor);
	RowVisual.LabelText->SetText(FText::FromString(FString::Printf(TEXT("%s  %.1f/%.0f"), *LabelString, Entry.Units, SafeReferenceUnits)));

	if (BarMesh && RowVisual.FillBar->GetStaticMesh() != BarMesh)
	{
		RowVisual.FillBar->SetStaticMesh(BarMesh);
	}

	const float Percent01 = FMath::Clamp(Entry.Percent01, 0.0f, 1.0f);
	const float WidthUnits = FMath::Max(0.0f, BarWidth) * Percent01;
	const float WidthScale = FMath::Max(KINDA_SMALL_NUMBER, WidthUnits / 100.0f);
	const float DepthScale = FMath::Max(KINDA_SMALL_NUMBER, FMath::Max(0.1f, BarDepth) / 100.0f);
	const float HeightScale = FMath::Max(KINDA_SMALL_NUMBER, FMath::Max(0.1f, BarHeight) / 100.0f);

	RowVisual.FillBar->SetRelativeScale3D(FVector(WidthScale, DepthScale, HeightScale));
	RowVisual.FillBar->SetRelativeLocation(BarStartOffset + FVector(WidthUnits * 0.5f, 0.0f, 0.0f));
	RowVisual.FillBar->SetVisibility(WidthUnits > KINDA_SMALL_NUMBER, true);
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
