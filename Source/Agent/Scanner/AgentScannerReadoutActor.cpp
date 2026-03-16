// Copyright Epic Games, Inc. All Rights Reserved.

#include "AgentScannerReadoutActor.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
void ConfigureSceneComponent(USceneComponent* SceneComponent)
{
	if (!SceneComponent)
	{
		return;
	}

	SceneComponent->SetMobility(EComponentMobility::Movable);
	SceneComponent->bEditableWhenInherited = true;
}

FLinearColor MultiplyOpacity(const FLinearColor& InColor, float AlphaMultiplier)
{
	FLinearColor Result = InColor;
	Result.A *= FMath::Clamp(AlphaMultiplier, 0.0f, 1.0f);
	return Result;
}

bool IsDescendantOfComponent(const USceneComponent* ChildComponent, const USceneComponent* PotentialAncestor)
{
	const USceneComponent* CurrentComponent = ChildComponent;
	while (CurrentComponent)
	{
		if (CurrentComponent == PotentialAncestor)
		{
			return true;
		}

		CurrentComponent = CurrentComponent->GetAttachParent();
	}

	return false;
}
}

AAgentScannerReadoutActor::AAgentScannerReadoutActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	ConfigureSceneComponent(SceneRoot);

	PanelRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PanelRoot"));
	PanelRoot->SetupAttachment(SceneRoot);
	ConfigureSceneComponent(PanelRoot);

	DecorRoot = CreateDefaultSubobject<USceneComponent>(TEXT("DecorRoot"));
	DecorRoot->SetupAttachment(PanelRoot);
	ConfigureSceneComponent(DecorRoot);

	TextRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TextRoot"));
	TextRoot->SetupAttachment(PanelRoot);
	ConfigureSceneComponent(TextRoot);

	TitleAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("TitleAnchor"));
	TitleAnchor->SetupAttachment(TextRoot);
	TitleAnchor->SetRelativeLocation(FVector(0.0f, 0.0f, 20.0f));
	ConfigureSceneComponent(TitleAnchor);

	SubtitleAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("SubtitleAnchor"));
	SubtitleAnchor->SetupAttachment(TextRoot);
	SubtitleAnchor->SetRelativeLocation(FVector(0.0f, 0.0f, 10.0f));
	ConfigureSceneComponent(SubtitleAnchor);

	RowsAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("RowsAnchor"));
	RowsAnchor->SetupAttachment(TextRoot);
	RowsAnchor->SetRelativeLocation(FVector(0.0f, 0.0f, -6.0f));
	ConfigureSceneComponent(RowsAnchor);

	ProgressAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("ProgressAnchor"));
	ProgressAnchor->SetupAttachment(TextRoot);
	ProgressAnchor->SetRelativeLocation(FVector(0.0f, -14.0f, 16.0f));
	ConfigureSceneComponent(ProgressAnchor);

	ProgressBarRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ProgressBarRoot"));
	ProgressBarRoot->SetupAttachment(ProgressAnchor);
	ConfigureSceneComponent(ProgressBarRoot);

	LeaderEnd = CreateDefaultSubobject<USceneComponent>(TEXT("LeaderEnd"));
	LeaderEnd->SetupAttachment(DecorRoot);
	LeaderEnd->SetRelativeLocation(FVector(0.0f, -8.0f, 10.0f));
	ConfigureSceneComponent(LeaderEnd);

	TitleText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("TitleText"));
	TitleText->SetupAttachment(TitleAnchor);
	TitleText->SetVerticalAlignment(EVRTA_TextCenter);
	TitleText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TitleText->SetCollisionResponseToAllChannels(ECR_Ignore);
	TitleText->SetGenerateOverlapEvents(false);
	TitleText->SetCastShadow(false);
	TitleText->SetCanEverAffectNavigation(false);
	TitleText->SetMobility(EComponentMobility::Movable);
	TitleText->bEditableWhenInherited = true;

	SubtitleText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("SubtitleText"));
	SubtitleText->SetupAttachment(SubtitleAnchor);
	SubtitleText->SetVerticalAlignment(EVRTA_TextCenter);
	SubtitleText->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SubtitleText->SetCollisionResponseToAllChannels(ECR_Ignore);
	SubtitleText->SetGenerateOverlapEvents(false);
	SubtitleText->SetCastShadow(false);
	SubtitleText->SetCanEverAffectNavigation(false);
	SubtitleText->SetMobility(EComponentMobility::Movable);
	SubtitleText->bEditableWhenInherited = true;

	LeaderLineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeaderLineMesh"));
	LeaderLineMesh->SetupAttachment(SceneRoot);
	LeaderLineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LeaderLineMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	LeaderLineMesh->SetGenerateOverlapEvents(false);
	LeaderLineMesh->SetCastShadow(false);
	LeaderLineMesh->SetCanEverAffectNavigation(false);
	LeaderLineMesh->SetMobility(EComponentMobility::Movable);
	LeaderLineMesh->bEditableWhenInherited = true;

	LeaderArmMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeaderArmMesh"));
	LeaderArmMesh->SetupAttachment(SceneRoot);
	LeaderArmMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LeaderArmMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	LeaderArmMesh->SetGenerateOverlapEvents(false);
	LeaderArmMesh->SetCastShadow(false);
	LeaderArmMesh->SetCanEverAffectNavigation(false);
	LeaderArmMesh->SetMobility(EComponentMobility::Movable);
	LeaderArmMesh->bEditableWhenInherited = true;

	LeaderStartSphereMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("LeaderStartSphereMesh"));
	LeaderStartSphereMesh->SetupAttachment(SceneRoot);
	LeaderStartSphereMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	LeaderStartSphereMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	LeaderStartSphereMesh->SetGenerateOverlapEvents(false);
	LeaderStartSphereMesh->SetCastShadow(false);
	LeaderStartSphereMesh->SetCanEverAffectNavigation(false);
	LeaderStartSphereMesh->SetMobility(EComponentMobility::Movable);
	LeaderStartSphereMesh->bEditableWhenInherited = true;

	ProgressBackgroundMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProgressBackgroundMesh"));
	ProgressBackgroundMesh->SetupAttachment(ProgressBarRoot);
	ProgressBackgroundMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProgressBackgroundMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	ProgressBackgroundMesh->SetGenerateOverlapEvents(false);
	ProgressBackgroundMesh->SetCastShadow(false);
	ProgressBackgroundMesh->SetCanEverAffectNavigation(false);
	ProgressBackgroundMesh->SetMobility(EComponentMobility::Movable);
	ProgressBackgroundMesh->bEditableWhenInherited = true;

	ProgressFillMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ProgressFillMesh"));
	ProgressFillMesh->SetupAttachment(ProgressBarRoot);
	ProgressFillMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProgressFillMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	ProgressFillMesh->SetGenerateOverlapEvents(false);
	ProgressFillMesh->SetCastShadow(false);
	ProgressFillMesh->SetCanEverAffectNavigation(false);
	ProgressFillMesh->SetMobility(EComponentMobility::Movable);
	ProgressFillMesh->bEditableWhenInherited = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		LeaderMesh = CylinderMesh.Object;
		LeaderLineMesh->SetStaticMesh(LeaderMesh);
		LeaderArmMesh->SetStaticMesh(LeaderMesh);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded())
	{
		LeaderSphereMesh = SphereMesh.Object;
		LeaderStartSphereMesh->SetStaticMesh(LeaderSphereMesh);
	}

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		ProgressBarMesh = PlaneMesh.Object;
		ProgressBackgroundMesh->SetStaticMesh(ProgressBarMesh);
		ProgressFillMesh->SetStaticMesh(ProgressBarMesh);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> ShapeMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (ShapeMaterial.Succeeded())
	{
		ScannerMasterMaterial = ShapeMaterial.Object;
		LeaderLineMesh->SetMaterial(0, ScannerMasterMaterial);
		LeaderArmMesh->SetMaterial(0, ScannerMasterMaterial);
		LeaderStartSphereMesh->SetMaterial(0, ScannerMasterMaterial);
		ProgressBackgroundMesh->SetMaterial(0, ScannerMasterMaterial);
		ProgressFillMesh->SetMaterial(0, ScannerMasterMaterial);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> DefaultScannerTextMaterial(
		TEXT("/Engine/EngineMaterials/DefaultTextMaterialTranslucent.DefaultTextMaterialTranslucent"));
	if (DefaultScannerTextMaterial.Succeeded())
	{
		ScannerTextMaterial = DefaultScannerTextMaterial.Object;
	}

	static ConstructorHelpers::FObjectFinder<UFont> DefaultScannerFont(TEXT("/Engine/EngineFonts/RobotoDistanceField.RobotoDistanceField"));
	if (DefaultScannerFont.Succeeded())
	{
		ScannerTextFont = DefaultScannerFont.Object;
	}

	PreviewTitle = NSLOCTEXT("AgentScannerReadoutActor", "PreviewTitle", "Scanner Target");
	PreviewSubtitle = NSLOCTEXT("AgentScannerReadoutActor", "PreviewSubtitle", "Passive Scan");

	FAgentScannerRowPresentation PreviewHealthRow;
	PreviewHealthRow.Label = NSLOCTEXT("AgentScannerReadoutActor", "PreviewHealthLabel", "Health");
	PreviewHealthRow.Value = FText::FromString(TEXT("65/100"));
	PreviewHealthRow.Tint = FLinearColor(0.38f, 1.0f, 0.52f, 1.0f);
	PreviewHealthRow.bUseTint = true;
	PreviewRows.Add(PreviewHealthRow);

	FAgentScannerRowPresentation PreviewMaterialRow;
	PreviewMaterialRow.Label = FText::FromString(TEXT("Iron"));
	PreviewMaterialRow.Value = FText::FromString(TEXT("24u"));
	PreviewMaterialRow.Tint = FLinearColor(0.98f, 0.78f, 0.26f, 1.0f);
	PreviewMaterialRow.bUseTint = true;
	PreviewRows.Add(PreviewMaterialRow);

	FAgentScannerRowPresentation PreviewRiskRow;
	PreviewRiskRow.Label = NSLOCTEXT("AgentScannerReadoutActor", "PreviewRiskLabel", "Fracture Risk");
	PreviewRiskRow.Value = FText::FromString(TEXT("Moderate"));
	PreviewRiskRow.Tint = FLinearColor(1.0f, 0.72f, 0.22f, 1.0f);
	PreviewRiskRow.bUseTint = true;
	PreviewRows.Add(PreviewRiskRow);

	ApplyLayoutSettings();

	SetActorHiddenInGame(true);
	SetActorEnableCollision(false);
}

void AAgentScannerReadoutActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RefreshVisualLayout();

	if (GetWorld() && !GetWorld()->IsGameWorld() && bShowPreviewInEditor)
	{
		Presentation = BuildPreviewPresentation();
		DesiredPanelLocation = GetActorLocation();
		DesiredAnchorLocation = GetActorLocation() - (GetActorForwardVector() * 18.0f);
		SmoothedPanelLocation = DesiredPanelLocation;
		SmoothedAnchorLocation = DesiredAnchorLocation;
		DisplayAlpha = 1.0f;
		bShouldDisplay = true;
		bHasInitializedTransforms = true;
		ApplyPresentationToVisuals();
	}
	else if (GetWorld() && !GetWorld()->IsGameWorld())
	{
		HideReadout();
	}
}

void AAgentScannerReadoutActor::BeginPlay()
{
	Super::BeginPlay();
	RefreshVisualLayout();
	UpdateLeaderLine(0.0f);
}

void AAgentScannerReadoutActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	FloatTime += FMath::Max(0.0f, DeltaSeconds) * FMath::Max(0.0f, FloatSpeed);

	if (!bHasInitializedTransforms)
	{
		SmoothedPanelLocation = DesiredPanelLocation;
		SmoothedAnchorLocation = DesiredAnchorLocation;
		bHasInitializedTransforms = true;
	}

	const float SafePositionInterp = FMath::Max(0.0f, PositionInterpSpeed);
	const float SafeRotationInterp = FMath::Max(0.0f, RotationInterpSpeed);
	SmoothedPanelLocation = SafePositionInterp > KINDA_SMALL_NUMBER
		? FMath::VInterpTo(SmoothedPanelLocation, DesiredPanelLocation, DeltaSeconds, SafePositionInterp)
		: DesiredPanelLocation;
	SmoothedAnchorLocation = SafePositionInterp > KINDA_SMALL_NUMBER
		? FMath::VInterpTo(SmoothedAnchorLocation, DesiredAnchorLocation, DeltaSeconds, SafePositionInterp)
		: DesiredAnchorLocation;

	const FVector FloatOffset = FVector::UpVector * (FMath::Sin(FloatTime) * FloatAmplitude);
	const FVector FinalPanelLocation = SmoothedPanelLocation + FloatOffset;
	SetActorLocation(FinalPanelLocation);

	if (!ViewerLocation.IsNearlyZero())
	{
		const FVector ToViewer = (ViewerLocation - FinalPanelLocation).GetSafeNormal();
		if (!ToViewer.IsNearlyZero())
		{
			FRotator TargetRotation = ToViewer.Rotation();
			TargetRotation.Roll = 0.0f;
			const FRotator SmoothedRotation = SafeRotationInterp > KINDA_SMALL_NUMBER
				? FMath::RInterpTo(GetActorRotation(), TargetRotation, DeltaSeconds, SafeRotationInterp)
				: TargetRotation;
			SetActorRotation(SmoothedRotation);
		}
	}

	UpdateLeaderLine(DisplayAlpha);

	const bool bVisible = bShouldDisplay && DisplayAlpha > KINDA_SMALL_NUMBER;
	SetActorHiddenInGame(!bVisible);
	if (TitleText)
	{
		TitleText->SetVisibility(bVisible, true);
	}
	if (SubtitleText)
	{
		SubtitleText->SetVisibility(bVisible, true);
	}
}

void AAgentScannerReadoutActor::UpdateReadout(
	const FAgentScannerPresentation& InPresentation,
	const FVector& InDesiredPanelLocation,
	const FVector& InAnchorLocation,
	const FVector& InViewerLocation,
	float InDisplayAlpha,
	bool bInShouldDisplay)
{
	Presentation = InPresentation;
	DesiredPanelLocation = InDesiredPanelLocation;
	DesiredAnchorLocation = InAnchorLocation;
	ViewerLocation = InViewerLocation;
	DisplayAlpha = FMath::Clamp(InDisplayAlpha, 0.0f, 1.0f);
	bShouldDisplay = bInShouldDisplay;

	if (bShouldDisplay && bSnapToNextReadoutLocation)
	{
		SmoothedPanelLocation = DesiredPanelLocation;
		SmoothedAnchorLocation = DesiredAnchorLocation;
		SetActorLocation(DesiredPanelLocation);
		bHasInitializedTransforms = true;
		bSnapToNextReadoutLocation = false;
	}

	ApplyPresentationToVisuals();
}

void AAgentScannerReadoutActor::HideReadout()
{
	DisplayAlpha = 0.0f;
	bShouldDisplay = false;
	bSnapToNextReadoutLocation = true;
	ApplyPresentationToVisuals();
	SetActorHiddenInGame(true);

	if (LeaderLineMesh)
	{
		LeaderLineMesh->SetVisibility(false, true);
	}
	if (LeaderArmMesh)
	{
		LeaderArmMesh->SetVisibility(false, true);
	}
	if (LeaderStartSphereMesh)
	{
		LeaderStartSphereMesh->SetVisibility(false, true);
	}
	if (ProgressBackgroundMesh)
	{
		ProgressBackgroundMesh->SetVisibility(false, true);
	}
	if (ProgressFillMesh)
	{
		ProgressFillMesh->SetVisibility(false, true);
	}
}

void AAgentScannerReadoutActor::RefreshVisualLayout()
{
	ApplyLayoutSettings();
	ApplyPresentationToVisuals();
}

void AAgentScannerReadoutActor::PreparePlacementMeasurement(
	const FAgentScannerPresentation& InPresentation,
	const FVector& InViewerLocation)
{
	Presentation = InPresentation;
	ViewerLocation = InViewerLocation;
	DisplayAlpha = 1.0f;
	bShouldDisplay = true;
	ApplyLayoutSettings();
	ApplyPresentationToVisuals();
}

bool AAgentScannerReadoutActor::GetPlacementLocalBounds(FVector& OutLocalCenter, FVector& OutLocalExtent) const
{
	return GetPanelContentLocalBounds(OutLocalCenter, OutLocalExtent);
}

void AAgentScannerReadoutActor::GetGeneratedTextComponents(
	TArray<UTextRenderComponent*>& OutRowTextComponents,
	TArray<UTextRenderComponent*>& OutProgressTextComponents) const
{
	OutRowTextComponents.Reset();
	for (UTextRenderComponent* TextComponent : RowTextComponents)
	{
		if (TextComponent)
		{
			OutRowTextComponents.Add(TextComponent);
		}
	}

	OutProgressTextComponents.Reset();
}

void AAgentScannerReadoutActor::HandlePresentationApplied_Implementation()
{
}

void AAgentScannerReadoutActor::ApplyLayoutSettings()
{
	if (PanelRoot && bApplyPanelTransformSettings)
	{
		PanelRoot->SetRelativeLocation(PanelOffset);
		PanelRoot->SetRelativeRotation(PanelRotationOffset);
		PanelRoot->SetRelativeScale3D(PanelScale);
	}

	if (TextRoot)
	{
		TextRoot->SetRelativeLocation(ContentOffset);
	}

	if (DecorRoot)
	{
		DecorRoot->SetRelativeLocation(ContentOffset);
	}

	if (ProgressBarRoot)
	{
		ProgressBarRoot->SetRelativeRotation(ProgressBarRotationOffset);
	}

	if (LeaderMesh)
	{
		if (LeaderLineMesh && LeaderLineMesh->GetStaticMesh() != LeaderMesh)
		{
			LeaderLineMesh->SetStaticMesh(LeaderMesh);
		}

		if (LeaderArmMesh && LeaderArmMesh->GetStaticMesh() != LeaderMesh)
		{
			LeaderArmMesh->SetStaticMesh(LeaderMesh);
		}
	}

	if (LeaderSphereMesh && LeaderStartSphereMesh && LeaderStartSphereMesh->GetStaticMesh() != LeaderSphereMesh)
	{
		LeaderStartSphereMesh->SetStaticMesh(LeaderSphereMesh);
	}

	if (ProgressBarMesh)
	{
		if (ProgressBackgroundMesh && ProgressBackgroundMesh->GetStaticMesh() != ProgressBarMesh)
		{
			ProgressBackgroundMesh->SetStaticMesh(ProgressBarMesh);
		}

		if (ProgressFillMesh && ProgressFillMesh->GetStaticMesh() != ProgressBarMesh)
		{
			ProgressFillMesh->SetStaticMesh(ProgressBarMesh);
		}
	}

	if (ScannerMasterMaterial)
	{
		if (LeaderLineMesh && LeaderLineMesh->GetMaterial(0) != ScannerMasterMaterial)
		{
			LeaderLineMaterial = nullptr;
			LeaderLineMesh->SetMaterial(0, ScannerMasterMaterial);
		}

		if (LeaderArmMesh && LeaderArmMesh->GetMaterial(0) != ScannerMasterMaterial)
		{
			LeaderArmMaterial = nullptr;
			LeaderArmMesh->SetMaterial(0, ScannerMasterMaterial);
		}

		if (LeaderStartSphereMesh && LeaderStartSphereMesh->GetMaterial(0) != ScannerMasterMaterial)
		{
			LeaderSphereMaterial = nullptr;
			LeaderStartSphereMesh->SetMaterial(0, ScannerMasterMaterial);
		}

		if (ProgressBackgroundMesh && ProgressBackgroundMesh->GetMaterial(0) != ScannerMasterMaterial)
		{
			ProgressBackgroundMaterial = nullptr;
			ProgressBackgroundMesh->SetMaterial(0, ScannerMasterMaterial);
		}

		if (ProgressFillMesh && ProgressFillMesh->GetMaterial(0) != ScannerMasterMaterial)
		{
			ProgressFillMaterial = nullptr;
			ProgressFillMesh->SetMaterial(0, ScannerMasterMaterial);
		}
	}

	if (TitleText)
	{
		TitleText->SetHorizontalAlignment(TitleHorizontalAlignment.GetValue());
		TitleText->SetRelativeRotation(TextRotationOffset);
		TitleText->SetTranslucentSortPriority(GeneratedTextSortPriority);
	}

	if (SubtitleText)
	{
		SubtitleText->SetHorizontalAlignment(SubtitleHorizontalAlignment.GetValue());
		SubtitleText->SetRelativeRotation(TextRotationOffset);
		SubtitleText->SetTranslucentSortPriority(GeneratedTextSortPriority);
	}

	if (LeaderArmMesh)
	{
		LeaderArmMesh->SetVisibility(false, true);
	}
}

void AAgentScannerReadoutActor::ApplyPresentationToVisuals()
{
	ConfigureTitleText();
	ConfigureSubtitleText();

	const bool bVisible = bShouldDisplay && DisplayAlpha > KINDA_SMALL_NUMBER;
	const FLinearColor AccentColor = Presentation.AccentColor.A > KINDA_SMALL_NUMBER ? Presentation.AccentColor : TitleColor;
	RuntimeAccentColor = AccentColor;

	if (TitleText)
	{
		TitleText->SetText(Presentation.Title);
		TitleText->SetTextRenderColor(MultiplyOpacity(AccentColor, DisplayAlpha).ToFColor(true));
		SetTextComponentEnabled(TitleText, bVisible && !Presentation.Title.IsEmpty());
	}

	if (SubtitleText)
	{
		SubtitleText->SetText(Presentation.Subtitle);
		SubtitleText->SetTextRenderColor(MultiplyOpacity(SubtitleColor, DisplayAlpha).ToFColor(true));
		SetTextComponentEnabled(SubtitleText, bVisible && !Presentation.Subtitle.IsEmpty());
	}

	const int32 DesiredRowCount = bUseDefaultRowText ? Presentation.Rows.Num() : 0;
	EnsureRowTextCount(DesiredRowCount);
	for (int32 RowIndex = 0; RowIndex < RowTextComponents.Num(); ++RowIndex)
	{
		UTextRenderComponent* RowText = RowTextComponents[RowIndex];
		if (!RowText)
		{
			continue;
		}

		if (RowIndex < DesiredRowCount)
		{
			ConfigureRowText(RowText, Presentation.Rows[RowIndex], RowIndex);
			SetTextComponentEnabled(RowText, bVisible);
		}
		else
		{
			SetTextComponentEnabled(RowText, false);
		}
	}

	ConfigureProgressBarVisuals(AccentColor, bVisible);

	HandlePresentationApplied();
}

void AAgentScannerReadoutActor::EnsureRowTextCount(int32 DesiredCount)
{
	RowTextComponents.RemoveAll([](const TObjectPtr<UTextRenderComponent>& TextComponent)
	{
		return !IsValid(TextComponent);
	});

	while (RowTextComponents.Num() < DesiredCount)
	{
		UTextRenderComponent* NewTextComponent = CreateTextComponent(
			FName(*FString::Printf(TEXT("ScannerRowText_%d"), RowTextComponents.Num())),
			RowsAnchor);
		if (!NewTextComponent)
		{
			break;
		}

		RowTextComponents.Add(NewTextComponent);
	}
}

bool AAgentScannerReadoutActor::GetPanelContentLocalBounds(FVector& OutLocalCenter, FVector& OutLocalExtent) const
{
	OutLocalCenter = FVector::ZeroVector;
	OutLocalExtent = FVector::ZeroVector;

	if (!PanelRoot)
	{
		return false;
	}

	TInlineComponentArray<UPrimitiveComponent*> PrimitiveComponents(this);
	GetComponents(PrimitiveComponents);

	FBox LocalBounds(ForceInit);
	const FTransform ActorTransform = GetActorTransform();
	for (const UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent || PrimitiveComponent == LeaderLineMesh || PrimitiveComponent == LeaderArmMesh)
		{
			continue;
		}

		if (!IsDescendantOfComponent(PrimitiveComponent, PanelRoot))
		{
			continue;
		}

		if (!PrimitiveComponent->IsVisible() || PrimitiveComponent->bHiddenInGame)
		{
			continue;
		}

		const FBox WorldBounds = PrimitiveComponent->Bounds.GetBox();
		if (!WorldBounds.IsValid)
		{
			continue;
		}

		const FBox LocalComponentBounds = WorldBounds.TransformBy(ActorTransform.Inverse());
		LocalBounds += LocalComponentBounds;
	}

	if (!LocalBounds.IsValid)
	{
		return false;
	}

	OutLocalCenter = LocalBounds.GetCenter();
	OutLocalExtent = LocalBounds.GetExtent();
	return true;
}

void AAgentScannerReadoutActor::ConfigureTitleText()
{
	if (!TitleText)
	{
		return;
	}

	TitleText->SetWorldSize(FMath::Max(1.0f, TitleTextWorldSize));
	TitleText->SetRelativeRotation(TextRotationOffset);
	if (ScannerTextMaterial)
	{
		TitleText->SetTextMaterial(ScannerTextMaterial);
	}
	if (ScannerTextFont)
	{
		TitleText->SetFont(ScannerTextFont);
	}
}

void AAgentScannerReadoutActor::ConfigureSubtitleText()
{
	if (!SubtitleText)
	{
		return;
	}

	SubtitleText->SetWorldSize(FMath::Max(1.0f, SubtitleTextWorldSize));
	SubtitleText->SetRelativeRotation(TextRotationOffset);
	if (ScannerTextMaterial)
	{
		SubtitleText->SetTextMaterial(ScannerTextMaterial);
	}
	if (ScannerTextFont)
	{
		SubtitleText->SetFont(ScannerTextFont);
	}
}

void AAgentScannerReadoutActor::ConfigureRowText(UTextRenderComponent* TextComponent, const FAgentScannerRowPresentation& Row, int32 RowIndex) const
{
	if (!TextComponent)
	{
		return;
	}

	TextComponent->SetRelativeLocation(FVector(0.0f, 0.0f, -FMath::Max(1.0f, RowSpacing) * static_cast<float>(RowIndex)));
	TextComponent->SetRelativeRotation(TextRotationOffset);
	TextComponent->SetWorldSize(FMath::Max(1.0f, RowTextWorldSize));
	TextComponent->SetHorizontalAlignment(RowHorizontalAlignment.GetValue());
	TextComponent->SetVerticalAlignment(EVRTA_TextCenter);
	TextComponent->SetTranslucentSortPriority(GeneratedTextSortPriority);
	TextComponent->SetText(FormatRowText(Row));
	TextComponent->SetTextRenderColor(
		MultiplyOpacity(Row.bUseTint ? Row.Tint : NeutralRowColor, DisplayAlpha).ToFColor(true));

	if (ScannerTextMaterial)
	{
		TextComponent->SetTextMaterial(ScannerTextMaterial);
	}
	if (ScannerTextFont)
	{
		TextComponent->SetFont(ScannerTextFont);
	}
}

void AAgentScannerReadoutActor::ConfigureProgressBarVisuals(const FLinearColor& AccentColor, bool bVisible)
{
	if (!ProgressBackgroundMesh || !ProgressFillMesh)
	{
		return;
	}

	const bool bShowProgressBar = bUseDefaultProgressReticle && Presentation.bHasTarget && bVisible;
	if (ProgressBarRoot)
	{
		ProgressBarRoot->SetVisibility(bShowProgressBar, true);
	}
	ProgressBackgroundMesh->SetVisibility(bShowProgressBar, true);
	ProgressFillMesh->SetVisibility(bShowProgressBar, true);
	if (!bShowProgressBar)
	{
		return;
	}

	const float SafeBarWidth = FMath::Max(1.0f, ProgressBarWidth);
	const float SafeBarHeight = FMath::Max(0.5f, ProgressBarHeight);
	const float SafePadding = FMath::Clamp(ProgressBarFillPadding, 0.0f, SafeBarWidth * 0.49f);
	const float FillProgress01 = FMath::Clamp(Presentation.ScanProgress01, 0.0f, 1.0f);

	const float BackgroundScaleX = SafeBarWidth / 100.0f;
	const float BackgroundScaleY = SafeBarHeight / 100.0f;
	ProgressBackgroundMesh->SetRelativeLocation(FVector::ZeroVector);
	ProgressBackgroundMesh->SetRelativeScale3D(FVector(BackgroundScaleX, BackgroundScaleY, 1.0f));

	const float InnerBarWidth = FMath::Max(0.0f, SafeBarWidth - (SafePadding * 2.0f));
	const float InnerBarHeight = FMath::Max(0.1f, SafeBarHeight - (SafePadding * 2.0f));
	const float FillWidth = InnerBarWidth * FillProgress01;
	const bool bShowFill = FillWidth > KINDA_SMALL_NUMBER;
	ProgressFillMesh->SetVisibility(bShowProgressBar && bShowFill, true);
	if (bShowFill)
	{
		const float FillCenterOffsetX = (-InnerBarWidth * 0.5f) + (FillWidth * 0.5f);
		ProgressFillMesh->SetRelativeLocation(FVector(FillCenterOffsetX, 0.0f, ProgressBarFillDepthOffset));
		ProgressFillMesh->SetRelativeScale3D(FVector(
			FMath::Max(0.001f, FillWidth / 100.0f),
			FMath::Max(0.001f, InnerBarHeight / 100.0f),
			1.0f));
	}

	const FLinearColor BackgroundColor = MultiplyOpacity(ProgressBarBackgroundColor, DisplayAlpha);
	const FLinearColor FillColor = MultiplyOpacity(AccentColor, DisplayAlpha);
	ApplyScannerMaterialParams(
		ProgressBackgroundMesh,
		ProgressBackgroundMaterial,
		BackgroundColor,
		ScannerEmissiveIntensity * FMath::Max(0.0f, ProgressBarBackgroundEmissiveMultiplier));
	ApplyScannerMaterialParams(
		ProgressFillMesh,
		ProgressFillMaterial,
		FillColor,
		ScannerEmissiveIntensity);
}

void AAgentScannerReadoutActor::SetTextComponentEnabled(UTextRenderComponent* TextComponent, bool bEnabled) const
{
	if (!TextComponent)
	{
		return;
	}

	TextComponent->SetHiddenInGame(!bEnabled);
	TextComponent->SetVisibility(bEnabled, true);
}

UTextRenderComponent* AAgentScannerReadoutActor::CreateTextComponent(const FName ComponentName, USceneComponent* AttachParent)
{
	UTextRenderComponent* TextComponent = NewObject<UTextRenderComponent>(this, ComponentName);
	if (!TextComponent)
	{
		return nullptr;
	}

	AddInstanceComponent(TextComponent);
	TextComponent->SetupAttachment(AttachParent ? AttachParent : PanelRoot.Get());
	TextComponent->SetHorizontalAlignment(RowHorizontalAlignment.GetValue());
	TextComponent->SetVerticalAlignment(EVRTA_TextCenter);
	TextComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TextComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	TextComponent->SetGenerateOverlapEvents(false);
	TextComponent->SetCastShadow(false);
	TextComponent->SetCanEverAffectNavigation(false);
	TextComponent->SetMobility(EComponentMobility::Movable);
	TextComponent->SetRelativeRotation(TextRotationOffset);
	TextComponent->SetWorldSize(FMath::Max(1.0f, RowTextWorldSize));
	TextComponent->SetTranslucentSortPriority(GeneratedTextSortPriority);
	TextComponent->bEditableWhenInherited = true;

	if (ScannerTextMaterial)
	{
		TextComponent->SetTextMaterial(ScannerTextMaterial);
	}
	if (ScannerTextFont)
	{
		TextComponent->SetFont(ScannerTextFont);
	}

	TextComponent->RegisterComponent();
	return TextComponent;
}

FText AAgentScannerReadoutActor::FormatRowText(const FAgentScannerRowPresentation& Row) const
{
	if (Row.Label.IsEmpty())
	{
		return Row.Value;
	}

	if (Row.Value.IsEmpty())
	{
		return Row.Label;
	}

	return FText::Format(
		NSLOCTEXT("AgentScannerReadoutActor", "RowFormat", "{0}  {1}"),
		Row.Label,
		Row.Value);
}

FAgentScannerPresentation AAgentScannerReadoutActor::BuildPreviewPresentation() const
{
	FAgentScannerPresentation PreviewPresentation;
	PreviewPresentation.bHasTarget = true;
	PreviewPresentation.bScanComplete = bPreviewScanComplete;
	PreviewPresentation.ScanProgress01 = bPreviewScanComplete ? 1.0f : FMath::Clamp(PreviewScanProgress01, 0.0f, 1.0f);
	PreviewPresentation.Title = PreviewTitle.IsEmpty()
		? NSLOCTEXT("AgentScannerReadoutActor", "DefaultPreviewTitle", "Scanner Target")
		: PreviewTitle;
	PreviewPresentation.Subtitle = PreviewSubtitle.IsEmpty()
		? NSLOCTEXT("AgentScannerReadoutActor", "DefaultPreviewSubtitle", "Passive Scan")
		: PreviewSubtitle;
	PreviewPresentation.AccentColor = LeaderColor;
	PreviewPresentation.Rows = PreviewRows;
	return PreviewPresentation;
}

void AAgentScannerReadoutActor::UpdateLeaderLine(float Alpha)
{
	if (!LeaderLineMesh || !LeaderStartSphereMesh || !LeaderEnd || !bShowLeader)
	{
		if (LeaderLineMesh)
		{
			LeaderLineMesh->SetVisibility(false, true);
		}
		if (LeaderArmMesh)
		{
			LeaderArmMesh->SetVisibility(false, true);
		}
		if (LeaderStartSphereMesh)
		{
			LeaderStartSphereMesh->SetVisibility(false, true);
		}
		return;
	}

	const bool bCanShowLeaderLine = GetWorld() && GetWorld()->IsGameWorld();
	const bool bShowLine = bCanShowLeaderLine && bShouldDisplay && Alpha > KINDA_SMALL_NUMBER;
	if (!bShowLine)
	{
		LeaderLineMesh->SetVisibility(false, true);
		LeaderArmMesh->SetVisibility(false, true);
		LeaderStartSphereMesh->SetVisibility(false, true);
		return;
	}

	const FVector LeaderStartWorldLocation = DesiredAnchorLocation;
	const FVector LeaderEndWorldLocation = LeaderEnd->GetComponentLocation();

	LeaderStartSphereMesh->SetVisibility(true, true);
	LeaderStartSphereMesh->SetWorldLocation(LeaderStartWorldLocation);
	LeaderStartSphereMesh->SetWorldScale3D(FVector::OneVector * FMath::Max(0.01f, LeaderSphereDiameter / 100.0f));
	LeaderArmMesh->SetVisibility(false, true);

	UpdateLeaderSegment(LeaderLineMesh, LeaderStartWorldLocation, LeaderEndWorldLocation, Alpha, bShowLine);

	const FLinearColor AccentColor = Presentation.AccentColor.A > KINDA_SMALL_NUMBER ? Presentation.AccentColor : LeaderColor;
	ApplyLeaderMaterialColor(MultiplyOpacity(AccentColor.CopyWithNewOpacity(LeaderColor.A), Alpha));
}

void AAgentScannerReadoutActor::UpdateLeaderSegment(
	UStaticMeshComponent* LineMesh,
	const FVector& StartLocation,
	const FVector& EndLocation,
	float Alpha,
	bool bCanShowLine) const
{
	if (!LineMesh)
	{
		return;
	}

	const FVector LineVector = EndLocation - StartLocation;
	const float Length = LineVector.Size();
	const bool bShowSegment = bCanShowLine && Length > 2.0f;
	LineMesh->SetVisibility(bShowSegment, true);
	if (!bShowSegment)
	{
		return;
	}

	const FVector Midpoint = StartLocation + (LineVector * 0.5f);
	const FRotator LineRotation = FRotationMatrix::MakeFromZ(LineVector.GetSafeNormal()).Rotator();
	LineMesh->SetWorldLocation(Midpoint);
	LineMesh->SetWorldRotation(LineRotation);
	LineMesh->SetWorldScale3D(FVector(
		FMath::Max(0.01f, LeaderThickness / 100.0f),
		FMath::Max(0.01f, LeaderThickness / 100.0f),
		FMath::Max(0.01f, Length / 200.0f)));
}

void AAgentScannerReadoutActor::ApplyLeaderMaterialColor(const FLinearColor& InColor)
{
	ApplyScannerMaterialParams(LeaderLineMesh, LeaderLineMaterial, InColor, ScannerEmissiveIntensity);
	ApplyScannerMaterialParams(LeaderStartSphereMesh, LeaderSphereMaterial, InColor, ScannerEmissiveIntensity);
}

void AAgentScannerReadoutActor::ApplyScannerMaterialParams(
	UStaticMeshComponent* MeshComponent,
	TObjectPtr<UMaterialInstanceDynamic>& MaterialInstance,
	const FLinearColor& InColor,
	float EmissiveIntensity)
{
	if (!MeshComponent)
	{
		return;
	}

	if (!MaterialInstance)
	{
		MaterialInstance = MeshComponent->CreateAndSetMaterialInstanceDynamic(0);
	}

	if (!MaterialInstance)
	{
		return;
	}

	MaterialInstance->SetVectorParameterValue(TEXT("Colour"), InColor);
	MaterialInstance->SetVectorParameterValue(TEXT("Color"), InColor);
	MaterialInstance->SetVectorParameterValue(TEXT("BaseColor"), InColor);
	MaterialInstance->SetVectorParameterValue(TEXT("EmissiveColor"), InColor * EmissiveIntensity);
	MaterialInstance->SetScalarParameterValue(TEXT("EmissiveIntensity"), EmissiveIntensity);
	MaterialInstance->SetScalarParameterValue(TEXT("Opacity"), InColor.A);
	MaterialInstance->SetScalarParameterValue(TEXT("Alpha"), InColor.A);
}
