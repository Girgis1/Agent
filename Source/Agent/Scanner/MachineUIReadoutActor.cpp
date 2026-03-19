// Copyright Epic Games, Inc. All Rights Reserved.

#include "MachineUIReadoutActor.h"
#include "AgentCharacter.h"
#include "MachineUIBlueprintLibrary.h"
#include "MachineUIDataProviderInterface.h"
#include "MachineUIButtonActor.h"
#include "MachineUIScannerTargeting.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Machine/MachineComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
FText FormatMachineUIFloat(float Value, int32 DecimalPlaces = 1)
{
	FNumberFormattingOptions FormattingOptions;
	FormattingOptions.MinimumFractionalDigits = FMath::Clamp(DecimalPlaces, 0, 3);
	FormattingOptions.MaximumFractionalDigits = FMath::Clamp(DecimalPlaces, 0, 3);
	return FText::AsNumber(Value, &FormattingOptions);
}

FText FormatMachineUISpeedText(float Speed)
{
	return FText::Format(
		NSLOCTEXT("MachineUI", "SpeedValueFormat", "{0}x"),
		FormatMachineUIFloat(Speed, 2));
}

FText FormatMachineUIUnitsText(float Units)
{
	return FText::Format(
		NSLOCTEXT("MachineUI", "UnitsValueFormat", "{0}u"),
		FormatMachineUIFloat(Units, Units >= 10.0f ? 0 : 1));
}

FText FormatMachineUIPercentText(float Percent01)
{
	return FText::Format(
		NSLOCTEXT("MachineUI", "PercentValueFormat", "{0}%"),
		FormatMachineUIFloat(Percent01 * 100.0f, 0));
}

FText FormatMachineUIPowerText(float Watts)
{
	return FText::Format(
		NSLOCTEXT("MachineUI", "PowerValueFormat", "{0} W"),
		FormatMachineUIFloat(Watts, Watts >= 100.0f ? 0 : 1));
}

FText FormatMachineUITimeText(float Seconds)
{
	if (Seconds < 0.0f)
	{
		return NSLOCTEXT("MachineUI", "TimeUnknown", "--");
	}

	if (Seconds >= 60.0f)
	{
		const int32 WholeMinutes = FMath::FloorToInt(Seconds / 60.0f);
		const int32 RemainingSeconds = FMath::FloorToInt(FMath::Fmod(Seconds, 60.0f));
		return FText::Format(
			NSLOCTEXT("MachineUI", "TimeMinutesSeconds", "{0}m {1}s"),
			FText::AsNumber(WholeMinutes),
			FText::AsNumber(RemainingSeconds));
	}

	return FText::Format(
		NSLOCTEXT("MachineUI", "TimeSeconds", "{0}s"),
		FormatMachineUIFloat(Seconds, 1));
}
}

AMachineUIReadoutActor::AMachineUIReadoutActor()
{
	PrimaryActorTick.bStartWithTickEnabled = true;
	bShowLeader = false;
	TitleHorizontalAlignment = EHTA_Center;
	SubtitleHorizontalAlignment = EHTA_Center;
	RowHorizontalAlignment = EHTA_Center;
	ContentOffset = FVector(0.0f, 50.0f, 0.0f);
	PositionInterpSpeed = 5.0f;
	RotationInterpSpeed = 6.5f;
	FloatAmplitude = 0.35f;
	FloatSpeed = 0.95f;

	FrameRoot = CreateDefaultSubobject<USceneComponent>(TEXT("FrameRoot"));
	FrameRoot->SetupAttachment(DecorRoot ? DecorRoot.Get() : PanelRoot.Get());
	FrameRoot->SetRelativeLocation(FVector::ZeroVector);
	FrameRoot->SetMobility(EComponentMobility::Movable);
	FrameRoot->bEditableWhenInherited = true;

	FrameMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrameMeshComponent"));
	FrameMeshComponent->SetupAttachment(FrameRoot);
	FrameMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	FrameMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	FrameMeshComponent->SetGenerateOverlapEvents(false);
	FrameMeshComponent->SetCastShadow(false);
	FrameMeshComponent->SetCanEverAffectNavigation(false);
	FrameMeshComponent->SetMobility(EComponentMobility::Movable);
	FrameMeshComponent->bEditableWhenInherited = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMesh(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneMesh.Succeeded())
	{
		FrameMeshAsset = PlaneMesh.Object;
		FrameMeshComponent->SetStaticMesh(FrameMeshAsset);
	}

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicMaterial(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
	if (BasicMaterial.Succeeded())
	{
		FrameMaterial = BasicMaterial.Object;
		FrameMeshComponent->SetMaterial(0, FrameMaterial);
	}
}

void AMachineUIReadoutActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	NeutralFacingRotation = GetActorRotation();
	bHasCapturedNeutralRotation = true;
	UpdateFrameVisuals();
	ApplyFrameMaterialParams();
}

void AMachineUIReadoutActor::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(true);

	NeutralFacingRotation = GetActorRotation();
	bHasCapturedNeutralRotation = true;
	ResolveMachineContext();
	RefreshMachineUIState();
	RefreshButtonCache(true);
	ResetButtonRaycastInteractionState();
	UpdateFrameVisuals();
	ApplyFrameMaterialParams();
}

void AMachineUIReadoutActor::Tick(float DeltaSeconds)
{
	TimeUntilStateRefresh = FMath::Max(0.0f, TimeUntilStateRefresh - FMath::Max(0.0f, DeltaSeconds));
	TimeUntilButtonCacheRefresh = FMath::Max(0.0f, TimeUntilButtonCacheRefresh - FMath::Max(0.0f, DeltaSeconds));

	FVector ViewerWorldLocation = FVector::ZeroVector;
	FVector ViewerWorldDirection = FVector::ForwardVector;
	const bool bHasViewer = ResolvePrimaryViewerPose(ViewerWorldLocation, ViewerWorldDirection);
	const bool bHasMachineContext = ResolveMachineContext();
	const bool bScannerModeActive = ResolveScannerModeActive();

	bool bShouldDisplayUI = bHasMachineContext;
	if (bShouldDisplayUI && bAutoVisibilityFromPlayerView)
	{
		if (!bHasViewer)
		{
			bShouldDisplayUI = !bHideWhenNoViewer;
		}
		else
		{
			bool bAimedAtMachine = false;
			bool bWithinRange = false;
			bShouldDisplayUI = ComputeMachineVisibilityForViewer(
				ViewerWorldLocation,
				ViewerWorldDirection,
				bAimedAtMachine,
				bWithinRange);
		}
	}

	if (bShouldDisplayUI && bRequireScannerModeForUI)
	{
		bShouldDisplayUI = bScannerModeActive;
	}

	if (bShouldDisplayUI)
	{
		if (TimeUntilStateRefresh <= KINDA_SMALL_NUMBER || !bIsMachineUIActive)
		{
			RefreshMachineUIState();
		}

		const FAgentScannerPresentation RuntimePresentation = BuildPresentationFromState(CachedMachineState);
		const FVector PanelLocation = GetActorLocation();
		const FVector AnchorLocation = PanelLocation;
		UpdateReadout(
			RuntimePresentation,
			PanelLocation,
			AnchorLocation,
			bHasViewer ? ViewerWorldLocation : FVector::ZeroVector,
			1.0f,
			true);

		if (bUseRaycastButtonInteraction && bHasViewer)
		{
			UpdateButtonRaycastInteraction(ViewerWorldLocation, ViewerWorldDirection, bScannerModeActive);
		}
		else
		{
			ResetButtonRaycastInteractionState();
		}

		SetMachineUIActive(true);
	}
	else
	{
		if (bIsMachineUIActive)
		{
			HideReadout();
		}

		ResetButtonRaycastInteractionState();
		SetMachineUIActive(false);
	}

	Super::Tick(DeltaSeconds);
}

void AMachineUIReadoutActor::SetMachineComponentOverride(UMachineComponent* InMachineComponent)
{
	MachineComponentOverride = InMachineComponent;
	CachedMachineComponent = InMachineComponent;
	CachedMachineActor = InMachineComponent ? InMachineComponent->GetOwner() : nullptr;
	TimeUntilStateRefresh = 0.0f;
}

bool AMachineUIReadoutActor::RefreshMachineUIState()
{
	if (!ResolveMachineContext())
	{
		return false;
	}

	UMachineComponent* MachineComponent = CachedMachineComponent.Get();
	AActor* MachineActor = CachedMachineActor.Get();
	if (!MachineActor)
	{
		return false;
	}

	FMachineUIState NewState;
	bool bResolvedState = false;
	if (MachineActor->GetClass()->ImplementsInterface(UMachineUIDataProviderInterface::StaticClass()))
	{
		bResolvedState = IMachineUIDataProviderInterface::Execute_BuildMachineUIState(MachineActor, NewState);
	}

	if (!bResolvedState && MachineComponent)
	{
		bResolvedState = UMachineUIBlueprintLibrary::BuildMachineUIStateFromMachineComponent(MachineComponent, NewState);
	}

	if (!bResolvedState)
	{
		return false;
	}

	CachedMachineState = MoveTemp(NewState);
	TimeUntilStateRefresh = FMath::Max(0.01f, StateRefreshInterval);
	HandleMachineUIStateUpdated(CachedMachineState);
	return true;
}

bool AMachineUIReadoutActor::ApplyMachineCommand(const FMachineUICommand& Command)
{
	if (!ResolveMachineContext())
	{
		return false;
	}

	UMachineComponent* MachineComponent = CachedMachineComponent.Get();
	AActor* MachineActor = CachedMachineActor.Get();
	if (!MachineActor)
	{
		return false;
	}

	bool bHandled = false;
	if (MachineActor->GetClass()->ImplementsInterface(UMachineUIDataProviderInterface::StaticClass()))
	{
		bHandled = IMachineUIDataProviderInterface::Execute_ExecuteMachineUICommand(MachineActor, Command);
	}

	if (!bHandled && MachineComponent)
	{
		bHandled = UMachineUIBlueprintLibrary::ApplyMachineUICommandToMachineComponent(MachineComponent, Command);
	}

	if (bHandled)
	{
		TimeUntilStateRefresh = 0.0f;
		RefreshMachineUIState();
	}

	return bHandled;
}

void AMachineUIReadoutActor::HandlePresentationApplied_Implementation()
{
	Super::HandlePresentationApplied_Implementation();
	UpdateFrameVisuals();
	ApplyFrameMaterialParams();
}

bool AMachineUIReadoutActor::ResolveTargetFacingRotation(
	const FVector& InPanelLocation,
	const FVector& InViewerLocation,
	FRotator& OutTargetRotation) const
{
	if (InViewerLocation.IsNearlyZero())
	{
		TiltLimitAlpha = 0.0f;
		return false;
	}

	const FVector ToViewer = (InViewerLocation - InPanelLocation).GetSafeNormal();
	if (ToViewer.IsNearlyZero())
	{
		TiltLimitAlpha = 0.0f;
		return false;
	}

	const FRotator BaseRotation = bHasCapturedNeutralRotation ? NeutralFacingRotation : GetActorRotation();
	const FRotator DesiredRotation = ToViewer.Rotation();
	if (MaxTiltDegrees <= KINDA_SMALL_NUMBER)
	{
		TiltLimitAlpha = 0.0f;
		OutTargetRotation = DesiredRotation;
		OutTargetRotation.Roll = BaseRotation.Roll;
		return true;
	}

	FRotator DeltaRotation = (DesiredRotation - BaseRotation).GetNormalized();
	DeltaRotation.Pitch = FMath::Clamp(DeltaRotation.Pitch, -MaxTiltDegrees, MaxTiltDegrees);
	DeltaRotation.Yaw = FMath::Clamp(DeltaRotation.Yaw, -MaxTiltDegrees, MaxTiltDegrees);
	DeltaRotation.Roll = 0.0f;

	TiltLimitAlpha = FMath::Clamp(
		FMath::Max(FMath::Abs(DeltaRotation.Pitch), FMath::Abs(DeltaRotation.Yaw)) / FMath::Max(1.0f, MaxTiltDegrees),
		0.0f,
		1.0f);

	OutTargetRotation = (BaseRotation + DeltaRotation).GetNormalized();
	OutTargetRotation.Roll = BaseRotation.Roll;
	return true;
}

float AMachineUIReadoutActor::ResolveRotationInterpSpeed(const FRotator& CurrentRotation, const FRotator& TargetRotation) const
{
	const float BaseInterpSpeed = Super::ResolveRotationInterpSpeed(CurrentRotation, TargetRotation);
	if (BaseInterpSpeed <= KINDA_SMALL_NUMBER || MaxTiltDegrees <= KINDA_SMALL_NUMBER)
	{
		return BaseInterpSpeed;
	}

	const float RemainingRatio = 1.0f - FMath::Clamp(TiltLimitAlpha, 0.0f, 1.0f);
	const float EdgeFalloff = FMath::Pow(RemainingRatio, FMath::Max(0.2f, TiltEdgeSlowdownExponent));
	const float MinScale = FMath::Clamp(MinTiltInterpScaleAtLimit, 0.01f, 1.0f);
	const float InterpScale = FMath::Lerp(MinScale, 1.0f, EdgeFalloff);
	return BaseInterpSpeed * InterpScale;
}

bool AMachineUIReadoutActor::ResolveMachineContext()
{
	if (MachineComponentOverride)
	{
		CachedMachineComponent = MachineComponentOverride;
		CachedMachineActor = MachineComponentOverride->GetOwner();
		return true;
	}

	if (CachedMachineActor.IsValid())
	{
		if (CachedMachineComponent.IsValid())
		{
			return true;
		}

		if (AActor* CachedActor = CachedMachineActor.Get())
		{
			if (CachedActor->GetClass()->ImplementsInterface(UMachineUIDataProviderInterface::StaticClass()))
			{
				return true;
			}
		}
	}

	if (!bAutoResolveMachineFromOwnership)
	{
		CachedMachineComponent = nullptr;
		CachedMachineActor = nullptr;
		return false;
	}

	AActor* ResolutionStart = GetAttachParentActor();
	if (!ResolutionStart)
	{
		ResolutionStart = GetOwner();
	}
	if (!ResolutionStart)
	{
		ResolutionStart = this;
	}

	UMachineComponent* FoundMachineComponent = AgentMachineUITargeting::ResolveMachineComponentFromCandidate(ResolutionStart);
	AActor* FoundMachineActor = FoundMachineComponent
		? FoundMachineComponent->GetOwner()
		: AgentMachineUITargeting::ResolveMachineActorFromCandidate(ResolutionStart);
	CachedMachineComponent = FoundMachineComponent;
	CachedMachineActor = FoundMachineActor;

	const bool bHasInterfaceProvider = FoundMachineActor
		&& FoundMachineActor->GetClass()->ImplementsInterface(UMachineUIDataProviderInterface::StaticClass());
	return CachedMachineActor.IsValid() && (CachedMachineComponent.IsValid() || bHasInterfaceProvider);
}

bool AMachineUIReadoutActor::ResolveScannerModeActive() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!PlayerController)
	{
		return false;
	}

	if (const AAgentCharacter* AgentCharacter = Cast<AAgentCharacter>(PlayerController->GetPawn()))
	{
		return AgentCharacter->IsScannerModeActive();
	}

	return PlayerController->IsInputKeyDown(EKeys::RightMouseButton);
}

bool AMachineUIReadoutActor::ResolvePrimaryViewerPose(FVector& OutViewLocation, FVector& OutViewDirection) const
{
	OutViewLocation = FVector::ZeroVector;
	OutViewDirection = FVector::ForwardVector;

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!PlayerController)
	{
		return false;
	}

	FRotator ViewRotation = FRotator::ZeroRotator;
	PlayerController->GetPlayerViewPoint(OutViewLocation, ViewRotation);
	OutViewDirection = ViewRotation.Vector();
	return !OutViewLocation.IsNearlyZero() && !OutViewDirection.IsNearlyZero();
}

void AMachineUIReadoutActor::UpdateButtonRaycastInteraction(
	const FVector& ViewLocation,
	const FVector& ViewDirection,
	bool bScannerModeActive)
{
	if (!bUseRaycastButtonInteraction)
	{
		ResetButtonRaycastInteractionState();
		return;
	}

	RefreshButtonCache(false);
	for (TWeakObjectPtr<AMachineUIButtonActor>& ButtonPtr : CachedButtons)
	{
		if (AMachineUIButtonActor* Button = ButtonPtr.Get())
		{
			Button->SetRayTraceInteractionEnabled(true);
		}
	}

	UWorld* World = GetWorld();
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	if (!World || !PlayerController || ViewDirection.IsNearlyZero())
	{
		ResetButtonRaycastInteractionState();
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MachineUIButtonTrace), false);
	QueryParams.AddIgnoredActor(this);
	if (bIgnoreMachineActorDuringButtonTrace)
	{
		if (AActor* MachineActor = CachedMachineActor.Get())
		{
			QueryParams.AddIgnoredActor(MachineActor);
		}
	}

	const FVector TraceEnd = ViewLocation + (ViewDirection.GetSafeNormal() * FMath::Max(100.0f, ButtonTraceDistance));
	TArray<FHitResult> HitResults;
	World->LineTraceMultiByChannel(
		HitResults,
		ViewLocation,
		TraceEnd,
		static_cast<ECollisionChannel>(ButtonTraceChannel.GetValue()),
		QueryParams);

	AMachineUIButtonActor* HitButton = nullptr;
	for (const FHitResult& HitResult : HitResults)
	{
		HitButton = ResolveButtonFromHitActor(HitResult.GetActor());
		if (HitButton)
		{
			break;
		}
	}

	AMachineUIButtonActor* PreviousHoveredButton = HoveredButton.Get();
	if (PreviousHoveredButton && PreviousHoveredButton != HitButton)
	{
		PreviousHoveredButton->SetHovered(false);
		PreviousHoveredButton->SetPressed(false);
	}

	HoveredButton = HitButton;
	if (HitButton)
	{
		HitButton->SetRayTraceInteractionEnabled(true);
		HitButton->SetHovered(true);
	}

	const bool bCanInteract = !bRequireScannerModeForButtonInteraction || bScannerModeActive;
	const bool bClickHeld = PlayerController->IsInputKeyDown(EKeys::LeftMouseButton);
	if (HitButton && bCanInteract)
	{
		HitButton->SetPressed(bClickHeld);
		if (bClickHeld && !bButtonClickHeldLastTick)
		{
			HitButton->TriggerClick();
		}
	}
	else if (HitButton)
	{
		HitButton->SetPressed(false);
	}

	bButtonClickHeldLastTick = bClickHeld && bCanInteract;
}

void AMachineUIReadoutActor::ResetButtonRaycastInteractionState()
{
	RefreshButtonCache(false);

	for (TWeakObjectPtr<AMachineUIButtonActor>& ButtonPtr : CachedButtons)
	{
		if (AMachineUIButtonActor* Button = ButtonPtr.Get())
		{
			Button->SetHovered(false);
			Button->SetPressed(false);
			Button->SetRayTraceInteractionEnabled(false);
		}
	}

	HoveredButton = nullptr;
	bButtonClickHeldLastTick = false;
}

void AMachineUIReadoutActor::RefreshButtonCache(bool bForceRefresh)
{
	if (bForceRefresh)
	{
		TimeUntilButtonCacheRefresh = 0.0f;
	}

	if (!bForceRefresh && TimeUntilButtonCacheRefresh > KINDA_SMALL_NUMBER)
	{
		return;
	}

	for (TWeakObjectPtr<AMachineUIButtonActor>& ButtonPtr : CachedButtons)
	{
		if (AMachineUIButtonActor* Button = ButtonPtr.Get())
		{
			Button->OnButtonClicked.RemoveDynamic(this, &AMachineUIReadoutActor::HandleRaycastButtonClicked);
		}
	}

	CachedButtons.Reset();

	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors, true, true);
	for (AActor* AttachedActor : AttachedActors)
	{
		AMachineUIButtonActor* Button = Cast<AMachineUIButtonActor>(AttachedActor);
		if (!Button)
		{
			continue;
		}

		CachedButtons.Add(Button);
		Button->OnButtonClicked.AddUniqueDynamic(this, &AMachineUIReadoutActor::HandleRaycastButtonClicked);
		Button->SetRayTraceInteractionEnabled(bIsMachineUIActive && bUseRaycastButtonInteraction);
	}

	TimeUntilButtonCacheRefresh = FMath::Max(0.01f, ButtonCacheRefreshInterval);
}

AMachineUIButtonActor* AMachineUIReadoutActor::ResolveButtonFromHitActor(AActor* HitActor) const
{
	for (AActor* CurrentActor = HitActor; CurrentActor; CurrentActor = CurrentActor->GetAttachParentActor())
	{
		if (AMachineUIButtonActor* ButtonActor = Cast<AMachineUIButtonActor>(CurrentActor))
		{
			return ButtonActor;
		}
	}

	return nullptr;
}

void AMachineUIReadoutActor::HandleRaycastButtonClicked(const FMachineUICommand& Command)
{
	ApplyMachineCommand(Command);
}

bool AMachineUIReadoutActor::ComputeMachineVisibilityForViewer(
	const FVector& ViewLocation,
	const FVector& ViewDirection,
	bool& OutIsAimedAtMachine,
	bool& OutIsWithinRange) const
{
	OutIsAimedAtMachine = false;
	OutIsWithinRange = false;

	const UWorld* World = GetWorld();
	AActor* MachineActor = CachedMachineActor.Get();
	UMachineComponent* MachineComponent = CachedMachineComponent.Get();
	if (!World || !MachineActor)
	{
		return false;
	}

	const FVector ToMachine = MachineActor->GetActorLocation() - ViewLocation;
	const float DistanceToMachine = ToMachine.Size();
	const FVector ToMachineDirection = ToMachine.GetSafeNormal();
	const float SafeActivationRange = ActivationRange > KINDA_SMALL_NUMBER
		? ActivationRange
		: 350.0f;
	const float SafeAimTraceDistance = FMath::Max(100.0f, AimTraceDistance);
	const float MaxRelevantDistance = FMath::Max(SafeActivationRange, SafeAimTraceDistance);
	if (DistanceToMachine > MaxRelevantDistance + 150.0f)
	{
		return false;
	}

	if (bShowWhenAimed && !ViewDirection.IsNearlyZero())
	{
		const float DotToMachine = FVector::DotProduct(ViewDirection, ToMachineDirection);
		const float SafeAimDotThreshold = FMath::Clamp(AimDotThreshold, -1.0f, 1.0f);
		OutIsAimedAtMachine = DotToMachine >= SafeAimDotThreshold && DistanceToMachine <= SafeAimTraceDistance;
	}

	if (bShowWhenInRange && DistanceToMachine <= SafeActivationRange)
	{
		bool bInRangeVisible = true;
		if (bRequireLineOfSightInRange)
		{
			FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MachineUIRangeTrace), false);
			QueryParams.AddIgnoredActor(this);

			FHitResult HitResult;
			const bool bHasRangeHit = World->LineTraceSingleByChannel(
				HitResult,
				ViewLocation,
				MachineActor->GetActorLocation(),
				ECC_Visibility,
				QueryParams);
			if (bHasRangeHit)
			{
				bInRangeVisible = MachineComponent
					? AgentMachineUITargeting::IsActorPartOfMachine(HitResult.GetActor(), MachineComponent)
					: AgentMachineUITargeting::IsActorPartOfMachineActor(HitResult.GetActor(), MachineActor);
			}
		}

		OutIsWithinRange = bInRangeVisible;
	}

	return OutIsAimedAtMachine || OutIsWithinRange;
}

FAgentScannerPresentation AMachineUIReadoutActor::BuildPresentationFromState(const FMachineUIState& State) const
{
	FAgentScannerPresentation RuntimePresentation;
	RuntimePresentation.bHasTarget = true;
	RuntimePresentation.Title = State.MachineName.IsEmpty()
		? NSLOCTEXT("MachineUI", "DefaultMachineTitle", "Machine")
		: State.MachineName;
	RuntimePresentation.Subtitle = State.RuntimeStatus.IsEmpty()
		? (State.bPowerEnabled
			? NSLOCTEXT("MachineUI", "DefaultRuntimeOnline", "Online")
			: NSLOCTEXT("MachineUI", "DefaultRuntimeOffline", "Offline"))
		: State.RuntimeStatus;
	RuntimePresentation.AccentColor = State.bPowerEnabled
		? FLinearColor(0.18f, 0.9f, 1.0f, 1.0f)
		: FLinearColor(0.58f, 0.67f, 0.78f, 0.95f);
	RuntimePresentation.ScanProgress01 = FMath::Clamp(State.CraftProgress01, 0.0f, 1.0f);
	RuntimePresentation.bScanComplete = RuntimePresentation.ScanProgress01 >= 1.0f - KINDA_SMALL_NUMBER;

	TArray<FAgentScannerRowPresentation> Rows;
	Rows.Reserve(FMath::Max(4, MaxDisplayRows));
	const int32 SafeMaxRows = FMath::Max(1, MaxDisplayRows);
	auto AddRow = [&Rows, SafeMaxRows](const FText& Label, const FText& Value, const FLinearColor& Tint, bool bUseTint)
	{
		if (Rows.Num() >= SafeMaxRows)
		{
			return;
		}

		FAgentScannerRowPresentation Row;
		Row.Label = Label;
		Row.Value = Value;
		Row.Tint = Tint;
		Row.bUseTint = bUseTint;
		Rows.Add(Row);
	};

	if (bShowPowerRows)
	{
		AddRow(
			NSLOCTEXT("MachineUI", "PowerLabel", "Power"),
			State.bPowerEnabled
				? NSLOCTEXT("MachineUI", "PowerOnValue", "ON")
				: NSLOCTEXT("MachineUI", "PowerOffValue", "OFF"),
			State.bPowerEnabled ? FLinearColor(0.36f, 1.0f, 0.54f, 1.0f) : FLinearColor(1.0f, 0.42f, 0.36f, 1.0f),
			true);

		AddRow(
			NSLOCTEXT("MachineUI", "SpeedLabel", "Speed"),
			FormatMachineUISpeedText(State.CurrentSpeed),
			RuntimePresentation.AccentColor,
			true);
	}

	if (bShowRecipeRows)
	{
		AddRow(
			NSLOCTEXT("MachineUI", "RecipeLabel", "Recipe"),
			State.ActiveRecipeName.IsEmpty()
				? NSLOCTEXT("MachineUI", "RecipeNoneValue", "None")
				: State.ActiveRecipeName,
			FLinearColor(0.92f, 0.97f, 1.0f, 1.0f),
			false);

		AddRow(
			NSLOCTEXT("MachineUI", "CraftRemainingLabel", "Craft ETA"),
			FormatMachineUITimeText(State.CraftRemainingSeconds),
			FLinearColor(0.72f, 0.9f, 1.0f, 0.95f),
			true);
	}

	if (bShowStorageRows)
	{
		const FText StorageValue = State.CapacityUnits > KINDA_SMALL_NUMBER
			? FText::Format(
				NSLOCTEXT("MachineUI", "StorageCapacityValue", "{0}/{1}u"),
				FormatMachineUIFloat(State.StoredUnits, State.StoredUnits >= 10.0f ? 0 : 1),
				FormatMachineUIFloat(State.CapacityUnits, State.CapacityUnits >= 10.0f ? 0 : 1))
			: FormatMachineUIUnitsText(State.StoredUnits);

		AddRow(
			NSLOCTEXT("MachineUI", "StorageLabel", "Storage"),
			StorageValue,
			FLinearColor(0.78f, 0.94f, 1.0f, 0.95f),
			true);
	}

	if (bShowBatteryRows && State.BatteryPercent01 >= 0.0f)
	{
		AddRow(
			NSLOCTEXT("MachineUI", "BatteryLabel", "Battery"),
			FormatMachineUIPercentText(FMath::Clamp(State.BatteryPercent01, 0.0f, 1.0f)),
			FLinearColor(0.58f, 0.98f, 0.62f, 1.0f),
			true);
	}

	if (bShowPowerUsageRows && State.PowerUsageWatts >= 0.0f)
	{
		AddRow(
			NSLOCTEXT("MachineUI", "PowerUsageLabel", "Power Use"),
			FormatMachineUIPowerText(State.PowerUsageWatts),
			FLinearColor(0.7f, 0.9f, 1.0f, 0.95f),
			true);
	}

	RuntimePresentation.Rows = MoveTemp(Rows);
	return RuntimePresentation;
}

void AMachineUIReadoutActor::UpdateFrameVisuals()
{
	if (!FrameMeshComponent)
	{
		return;
	}

	const bool bFrameVisible = bShowFrame && bShouldDisplay;
	FrameMeshComponent->SetVisibility(bFrameVisible, true);
	if (!bFrameVisible)
	{
		return;
	}

	if (FrameMeshAsset && FrameMeshComponent->GetStaticMesh() != FrameMeshAsset)
	{
		FrameMeshComponent->SetStaticMesh(FrameMeshAsset);
	}

	if (FrameMaterial && FrameMeshComponent->GetMaterial(0) != FrameMaterial)
	{
		FrameMaterialInstance = nullptr;
		FrameMeshComponent->SetMaterial(0, FrameMaterial);
	}

	if (bAutoSizeFrameToContent)
	{
		FVector LocalCenter = FVector::ZeroVector;
		FVector LocalExtent = FVector::ZeroVector;
		if (GetPlacementLocalBounds(LocalCenter, LocalExtent))
		{
			const FVector SafePadding = FramePadding.ComponentMax(FVector::ZeroVector);
			const FVector EffectiveExtent = LocalExtent + SafePadding;
			const float FrameWidth = FMath::Max(2.0f, EffectiveExtent.Y * 2.0f);
			const float FrameHeight = FMath::Max(2.0f, EffectiveExtent.Z * 2.0f);

			FrameRoot->SetRelativeLocation(LocalCenter + FrameLocalOffset);
			FrameRoot->SetRelativeRotation(FrameRotationOffset);
			FrameMeshComponent->SetRelativeLocation(FVector::ZeroVector);
			FrameMeshComponent->SetRelativeScale3D(FVector(
				FMath::Max(0.01f, FrameWidth / 100.0f),
				FMath::Max(0.01f, FrameHeight / 100.0f),
				1.0f));
		}
	}
}

void AMachineUIReadoutActor::ApplyFrameMaterialParams()
{
	if (!FrameMeshComponent || !bShowFrame)
	{
		return;
	}

	if (!FrameMaterialInstance)
	{
		FrameMaterialInstance = FrameMeshComponent->CreateAndSetMaterialInstanceDynamic(0);
	}

	if (!FrameMaterialInstance)
	{
		return;
	}

	const FLinearColor EffectiveColor = FrameColor.CopyWithNewOpacity(FrameColor.A * FMath::Clamp(DisplayAlpha, 0.0f, 1.0f));
	FrameMaterialInstance->SetVectorParameterValue(TEXT("Colour"), EffectiveColor);
	FrameMaterialInstance->SetVectorParameterValue(TEXT("Color"), EffectiveColor);
	FrameMaterialInstance->SetVectorParameterValue(TEXT("BaseColor"), EffectiveColor);
	FrameMaterialInstance->SetVectorParameterValue(TEXT("EmissiveColor"), EffectiveColor * FMath::Max(0.0f, FrameEmissiveIntensity));
	FrameMaterialInstance->SetScalarParameterValue(TEXT("EmissiveIntensity"), FMath::Max(0.0f, FrameEmissiveIntensity));
	FrameMaterialInstance->SetScalarParameterValue(TEXT("Opacity"), EffectiveColor.A);
	FrameMaterialInstance->SetScalarParameterValue(TEXT("Alpha"), EffectiveColor.A);
}

void AMachineUIReadoutActor::SetMachineUIActive(bool bNewIsActive)
{
	if (bIsMachineUIActive == bNewIsActive)
	{
		return;
	}

	bIsMachineUIActive = bNewIsActive;
	HandleMachineUIActivationChanged(bIsMachineUIActive);
}
