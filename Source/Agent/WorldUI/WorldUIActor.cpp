// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldUIActor.h"
#include "WorldUIButtonActor.h"
#include "WorldUIHostComponent.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"

AWorldUIActor::AWorldUIActor()
{
	bUseDefaultRowText = false;
	bUseDefaultProgressReticle = false;
	bShowLeader = false;
	ContentOffset = FVector::ZeroVector;
	PositionInterpSpeed = 5.0f;
	RotationInterpSpeed = 6.5f;
	FloatAmplitude = 0.35f;
	FloatSpeed = 0.95f;
}

void AWorldUIActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	CacheTaggedComponents(true);
}

void AWorldUIActor::BeginPlay()
{
	Super::BeginPlay();
	CacheTaggedComponents(true);
}

void AWorldUIActor::SetHostComponent(UWorldUIHostComponent* InHostComponent)
{
	HostComponent = InHostComponent;
	if (InHostComponent)
	{
		bShowLeader = InHostComponent->bUseLeader;
		FacingMode = InHostComponent->FacingMode;
		RuntimeMaxTiltDegrees = FMath::Max(0.0f, InHostComponent->MaxTiltDegrees);
		RuntimeMinTiltInterpScaleAtLimit = FMath::Clamp(InHostComponent->MinTiltInterpScaleAtLimit, 0.01f, 1.0f);
		RuntimeTiltEdgeSlowdownExponent = FMath::Max(0.2f, InHostComponent->TiltEdgeSlowdownExponent);
		SetNeutralFacingRotation(InHostComponent->GetPanelWorldRotation());
	}
}

void AWorldUIActor::SetFacingMode(EWorldUIFacingMode InFacingMode)
{
	FacingMode = InFacingMode;
}

void AWorldUIActor::SetNeutralFacingRotation(const FRotator& InRotation)
{
	NeutralFacingRotation = InRotation;
	bHasCapturedNeutralRotation = true;
	SetActorRotation(InRotation);
}

void AWorldUIActor::ApplyWorldUIModel(const FWorldUIModel& InModel)
{
	CurrentModel = InModel;
	CacheTaggedComponents(false);

	TSet<FName> BoundTextTags;
	for (const FWorldUITextBinding& Binding : CurrentModel.TextBindings)
	{
		if (Binding.FieldTag.IsNone())
		{
			continue;
		}

		BoundTextTags.Add(Binding.FieldTag);

		TArray<TWeakObjectPtr<UTextRenderComponent>> TextComponents;
		TaggedTextFields.MultiFind(Binding.FieldTag, TextComponents);
		for (const TWeakObjectPtr<UTextRenderComponent>& TextPtr : TextComponents)
		{
			if (UTextRenderComponent* TextComponent = TextPtr.Get())
			{
				TextComponent->SetText(Binding.Value);
				TextComponent->SetHiddenInGame(false);
				TextComponent->SetVisibility(true, true);

				FLinearColor BoundColor = FLinearColor::White;
				if (FindColorBinding(Binding.FieldTag, BoundColor))
				{
					TextComponent->SetTextRenderColor(BoundColor.ToFColor(true));
				}
			}
		}
	}

	if (bHideUnboundTaggedTextFields)
	{
		TArray<FName> UniqueTextTags;
		TaggedTextFields.GetKeys(UniqueTextTags);
		for (const FName& Tag : UniqueTextTags)
		{
			if (BoundTextTags.Contains(Tag))
			{
				continue;
			}

			TArray<TWeakObjectPtr<UTextRenderComponent>> TextComponents;
			TaggedTextFields.MultiFind(Tag, TextComponents);
			for (const TWeakObjectPtr<UTextRenderComponent>& TextPtr : TextComponents)
			{
				if (UTextRenderComponent* TextComponent = TextPtr.Get())
				{
					TextComponent->SetText(FText::GetEmpty());
					TextComponent->SetHiddenInGame(true);
					TextComponent->SetVisibility(false, true);
				}
			}
		}
	}

	for (const FWorldUIVisibilityBinding& Binding : CurrentModel.VisibilityBindings)
	{
		if (Binding.FieldTag.IsNone())
		{
			continue;
		}

		TArray<TWeakObjectPtr<USceneComponent>> SceneComponents;
		TaggedSceneComponents.MultiFind(Binding.FieldTag, SceneComponents);
		for (const TWeakObjectPtr<USceneComponent>& ComponentPtr : SceneComponents)
		{
			if (USceneComponent* SceneComponent = ComponentPtr.Get())
			{
				SceneComponent->SetVisibility(Binding.bVisible, true);
			}
		}
	}

	HandleWorldUIModelUpdated(CurrentModel);
}

void AWorldUIActor::UpdateWorldUI(
	const FWorldUIModel& InModel,
	const FVector& InPanelLocation,
	const FVector& InAnchorLocation,
	const FVector& InViewerLocation,
	float InDisplayAlpha,
	bool bInShouldDisplay)
{
	ApplyWorldUIModel(InModel);

	FAgentScannerPresentation RuntimePresentation;
	RuntimePresentation.bHasTarget = true;
	RuntimePresentation.AccentColor = InModel.AccentColor;
	UpdateReadout(
		RuntimePresentation,
		InPanelLocation,
		InAnchorLocation,
		InViewerLocation,
		InDisplayAlpha,
		bInShouldDisplay);
}

void AWorldUIActor::UpdateButtonInteractionFromTrace(AActor* HitActor, bool bCanInteract, bool bClickHeld)
{
	RefreshButtonCache();

	AWorldUIButtonActor* HitButton = ResolveButtonFromHitActor(HitActor);
	AWorldUIButtonActor* PreviousHoveredButton = HoveredButton.Get();
	if (PreviousHoveredButton && PreviousHoveredButton != HitButton)
	{
		PreviousHoveredButton->SetHovered(false);
		PreviousHoveredButton->SetPressed(false);
	}

	HoveredButton = HitButton;
	if (HitButton)
	{
		HitButton->SetHovered(true);
	}

	if (HitButton && bCanInteract)
	{
		HitButton->SetPressed(bClickHeld);
		if (bClickHeld && !bButtonClickHeldLastTick)
		{
			HitButton->TriggerClick();

			if (UWorldUIHostComponent* Host = HostComponent.Get())
			{
				Host->HandleWorldUIAction(HitButton->Action);

				FWorldUIModel RefreshedModel;
				if (Host->BuildWorldUIModel(RefreshedModel))
				{
					ApplyWorldUIModel(RefreshedModel);
				}
			}
		}
	}
	else if (HitButton)
	{
		HitButton->SetPressed(false);
	}

	bButtonClickHeldLastTick = bClickHeld && bCanInteract;
}

void AWorldUIActor::ResetButtonInteractionState()
{
	RefreshButtonCache();

	for (TWeakObjectPtr<AWorldUIButtonActor>& ButtonPtr : CachedButtons)
	{
		if (AWorldUIButtonActor* Button = ButtonPtr.Get())
		{
			Button->SetHovered(false);
			Button->SetPressed(false);
			Button->SetRayTraceInteractionEnabled(false);
		}
	}

	HoveredButton = nullptr;
	bButtonClickHeldLastTick = false;
}

bool AWorldUIActor::FindTextBinding(FName FieldTag, FText& OutValue) const
{
	for (const FWorldUITextBinding& Binding : CurrentModel.TextBindings)
	{
		if (Binding.FieldTag == FieldTag)
		{
			OutValue = Binding.Value;
			return true;
		}
	}

	OutValue = FText::GetEmpty();
	return false;
}

bool AWorldUIActor::FindScalarBinding(FName FieldTag, float& OutValue) const
{
	for (const FWorldUIScalarBinding& Binding : CurrentModel.ScalarBindings)
	{
		if (Binding.FieldTag == FieldTag)
		{
			OutValue = Binding.Value;
			return true;
		}
	}

	OutValue = 0.0f;
	return false;
}

bool AWorldUIActor::FindBoolBinding(FName FieldTag, bool& OutValue) const
{
	for (const FWorldUIBoolBinding& Binding : CurrentModel.BoolBindings)
	{
		if (Binding.FieldTag == FieldTag)
		{
			OutValue = Binding.bValue;
			return true;
		}
	}

	OutValue = false;
	return false;
}

bool AWorldUIActor::FindColorBinding(FName FieldTag, FLinearColor& OutValue) const
{
	for (const FWorldUIColorBinding& Binding : CurrentModel.ColorBindings)
	{
		if (Binding.FieldTag == FieldTag)
		{
			OutValue = Binding.Value;
			return true;
		}
	}

	OutValue = FLinearColor::White;
	return false;
}

bool AWorldUIActor::FindVisibilityBinding(FName FieldTag, bool& bOutVisible) const
{
	for (const FWorldUIVisibilityBinding& Binding : CurrentModel.VisibilityBindings)
	{
		if (Binding.FieldTag == FieldTag)
		{
			bOutVisible = Binding.bVisible;
			return true;
		}
	}

	bOutVisible = true;
	return false;
}

AWorldUIButtonActor* AWorldUIActor::ResolveButtonFromHitActor(AActor* HitActor) const
{
	for (AActor* CurrentActor = HitActor; CurrentActor; CurrentActor = CurrentActor->GetAttachParentActor())
	{
		if (AWorldUIButtonActor* ButtonActor = Cast<AWorldUIButtonActor>(CurrentActor))
		{
			return ButtonActor;
		}
	}

	return nullptr;
}

void AWorldUIActor::HandlePresentationApplied_Implementation()
{
	Super::HandlePresentationApplied_Implementation();
	CacheTaggedComponents(false);
}

bool AWorldUIActor::ResolveTargetFacingRotation(
	const FVector& InPanelLocation,
	const FVector& InViewerLocation,
	FRotator& OutTargetRotation) const
{
	if (FacingMode == EWorldUIFacingMode::None || InViewerLocation.IsNearlyZero())
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

	if (FacingMode == EWorldUIFacingMode::FaceCamera)
	{
		TiltLimitAlpha = 0.0f;
		OutTargetRotation = ToViewer.Rotation();
		OutTargetRotation.Roll = 0.0f;
		return true;
	}

	const FRotator BaseRotation = bHasCapturedNeutralRotation ? NeutralFacingRotation : GetActorRotation();
	const FRotator DesiredRotation = ToViewer.Rotation();
	if (RuntimeMaxTiltDegrees <= KINDA_SMALL_NUMBER)
	{
		TiltLimitAlpha = 0.0f;
		OutTargetRotation = DesiredRotation;
		OutTargetRotation.Roll = BaseRotation.Roll;
		return true;
	}

	FRotator DeltaRotation = (DesiredRotation - BaseRotation).GetNormalized();
	DeltaRotation.Pitch = FMath::Clamp(DeltaRotation.Pitch, -RuntimeMaxTiltDegrees, RuntimeMaxTiltDegrees);
	DeltaRotation.Yaw = FMath::Clamp(DeltaRotation.Yaw, -RuntimeMaxTiltDegrees, RuntimeMaxTiltDegrees);
	DeltaRotation.Roll = 0.0f;

	TiltLimitAlpha = FMath::Clamp(
		FMath::Max(FMath::Abs(DeltaRotation.Pitch), FMath::Abs(DeltaRotation.Yaw)) / FMath::Max(1.0f, RuntimeMaxTiltDegrees),
		0.0f,
		1.0f);

	OutTargetRotation = (BaseRotation + DeltaRotation).GetNormalized();
	OutTargetRotation.Roll = BaseRotation.Roll;
	return true;
}

float AWorldUIActor::ResolveRotationInterpSpeed(const FRotator& CurrentRotation, const FRotator& TargetRotation) const
{
	const float BaseInterpSpeed = Super::ResolveRotationInterpSpeed(CurrentRotation, TargetRotation);
	if (FacingMode != EWorldUIFacingMode::LimitedFaceCamera || BaseInterpSpeed <= KINDA_SMALL_NUMBER || RuntimeMaxTiltDegrees <= KINDA_SMALL_NUMBER)
	{
		return BaseInterpSpeed;
	}

	const float RemainingRatio = 1.0f - FMath::Clamp(TiltLimitAlpha, 0.0f, 1.0f);
	const float EdgeFalloff = FMath::Pow(RemainingRatio, FMath::Max(0.2f, RuntimeTiltEdgeSlowdownExponent));
	const float InterpScale = FMath::Lerp(RuntimeMinTiltInterpScaleAtLimit, 1.0f, EdgeFalloff);
	return BaseInterpSpeed * InterpScale;
}

void AWorldUIActor::CacheTaggedComponents(bool bForceRefresh)
{
	if (!bForceRefresh && TaggedSceneComponents.Num() > 0)
	{
		return;
	}

	TaggedTextFields.Reset();
	TaggedSceneComponents.Reset();

	TInlineComponentArray<USceneComponent*> SceneComponents(this);
	GetComponents(SceneComponents);
	for (USceneComponent* SceneComponent : SceneComponents)
	{
		if (!SceneComponent)
		{
			continue;
		}

		for (const FName& ComponentTag : SceneComponent->ComponentTags)
		{
			if (ComponentTag.IsNone())
			{
				continue;
			}

			TaggedSceneComponents.Add(ComponentTag, SceneComponent);
			if (UTextRenderComponent* TextComponent = Cast<UTextRenderComponent>(SceneComponent))
			{
				TaggedTextFields.Add(ComponentTag, TextComponent);
			}
		}
	}
}

void AWorldUIActor::RefreshButtonCache()
{
	CachedButtons.Reset();

	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors, true, true);
	for (AActor* AttachedActor : AttachedActors)
	{
		AWorldUIButtonActor* Button = Cast<AWorldUIButtonActor>(AttachedActor);
		if (!Button)
		{
			continue;
		}

		CachedButtons.Add(Button);
		Button->SetRayTraceInteractionEnabled(!IsHidden());
	}
}
