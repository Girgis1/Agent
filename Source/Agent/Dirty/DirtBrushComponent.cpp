// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dirty/DirtBrushComponent.h"

#include "Dirty/DirtDecalSubsystem.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UDirtBrushComponent::UDirtBrushComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UDirtBrushComponent::BeginPlay()
{
	Super::BeginPlay();
	bBrushActive = bStartActive;
	LastTrailLocation = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
	bHasLastTrailLocation = GetOwner() != nullptr;
}

void UDirtBrushComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bBrushEnabled || !bBrushActive || DeltaTime <= 0.0f)
	{
		return;
	}

	TimeUntilNextApplication -= DeltaTime;
	if (TimeUntilNextApplication > 0.0f)
	{
		return;
	}

	TimeUntilNextApplication = FMath::Max(0.01f, ApplyIntervalSeconds);

	switch (ApplicationType)
	{
	case EDirtBrushApplicationType::Trail:
		TickTrailBrush(DeltaTime);
		break;

	case EDirtBrushApplicationType::Area:
		if (AActor* OwnerActor = GetOwner())
		{
			ApplyAreaBrush(OwnerActor->GetActorLocation(), DeltaTime);
		}
		break;

	case EDirtBrushApplicationType::Hit:
	default:
		break;
	}
}

void UDirtBrushComponent::SetBrushActive(bool bNewActive)
{
	bBrushActive = bNewActive;
	if (!bBrushActive)
	{
		bHasLastTrailLocation = false;
	}
}

FDirtBrushStamp UDirtBrushComponent::BuildBrushStamp() const
{
	FDirtBrushStamp Stamp;
	Stamp.Mode = BrushMode;
	Stamp.BrushTexture = BrushTexture;
	Stamp.BrushSizeCm = BrushSizeCm;
	Stamp.BrushStrengthPerSecond = BrushStrengthPerSecond;
	Stamp.BrushHardness = BrushHardness;
	return Stamp;
}

bool UDirtBrushComponent::ApplyHitBrush(const FHitResult& Hit, float DeltaTime)
{
	if (!bBrushEnabled)
	{
		return false;
	}

	return TryApplyBrushToHit(Hit, DeltaTime);
}

void UDirtBrushComponent::ApplyTrailBrush(const FVector& Start, const FVector& End, float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector DownVector = ResolveTraceDownVector();
	const float SegmentLength = FVector::Distance(Start, End);
	const float StepDistance = FMath::Max(TrailMinSegmentLengthCm, BrushSizeCm * 0.5f);
	const int32 StepCount = FMath::Max(1, FMath::CeilToInt(SegmentLength / FMath::Max(1.0f, StepDistance)));

	for (int32 StepIndex = 0; StepIndex <= StepCount; ++StepIndex)
	{
		const float Alpha = StepCount > 0 ? static_cast<float>(StepIndex) / static_cast<float>(StepCount) : 0.0f;
		const FVector SampleLocation = FMath::Lerp(Start, End, Alpha);
		const FVector TraceStart = SampleLocation - (DownVector * TrailTraceStartOffsetCm);
		const FVector TraceEnd = TraceStart + (DownVector * (TrailTraceStartOffsetCm + TrailTraceLengthCm));

		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, BuildTraceParams()))
		{
			TryApplyBrushToHit(Hit, DeltaTime);
		}

		if (bDrawDebug)
		{
			DrawDebugLine(World, TraceStart, TraceEnd, FColor::Cyan, false, TimeUntilNextApplication, 0, 1.5f);
		}
	}
}

void UDirtBrushComponent::ApplyAreaBrush(const FVector& Origin, float DeltaTime)
{
	UWorld* World = GetWorld();
	AActor* OwnerActor = GetOwner();
	if (!World || !OwnerActor)
	{
		return;
	}

	const FVector UpVector = bTrailUseOwnerDownVector ? OwnerActor->GetActorUpVector() : FVector::UpVector;
	const FVector ForwardVector = OwnerActor->GetActorForwardVector();
	const FVector RightVector = OwnerActor->GetActorRightVector();
	const FVector TraceDownVector = -UpVector;

	const int32 SampleCount = FMath::Max(1, AreaTraceCount);
	for (int32 SampleIndex = 0; SampleIndex < SampleCount; ++SampleIndex)
	{
		const float AngleRadians = (static_cast<float>(SampleIndex) / static_cast<float>(SampleCount)) * PI * 2.0f;
		const FVector RingOffset = (ForwardVector * FMath::Cos(AngleRadians) + RightVector * FMath::Sin(AngleRadians)) * AreaRadiusCm;
		const FVector SampleLocation = Origin + RingOffset;
		const FVector TraceStart = SampleLocation + (UpVector * AreaTraceHeightCm);
		const FVector TraceEnd = TraceStart + (TraceDownVector * (AreaTraceHeightCm + AreaTraceDepthCm));

		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, TraceStart, TraceEnd, ECC_Visibility, BuildTraceParams()))
		{
			TryApplyBrushToHit(Hit, DeltaTime);
		}

		if (bDrawDebug)
		{
			DrawDebugLine(World, TraceStart, TraceEnd, FColor::Yellow, false, TimeUntilNextApplication, 0, 1.5f);
		}
	}

	FHitResult CenterHit;
	const FVector CenterTraceStart = Origin + (UpVector * AreaTraceHeightCm);
	const FVector CenterTraceEnd = CenterTraceStart + (TraceDownVector * (AreaTraceHeightCm + AreaTraceDepthCm));
	if (World->LineTraceSingleByChannel(CenterHit, CenterTraceStart, CenterTraceEnd, ECC_Visibility, BuildTraceParams()))
	{
		TryApplyBrushToHit(CenterHit, DeltaTime);
	}
}

bool UDirtBrushComponent::TryApplyBrushToHit(const FHitResult& Hit, float DeltaTime)
{
	if (UWorld* World = GetWorld())
	{
		if (UDirtDecalSubsystem* DirtDecalSubsystem = World->GetSubsystem<UDirtDecalSubsystem>())
		{
			return DirtDecalSubsystem->ApplyBrushAtHit(Hit, BuildBrushStamp(), DeltaTime);
		}
	}

	return false;
}

void UDirtBrushComponent::TickTrailBrush(float DeltaTime)
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	const FVector CurrentLocation = OwnerActor->GetActorLocation();
	if (!bHasLastTrailLocation)
	{
		LastTrailLocation = CurrentLocation;
		bHasLastTrailLocation = true;
		return;
	}

	if (FVector::Distance(LastTrailLocation, CurrentLocation) >= FMath::Max(1.0f, TrailMinSegmentLengthCm))
	{
		ApplyTrailBrush(LastTrailLocation, CurrentLocation, DeltaTime);
		LastTrailLocation = CurrentLocation;
	}
}

FVector UDirtBrushComponent::ResolveTraceDownVector() const
{
	const AActor* OwnerActor = GetOwner();
	if (bTrailUseOwnerDownVector && OwnerActor)
	{
		return -OwnerActor->GetActorUpVector();
	}

	return FVector(0.0f, 0.0f, -1.0f);
}

FCollisionQueryParams UDirtBrushComponent::BuildTraceParams() const
{
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DirtBrushTrace), false, GetOwner());
	QueryParams.bTraceComplex = true;
	QueryParams.bReturnPhysicalMaterial = false;
	if (const AActor* OwnerActor = GetOwner())
	{
		QueryParams.AddIgnoredActor(OwnerActor);
	}

	return QueryParams;
}
