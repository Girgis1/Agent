// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/ToolSystemComponent.h"

#include "AgentCharacter.h"
#include "Tools/ToolBehaviorComponent.h"
#include "Tools/ToolDefinition.h"
#include "Tools/ToolWorldComponent.h"
#include "CollisionShape.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"

UToolSystemComponent::UToolSystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UToolSystemComponent::BeginPlay()
{
	Super::BeginPlay();
	ClearInputs();
}

void UToolSystemComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsToolEquipped())
	{
		return;
	}

	if (!CanMaintainEquippedTool())
	{
		DropEquippedTool(true);
		return;
	}

	if (UToolBehaviorComponent* BehaviorComponent = EquippedBehavior.Get())
	{
		BehaviorComponent->TickEquipped(DeltaTime);
	}
}

bool UToolSystemComponent::TryEquipNearestTool()
{
	if (IsToolEquipped())
	{
		return true;
	}

	AAgentCharacter* AgentCharacter = ResolveAgentCharacter();
	if (!AgentCharacter || !AgentCharacter->CanUseHeldToolInteraction())
	{
		return false;
	}

	int32 CandidateCount = 0;
	UToolWorldComponent* BestToolComponent = FindBestToolComponent(CandidateCount);
	if (!BestToolComponent)
	{
		return false;
	}

	AActor* ToolActor = BestToolComponent->GetOwner();
	UToolBehaviorComponent* BehaviorComponent = BestToolComponent->ResolveBehavior();
	if (!ToolActor || !BehaviorComponent)
	{
		return false;
	}

	ToolActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	if (!BestToolComponent->OnEquipped(AgentCharacter))
	{
		return false;
	}

	EquippedTool = BestToolComponent;
	EquippedBehavior = BehaviorComponent;
	BehaviorComponent->OnToolEquipped(this, BestToolComponent);
	ClearInputs();
	return true;
}

void UToolSystemComponent::DropEquippedTool(bool bForceDrop)
{
	UToolWorldComponent* ToolComponent = EquippedTool.Get();
	if (!ToolComponent)
	{
		EquippedTool.Reset();
		EquippedBehavior.Reset();
		ClearInputs();
		return;
	}

	if (UToolBehaviorComponent* BehaviorComponent = EquippedBehavior.Get())
	{
		BehaviorComponent->OnToolDropped();
	}

	AAgentCharacter* AgentCharacter = ResolveAgentCharacter();
	const FVector CharacterVelocity = AgentCharacter ? AgentCharacter->GetVelocity() : FVector::ZeroVector;
	float DropForwardBoostSpeed = 0.0f;
	if (const UToolDefinition* ToolDefinition = ToolComponent->GetToolDefinition())
	{
		DropForwardBoostSpeed = FMath::Max(0.0f, ToolDefinition->DropForwardBoostSpeed);
	}

	const FVector ForwardBoost = (!bForceDrop && AgentCharacter)
		? AgentCharacter->GetActorForwardVector() * DropForwardBoostSpeed
		: FVector::ZeroVector;
	const FVector DropLinearVelocity = CharacterVelocity + ForwardBoost;

	ToolComponent->OnDropped(DropLinearVelocity, FVector::ZeroVector);
	EquippedTool.Reset();
	EquippedBehavior.Reset();
	ClearInputs();
}

bool UToolSystemComponent::IsToolEquipped() const
{
	return EquippedTool.IsValid();
}

void UToolSystemComponent::SetPushInputActive(bool bActive)
{
	bPushInputHeld = bActive;
	if (UToolBehaviorComponent* BehaviorComponent = EquippedBehavior.Get())
	{
		BehaviorComponent->SetPrimaryUseActive(bPushInputHeld);
	}
}

void UToolSystemComponent::ClearInputs()
{
	bPushInputHeld = false;
	if (UToolBehaviorComponent* BehaviorComponent = EquippedBehavior.Get())
	{
		BehaviorComponent->SetPrimaryUseActive(false);
	}
}

AAgentCharacter* UToolSystemComponent::ResolveAgentCharacter() const
{
	return Cast<AAgentCharacter>(GetOwner());
}

bool UToolSystemComponent::CanMaintainEquippedTool() const
{
	const AAgentCharacter* AgentCharacter = ResolveAgentCharacter();
	if (!AgentCharacter || !EquippedTool.IsValid())
	{
		return false;
	}

	if (!AgentCharacter->CanUseHeldToolInteraction())
	{
		return false;
	}

	return !AgentCharacter->IsRagdolling();
}

UToolWorldComponent* UToolSystemComponent::FindBestToolComponent(int32& OutCandidateCount) const
{
	OutCandidateCount = 0;

	AAgentCharacter* AgentCharacter = ResolveAgentCharacter();
	UWorld* World = GetWorld();
	if (!AgentCharacter || !World)
	{
		return nullptr;
	}

	FVector ViewLocation = AgentCharacter->GetActorLocation();
	FRotator ViewRotation = AgentCharacter->GetActorRotation();
	ResolveHoldView(ViewLocation, ViewRotation);
	const FVector ViewForward = ViewRotation.Vector().GetSafeNormal();
	const float FacingWeight = FMath::Clamp(NearbyCandidateFacingWeight, 0.0f, 1.0f);

	UToolWorldComponent* BestToolComponent = nullptr;
	float BestScore = TNumericLimits<float>::Lowest();
	TSet<TWeakObjectPtr<UToolWorldComponent>> SeenTools;

	auto ScoreCandidateTool = [&](UToolWorldComponent* ToolComponent, bool bFromTrace)
	{
		if (!ToolComponent || SeenTools.Contains(ToolComponent))
		{
			return;
		}

		SeenTools.Add(ToolComponent);
		++OutCandidateCount;

		if (!ToolComponent->CanBeEquippedBy(AgentCharacter))
		{
			return;
		}

		const UToolDefinition* ToolDefinition = ToolComponent->GetToolDefinition();
		const float MaxDistance = FMath::Max(1.0f, ToolDefinition ? ToolDefinition->MaxEquipDistance : 1.0f);
		const FVector ToolLocation = ToolComponent->GetGripLocation();
		const float DistanceSq = FVector::DistSquared(AgentCharacter->GetActorLocation(), ToolLocation);
		if (DistanceSq > FMath::Square(MaxDistance))
		{
			return;
		}

		const FVector ToCandidate = (ToolLocation - ViewLocation).GetSafeNormal();
		const float FacingDot = FVector::DotProduct(ViewForward, ToCandidate);
		const float DistanceWeight = 1.0f - FMath::Clamp(FMath::Sqrt(DistanceSq) / MaxDistance, 0.0f, 1.0f);
		float Score = DistanceWeight + (FacingDot * FacingWeight);
		if (bFromTrace)
		{
			Score += 1.0f;
		}

		if (Score > BestScore)
		{
			BestScore = Score;
			BestToolComponent = ToolComponent;
		}
	};

	auto GatherToolComponentsFromActor = [&](AActor* CandidateActor, bool bFromTrace)
	{
		if (!CandidateActor)
		{
			return;
		}

		TArray<UToolWorldComponent*> CandidateTools;
		CandidateActor->GetComponents<UToolWorldComponent>(CandidateTools);
		for (UToolWorldComponent* CandidateTool : CandidateTools)
		{
			ScoreCandidateTool(CandidateTool, bFromTrace);
		}

		if (AActor* ParentActor = CandidateActor->GetAttachParentActor())
		{
			CandidateTools.Reset();
			ParentActor->GetComponents<UToolWorldComponent>(CandidateTools);
			for (UToolWorldComponent* CandidateTool : CandidateTools)
			{
				ScoreCandidateTool(CandidateTool, bFromTrace);
			}
		}
	};

	{
		TArray<FHitResult> Hits;
		const FVector TraceEnd = ViewLocation + (ViewForward * FMath::Max(0.0f, InteractionTraceDistance));
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ToolSystemTrace), false, AgentCharacter);
		QueryParams.AddIgnoredActor(AgentCharacter);
		World->SweepMultiByChannel(
			Hits,
			ViewLocation,
			TraceEnd,
			FQuat::Identity,
			TraceChannel,
			FCollisionShape::MakeSphere(FMath::Max(0.0f, InteractionTraceRadius)),
			QueryParams);

		for (const FHitResult& HitResult : Hits)
		{
			GatherToolComponentsFromActor(HitResult.GetActor(), true);
		}
	}

	{
		TArray<FOverlapResult> Overlaps;
		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
		ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(ToolSystemOverlap), false, AgentCharacter);
		QueryParams.AddIgnoredActor(AgentCharacter);
		World->OverlapMultiByObjectType(
			Overlaps,
			AgentCharacter->GetActorLocation(),
			FQuat::Identity,
			ObjectQueryParams,
			FCollisionShape::MakeSphere(FMath::Max(0.0f, NearbySearchRadius)),
			QueryParams);

		for (const FOverlapResult& OverlapResult : Overlaps)
		{
			GatherToolComponentsFromActor(OverlapResult.GetActor(), false);
		}
	}

	return BestToolComponent;
}

bool UToolSystemComponent::ResolveHoldView(FVector& OutLocation, FRotator& OutRotation) const
{
	const AAgentCharacter* AgentCharacter = ResolveAgentCharacter();
	if (!AgentCharacter)
	{
		return false;
	}

	return AgentCharacter->GetHeldToolView(OutLocation, OutRotation);
}
