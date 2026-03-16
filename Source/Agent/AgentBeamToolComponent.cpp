// Copyright Epic Games, Inc. All Rights Reserved.

#include "AgentBeamToolComponent.h"
#include "Dirty/DirtDecalSubsystem.h"
#include "Dirty/DirtySurfaceComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Objects/Components/ObjectHealthComponent.h"

UAgentBeamToolComponent::UAgentBeamToolComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UAgentBeamToolComponent::BeginPlay()
{
	Super::BeginPlay();
	SetBeamScale(BeamScale);
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
	ResetTraceState();
}

void UAgentBeamToolComponent::SetBeamMode(EAgentBeamMode NewMode)
{
	BeamMode = NewMode;
}

void UAgentBeamToolComponent::CycleBeamMode(int32 Direction)
{
	const int32 ModeCount = 2;
	const int32 CurrentIndex = static_cast<int32>(BeamMode);
	const int32 NextIndex = (CurrentIndex + (Direction >= 0 ? 1 : -1) + ModeCount) % ModeCount;
	BeamMode = static_cast<EAgentBeamMode>(NextIndex);
}

void UAgentBeamToolComponent::SetBeamScale(float NewBeamScale)
{
	BeamScale = FMath::Max(0.01f, NewBeamScale);
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
	ResetTraceState();
}

float UAgentBeamToolComponent::GetEffectiveTraceRadius() const
{
	return FMath::Max(0.0f, BaseTraceRadius) * FMath::Max(0.01f, BeamScale);
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
	QueryParams.bTraceComplex = false;
	if (bIgnoreOwner)
	{
		if (const AActor* OwnerActor = GetOwner())
		{
			QueryParams.AddIgnoredActor(OwnerActor);
		}
	}

	return QueryParams;
}

void UAgentBeamToolComponent::UpdateBeamTrace(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		ResetTraceState();
		return;
	}

	TraceState = FAgentBeamTraceState();
	TraceState.ViewOrigin = CurrentViewOrigin;
	TraceState.VisualOrigin = CurrentVisualOrigin;

	const float SafeRange = FMath::Max(1.0f, BeamRange);
	const FVector TraceEnd = CurrentViewOrigin + (CurrentViewDirection * SafeRange);

	FHitResult HitResult;
	bool bHasHit = false;
	const float TraceRadius = GetEffectiveTraceRadius();
	if (TraceRadius > KINDA_SMALL_NUMBER)
	{
		bHasHit = World->SweepSingleByChannel(
			HitResult,
			CurrentViewOrigin,
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
			CurrentViewOrigin,
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

	if (TraceState.HitActor)
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

		if (bAffectDirtySurfaces)
		{
			if (UDirtySurfaceComponent* DirtySurfaceComponent = UDirtySurfaceComponent::FindDirtySurfaceComponent(
					TraceState.HitActor,
					TraceState.HitComponent))
			{
				DirtySurfaceComponent->ApplyBrushHit(HitResult, BuildDirtyBrushStamp(), DeltaTime);
			}
		}
	}

	if (bDrawDebugWhileFiring)
	{
		DrawBeamDebug();
	}
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
		FMath::Max(0.0f, DebugLineThickness) * FMath::Max(0.01f, BeamScale));

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

void UAgentBeamToolComponent::ResetTraceState()
{
	TraceState = FAgentBeamTraceState();
	TraceState.ViewOrigin = CurrentViewOrigin;
	TraceState.VisualOrigin = CurrentVisualOrigin;
	TraceState.EndPoint = CurrentVisualOrigin;
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
