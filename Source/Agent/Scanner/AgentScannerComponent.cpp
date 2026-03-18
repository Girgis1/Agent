// Copyright Epic Games, Inc. All Rights Reserved.

#include "AgentScannerComponent.h"
#include "AgentBeamToolComponent.h"
#include "AgentScannerReadoutActor.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Factory/MaterialNodeActor.h"
#include "Material/AgentResourceTypes.h"
#include "Material/MaterialComponent.h"
#include "Material/MaterialDefinitionAsset.h"
#include "Misc/ScopeExit.h"
#include "Objects/Components/ObjectFractureComponent.h"
#include "Objects/Components/ObjectHealthComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "UObject/UObjectGlobals.h"

namespace
{
UPrimitiveComponent* ResolveBestPrimitiveOnActorForScanner(AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
	{
		return RootPrimitive;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	UPrimitiveComponent* FirstUsablePrimitive = nullptr;
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent || PrimitiveComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
		{
			continue;
		}

		if (!FirstUsablePrimitive)
		{
			FirstUsablePrimitive = PrimitiveComponent;
		}

		if (PrimitiveComponent->IsSimulatingPhysics())
		{
			return PrimitiveComponent;
		}
	}

	return FirstUsablePrimitive;
}

float ResolvePrimitiveMassKgWithoutWarningForScanner(const UPrimitiveComponent* PrimitiveComponent)
{
	if (!PrimitiveComponent)
	{
		return 0.0f;
	}

	if (const FBodyInstance* BodyInstance = PrimitiveComponent->GetBodyInstance())
	{
		const float BodyMassKg = BodyInstance->GetBodyMass();
		if (BodyMassKg > KINDA_SMALL_NUMBER)
		{
			return FMath::Max(0.0f, BodyMassKg);
		}
	}

	if (PrimitiveComponent->IsSimulatingPhysics())
	{
		return FMath::Max(0.0f, PrimitiveComponent->GetMass());
	}

	return 0.0f;
}

UMaterialDefinitionAsset* FindMaterialDefinitionByIdForScanner(FName ResourceId)
{
	static TMap<FName, TWeakObjectPtr<UMaterialDefinitionAsset>> DefinitionCache;

	if (ResourceId.IsNone())
	{
		return nullptr;
	}

	if (const TWeakObjectPtr<UMaterialDefinitionAsset>* CachedDefinition = DefinitionCache.Find(ResourceId))
	{
		if (CachedDefinition->IsValid())
		{
			return CachedDefinition->Get();
		}
	}

	for (TObjectIterator<UMaterialDefinitionAsset> It; It; ++It)
	{
		UMaterialDefinitionAsset* Candidate = *It;
		if (Candidate && Candidate->GetResolvedMaterialId() == ResourceId)
		{
			DefinitionCache.Add(ResourceId, Candidate);
			return Candidate;
		}
	}

	return nullptr;
}

FText FormatFloatText(float Value, int32 DecimalPlaces)
{
	FNumberFormattingOptions FormattingOptions;
	FormattingOptions.MinimumFractionalDigits = FMath::Clamp(DecimalPlaces, 0, 4);
	FormattingOptions.MaximumFractionalDigits = FMath::Clamp(DecimalPlaces, 0, 4);
	return FText::AsNumber(Value, &FormattingOptions);
}

FText FormatUnitsText(float Units)
{
	return FText::Format(
		NSLOCTEXT("AgentScanner", "UnitsFormat", "{0}u"),
		FormatFloatText(Units, Units >= 10.0f ? 0 : 1));
}

FText FormatMassText(float MassKg)
{
	return FText::Format(
		NSLOCTEXT("AgentScanner", "MassFormat", "{0} kg"),
		FormatFloatText(MassKg, MassKg >= 10.0f ? 0 : 1));
}

bool IsMeaningfulScannerText(const FText& Text)
{
	return !Text.ToString().TrimStartAndEnd().IsEmpty();
}

float ComputeScannerColorLuminance(const FLinearColor& Color)
{
	return (0.2126f * Color.R) + (0.7152f * Color.G) + (0.0722f * Color.B);
}

FLinearColor EnsureReadableScannerTint(const FLinearColor& CandidateColor, const FLinearColor& FallbackColor)
{
	FLinearColor SafeColor = CandidateColor;
	SafeColor.R = FMath::Clamp(SafeColor.R, 0.0f, 1.0f);
	SafeColor.G = FMath::Clamp(SafeColor.G, 0.0f, 1.0f);
	SafeColor.B = FMath::Clamp(SafeColor.B, 0.0f, 1.0f);
	SafeColor.A = SafeColor.A > KINDA_SMALL_NUMBER ? SafeColor.A : 1.0f;

	constexpr float MinReadableLuminance = 0.22f;
	const float CurrentLuminance = ComputeScannerColorLuminance(SafeColor);
	if (CurrentLuminance >= MinReadableLuminance)
	{
		return SafeColor;
	}

	FLinearColor LiftTarget = FallbackColor;
	LiftTarget.R = FMath::Clamp(LiftTarget.R, 0.0f, 1.0f);
	LiftTarget.G = FMath::Clamp(LiftTarget.G, 0.0f, 1.0f);
	LiftTarget.B = FMath::Clamp(LiftTarget.B, 0.0f, 1.0f);
	LiftTarget.A = SafeColor.A;
	if (ComputeScannerColorLuminance(LiftTarget) < MinReadableLuminance)
	{
		LiftTarget = FLinearColor(0.85f, 0.88f, 0.92f, SafeColor.A);
	}

	const float LiftAlpha = FMath::Clamp(
		(MinReadableLuminance - CurrentLuminance) / FMath::Max(KINDA_SMALL_NUMBER, 1.0f - CurrentLuminance),
		0.0f,
		0.85f);
	FLinearColor AdjustedColor = FLinearColor::LerpUsingHSV(SafeColor, LiftTarget, LiftAlpha);
	AdjustedColor.A = SafeColor.A;
	return AdjustedColor;
}

FText ResolveMaterialLabel(FName ResourceId)
{
	if (UMaterialDefinitionAsset* Definition = FindMaterialDefinitionByIdForScanner(ResourceId))
	{
		if (IsMeaningfulScannerText(Definition->DisplayName))
		{
			return Definition->DisplayName;
		}
	}

	return ResourceId.IsNone() ? FText::FromString(TEXT("Unknown")) : FText::FromName(ResourceId);
}

FLinearColor ResolveMaterialTint(FName ResourceId, const FLinearColor& FallbackColor)
{
	if (UMaterialDefinitionAsset* Definition = FindMaterialDefinitionByIdForScanner(ResourceId))
	{
		const FLinearColor VisualColor = Definition->GetResolvedVisualColor();
		if (VisualColor.A > KINDA_SMALL_NUMBER)
		{
			return EnsureReadableScannerTint(VisualColor, FallbackColor);
		}

		if (Definition->DebugColor.A > 0)
		{
			return EnsureReadableScannerTint(Definition->DebugColor, FallbackColor);
		}
	}

	return EnsureReadableScannerTint(FallbackColor, FallbackColor);
}

FText ResolveTargetTitle(AActor* TargetActor)
{
	if (!TargetActor)
	{
		return FText::FromString(TEXT("Scanner"));
	}

	if (TargetActor->IsA<AMaterialNodeActor>())
	{
		return NSLOCTEXT("AgentScanner", "MaterialNodeTitle", "Material Node");
	}

	if (const UClass* TargetClass = TargetActor->GetClass())
	{
		return FText::FromString(TargetClass->GetName());
	}

	return FText::FromString(TargetActor->GetName());
}
}

UAgentScannerComponent::UAgentScannerComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAgentScannerComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UAgentScannerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearScanner(true);
	UnbindAllObservedHealthComponents();
	Super::EndPlay(EndPlayReason);
}

AAgentScannerReadoutActor* UAgentScannerComponent::GetReadoutActor() const
{
	return bHasActiveReadout ? ActiveReadout.Actor.Get() : nullptr;
}

void UAgentScannerComponent::UpdateScanner(
	float DeltaTime,
	bool bAimActive,
	const FVector& ViewOrigin,
	const FVector& ViewDirection,
	const FAgentBeamTraceState* BeamTraceState)
{
	ON_SCOPE_EXIT
	{
		SyncObservedHealthComponents();
	};

	if (!GetWorld())
	{
		ClearScanner(true);
		return;
	}

	const FVector SafeViewDirection = ViewDirection.GetSafeNormal();
	const bool bHasViewerLocation = !ViewOrigin.IsNearlyZero();
	const bool bCanUpdateAim = bHasViewerLocation && !SafeViewDirection.IsNearlyZero();

	UpdateDwellingReadouts(DeltaTime, ViewOrigin, SafeViewDirection, bHasViewerLocation);

	if (bHasActiveReadout && !IsReadoutTargetValid(ActiveReadout))
	{
		ResetCandidate();
		ClearActiveReadout(true);
	}

	if (!bScannerEnabled)
	{
		ClearScanner(true);
		return;
	}

	if (!bAimActive || !bCanUpdateAim)
	{
		bWasAimActiveLastTick = false;
		ResetCandidate();

		if (bHasActiveReadout)
		{
			if (ActiveReadout.bLocked)
			{
				DetachActiveReadoutToDwelling();
			}
			else
			{
				ClearActiveReadout(true);
			}
		}

		return;
	}

	bWasAimActiveLastTick = true;

	FScannerTraceResult TraceResult;
	if (!ResolveTraceResult(ViewOrigin, SafeViewDirection, BeamTraceState, TraceResult)
		|| !TraceResult.bHasHit
		|| !TraceResult.HitActor.IsValid())
	{
		if (!TryHoldCandidateProgress(DeltaTime, ViewOrigin, SafeViewDirection, bHasViewerLocation, true))
		{
			ResetCandidate();
			if (bHasActiveReadout)
			{
				if (ActiveReadout.bLocked)
				{
					DetachActiveReadoutToDwelling();
				}
				else
				{
					ClearActiveReadout(true);
				}
			}
		}
		return;
	}

	FAgentScannerPresentation FullPresentation;
	FLinearColor DominantColor = DefaultAccentColor;
	if (!BuildPresentationForTrace(TraceResult, FullPresentation, DominantColor))
	{
		if (!TryHoldCandidateProgress(DeltaTime, ViewOrigin, SafeViewDirection, bHasViewerLocation, true))
		{
			ResetCandidate();
			if (bHasActiveReadout)
			{
				if (ActiveReadout.bLocked)
				{
					DetachActiveReadoutToDwelling();
				}
				else
				{
					ClearActiveReadout(true);
				}
			}
		}
		return;
	}

	bool bReadoutWasAlreadyLocked = false;
	if (!ActivateReadoutForTarget(TraceResult.HitActor.Get(), bReadoutWasAlreadyLocked))
	{
		ResetCandidate();
		ClearActiveReadout(true);
		return;
	}

	if (!IsSameCandidateActor(TraceResult))
	{
		CandidateActor = TraceResult.HitActor;
		CandidateComponent = TraceResult.HitComponent;
		CandidateImpactPoint = TraceResult.ImpactPoint;
		CandidateImpactNormal = TraceResult.ImpactNormal;
		CandidateResetGraceRemaining = 0.0f;
		CandidateDwellTime = bReadoutWasAlreadyLocked
			? FMath::Max(ScanLockDuration, FMath::Max(0.0f, DeltaTime))
			: FMath::Max(0.0f, DeltaTime);
		bHasLockedScan = bReadoutWasAlreadyLocked;
	}
	else
	{
		CandidateComponent = TraceResult.HitComponent;
		CandidateImpactPoint = TraceResult.ImpactPoint;
		CandidateImpactNormal = TraceResult.ImpactNormal;
		CandidateResetGraceRemaining = 0.0f;
		if (!bHasLockedScan)
		{
			CandidateDwellTime += FMath::Max(0.0f, DeltaTime);
		}
		else
		{
			CandidateDwellTime = FMath::Max(CandidateDwellTime, ScanLockDuration);
		}
	}

	const float Progress01 = ScanLockDuration > KINDA_SMALL_NUMBER
		? FMath::Clamp(CandidateDwellTime / ScanLockDuration, 0.0f, 1.0f)
		: 1.0f;
	bHasLockedScan = bHasLockedScan || Progress01 >= 1.0f;

	FAgentScannerPresentation DisplayPresentation = FullPresentation;
	DisplayPresentation.bHasTarget = true;
	DisplayPresentation.ScanProgress01 = bHasLockedScan ? 1.0f : Progress01;
	DisplayPresentation.bScanComplete = bHasLockedScan;
	DisplayPresentation.AccentColor = DominantColor;
	DisplayPresentation.Subtitle = bHasLockedScan
		? NSLOCTEXT("AgentScanner", "ScanCompleteSubtitle", "Passive Scan")
		: NSLOCTEXT("AgentScanner", "ScanningSubtitle", "Analyzing target...");

	if (!bHasLockedScan)
	{
		DisplayPresentation.Rows.Reset();
	}

	FVector PanelLocation = TraceResult.ImpactPoint;
	FVector AnchorLocation = TraceResult.ImpactPoint;
	SolvePanelPlacement(
		ViewOrigin,
		SafeViewDirection,
		ActiveReadout.Actor.Get(),
		DisplayPresentation,
		TraceResult,
		PanelLocation,
		AnchorLocation);

	ActiveReadout.TargetActor = TraceResult.HitActor;
	ActiveReadout.TargetComponent = TraceResult.HitComponent;
	ActiveReadout.Presentation = DisplayPresentation;
	ActiveReadout.PanelLocation = PanelLocation;
	ActiveReadout.AnchorLocation = AnchorLocation;
	ActiveReadout.ViewerLocation = ViewOrigin;
	ActiveReadout.bLocked = bHasLockedScan;
	ConfigureReadoutLiveRefresh(ActiveReadout);
	CacheReadoutAnchor(ActiveReadout, TraceResult);
	CurrentPresentation = DisplayPresentation;

	UpdateReadoutActor(ActiveReadout, 1.0f, true);
}

void UAgentScannerComponent::ClearScanner(bool bImmediate)
{
	ResetCandidate();
	CurrentPresentation = FAgentScannerPresentation();
	bWasAimActiveLastTick = false;

	if (bImmediate)
	{
		ClearActiveReadout(true);
		for (FScannerReadoutState& DwellingReadout : DwellingReadouts)
		{
			DestroyReadoutState(DwellingReadout);
		}
		DwellingReadouts.Reset();
		SyncObservedHealthComponents();
		return;
	}

	if (bHasActiveReadout)
	{
		if (ActiveReadout.bLocked)
		{
			DetachActiveReadoutToDwelling();
		}
		else
		{
			ClearActiveReadout(true);
		}
	}

	SyncObservedHealthComponents();
}

bool UAgentScannerComponent::ResolveTraceResult(
	const FVector& ViewOrigin,
	const FVector& ViewDirection,
	const FAgentBeamTraceState* BeamTraceState,
	FScannerTraceResult& OutTraceResult) const
{
	OutTraceResult = FScannerTraceResult();
	OutTraceResult.TraceEnd = ViewOrigin + (ViewDirection * FMath::Max(100.0f, ScannerTraceDistance));

	if (BeamTraceState && BeamTraceState->bHasHit && BeamTraceState->HitActor.Get() != nullptr)
	{
		OutTraceResult.bHasHit = true;
		OutTraceResult.ImpactPoint = BeamTraceState->ImpactPoint;
		OutTraceResult.ImpactNormal = BeamTraceState->ImpactNormal;
		OutTraceResult.HitActor = BeamTraceState->HitActor;
		OutTraceResult.HitComponent = BeamTraceState->HitComponent;
		OutTraceResult.TraceEnd = BeamTraceState->EndPoint;
		return true;
	}

	UWorld* World = GetWorld();
	AActor* OwnerActor = GetOwner();
	if (!World || !OwnerActor)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AgentScannerTrace), false, OwnerActor);
	QueryParams.bReturnPhysicalMaterial = false;
	QueryParams.bTraceComplex = false;
	QueryParams.AddIgnoredActor(OwnerActor);

	TArray<AActor*> AttachedActors;
	OwnerActor->GetAttachedActors(AttachedActors, true, true);
	for (AActor* AttachedActor : AttachedActors)
	{
		if (AttachedActor)
		{
			QueryParams.AddIgnoredActor(AttachedActor);
		}
	}

	FHitResult HitResult;
	const bool bHasHit = World->LineTraceSingleByChannel(
		HitResult,
		ViewOrigin,
		OutTraceResult.TraceEnd,
		TraceChannel,
		QueryParams);

	OutTraceResult.bHasHit = bHasHit;
	if (!bHasHit)
	{
		return true;
	}

	OutTraceResult.ImpactPoint = HitResult.ImpactPoint.IsNearlyZero() ? HitResult.Location : HitResult.ImpactPoint;
	OutTraceResult.ImpactNormal = HitResult.ImpactNormal;
	OutTraceResult.HitActor = HitResult.GetActor();
	OutTraceResult.HitComponent = HitResult.GetComponent();
	return true;
}

bool UAgentScannerComponent::BuildPresentationForTrace(
	const FScannerTraceResult& TraceResult,
	FAgentScannerPresentation& OutPresentation,
	FLinearColor& OutDominantColor) const
{
	OutPresentation = FAgentScannerPresentation();
	OutDominantColor = DefaultAccentColor;

	AActor* TargetActor = TraceResult.HitActor.Get();
	if (!TargetActor)
	{
		return false;
	}

	OutPresentation.bHasTarget = true;
	OutPresentation.Title = ResolveTargetTitle(TargetActor);

	TArray<FAgentScannerRowPresentation> Rows;
	Rows.Reserve(10);

	if (bShowHealthInScan)
	{
		AddHealthRow(TargetActor, Rows, OutDominantColor);
	}

	if (bShowMassInScan)
	{
		AddMassRow(TargetActor, TraceResult.HitComponent.Get(), Rows);
	}

	if (!BuildNodeRows(TargetActor, Rows, OutDominantColor))
	{
		BuildMaterialRows(TargetActor, TraceResult.HitComponent.Get(), Rows, OutDominantColor);
	}

	if (bShowSalvageYieldInScan)
	{
		AddSalvageRow(TargetActor, Rows);
	}

	if (bShowFractureRiskInScan)
	{
		AddFractureRiskRow(TargetActor, Rows);
	}

	OutPresentation.Rows = MoveTemp(Rows);
	OutPresentation.AccentColor = OutDominantColor;
	return OutPresentation.Rows.Num() > 0;
}

bool UAgentScannerComponent::BuildMaterialRows(
	AActor* TargetActor,
	UPrimitiveComponent* HitPrimitive,
	TArray<FAgentScannerRowPresentation>& OutRows,
	FLinearColor& OutDominantColor) const
{
	if (!TargetActor)
	{
		return false;
	}

	UMaterialComponent* MaterialComponent = TargetActor->FindComponentByClass<UMaterialComponent>();
	if (!MaterialComponent)
	{
		return false;
	}

	UPrimitiveComponent* SourcePrimitive = HitPrimitive ? HitPrimitive : ResolveBestPrimitiveOnActorForScanner(TargetActor);
	TMap<FName, int32> ResourceQuantitiesScaled;
	if (!MaterialComponent->BuildResolvedResourceQuantitiesScaled(SourcePrimitive, ResourceQuantitiesScaled))
	{
		return false;
	}

	TArray<TPair<FName, int32>> SortedEntries;
	SortedEntries.Reserve(ResourceQuantitiesScaled.Num());
	for (const TPair<FName, int32>& Entry : ResourceQuantitiesScaled)
	{
		if (!Entry.Key.IsNone() && Entry.Value > 0)
		{
			SortedEntries.Add(Entry);
		}
	}

	SortedEntries.Sort([](const TPair<FName, int32>& Left, const TPair<FName, int32>& Right)
	{
		if (Left.Value != Right.Value)
		{
			return Left.Value > Right.Value;
		}

		return Left.Key.LexicalLess(Right.Key);
	});

	const int32 SafeMaxRows = FMath::Max(1, MaxMaterialRows);
	int32 DisplayedRows = 0;
	for (const TPair<FName, int32>& Entry : SortedEntries)
	{
		if (DisplayedRows >= SafeMaxRows)
		{
			break;
		}

		FAgentScannerRowPresentation Row;
		Row.Label = ResolveMaterialLabel(Entry.Key);
		Row.Value = FormatUnitsText(AgentResource::ScaledToUnits(Entry.Value));
		Row.Tint = ResolveMaterialTint(Entry.Key, DefaultAccentColor);
		Row.bUseTint = true;
		OutRows.Add(Row);

		if (DisplayedRows == 0)
		{
			OutDominantColor = Row.Tint;
		}

		++DisplayedRows;
	}

	if (SortedEntries.Num() > SafeMaxRows)
	{
		FAgentScannerRowPresentation OverflowRow;
		OverflowRow.Label = NSLOCTEXT("AgentScanner", "MoreMaterialsLabel", "Additional Materials");
		OverflowRow.Value = FText::Format(
			NSLOCTEXT("AgentScanner", "MoreMaterialsValue", "+{0}"),
			FText::AsNumber(SortedEntries.Num() - SafeMaxRows));
		OverflowRow.Tint = DefaultAccentColor.CopyWithNewOpacity(0.8f);
		OverflowRow.bUseTint = true;
		OutRows.Add(OverflowRow);
	}

	return DisplayedRows > 0;
}

bool UAgentScannerComponent::BuildNodeRows(
	AActor* TargetActor,
	TArray<FAgentScannerRowPresentation>& OutRows,
	FLinearColor& OutDominantColor) const
{
	AMaterialNodeActor* MaterialNode = Cast<AMaterialNodeActor>(TargetActor);
	if (!MaterialNode)
	{
		return false;
	}

	TArray<FMaterialNodeResourcePreview> PreviewEntries;
	MaterialNode->GetResourcePreview(PreviewEntries);
	if (PreviewEntries.Num() == 0)
	{
		return false;
	}

	PreviewEntries.Sort([](const FMaterialNodeResourcePreview& Left, const FMaterialNodeResourcePreview& Right)
	{
		if (Left.RemainingQuantityScaled != Right.RemainingQuantityScaled)
		{
			return Left.RemainingQuantityScaled > Right.RemainingQuantityScaled;
		}

		return Left.ResourceId.LexicalLess(Right.ResourceId);
	});

	const int32 SafeMaxRows = FMath::Max(1, MaxMaterialRows);
	for (int32 Index = 0; Index < PreviewEntries.Num() && Index < SafeMaxRows; ++Index)
	{
		const FMaterialNodeResourcePreview& Preview = PreviewEntries[Index];
		FAgentScannerRowPresentation Row;
		Row.Label = ResolveMaterialLabel(Preview.ResourceId);

		const float RemainingUnits = AgentResource::ScaledToUnits(Preview.RemainingQuantityScaled);
		const float InitialUnits = AgentResource::ScaledToUnits(Preview.InitialQuantityScaled);
		Row.Value = FText::Format(
			NSLOCTEXT("AgentScanner", "NodeMaterialValue", "{0}/{1}u"),
			FormatFloatText(RemainingUnits, RemainingUnits >= 10.0f ? 0 : 1),
			FormatFloatText(InitialUnits, InitialUnits >= 10.0f ? 0 : 1));
		Row.Tint = ResolveMaterialTint(Preview.ResourceId, DefaultAccentColor);
		Row.bUseTint = true;
		OutRows.Add(Row);

		if (Index == 0)
		{
			OutDominantColor = Row.Tint;
		}
	}

	return OutRows.Num() > 0;
}

void UAgentScannerComponent::AddHealthRow(
	AActor* TargetActor,
	TArray<FAgentScannerRowPresentation>& OutRows,
	FLinearColor& OutAccentColor) const
{
	if (!TargetActor)
	{
		return;
	}

	UObjectHealthComponent* HealthComponent = UObjectHealthComponent::FindObjectHealthComponent(TargetActor);
	if (!HealthComponent || !HealthComponent->IsHealthEnabled())
	{
		return;
	}

	const float MaxHealth = HealthComponent->GetMaxHealth();
	if (MaxHealth <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float CurrentHealth = HealthComponent->GetCurrentHealth();
	const float HealthPercent = FMath::Clamp(CurrentHealth / MaxHealth, 0.0f, 1.0f);

	FAgentScannerRowPresentation Row;
	Row.Label = NSLOCTEXT("AgentScanner", "HealthLabel", "Health");
	Row.Value = FText::Format(
		NSLOCTEXT("AgentScanner", "HealthValue", "{0}/{1}"),
		FormatFloatText(CurrentHealth, 0),
		FormatFloatText(MaxHealth, 0));
	Row.Tint = FLinearColor::LerpUsingHSV(
		FLinearColor(1.0f, 0.3f, 0.16f, 1.0f),
		FLinearColor(0.18f, 1.0f, 0.35f, 1.0f),
		HealthPercent);
	Row.bUseTint = true;
	OutRows.Add(Row);

	OutAccentColor = Row.Tint;
}

void UAgentScannerComponent::AddMassRow(
	AActor* TargetActor,
	UPrimitiveComponent* HitPrimitive,
	TArray<FAgentScannerRowPresentation>& OutRows) const
{
	if (!TargetActor)
	{
		return;
	}

	float MassKg = 0.0f;
	if (UObjectHealthComponent* HealthComponent = UObjectHealthComponent::FindObjectHealthComponent(TargetActor))
	{
		MassKg = HealthComponent->ResolveCurrentSourceMassKg();
	}

	if (MassKg <= KINDA_SMALL_NUMBER)
	{
		MassKg = ResolvePrimitiveMassKgWithoutWarningForScanner(HitPrimitive ? HitPrimitive : ResolveBestPrimitiveOnActorForScanner(TargetActor));
	}

	if (MassKg <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	FAgentScannerRowPresentation Row;
	Row.Label = NSLOCTEXT("AgentScanner", "MassLabel", "Mass");
	Row.Value = FormatMassText(MassKg);
	OutRows.Add(Row);
}

void UAgentScannerComponent::AddSalvageRow(AActor* TargetActor, TArray<FAgentScannerRowPresentation>& OutRows) const
{
	if (!TargetActor || !TargetActor->FindComponentByClass<UMaterialComponent>())
	{
		return;
	}

	const float PenaltyPercent = UObjectHealthComponent::GetActorTotalDamagedPenaltyPercent(TargetActor);
	const float YieldPercent = FMath::Clamp(100.0f - PenaltyPercent, 0.0f, 100.0f);

	FAgentScannerRowPresentation Row;
	Row.Label = NSLOCTEXT("AgentScanner", "SalvageYieldLabel", "Salvage Yield");
	Row.Value = FText::Format(
		NSLOCTEXT("AgentScanner", "SalvageYieldValue", "{0}%"),
		FormatFloatText(YieldPercent, 0));
	OutRows.Add(Row);
}

void UAgentScannerComponent::AddFractureRiskRow(AActor* TargetActor, TArray<FAgentScannerRowPresentation>& OutRows) const
{
	if (!TargetActor)
	{
		return;
	}

	UObjectFractureComponent* FractureComponent = TargetActor->FindComponentByClass<UObjectFractureComponent>();
	if (!FractureComponent || !FractureComponent->CanFracture())
	{
		return;
	}

	FText RiskText = NSLOCTEXT("AgentScanner", "FractureRiskPotential", "Potential");
	FLinearColor RiskColor = FLinearColor(0.98f, 0.75f, 0.18f, 1.0f);

	if (UObjectHealthComponent* HealthComponent = UObjectHealthComponent::FindObjectHealthComponent(TargetActor))
	{
		const float MaxHealth = HealthComponent->GetMaxHealth();
		const float CurrentHealth = HealthComponent->GetCurrentHealth();
		const float HealthPercent = MaxHealth > KINDA_SMALL_NUMBER
			? FMath::Clamp(CurrentHealth / MaxHealth, 0.0f, 1.0f)
			: 1.0f;

		if (HealthPercent <= 0.2f)
		{
			RiskText = NSLOCTEXT("AgentScanner", "FractureRiskCritical", "Critical");
			RiskColor = FLinearColor(1.0f, 0.22f, 0.18f, 1.0f);
		}
		else if (HealthPercent <= 0.45f)
		{
			RiskText = NSLOCTEXT("AgentScanner", "FractureRiskHigh", "High");
			RiskColor = FLinearColor(1.0f, 0.52f, 0.16f, 1.0f);
		}
		else if (HealthPercent <= 0.75f)
		{
			RiskText = NSLOCTEXT("AgentScanner", "FractureRiskModerate", "Moderate");
			RiskColor = FLinearColor(0.96f, 0.76f, 0.22f, 1.0f);
		}
		else
		{
			RiskText = NSLOCTEXT("AgentScanner", "FractureRiskLow", "Low");
			RiskColor = FLinearColor(0.34f, 0.92f, 0.42f, 1.0f);
		}
	}

	FAgentScannerRowPresentation Row;
	Row.Label = NSLOCTEXT("AgentScanner", "FractureRiskLabel", "Fracture Risk");
	Row.Value = RiskText;
	Row.Tint = RiskColor;
	Row.bUseTint = true;
	OutRows.Add(Row);
}

void UAgentScannerComponent::ResetCandidate()
{
	CandidateActor.Reset();
	CandidateComponent.Reset();
	CandidateImpactPoint = FVector::ZeroVector;
	CandidateImpactNormal = FVector::ZeroVector;
	CandidateDwellTime = 0.0f;
	CandidateResetGraceRemaining = 0.0f;
	bHasLockedScan = false;
}

void UAgentScannerComponent::DestroyReadoutState(FScannerReadoutState& ReadoutState)
{
	if (ReadoutState.Actor.IsValid())
	{
		ReadoutState.Actor->Destroy();
	}

	ReadoutState = FScannerReadoutState();
}

void UAgentScannerComponent::ClearActiveReadout(bool bDestroyActor)
{
	if (!bHasActiveReadout)
	{
		return;
	}

	if (bDestroyActor)
	{
		DestroyReadoutState(ActiveReadout);
	}
	else if (ActiveReadout.Actor.IsValid())
	{
		ActiveReadout.Actor->HideReadout();
	}

	ActiveReadout = FScannerReadoutState();
	bHasActiveReadout = false;
	CurrentPresentation = FAgentScannerPresentation();
}

void UAgentScannerComponent::DetachActiveReadoutToDwelling()
{
	if (!bHasActiveReadout)
	{
		return;
	}

	if (!ActiveReadout.Actor.IsValid() || !ActiveReadout.bLocked || ScanLingerDuration <= KINDA_SMALL_NUMBER || !IsReadoutTargetValid(ActiveReadout))
	{
		ClearActiveReadout(true);
		return;
	}

	ActiveReadout.LingerTimeRemaining = ScanLingerDuration;
	DwellingReadouts.Add(ActiveReadout);
	EnforceDwellingLimit();
	ActiveReadout = FScannerReadoutState();
	bHasActiveReadout = false;
	CurrentPresentation = FAgentScannerPresentation();
}

bool UAgentScannerComponent::IsReadoutTargetValid(const FScannerReadoutState& ReadoutState) const
{
	AActor* TargetActor = ReadoutState.TargetActor.Get();
	if (!TargetActor || TargetActor->IsActorBeingDestroyed())
	{
		return false;
	}

	if (ReadoutState.TargetComponent.IsValid())
	{
		UPrimitiveComponent* TargetComponent = ReadoutState.TargetComponent.Get();
		if (!TargetComponent
			|| TargetComponent->GetOwner() != TargetActor
			|| !TargetComponent->IsRegistered())
		{
			return false;
		}
	}

	return true;
}

void UAgentScannerComponent::ConfigureReadoutLiveRefresh(FScannerReadoutState& ReadoutState) const
{
	AActor* TargetActor = ReadoutState.TargetActor.Get();
	ReadoutState.bLiveRefreshPresentation =
		TargetActor != nullptr
		&& TargetActor->IsA<AMaterialNodeActor>()
		&& NodeReadoutRefreshInterval > KINDA_SMALL_NUMBER;
	ReadoutState.PresentationRefreshTimeRemaining = ReadoutState.bLiveRefreshPresentation
		? NodeReadoutRefreshInterval
		: 0.0f;
}

void UAgentScannerComponent::CacheReadoutAnchor(FScannerReadoutState& ReadoutState, const FScannerTraceResult& TraceResult) const
{
	ReadoutState.TargetComponent = TraceResult.HitComponent;
	ReadoutState.bFollowTargetMotion = false;
	ReadoutState.LocalAnchorLocation = FVector::ZeroVector;
	ReadoutState.LocalAnchorNormal = FVector::UpVector;

	UPrimitiveComponent* HitComponent = TraceResult.HitComponent.Get();
	if (!HitComponent)
	{
		return;
	}

	ReadoutState.bFollowTargetMotion =
		HitComponent->IsSimulatingPhysics()
		|| HitComponent->Mobility == EComponentMobility::Movable;

	if (!ReadoutState.bFollowTargetMotion)
	{
		return;
	}

	const FTransform ComponentTransform = HitComponent->GetComponentTransform();
	ReadoutState.LocalAnchorLocation = ComponentTransform.InverseTransformPosition(TraceResult.ImpactPoint);

	const FVector SafeImpactNormal = TraceResult.ImpactNormal.GetSafeNormal();
	ReadoutState.LocalAnchorNormal = SafeImpactNormal.IsNearlyZero()
		? FVector::UpVector
		: ComponentTransform.InverseTransformVectorNoScale(SafeImpactNormal).GetSafeNormal();
}

void UAgentScannerComponent::RefreshReadoutAnchorFromTarget(
	FScannerReadoutState& ReadoutState,
	const FVector& ViewOrigin,
	const FVector& ViewDirection,
	bool bHasViewerLocation)
{
	if (!ReadoutState.bFollowTargetMotion || !ReadoutState.TargetComponent.IsValid())
	{
		return;
	}

	UPrimitiveComponent* TargetComponent = ReadoutState.TargetComponent.Get();
	if (!TargetComponent)
	{
		return;
	}

	const FTransform ComponentTransform = TargetComponent->GetComponentTransform();
	const FVector UpdatedImpactPoint = ComponentTransform.TransformPosition(ReadoutState.LocalAnchorLocation);
	FVector UpdatedImpactNormal = ComponentTransform.TransformVectorNoScale(ReadoutState.LocalAnchorNormal).GetSafeNormal();
	if (UpdatedImpactNormal.IsNearlyZero())
	{
		UpdatedImpactNormal = FVector::UpVector;
	}

	FScannerTraceResult UpdatedTraceResult;
	UpdatedTraceResult.bHasHit = true;
	UpdatedTraceResult.ImpactPoint = UpdatedImpactPoint;
	UpdatedTraceResult.ImpactNormal = UpdatedImpactNormal;
	UpdatedTraceResult.HitActor = ReadoutState.TargetActor;
	UpdatedTraceResult.HitComponent = ReadoutState.TargetComponent;

	const FVector EffectiveViewDirection = !ViewDirection.IsNearlyZero()
		? ViewDirection
		: (UpdatedImpactPoint - ViewOrigin).GetSafeNormal();
	if (!bHasViewerLocation || EffectiveViewDirection.IsNearlyZero())
	{
		ReadoutState.AnchorLocation = UpdatedImpactPoint;
		return;
	}

	FVector UpdatedPanelLocation = UpdatedImpactPoint;
	FVector UpdatedAnchorLocation = UpdatedImpactPoint;
	SolvePanelPlacement(
		ViewOrigin,
		EffectiveViewDirection,
		ReadoutState.Actor.Get(),
		ReadoutState.Presentation,
		UpdatedTraceResult,
		UpdatedPanelLocation,
		UpdatedAnchorLocation);

	ReadoutState.PanelLocation = UpdatedPanelLocation;
	ReadoutState.AnchorLocation = UpdatedAnchorLocation;
}

bool UAgentScannerComponent::RefreshReadoutPresentation(FScannerReadoutState& ReadoutState) const
{
	if (!IsReadoutTargetValid(ReadoutState))
	{
		return false;
	}

	FScannerTraceResult TraceResult;
	TraceResult.bHasHit = true;
	TraceResult.HitActor = ReadoutState.TargetActor;
	TraceResult.HitComponent = ReadoutState.TargetComponent;
	TraceResult.ImpactPoint = ReadoutState.AnchorLocation;

	if (ReadoutState.TargetComponent.IsValid())
	{
		const UPrimitiveComponent* TargetComponent = ReadoutState.TargetComponent.Get();
		const FTransform ComponentTransform = TargetComponent->GetComponentTransform();
		FVector RefreshedNormal = ComponentTransform.TransformVectorNoScale(ReadoutState.LocalAnchorNormal).GetSafeNormal();
		if (RefreshedNormal.IsNearlyZero())
		{
			RefreshedNormal = FVector::UpVector;
		}
		TraceResult.ImpactNormal = RefreshedNormal;
	}
	else
	{
		TraceResult.ImpactNormal = FVector::UpVector;
	}

	FAgentScannerPresentation RefreshedPresentation;
	FLinearColor DominantColor = DefaultAccentColor;
	if (!BuildPresentationForTrace(TraceResult, RefreshedPresentation, DominantColor))
	{
		return false;
	}

	RefreshedPresentation.bHasTarget = true;
	RefreshedPresentation.ScanProgress01 = ReadoutState.bLocked ? 1.0f : ReadoutState.Presentation.ScanProgress01;
	RefreshedPresentation.bScanComplete = ReadoutState.bLocked;
	RefreshedPresentation.AccentColor = DominantColor;
	RefreshedPresentation.Subtitle = ReadoutState.bLocked
		? NSLOCTEXT("AgentScanner", "ScanCompleteSubtitle", "Passive Scan")
		: NSLOCTEXT("AgentScanner", "ScanningSubtitle", "Analyzing target...");

	if (!ReadoutState.bLocked)
	{
		RefreshedPresentation.Rows.Reset();
	}

	ReadoutState.Presentation = RefreshedPresentation;
	return true;
}

void UAgentScannerComponent::UpdateDwellingReadouts(float DeltaTime, const FVector& ViewOrigin, const FVector& ViewDirection, bool bHasViewerLocation)
{
	for (int32 Index = DwellingReadouts.Num() - 1; Index >= 0; --Index)
	{
		FScannerReadoutState& DwellingReadout = DwellingReadouts[Index];
		if (!DwellingReadout.Actor.IsValid() || !IsReadoutTargetValid(DwellingReadout))
		{
			DestroyReadoutState(DwellingReadout);
			DwellingReadouts.RemoveAt(Index);
			continue;
		}

		DwellingReadout.LingerTimeRemaining = FMath::Max(0.0f, DwellingReadout.LingerTimeRemaining - FMath::Max(0.0f, DeltaTime));
		const float Alpha = ScanLingerDuration > KINDA_SMALL_NUMBER
			? FMath::Clamp(DwellingReadout.LingerTimeRemaining / ScanLingerDuration, 0.0f, 1.0f)
			: 0.0f;

		if (Alpha <= KINDA_SMALL_NUMBER)
		{
			DestroyReadoutState(DwellingReadout);
			DwellingReadouts.RemoveAt(Index);
			continue;
		}

		if (DwellingReadout.bLiveRefreshPresentation)
		{
			DwellingReadout.PresentationRefreshTimeRemaining -= FMath::Max(0.0f, DeltaTime);
			if (DwellingReadout.PresentationRefreshTimeRemaining <= KINDA_SMALL_NUMBER)
			{
				DwellingReadout.PresentationRefreshTimeRemaining = NodeReadoutRefreshInterval;
				if (!RefreshReadoutPresentation(DwellingReadout))
				{
					DestroyReadoutState(DwellingReadout);
					DwellingReadouts.RemoveAt(Index);
					continue;
				}
			}
		}

		if (bHasViewerLocation)
		{
			DwellingReadout.ViewerLocation = ViewOrigin;
		}

		RefreshReadoutAnchorFromTarget(DwellingReadout, ViewOrigin, ViewDirection, bHasViewerLocation);

		UpdateReadoutActor(DwellingReadout, Alpha, true);
	}
}

void UAgentScannerComponent::EnforceDwellingLimit()
{
	const int32 SafeMaxDwellingReadouts = FMath::Max(1, MaxDwellingReadouts);
	while (DwellingReadouts.Num() > SafeMaxDwellingReadouts)
	{
		DestroyReadoutState(DwellingReadouts[0]);
		DwellingReadouts.RemoveAt(0);
	}
}

AAgentScannerReadoutActor* UAgentScannerComponent::SpawnReadoutActor()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UClass* SpawnClass = ReadoutActorClass ? ReadoutActorClass.Get() : nullptr;
	if (!SpawnClass)
	{
		SpawnClass = LoadClass<AAgentScannerReadoutActor>(nullptr, TEXT("/Game/Scanner/BP_ScannerUI.BP_ScannerUI_C"));
	}

	if (!SpawnClass)
	{
		SpawnClass = AAgentScannerReadoutActor::StaticClass();
	}

	if (!SpawnClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwner();
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	return World->SpawnActor<AAgentScannerReadoutActor>(
		SpawnClass,
		FTransform::Identity,
		SpawnParameters);
}

bool UAgentScannerComponent::ActivateReadoutForTarget(AActor* TargetActor, bool& bOutWasAlreadyLocked)
{
	bOutWasAlreadyLocked = false;

	if (!TargetActor)
	{
		return false;
	}

	if (bHasActiveReadout && ActiveReadout.TargetActor.Get() == TargetActor && ActiveReadout.Actor.IsValid())
	{
		bOutWasAlreadyLocked = ActiveReadout.bLocked;
		return true;
	}

	if (bHasActiveReadout)
	{
		if (IsReadoutTargetValid(ActiveReadout))
		{
			DetachActiveReadoutToDwelling();
		}
		else
		{
			ClearActiveReadout(true);
		}
	}

	const int32 ExistingIndex = FindDwellingReadoutIndexByTarget(TargetActor);
	if (ExistingIndex != INDEX_NONE)
	{
		ActiveReadout = DwellingReadouts[ExistingIndex];
		DwellingReadouts.RemoveAt(ExistingIndex);
		ActiveReadout.LingerTimeRemaining = 0.0f;
		ConfigureReadoutLiveRefresh(ActiveReadout);
		bHasActiveReadout = ActiveReadout.Actor.IsValid();
		bOutWasAlreadyLocked = ActiveReadout.bLocked;
		return bHasActiveReadout;
	}

	ActiveReadout = FScannerReadoutState();
	ActiveReadout.Actor = SpawnReadoutActor();
	ActiveReadout.TargetActor = TargetActor;
	ActiveReadout.bLocked = false;
	ConfigureReadoutLiveRefresh(ActiveReadout);
	bHasActiveReadout = ActiveReadout.Actor.IsValid();
	return bHasActiveReadout;
}

int32 UAgentScannerComponent::FindDwellingReadoutIndexByTarget(AActor* TargetActor) const
{
	if (!TargetActor)
	{
		return INDEX_NONE;
	}

	for (int32 Index = 0; Index < DwellingReadouts.Num(); ++Index)
	{
		if (DwellingReadouts[Index].TargetActor.Get() == TargetActor && DwellingReadouts[Index].Actor.IsValid())
		{
			return Index;
		}
	}

	return INDEX_NONE;
}

void UAgentScannerComponent::UpdateReadoutActor(FScannerReadoutState& ReadoutState, float Alpha, bool bShouldDisplay)
{
	if (!ReadoutState.Actor.IsValid())
	{
		return;
	}

	ReadoutState.Actor->UpdateReadout(
		ReadoutState.Presentation,
		ReadoutState.PanelLocation,
		ReadoutState.AnchorLocation,
		ReadoutState.ViewerLocation,
		Alpha,
		bShouldDisplay);
}

bool UAgentScannerComponent::SolvePanelPlacement(
	const FVector& ViewOrigin,
	const FVector& ViewDirection,
	AAgentScannerReadoutActor* PlacementReadoutActor,
	const FAgentScannerPresentation& Presentation,
	const FScannerTraceResult& TraceResult,
	FVector& OutPanelLocation,
	FVector& OutAnchorLocation)
{
	OutAnchorLocation = TraceResult.ImpactPoint;
	OutPanelLocation = TraceResult.ImpactPoint;

	AActor* TargetActor = TraceResult.HitActor.Get();
	UWorld* World = GetWorld();
	AActor* OwnerActor = GetOwner();
	if (!TargetActor || !World || !OwnerActor)
	{
		return false;
	}

	FVector BoundsOrigin = FVector::ZeroVector;
	FVector BoundsExtent = FVector::ZeroVector;
	TargetActor->GetActorBounds(true, BoundsOrigin, BoundsExtent);

	FVector SurfaceNormal = TraceResult.ImpactNormal.GetSafeNormal();
	if (SurfaceNormal.IsNearlyZero())
	{
		SurfaceNormal = (ViewOrigin - TraceResult.ImpactPoint).GetSafeNormal();
	}

	FVector TowardViewer = (ViewOrigin - TraceResult.ImpactPoint).GetSafeNormal();
	if (TowardViewer.IsNearlyZero())
	{
		TowardViewer = (-ViewDirection).GetSafeNormal();
	}

	FVector RightVector = FVector::CrossProduct(FVector::UpVector, TowardViewer).GetSafeNormal();
	if (RightVector.IsNearlyZero())
	{
		RightVector = FVector::CrossProduct(ViewDirection, FVector::UpVector).GetSafeNormal();
	}

	FVector UpVector = FVector::CrossProduct(RightVector, TowardViewer).GetSafeNormal();
	if (UpVector.IsNearlyZero())
	{
		UpVector = FVector::UpVector;
	}

	const FVector SurfaceAnchor = TraceResult.ImpactPoint + (SurfaceNormal * FMath::Max(0.0f, SurfaceAnchorOffset));
	const FVector BaseLocation = SurfaceAnchor
		+ (TowardViewer * FMath::Max(0.0f, TowardViewerOffset))
		+ (UpVector * FMath::Max(0.0f, VerticalPlacementOffset));

	FVector PlacementLocalCenter = FVector::ZeroVector;
	FVector PlacementLocalExtent = FVector::ZeroVector;
	if (bUseDynamicReadoutBoundsForPlacement && PlacementReadoutActor)
	{
		PlacementReadoutActor->PreparePlacementMeasurement(Presentation, ViewOrigin);
		PlacementReadoutActor->GetPlacementLocalBounds(PlacementLocalCenter, PlacementLocalExtent);
	}

	const FVector FallbackPlacementExtent = PlacementClearanceExtents.ComponentMax(FVector(2.0f, 2.0f, 2.0f));
	const FVector PlacementExtent = (bUseDynamicReadoutBoundsForPlacement && PlacementReadoutActor)
		? (PlacementLocalExtent + PlacementClearanceExtents).ComponentMax(FVector(2.0f, 2.0f, 2.0f))
		: FallbackPlacementExtent;

	auto MakeCandidateRotation = [&](const FVector& CandidateLocation) -> FQuat
	{
		const FVector ToViewerFromCandidate = (ViewOrigin - CandidateLocation).GetSafeNormal();
		if (ToViewerFromCandidate.IsNearlyZero())
		{
			return FQuat::Identity;
		}

		FRotator CandidateRotation = ToViewerFromCandidate.Rotation();
		CandidateRotation.Roll = 0.0f;
		return CandidateRotation.Quaternion();
	};

	auto ResolveBoundsCenter = [&](const FVector& CandidateLocation, const FQuat& CandidateRotation) -> FVector
	{
		return CandidateLocation + CandidateRotation.RotateVector(PlacementLocalCenter);
	};

	auto ScoreLocation = [&](const FVector& CandidateLocation, FVector& OutResolvedLocation, FVector& OutResolvedBoundsCenter) -> float
	{
		const float SafeLiftStepHeight = FMath::Max(0.0f, PlacementLiftStepHeight);
		const int32 SafeMaxLiftSteps = FMath::Max(0, MaxPlacementLiftSteps);

		OutResolvedLocation = CandidateLocation;
		FQuat CandidateRotation = MakeCandidateRotation(OutResolvedLocation);
		OutResolvedBoundsCenter = ResolveBoundsCenter(OutResolvedLocation, CandidateRotation);

		float Score = FVector::DistSquared(CandidateLocation, BaseLocation) * 0.0025f;
		const FVector ToCandidateBounds = (OutResolvedBoundsCenter - ViewOrigin).GetSafeNormal();
		if (FVector::DotProduct(ToCandidateBounds, ViewDirection) <= 0.1f)
		{
			Score += 100000.0f;
		}

		const FVector ExpandedExtent = BoundsExtent + PlacementExtent;
		const bool bInsideExpandedBounds =
			FMath::Abs(OutResolvedBoundsCenter.X - BoundsOrigin.X) <= ExpandedExtent.X
			&& FMath::Abs(OutResolvedBoundsCenter.Y - BoundsOrigin.Y) <= ExpandedExtent.Y
			&& FMath::Abs(OutResolvedBoundsCenter.Z - BoundsOrigin.Z) <= ExpandedExtent.Z;
		if (bInsideExpandedBounds)
		{
			Score += 50000.0f;
		}

		FCollisionQueryParams OverlapParams(SCENE_QUERY_STAT(AgentScannerPlacementOverlap), false, OwnerActor);
		OverlapParams.AddIgnoredActor(OwnerActor);
		OverlapParams.AddIgnoredActor(TargetActor);

		TArray<FOverlapResult> OverlapResults;
		bool bHasOverlap = World->OverlapMultiByChannel(
			OverlapResults,
			OutResolvedBoundsCenter,
			CandidateRotation,
			TraceChannel,
			FCollisionShape::MakeBox(PlacementExtent),
			OverlapParams);

		int32 LiftStepsUsed = 0;
		while (bHasOverlap && LiftStepsUsed < SafeMaxLiftSteps && SafeLiftStepHeight > KINDA_SMALL_NUMBER)
		{
			++LiftStepsUsed;
			OutResolvedLocation += (FVector::UpVector * SafeLiftStepHeight);
			CandidateRotation = MakeCandidateRotation(OutResolvedLocation);
			OutResolvedBoundsCenter = ResolveBoundsCenter(OutResolvedLocation, CandidateRotation);
			OverlapResults.Reset();
			bHasOverlap = World->OverlapMultiByChannel(
				OverlapResults,
				OutResolvedBoundsCenter,
				CandidateRotation,
				TraceChannel,
				FCollisionShape::MakeBox(PlacementExtent),
				OverlapParams);
		}

		Score += static_cast<float>(LiftStepsUsed) * 180.0f;

		FCollisionQueryParams VisibilityParams(SCENE_QUERY_STAT(AgentScannerPlacementLOS), false, OwnerActor);
		VisibilityParams.AddIgnoredActor(OwnerActor);
		VisibilityParams.AddIgnoredActor(TargetActor);

		FHitResult VisibilityHit;
		if (World->LineTraceSingleByChannel(
			VisibilityHit,
			ViewOrigin,
			OutResolvedBoundsCenter,
			TraceChannel,
			VisibilityParams))
		{
			Score += 5000.0f;
		}

		if (bHasOverlap)
		{
			Score += 2500.0f + (OverlapResults.Num() * 200.0f);
		}

		return Score;
	};

	TArray<FVector> CandidateLocations;
	CandidateLocations.Add(BaseLocation);
	CandidateLocations.Add(BaseLocation + (RightVector * SidePlacementOffset));
	CandidateLocations.Add(BaseLocation - (RightVector * SidePlacementOffset));
	CandidateLocations.Add(BaseLocation + (UpVector * SecondaryVerticalPlacementOffset));
	CandidateLocations.Add(BaseLocation + (UpVector * (SecondaryVerticalPlacementOffset * 0.5f)) + (RightVector * (SidePlacementOffset * 0.5f)));
	CandidateLocations.Add(BaseLocation + (UpVector * (SecondaryVerticalPlacementOffset * 0.5f)) - (RightVector * (SidePlacementOffset * 0.5f)));

	float BestScore = TNumericLimits<float>::Max();
	FVector BestLocation = BaseLocation;

	for (const FVector& CandidateLocation : CandidateLocations)
	{
		FVector ResolvedLocation = CandidateLocation;
		FVector ResolvedBoundsCenter = CandidateLocation;
		const float Score = ScoreLocation(CandidateLocation, ResolvedLocation, ResolvedBoundsCenter);

		if (Score < BestScore)
		{
			BestScore = Score;
			BestLocation = ResolvedLocation;
		}
	}

	OutAnchorLocation = TraceResult.ImpactPoint;
	OutPanelLocation = BestLocation;
	return true;
}

bool UAgentScannerComponent::IsSameCandidateActor(const FScannerTraceResult& TraceResult) const
{
	if (!CandidateActor.IsValid() || !TraceResult.HitActor.IsValid())
	{
		return false;
	}

	return CandidateActor.Get() == TraceResult.HitActor.Get();
}

bool UAgentScannerComponent::TryHoldCandidateProgress(
	float DeltaTime,
	const FVector& ViewOrigin,
	const FVector& ViewDirection,
	bool bHasViewerLocation,
	bool bAllowHoldDisplay)
{
	if (bHasLockedScan || !CandidateActor.IsValid() || CandidateDwellTime <= KINDA_SMALL_NUMBER || !bHasActiveReadout || !ActiveReadout.Actor.IsValid())
	{
		return false;
	}

	if (ScanProgressResetDelay <= KINDA_SMALL_NUMBER)
	{
		ResetCandidate();
		ClearActiveReadout(true);
		return true;
	}

	if (CandidateResetGraceRemaining <= KINDA_SMALL_NUMBER)
	{
		CandidateResetGraceRemaining = ScanProgressResetDelay;
	}

	CandidateResetGraceRemaining = FMath::Max(0.0f, CandidateResetGraceRemaining - FMath::Max(0.0f, DeltaTime));

	if (bAllowHoldDisplay && ActiveReadout.Presentation.bHasTarget)
	{
		if (bHasViewerLocation)
		{
			ActiveReadout.ViewerLocation = ViewOrigin;
		}

		RefreshReadoutAnchorFromTarget(ActiveReadout, ViewOrigin, ViewDirection, bHasViewerLocation);
		CurrentPresentation = ActiveReadout.Presentation;
		UpdateReadoutActor(ActiveReadout, 1.0f, true);
	}

	if (CandidateResetGraceRemaining <= KINDA_SMALL_NUMBER)
	{
		ResetCandidate();
		ClearActiveReadout(true);
	}

	return true;
}

void UAgentScannerComponent::SyncObservedHealthComponents()
{
	TSet<UObjectHealthComponent*> DesiredHealthComponents;
	if (bShowHealthInScan)
	{
		auto AddStateHealthComponent = [this, &DesiredHealthComponents](const FScannerReadoutState& ReadoutState)
		{
			AActor* TargetActor = ReadoutState.TargetActor.Get();
			if (!TargetActor || !IsReadoutTargetValid(ReadoutState))
			{
				return;
			}

			if (UObjectHealthComponent* HealthComponent = UObjectHealthComponent::FindObjectHealthComponent(TargetActor))
			{
				DesiredHealthComponents.Add(HealthComponent);
			}
		};

		if (bHasActiveReadout)
		{
			AddStateHealthComponent(ActiveReadout);
		}

		for (const FScannerReadoutState& DwellingReadout : DwellingReadouts)
		{
			AddStateHealthComponent(DwellingReadout);
		}
	}

	for (int32 Index = ObservedHealthComponents.Num() - 1; Index >= 0; --Index)
	{
		UObjectHealthComponent* HealthComponent = ObservedHealthComponents[Index].Get();
		if (!HealthComponent || !DesiredHealthComponents.Contains(HealthComponent))
		{
			if (HealthComponent)
			{
				HealthComponent->OnHealthChanged.RemoveDynamic(this, &UAgentScannerComponent::HandleTrackedTargetHealthChanged);
			}

			ObservedHealthComponents.RemoveAt(Index);
		}
	}

	for (UObjectHealthComponent* DesiredHealthComponent : DesiredHealthComponents)
	{
		if (!DesiredHealthComponent)
		{
			continue;
		}

		bool bAlreadyObserved = false;
		for (const TWeakObjectPtr<UObjectHealthComponent>& ObservedHealthComponent : ObservedHealthComponents)
		{
			if (ObservedHealthComponent.Get() == DesiredHealthComponent)
			{
				bAlreadyObserved = true;
				break;
			}
		}

		if (bAlreadyObserved)
		{
			continue;
		}

		DesiredHealthComponent->OnHealthChanged.AddUniqueDynamic(this, &UAgentScannerComponent::HandleTrackedTargetHealthChanged);
		ObservedHealthComponents.Add(DesiredHealthComponent);
	}
}

void UAgentScannerComponent::UnbindAllObservedHealthComponents()
{
	for (const TWeakObjectPtr<UObjectHealthComponent>& ObservedHealthComponent : ObservedHealthComponents)
	{
		if (UObjectHealthComponent* HealthComponent = ObservedHealthComponent.Get())
		{
			HealthComponent->OnHealthChanged.RemoveDynamic(this, &UAgentScannerComponent::HandleTrackedTargetHealthChanged);
		}
	}

	ObservedHealthComponents.Reset();
}

void UAgentScannerComponent::RefreshTrackedReadoutsForHealthChange()
{
	if (bHasActiveReadout)
	{
		if (!ActiveReadout.Actor.IsValid() || !IsReadoutTargetValid(ActiveReadout))
		{
			ClearActiveReadout(true);
		}
		else if (RefreshReadoutPresentation(ActiveReadout))
		{
			CurrentPresentation = ActiveReadout.Presentation;
			UpdateReadoutActor(ActiveReadout, 1.0f, true);
		}
	}

	for (int32 Index = DwellingReadouts.Num() - 1; Index >= 0; --Index)
	{
		FScannerReadoutState& DwellingReadout = DwellingReadouts[Index];
		if (!DwellingReadout.Actor.IsValid() || !IsReadoutTargetValid(DwellingReadout))
		{
			DestroyReadoutState(DwellingReadout);
			DwellingReadouts.RemoveAt(Index);
			continue;
		}

		if (!RefreshReadoutPresentation(DwellingReadout))
		{
			continue;
		}

		const float Alpha = ScanLingerDuration > KINDA_SMALL_NUMBER
			? FMath::Clamp(DwellingReadout.LingerTimeRemaining / ScanLingerDuration, 0.0f, 1.0f)
			: 1.0f;
		UpdateReadoutActor(DwellingReadout, Alpha, true);
	}
}

void UAgentScannerComponent::HandleTrackedTargetHealthChanged(
	float PreviousHealth,
	float NewHealth,
	float MaxHealth,
	float TotalDamagedPenaltyPercent)
{
	(void)PreviousHealth;
	(void)NewHealth;
	(void)MaxHealth;
	(void)TotalDamagedPenaltyPercent;

	RefreshTrackedReadoutsForHealthChange();
	SyncObservedHealthComponents();
}
