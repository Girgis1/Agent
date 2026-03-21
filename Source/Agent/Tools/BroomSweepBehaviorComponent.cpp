// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/BroomSweepBehaviorComponent.h"

#include "AgentCharacter.h"
#include "Dirty/DirtBrushComponent.h"
#include "Tools/BroomToolDefinition.h"
#include "Tools/ToolSystemComponent.h"
#include "Tools/ToolWorldComponent.h"
#include "CollisionShape.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

void UBroomSweepBehaviorComponent::OnToolEquipped(UToolSystemComponent* InToolSystem, UToolWorldComponent* InToolWorld)
{
	Super::OnToolEquipped(InToolSystem, InToolWorld);
	ResetRuntimeState();

	const UBroomToolDefinition* Definition = ResolveDefinition();
	UToolWorldComponent* ToolWorldComponent = GetToolWorld();
	AActor* ToolActor = ToolWorldComponent ? ToolWorldComponent->GetOwner() : nullptr;
	USceneComponent* GripPoint = ToolWorldComponent ? ToolWorldComponent->ResolveGripPoint() : nullptr;
	USceneComponent* HeadPoint = ToolWorldComponent ? ToolWorldComponent->ResolveHeadPoint() : nullptr;
	if (!Definition || !ToolActor || !GripPoint || !HeadPoint)
	{
		if (UToolSystemComponent* ToolSystemComponent = GetToolSystem())
		{
			ToolSystemComponent->DropEquippedTool(true);
		}
		return;
	}

	const FTransform ActorTransform = ToolActor->GetActorTransform();
	const FVector GripWorld = GripPoint->GetComponentLocation();
	const FVector HeadWorld = HeadPoint->GetComponentLocation();

	LocalGripPosition = ActorTransform.InverseTransformPositionNoScale(GripWorld);
	LocalHeadPosition = ActorTransform.InverseTransformPositionNoScale(HeadWorld);
	const FVector LocalGripToHead = LocalHeadPosition - LocalGripPosition;
	LocalGripToHeadLength = FMath::Max(1.0f, LocalGripToHead.Size());
	LocalGripToHeadDirection = LocalGripToHead.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);

	SolvedHeadWorld = HeadWorld;
	PreviousHeadWorld = HeadWorld;
	PreviousPushHeadWorld = HeadWorld;
	bHasSolvedHead = true;
	bHasPreviousHead = true;
	bHasPreviousPushHead = true;
}

void UBroomSweepBehaviorComponent::OnToolDropped()
{
	if (UToolWorldComponent* ToolWorldComponent = GetToolWorld())
	{
		if (UDirtBrushComponent* DirtBrushComponent = ToolWorldComponent->ResolveDirtBrush())
		{
			if (const UBroomToolDefinition* Definition = ResolveDefinition())
			{
				DirtBrushComponent->SetBrushActive(Definition->bKeepBrushActiveWhenDropped);
			}
		}
	}

	ResetRuntimeState();
	Super::OnToolDropped();
}

void UBroomSweepBehaviorComponent::TickEquipped(float DeltaTime)
{
	UToolWorldComponent* ToolWorldComponent = GetToolWorld();
	AAgentCharacter* AgentCharacter = GetOwningCharacter();
	AActor* ToolActor = ToolWorldComponent ? ToolWorldComponent->GetOwner() : nullptr;
	const UBroomToolDefinition* Definition = ResolveDefinition();
	if (!ToolWorldComponent || !AgentCharacter || !ToolActor || !Definition)
	{
		return;
	}

	FVector PivotLocation = FVector::ZeroVector;
	FRotator ViewRotation = FRotator::ZeroRotator;
	if (!ResolvePivotAndView(PivotLocation, ViewRotation))
	{
		if (UToolSystemComponent* ToolSystemComponent = GetToolSystem())
		{
			ToolSystemComponent->DropEquippedTool(true);
		}
		return;
	}

	const FVector ViewForward = ViewRotation.Vector().GetSafeNormal();
	const FVector ViewRight = FRotationMatrix(ViewRotation).GetUnitAxis(EAxis::Y);
	const FVector UpVector = FVector::UpVector;

	const FVector RestDirVector =
		(ViewForward * Definition->RestHeadOffset.X)
		+ (ViewRight * Definition->RestHeadOffset.Y)
		+ (UpVector * Definition->RestHeadOffset.Z);
	const FVector SweepDirVector =
		(ViewForward * Definition->SweepHeadOffset.X)
		+ (ViewRight * Definition->SweepHeadOffset.Y)
		+ (UpVector * Definition->SweepHeadOffset.Z);

	const FVector RestDirection = RestDirVector.GetSafeNormal(UE_SMALL_NUMBER, ViewForward);
	const FVector SweepDirection = SweepDirVector.GetSafeNormal(UE_SMALL_NUMBER, ViewForward);
	const FVector DesiredDirection = IsPrimaryUseActive()
		? SweepDirection
		: RestDirection;

	FVector DesiredHead = PivotLocation + (DesiredDirection * LocalGripToHeadLength);
	bContactOnGround = false;

	UWorld* World = GetWorld();
	if (World && Definition->bEnableGroundFollow)
	{
		const FVector TraceStart = DesiredHead + (UpVector * FMath::Max(0.0f, Definition->GroundTraceUpHeightCm));
		const FVector TraceEnd = TraceStart - (UpVector * FMath::Max(1.0f, Definition->GroundTraceDownDepthCm));
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BroomGroundTrace), false, AgentCharacter);
		QueryParams.AddIgnoredActor(AgentCharacter);
		QueryParams.AddIgnoredActor(ToolActor);

		FHitResult GroundHit;
		if (World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
		{
			const FVector GroundHead = GroundHit.ImpactPoint + (GroundHit.ImpactNormal * FMath::Max(0.0f, Definition->GroundClearanceCm));
			const float GroundFollowAlpha = FMath::Clamp(Definition->GroundFollowStrength, 0.0f, 1.0f);
			DesiredHead = FMath::Lerp(DesiredHead, GroundHead, GroundFollowAlpha);
			bContactOnGround = true;

			if (Definition->bDrawDebug)
			{
				DrawDebugLine(World, TraceStart, TraceEnd, FColor::Green, false, 0.03f, 0, 1.2f);
				DrawDebugSphere(World, GroundHead, 4.0f, 8, FColor::Green, false, 0.03f, 0, 1.2f);
			}
		}
		else if (Definition->bDrawDebug)
		{
			DrawDebugLine(World, TraceStart, TraceEnd, FColor::Red, false, 0.03f, 0, 1.0f);
		}
	}

	const FVector SweepStart = bHasSolvedHead ? SolvedHeadWorld : DesiredHead;
	if (World)
	{
		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
		ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BroomHeadSweep), false, AgentCharacter);
		QueryParams.AddIgnoredActor(AgentCharacter);
		QueryParams.AddIgnoredActor(ToolActor);

		FHitResult SweepHit;
		const bool bBlocked = World->SweepSingleByObjectType(
			SweepHit,
			SweepStart,
			DesiredHead,
			FQuat::Identity,
			ObjectQueryParams,
			FCollisionShape::MakeSphere(FMath::Max(0.0f, Definition->HeadCollisionRadius)),
			QueryParams);

		if (bBlocked && SweepHit.bBlockingHit)
		{
			const FVector Remaining = DesiredHead - SweepHit.ImpactPoint;
			const FVector SlideVector = FVector::VectorPlaneProject(Remaining, SweepHit.ImpactNormal);
			DesiredHead = SweepHit.ImpactPoint
				+ (SlideVector * FMath::Clamp(Definition->CollisionSlideFraction, 0.0f, 1.0f))
				+ (SweepHit.ImpactNormal * 1.0f);
			CatchSoftTimeRemaining = FMath::Max(CatchSoftTimeRemaining, FMath::Max(0.0f, Definition->CatchSoftDurationSeconds));

			if (Definition->bDrawDebug)
			{
				DrawDebugSphere(World, SweepHit.ImpactPoint, 5.0f, 8, FColor::Orange, false, 0.06f, 0, 1.0f);
			}
		}

		if (Definition->bDrawDebug)
		{
			DrawDebugLine(World, SweepStart, DesiredHead, FColor::Silver, false, 0.06f, 0, 1.2f);
		}
	}

	if (CatchSoftTimeRemaining > 0.0f)
	{
		CatchSoftTimeRemaining = FMath::Max(0.0f, CatchSoftTimeRemaining - FMath::Max(0.0f, DeltaTime));
	}

	const float BaseFollowSpeed = IsPrimaryUseActive()
		? Definition->HeadFollowSpeedSweep
		: Definition->HeadFollowSpeedIdle;
	const float FollowSpeed = CatchSoftTimeRemaining > 0.0f
		? FMath::Max(0.01f, Definition->HeadFollowSpeedCatch)
		: FMath::Max(0.01f, BaseFollowSpeed);

	if (!bHasSolvedHead)
	{
		SolvedHeadWorld = DesiredHead;
		bHasSolvedHead = true;
	}
	else
	{
		SolvedHeadWorld = FMath::VInterpTo(SolvedHeadWorld, DesiredHead, DeltaTime, FollowSpeed);
	}

	FVector PivotToHead = SolvedHeadWorld - PivotLocation;
	FVector FinalDirection = PivotToHead.GetSafeNormal(UE_SMALL_NUMBER, DesiredDirection);
	if (FinalDirection.IsNearlyZero())
	{
		FinalDirection = DesiredDirection.IsNearlyZero() ? ViewForward : DesiredDirection;
	}
	SolvedHeadWorld = PivotLocation + (FinalDirection * LocalGripToHeadLength);

	const FQuat DesiredRotation = FQuat::FindBetweenNormals(LocalGripToHeadDirection, FinalDirection);
	const FVector DesiredActorLocation = PivotLocation - DesiredRotation.RotateVector(LocalGripPosition);
	ToolActor->SetActorLocationAndRotation(DesiredActorLocation, DesiredRotation, false, nullptr, ETeleportType::TeleportPhysics);

	const FVector ActualHeadWorld = DesiredRotation.RotateVector(LocalHeadPosition) + DesiredActorLocation;
	if (UToolWorldComponent* ToolComponent = GetToolWorld())
	{
		if (UDirtBrushComponent* DirtBrushComponent = ToolComponent->ResolveDirtBrush())
		{
			const bool bCanApplyDirt = Definition->bAllowDirtWhileEquipped && bContactOnGround;
			DirtBrushComponent->SetBrushActive(bCanApplyDirt);
			if (bCanApplyDirt && bHasPreviousHead)
			{
				DirtBrushComponent->ApplyTrailBrush(PreviousHeadWorld, ActualHeadWorld, DeltaTime);
			}
		}
	}

	TickPushInteraction(*Definition, ActualHeadWorld, bContactOnGround, DeltaTime);
	PreviousHeadWorld = ActualHeadWorld;
	bHasPreviousHead = true;
}

const UBroomToolDefinition* UBroomSweepBehaviorComponent::ResolveDefinition() const
{
	const UToolWorldComponent* ToolWorldComponent = GetToolWorld();
	const UToolDefinition* ToolDefinition = ToolWorldComponent ? ToolWorldComponent->GetToolDefinition() : nullptr;
	return Cast<UBroomToolDefinition>(ToolDefinition);
}

bool UBroomSweepBehaviorComponent::ResolvePivotAndView(FVector& OutPivotLocation, FRotator& OutViewRotation) const
{
	const AAgentCharacter* AgentCharacter = GetOwningCharacter();
	const UBroomToolDefinition* Definition = ResolveDefinition();
	if (!AgentCharacter || !Definition)
	{
		return false;
	}

	if (USkeletalMeshComponent* CharacterMesh = AgentCharacter->GetMesh())
	{
		if (!Definition->HandSocketName.IsNone() && CharacterMesh->DoesSocketExist(Definition->HandSocketName))
		{
			const FTransform SocketTransform = CharacterMesh->GetSocketTransform(Definition->HandSocketName, RTS_World);
			OutPivotLocation = SocketTransform.GetLocation();
		}
		else
		{
			return false;
		}
	}
	else
	{
		return false;
	}

	if (!AgentCharacter->GetHeldToolView(OutPivotLocation, OutViewRotation))
	{
		OutViewRotation = AgentCharacter->GetActorRotation();
	}

	if (USkeletalMeshComponent* CharacterMesh = AgentCharacter->GetMesh())
	{
		const FTransform SocketTransform = CharacterMesh->GetSocketTransform(Definition->HandSocketName, RTS_World);
		OutPivotLocation = SocketTransform.GetLocation();
	}

	return true;
}

void UBroomSweepBehaviorComponent::TickPushInteraction(
	const UBroomToolDefinition& Definition,
	const FVector& CurrentHeadLocation,
	bool bCanPush,
	float DeltaTime)
{
	AAgentCharacter* AgentCharacter = GetOwningCharacter();
	UToolWorldComponent* ToolWorldComponent = GetToolWorld();
	UWorld* World = GetWorld();
	AActor* ToolActor = ToolWorldComponent ? ToolWorldComponent->GetOwner() : nullptr;
	if (!AgentCharacter || !World || !ToolActor)
	{
		return;
	}

	if (!IsPrimaryUseActive() || !bCanPush)
	{
		PushApplyAccumulator = 0.0f;
		bHasPreviousPushHead = false;
		return;
	}

	if (!bHasPreviousPushHead)
	{
		PreviousPushHeadWorld = CurrentHeadLocation;
		bHasPreviousPushHead = true;
		return;
	}

	PushApplyAccumulator += FMath::Max(0.0f, DeltaTime);
	if (PushApplyAccumulator < FMath::Max(0.005f, Definition.PushApplyIntervalSeconds))
	{
		return;
	}
	PushApplyAccumulator = 0.0f;

	const FVector SweepStart = PreviousPushHeadWorld;
	const FVector SweepEnd = CurrentHeadLocation;
	PreviousPushHeadWorld = CurrentHeadLocation;
	const FVector Segment = SweepEnd - SweepStart;
	if (Segment.SizeSquared() <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	TArray<FHitResult> HitResults;
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BroomPushSweep), false, AgentCharacter);
	QueryParams.AddIgnoredActor(AgentCharacter);
	QueryParams.AddIgnoredActor(ToolActor);

	const bool bHitAnything = World->SweepMultiByObjectType(
		HitResults,
		SweepStart,
		SweepEnd,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(FMath::Max(0.0f, Definition.HeadCollisionRadius)),
		QueryParams);

	if (!bHitAnything)
	{
		return;
	}

	const FVector PushDirection = Segment.GetSafeNormal();
	const float StrengthKg = AgentCharacter->GetHeldToolStrengthKg();
	const float PushExponent = FMath::Max(0.1f, Definition.StrengthToPushExponent);
	const float MinTransfer = FMath::Clamp(Definition.MinPushTransferFraction, 0.0f, 1.0f);
	const float CatchMassRatio = FMath::Max(0.1f, Definition.CatchMassOverStrengthRatio);
	const float BasePushImpulse = FMath::Max(0.0f, Definition.PushImpulse);
	bool bTriggerCatchSoftMode = false;

	TSet<TWeakObjectPtr<UPrimitiveComponent>> ProcessedComponents;
	for (const FHitResult& HitResult : HitResults)
	{
		UPrimitiveComponent* HitComponent = HitResult.GetComponent();
		if (!HitComponent || ProcessedComponents.Contains(HitComponent))
		{
			continue;
		}

		ProcessedComponents.Add(HitComponent);
		if (!HitComponent->IsSimulatingPhysics())
		{
			bTriggerCatchSoftMode = bTriggerCatchSoftMode || HitResult.bBlockingHit;
			continue;
		}

		const float HitMassKg = FMath::Max(1.0f, HitComponent->GetMass());
		const float StrengthRatio = StrengthKg / HitMassKg;
		const float TransferAlpha = FMath::Clamp(FMath::Pow(FMath::Max(0.0f, StrengthRatio), PushExponent), 0.0f, 1.0f);
		const float ImpulseMagnitude = BasePushImpulse * FMath::Lerp(MinTransfer, 1.0f, TransferAlpha);
		if (ImpulseMagnitude > KINDA_SMALL_NUMBER)
		{
			HitComponent->AddImpulseAtLocation(PushDirection * ImpulseMagnitude, HitResult.ImpactPoint, NAME_None);
		}

		if (HitMassKg > StrengthKg * CatchMassRatio)
		{
			bTriggerCatchSoftMode = true;
		}
	}

	if (bTriggerCatchSoftMode)
	{
		CatchSoftTimeRemaining = FMath::Max(CatchSoftTimeRemaining, FMath::Max(0.0f, Definition.CatchSoftDurationSeconds));
	}
}

void UBroomSweepBehaviorComponent::ResetRuntimeState()
{
	SolvedHeadWorld = FVector::ZeroVector;
	PreviousHeadWorld = FVector::ZeroVector;
	PreviousPushHeadWorld = FVector::ZeroVector;
	bHasSolvedHead = false;
	bHasPreviousHead = false;
	bHasPreviousPushHead = false;
	bContactOnGround = false;
	PushApplyAccumulator = 0.0f;
	CatchSoftTimeRemaining = 0.0f;
}
