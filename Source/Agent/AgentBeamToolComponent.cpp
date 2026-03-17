// Copyright Epic Games, Inc. All Rights Reserved.

#include "AgentBeamToolComponent.h"
#include "Dirty/DirtDecalSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Objects/Components/ObjectHealthComponent.h"
#include "Objects/Components/ObjectSliceComponent.h"

UAgentBeamToolComponent::UAgentBeamToolComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UAgentBeamToolComponent::BeginPlay()
{
	Super::BeginPlay();
	SetBeamScale(BeamScale);
	ResetSliceTraceState(true);
}

void UAgentBeamToolComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bBeamEnabled || !bBeamActive || !bHasBeamPose)
	{
		return;
	}

	UpdateBeamTrace(DeltaTime);
}

void UAgentBeamToolComponent::StartBeam()
{
	if (!bBeamEnabled)
	{
		return;
	}

	bBeamActive = true;
	SetComponentTickEnabled(true);
}

void UAgentBeamToolComponent::StopBeam()
{
	if (!bBeamActive && !PrimaryComponentTick.IsTickFunctionEnabled())
	{
		return;
	}

	bBeamActive = false;
	SetComponentTickEnabled(false);
	ResetSliceTraceState(true);
	ResetTraceState();
}

void UAgentBeamToolComponent::SetBeamMode(EAgentBeamMode NewMode)
{
	BeamMode = NewMode;
	if (BeamMode != EAgentBeamMode::Damage)
	{
		ResetSliceTraceState(true);
	}
}

void UAgentBeamToolComponent::CycleBeamMode(int32 Direction)
{
	const int32 ModeCount = 2;
	const int32 CurrentIndex = static_cast<int32>(BeamMode);
	const int32 NextIndex = (CurrentIndex + (Direction >= 0 ? 1 : -1) + ModeCount) % ModeCount;
	SetBeamMode(static_cast<EAgentBeamMode>(NextIndex));
}

void UAgentBeamToolComponent::SetBeamScale(float NewBeamScale)
{
	BeamScale = FMath::Max(0.01f, NewBeamScale);
}

void UAgentBeamToolComponent::SetSliceIntentActive(bool bNewSliceIntentActive)
{
	bSliceIntentActive = bNewSliceIntentActive;
	if (!ShouldUseSliceMode())
	{
		ResetSliceTraceState(true);
	}
}

void UAgentBeamToolComponent::SetBeamPose(
	const FVector& InViewOrigin,
	const FVector& InViewDirection,
	const FVector& InVisualOrigin)
{
	CurrentViewOrigin = InViewOrigin;
	CurrentViewDirection = InViewDirection.GetSafeNormal();
	CurrentVisualOrigin = InVisualOrigin;
	bHasBeamPose = !CurrentViewDirection.IsNearlyZero();
}

void UAgentBeamToolComponent::ClearBeamPose()
{
	bHasBeamPose = false;
	CurrentViewOrigin = FVector::ZeroVector;
	CurrentViewDirection = FVector::ForwardVector;
	CurrentVisualOrigin = FVector::ZeroVector;
	ResetSliceTraceState(true);
	ResetTraceState();
}

float UAgentBeamToolComponent::GetEffectiveTraceRadius() const
{
	const float TargetBaseTraceRadius = ShouldUseSliceMode()
		? FMath::Lerp(FMath::Max(0.0f, BaseTraceRadius), FMath::Max(0.0f, SliceTraceRadius), SliceWarmupAlpha)
		: FMath::Max(0.0f, BaseTraceRadius);
	return TargetBaseTraceRadius * FMath::Max(0.01f, BeamScale);
}

FLinearColor UAgentBeamToolComponent::GetCurrentBeamColor() const
{
	if (!TraceState.bHasHit)
	{
		return NoTargetBeamColor;
	}

	return BeamMode == EAgentBeamMode::Heal ? HealBeamColor : DamageBeamColor;
}

FCollisionQueryParams UAgentBeamToolComponent::BuildTraceQueryParams() const
{
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AgentBeamTrace), false, GetOwner());
	QueryParams.bReturnPhysicalMaterial = false;
	QueryParams.bTraceComplex = ShouldUseSliceMode();
	if (bIgnoreOwner)
	{
		if (const AActor* OwnerActor = GetOwner())
		{
			QueryParams.AddIgnoredActor(OwnerActor);
		}
	}

	return QueryParams;
}

bool UAgentBeamToolComponent::ShouldUseSliceMode() const
{
	return bSliceModeEnabled && bSliceIntentActive && BeamMode == EAgentBeamMode::Damage;
}

void UAgentBeamToolComponent::UpdateBeamTrace(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		ResetSliceTraceState(true);
		ResetTraceState();
		return;
	}

	UpdateSliceWarmup(DeltaTime);

	TraceState = FAgentBeamTraceState();
	TraceState.ViewOrigin = CurrentViewOrigin;
	TraceState.VisualOrigin = CurrentVisualOrigin;

	const bool bSliceModeActive = ShouldUseSliceMode();
	const FVector TraceStart = (bSliceModeActive && !CurrentVisualOrigin.IsNearlyZero()) ? CurrentVisualOrigin : CurrentViewOrigin;
	const float SafeRange = FMath::Max(1.0f, BeamRange);
	const FVector TraceEnd = TraceStart + (CurrentViewDirection * SafeRange);

	FHitResult HitResult;
	bool bHasHit = false;
	const float TraceRadius = GetEffectiveTraceRadius();
	if (TraceRadius > KINDA_SMALL_NUMBER)
	{
		bHasHit = World->SweepSingleByChannel(
			HitResult,
			TraceStart,
			TraceEnd,
			FQuat::Identity,
			TraceChannel,
			FCollisionShape::MakeSphere(TraceRadius),
			BuildTraceQueryParams());
	}
	else
	{
		bHasHit = World->LineTraceSingleByChannel(
			HitResult,
			TraceStart,
			TraceEnd,
			TraceChannel,
			BuildTraceQueryParams());
	}

	TraceState.bHasHit = bHasHit;
	TraceState.EndPoint = bHasHit
		? (HitResult.ImpactPoint.IsNearlyZero() ? HitResult.Location : HitResult.ImpactPoint)
		: TraceEnd;
	TraceState.ImpactPoint = TraceState.EndPoint;
	TraceState.ImpactNormal = bHasHit ? HitResult.ImpactNormal : FVector::ZeroVector;
	TraceState.HitActor = bHasHit ? HitResult.GetActor() : nullptr;
	TraceState.HitComponent = bHasHit ? HitResult.GetComponent() : nullptr;

	if (bHasHit && bAffectDirtySurfaces)
	{
		if (UDirtDecalSubsystem* DirtDecalSubsystem = World->GetSubsystem<UDirtDecalSubsystem>())
		{
			DirtDecalSubsystem->ApplyBrushAtHit(HitResult, BuildDirtyBrushStamp(), DeltaTime);
		}
	}

	bool bSliceConsumedBeam = false;
	if (bSliceModeActive)
	{
		bSliceConsumedBeam = HandleSliceMode(DeltaTime, TraceStart, TraceEnd, HitResult, bHasHit);
	}
	else
	{
		ResetSliceTraceState(true);
	}

	if (!bSliceModeActive && !bSliceConsumedBeam && TraceState.HitActor)
	{
		if (UObjectHealthComponent* HealthComponent = UObjectHealthComponent::FindObjectHealthComponent(TraceState.HitActor))
		{
			TraceState.bHasHealthTarget = true;
			if (BeamMode == EAgentBeamMode::Heal)
			{
				TraceState.LastAppliedAmount = HealthComponent->ApplyHealing(
					FMath::Max(0.0f, HealingPerSecond) * FMath::Max(0.0f, DeltaTime),
					GetOwner());
			}
			else
			{
				const FObjectDamageResult DamageResult = HealthComponent->ApplyDamagePerSecond(
					FMath::Max(0.0f, DamagePerSecond),
					FMath::Max(0.0f, DeltaTime),
					DamageSourceKind,
					GetOwner(),
					TraceState.ImpactPoint,
					FVector::ZeroVector,
					ResolveEffectiveSourceName());
				TraceState.LastAppliedAmount = DamageResult.AppliedDamage;
			}
		}
	}

	if (bDrawDebugWhileFiring)
	{
		DrawBeamDebug();
	}

	if (bDrawSliceDebug)
	{
		DrawSliceDebug();
	}
}

void UAgentBeamToolComponent::UpdateSliceWarmup(float DeltaTime)
{
	if (!ShouldUseSliceMode())
	{
		SliceWarmupAlpha = 0.0f;
		return;
	}

	if (SliceWarmupDuration <= KINDA_SMALL_NUMBER)
	{
		SliceWarmupAlpha = 1.0f;
		return;
	}

	SliceWarmupAlpha = FMath::Clamp(SliceWarmupAlpha + (FMath::Max(0.0f, DeltaTime) / SliceWarmupDuration), 0.0f, 1.0f);
}

bool UAgentBeamToolComponent::HandleSliceMode(float DeltaTime, const FVector& TraceStart, const FVector& TraceEnd, const FHitResult& HitResult, bool bHasHit)
{
	(void)DeltaTime;
	(void)TraceEnd;

	SliceTraceState.bSliceModeActive = true;
	SliceTraceState.WarmupAlpha = SliceWarmupAlpha;
	SliceTraceState.BeamOrigin = TraceStart;

	if (SliceWarmupAlpha < 1.0f)
	{
		SliceTraceState.SliceState = EAgentBeamSliceState::Warmup;
		SliceTraceState.StatusText = FString::Printf(TEXT("Slice warmup %.2f"), SliceWarmupAlpha);
		return true;
	}

	if (!bHasHit || !TraceState.HitActor || !TraceState.HitComponent)
	{
		FString CommitFailureReason;
		if (TryCommitTrackedSlice(TraceStart, CommitFailureReason))
		{
			ResetSliceTraceState(true);
			return true;
		}

		ResetSliceTraceState(false);
		if (!CommitFailureReason.IsEmpty())
		{
			SliceTraceState.bSliceModeActive = true;
			SliceTraceState.WarmupAlpha = SliceWarmupAlpha;
			SliceTraceState.BeamOrigin = TraceStart;
			SliceTraceState.SliceState = EAgentBeamSliceState::Rejected;
			SliceTraceState.StatusText = CommitFailureReason;
			return true;
		}

		return true;
	}

	UObjectSliceComponent* SliceComponent = UObjectSliceComponent::FindObjectSliceComponent(TraceState.HitActor, TraceState.HitComponent);
	if (!SliceComponent)
	{
		FString CommitFailureReason;
		if (TryCommitTrackedSlice(TraceStart, CommitFailureReason))
		{
			ResetSliceTraceState(true);
			return true;
		}

		ResetSliceTraceState(false);
		if (!CommitFailureReason.IsEmpty())
		{
			SliceTraceState.bSliceModeActive = true;
			SliceTraceState.WarmupAlpha = SliceWarmupAlpha;
			SliceTraceState.BeamOrigin = TraceStart;
			SliceTraceState.TargetActor = TraceState.HitActor;
			SliceTraceState.TargetComponent = TraceState.HitComponent;
			SliceTraceState.SliceState = EAgentBeamSliceState::Rejected;
			SliceTraceState.StatusText = CommitFailureReason;
			return true;
		}

		SliceTraceState.bSliceModeActive = true;
		SliceTraceState.WarmupAlpha = SliceWarmupAlpha;
		SliceTraceState.BeamOrigin = TraceStart;
		SliceTraceState.TargetActor = TraceState.HitActor;
		SliceTraceState.TargetComponent = TraceState.HitComponent;
		SliceTraceState.SliceState = EAgentBeamSliceState::Rejected;
		SliceTraceState.StatusText = TEXT("Target is not sliceable");
		return true;
	}

	FString SliceUnavailableReason;
	if (!SliceComponent->CanSliceNow(SliceUnavailableReason))
	{
		FString CommitFailureReason;
		if (TryCommitTrackedSlice(TraceStart, CommitFailureReason))
		{
			ResetSliceTraceState(true);
			return true;
		}

		ResetSliceTraceState(false);
		if (!CommitFailureReason.IsEmpty())
		{
			SliceTraceState.bSliceModeActive = true;
			SliceTraceState.WarmupAlpha = SliceWarmupAlpha;
			SliceTraceState.BeamOrigin = TraceStart;
			SliceTraceState.TargetActor = TraceState.HitActor;
			SliceTraceState.TargetComponent = TraceState.HitComponent;
			SliceTraceState.SliceState = EAgentBeamSliceState::Rejected;
			SliceTraceState.StatusText = CommitFailureReason;
			return true;
		}

		SliceTraceState.bSliceModeActive = true;
		SliceTraceState.WarmupAlpha = SliceWarmupAlpha;
		SliceTraceState.BeamOrigin = TraceStart;
		SliceTraceState.TargetActor = TraceState.HitActor;
		SliceTraceState.TargetComponent = TraceState.HitComponent;
		SliceTraceState.SliceState = EAgentBeamSliceState::Rejected;
		SliceTraceState.StatusText = SliceUnavailableReason;
		return true;
	}

	FObjectSliceBeamIntersection Penetration;
	FString PenetrationFailureReason;
	if (!SliceComponent->QueryBeamPenetration(TraceStart, CurrentViewDirection, BeamRange, Penetration, PenetrationFailureReason))
	{
		FString CommitFailureReason;
		if (TryCommitTrackedSlice(TraceStart, CommitFailureReason))
		{
			ResetSliceTraceState(true);
			return true;
		}

		ResetSliceTraceState(false);
		SliceTraceState.bSliceModeActive = true;
		SliceTraceState.WarmupAlpha = SliceWarmupAlpha;
		SliceTraceState.BeamOrigin = TraceStart;
		SliceTraceState.TargetActor = TraceState.HitActor;
		SliceTraceState.TargetComponent = TraceState.HitComponent;
		SliceTraceState.SliceState = EAgentBeamSliceState::Rejected;
		SliceTraceState.StatusText = PenetrationFailureReason;
		return true;
	}

	const bool bTargetChanged = ActiveSliceTarget != SliceComponent;
	if (bTargetChanged)
	{
		FString CommitFailureReason;
		if (TryCommitTrackedSlice(TraceStart, CommitFailureReason))
		{
			ResetSliceTraceState(true);
			return true;
		}

		if (!CommitFailureReason.IsEmpty())
		{
			ResetSliceTraceState(false);
			SliceTraceState.bSliceModeActive = true;
			SliceTraceState.WarmupAlpha = SliceWarmupAlpha;
			SliceTraceState.BeamOrigin = TraceStart;
			SliceTraceState.TargetActor = TraceState.HitActor;
			SliceTraceState.TargetComponent = TraceState.HitComponent;
			SliceTraceState.SliceState = EAgentBeamSliceState::Rejected;
			SliceTraceState.StatusText = CommitFailureReason;
			return true;
		}
	}

	ActiveSliceTarget = SliceComponent;

	SliceTraceState.TargetActor = TraceState.HitActor;
	SliceTraceState.TargetComponent = TraceState.HitComponent;
	if (bTargetChanged)
	{
		SliceTraceState.FirstEntryPoint = Penetration.EntryPoint;
	}

	SliceTraceState.CurrentEntryPoint = Penetration.EntryPoint;
	SliceTraceState.ExitPoint = Penetration.ExitPoint;

	const FVector GestureStartVector = SliceTraceState.FirstEntryPoint - TraceStart;
	const FVector GestureCurrentVector = SliceTraceState.CurrentEntryPoint - TraceStart;
	const FVector PlaneNormal = FVector::CrossProduct(GestureStartVector, GestureCurrentVector).GetSafeNormal();
	const float DragDistance = FVector::Distance(SliceTraceState.FirstEntryPoint, SliceTraceState.CurrentEntryPoint);

	SliceTraceState.PlaneNormal = PlaneNormal;
	SliceTraceState.PlaneAxisX = (Penetration.ExitPoint - Penetration.EntryPoint).GetSafeNormal();
	SliceTraceState.PlaneAxisY = FVector::CrossProduct(SliceTraceState.PlaneNormal, SliceTraceState.PlaneAxisX).GetSafeNormal();

	if (PlaneNormal.IsNearlyZero() || DragDistance < FMath::Max(0.0f, SliceMinimumDragDistanceCm))
	{
		SliceTraceState.SliceState = EAgentBeamSliceState::Tracking;
		SliceTraceState.StatusText = bTargetChanged
			? TEXT("Slice point armed")
			: FString::Printf(TEXT("Tracking slice %.1f / %.1f"), DragDistance, SliceMinimumDragDistanceCm);
		return true;
	}

	SliceTraceState.SliceState = EAgentBeamSliceState::Ready;
	SliceTraceState.StatusText = TEXT("Slice ready - exit target to cut");
	return true;
}

bool UAgentBeamToolComponent::TryCommitTrackedSlice(const FVector& BeamOrigin, FString& OutFailureReason)
{
	OutFailureReason.Reset();

	if (!IsValid(ActiveSliceTarget)
		|| SliceTraceState.SliceState != EAgentBeamSliceState::Ready
		|| SliceTraceState.FirstEntryPoint.IsNearlyZero()
		|| SliceTraceState.CurrentEntryPoint.IsNearlyZero()
		|| SliceTraceState.ExitPoint.IsNearlyZero())
	{
		return false;
	}

	return ActiveSliceTarget->TryExecuteSliceFromBeamGesture(
		BeamOrigin,
		SliceTraceState.FirstEntryPoint,
		SliceTraceState.CurrentEntryPoint,
		SliceTraceState.ExitPoint,
		GetOwner(),
		OutFailureReason);
}

void UAgentBeamToolComponent::DrawBeamDebug() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FColor BeamColor = GetCurrentBeamColor().ToFColor(true);
	const FVector LineStart = CurrentVisualOrigin.IsNearlyZero() ? CurrentViewOrigin : CurrentVisualOrigin;
	DrawDebugLine(
		World,
		LineStart,
		TraceState.EndPoint,
		BeamColor,
		false,
		0.0f,
		0,
		FMath::Max(0.5f, GetEffectiveTraceRadius()));

	if (TraceState.bHasHit)
	{
		DrawDebugSphere(
			World,
			TraceState.EndPoint,
			FMath::Max(4.0f, GetEffectiveTraceRadius()),
			12,
			BeamColor,
			false,
			0.0f,
			0,
			1.5f);
	}
}

void UAgentBeamToolComponent::DrawSliceDebug() const
{
	if (!SliceTraceState.bSliceModeActive)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	DrawDebugSphere(
		World,
		SliceTraceState.BeamOrigin,
		3.0f,
		8,
		FColor::Cyan,
		false,
		0.0f,
		0,
		0.75f);

	if (!SliceTraceState.FirstEntryPoint.IsNearlyZero())
	{
		DrawDebugSphere(
			World,
			SliceTraceState.FirstEntryPoint,
			4.0f,
			12,
			FColor::Green,
			false,
			0.0f,
			0,
			1.0f);
	}

	if (!SliceTraceState.CurrentEntryPoint.IsNearlyZero())
	{
		DrawDebugSphere(
			World,
			SliceTraceState.CurrentEntryPoint,
			4.0f,
			12,
			FColor::Yellow,
			false,
			0.0f,
			0,
			1.0f);
	}

	if (!SliceTraceState.ExitPoint.IsNearlyZero())
	{
		DrawDebugSphere(
			World,
			SliceTraceState.ExitPoint,
			4.0f,
			12,
			FColor(255, 165, 0),
			false,
			0.0f,
			0,
			1.0f);
	}

	if (!SliceTraceState.PlaneNormal.IsNearlyZero()
		&& !SliceTraceState.PlaneAxisX.IsNearlyZero()
		&& !SliceTraceState.PlaneAxisY.IsNearlyZero())
	{
		const FVector PlaneCenter = (SliceTraceState.CurrentEntryPoint + SliceTraceState.ExitPoint) * 0.5f;
		const float PlaneExtent = FMath::Max(1.0f, SliceDebugPlaneExtent);
		const FVector AxisX = SliceTraceState.PlaneAxisX * PlaneExtent;
		const FVector AxisY = SliceTraceState.PlaneAxisY * PlaneExtent;
		const FVector CornerA = PlaneCenter + AxisX + AxisY;
		const FVector CornerB = PlaneCenter + AxisX - AxisY;
		const FVector CornerC = PlaneCenter - AxisX - AxisY;
		const FVector CornerD = PlaneCenter - AxisX + AxisY;

		DrawDebugLine(World, CornerA, CornerB, FColor::Cyan, false, 0.0f, 0, 1.0f);
		DrawDebugLine(World, CornerB, CornerC, FColor::Cyan, false, 0.0f, 0, 1.0f);
		DrawDebugLine(World, CornerC, CornerD, FColor::Cyan, false, 0.0f, 0, 1.0f);
		DrawDebugLine(World, CornerD, CornerA, FColor::Cyan, false, 0.0f, 0, 1.0f);
		DrawDebugDirectionalArrow(
			World,
			PlaneCenter,
			PlaneCenter + (SliceTraceState.PlaneNormal * (PlaneExtent * 0.75f)),
			10.0f,
			FColor::Blue,
			false,
			0.0f,
			0,
			1.0f);
	}

	if (!SliceTraceState.StatusText.IsEmpty())
	{
		DrawDebugString(
			World,
			TraceState.EndPoint + FVector(0.0f, 0.0f, 12.0f),
			SliceTraceState.StatusText,
			nullptr,
			FColor::White,
			0.0f,
			false);
	}
}

void UAgentBeamToolComponent::ResetTraceState()
{
	TraceState = FAgentBeamTraceState();
	TraceState.ViewOrigin = CurrentViewOrigin;
	TraceState.VisualOrigin = CurrentVisualOrigin;
	TraceState.EndPoint = CurrentVisualOrigin.IsNearlyZero() ? CurrentViewOrigin : CurrentVisualOrigin;
}

void UAgentBeamToolComponent::ResetSliceTraceState(bool bResetWarmupAlpha)
{
	ActiveSliceTarget = nullptr;
	SliceTraceState = FAgentBeamSliceTraceState();
	if (bResetWarmupAlpha)
	{
		SliceWarmupAlpha = 0.0f;
	}
}

FName UAgentBeamToolComponent::ResolveEffectiveSourceName() const
{
	if (!BeamSourceName.IsNone())
	{
		return BeamSourceName;
	}

	if (const AActor* OwnerActor = GetOwner())
	{
		return OwnerActor->GetFName();
	}

	return NAME_None;
}

FDirtBrushStamp UAgentBeamToolComponent::BuildDirtyBrushStamp() const
{
	FDirtBrushStamp BrushStamp;
	BrushStamp.Mode = DirtyBrushMode;
	BrushStamp.BrushTexture = DirtyBrushTexture;
	BrushStamp.BrushSizeCm = DirtyBrushSizeCm;
	BrushStamp.BrushStrengthPerSecond = DirtyBrushStrengthPerSecond;
	BrushStamp.BrushHardness = DirtyBrushHardness;
	return BrushStamp;
}
