// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interact/Components/AgentInteractorComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "Interact/Interfaces/AgentInteractable.h"

namespace
{
	struct FInteractCandidate
	{
		TWeakObjectPtr<AActor> Actor;
		FVector InteractionLocation = FVector::ZeroVector;
		float DistanceSq = TNumericLimits<float>::Max();
		bool bFromViewTrace = false;
	};
}

UAgentInteractorComponent::UAgentInteractorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UAgentInteractorComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UAgentInteractorComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	AActor* ActiveActor = ActiveInteractableActor.Get();
	if (ActiveActor && !CanUseInteractableActor(ActiveActor))
	{
		EndCurrentInteraction();
	}
}

void UAgentInteractorComponent::RegisterInteractVolume(UInteractVolumeComponent* InVolume)
{
	// Legacy no-op. Query-based interaction does not depend on overlap registration.
}

void UAgentInteractorComponent::UnregisterInteractVolume(UInteractVolumeComponent* InVolume)
{
	// Legacy no-op. Query-based interaction does not depend on overlap registration.
}

bool UAgentInteractorComponent::ToggleInteraction()
{
	if (IsInteracting())
	{
		EndCurrentInteraction();
		return true;
	}

	return BeginBestInteraction();
}

void UAgentInteractorComponent::EndCurrentInteraction()
{
	AActor* InteractableActor = ActiveInteractableActor.Get();
	ActiveInteractableActor.Reset();

	AActor* OwnerActor = GetOwner();
	if (OwnerActor
		&& InteractableActor
		&& InteractableActor->GetClass()->ImplementsInterface(UAgentInteractable::StaticClass()))
	{
		IAgentInteractable::Execute_EndInteract(InteractableActor, OwnerActor);
	}
}

bool UAgentInteractorComponent::IsInteracting() const
{
	return ActiveInteractableActor.IsValid();
}

AActor* UAgentInteractorComponent::GetActiveInteractable() const
{
	return ActiveInteractableActor.Get();
}

int32 UAgentInteractorComponent::GetCandidateCount() const
{
	return LastQueryCandidateCount;
}

bool UAgentInteractorComponent::BeginBestInteraction()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		LastQueryCandidateCount = 0;
		return false;
	}

	int32 CandidateCount = 0;
	AActor* InteractableActor = FindBestInteractableActor(CandidateCount);
	LastQueryCandidateCount = CandidateCount;
	if (!CanUseInteractableActor(InteractableActor))
	{
		return false;
	}

	if (!IAgentInteractable::Execute_BeginInteract(InteractableActor, OwnerActor))
	{
		return false;
	}

	ActiveInteractableActor = InteractableActor;
	return true;
}

AActor* UAgentInteractorComponent::FindBestInteractableActor(int32& OutCandidateCount) const
{
	OutCandidateCount = 0;

	const AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!OwnerActor || !World)
	{
		return nullptr;
	}

	FVector ViewLocation = OwnerActor->GetActorLocation();
	FRotator ViewRotation = OwnerActor->GetActorRotation();
	if (const APawn* OwnerPawn = Cast<APawn>(OwnerActor))
	{
		OwnerPawn->GetActorEyesViewPoint(ViewLocation, ViewRotation);
	}

	const FVector OwnerLocation = OwnerActor->GetActorLocation();
	const FVector ViewForward = ViewRotation.Vector().GetSafeNormal();
	TMap<TWeakObjectPtr<AActor>, FInteractCandidate> CandidateMap;

	auto AddCandidate = [&](AActor* CandidateActor, const FVector& CandidateLocation, bool bFromViewTrace)
	{
		if (!CandidateActor || CandidateActor == OwnerActor)
		{
			return;
		}

		if (!CandidateActor->GetClass()->ImplementsInterface(UAgentInteractable::StaticClass()))
		{
			return;
		}

		const float DistanceSq = FVector::DistSquared(OwnerLocation, CandidateLocation);
		const float MaxRange = FMath::Max(
			FMath::Max(0.0f, NearbyInteractRadius),
			FMath::Max(0.0f, MaxViewTraceDistance));
		if (MaxRange > 0.0f && DistanceSq > FMath::Square(MaxRange))
		{
			return;
		}

		if (FInteractCandidate* Existing = CandidateMap.Find(CandidateActor))
		{
			if ((bFromViewTrace && !Existing->bFromViewTrace) || DistanceSq < Existing->DistanceSq)
			{
				Existing->InteractionLocation = CandidateLocation;
				Existing->DistanceSq = DistanceSq;
				Existing->bFromViewTrace = bFromViewTrace;
			}
			return;
		}

		FInteractCandidate& NewCandidate = CandidateMap.Add(CandidateActor);
		NewCandidate.Actor = CandidateActor;
		NewCandidate.InteractionLocation = CandidateLocation;
		NewCandidate.DistanceSq = DistanceSq;
		NewCandidate.bFromViewTrace = bFromViewTrace;
	};

	if (bPreferLookAtTarget && MaxViewTraceDistance > 0.0f)
	{
		const FVector TraceStart = ViewLocation;
		const FVector TraceEnd = TraceStart + (ViewForward * MaxViewTraceDistance);
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(InteractorViewSweep), false, OwnerActor);
		QueryParams.AddIgnoredActor(OwnerActor);

		TArray<FHitResult> Hits;
		const bool bHit = World->SweepMultiByChannel(
			Hits,
			TraceStart,
			TraceEnd,
			FQuat::Identity,
			ECC_Visibility,
			FCollisionShape::MakeSphere(FMath::Max(0.0f, ViewTraceRadius)),
			QueryParams);
		if (bHit)
		{
			for (const FHitResult& Hit : Hits)
			{
				AActor* CandidateActor = ResolveActorFromHit(Hit);
				if (!CandidateActor)
				{
					continue;
				}

				FVector InteractionLocation = IAgentInteractable::Execute_GetInteractionLocation(
					CandidateActor,
					const_cast<AActor*>(OwnerActor));
				if (InteractionLocation.IsNearlyZero())
				{
					InteractionLocation = Hit.ImpactPoint.IsNearlyZero()
						? CandidateActor->GetActorLocation()
						: FVector(Hit.ImpactPoint);
				}

				AddCandidate(CandidateActor, InteractionLocation, true);
			}
		}
	}

	if (NearbyInteractRadius > 0.0f)
	{
		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
		ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
		ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(InteractorNearbyOverlap), false, OwnerActor);
		QueryParams.AddIgnoredActor(OwnerActor);

		TArray<FOverlapResult> Overlaps;
		const bool bAnyOverlap = World->OverlapMultiByObjectType(
			Overlaps,
			OwnerLocation,
			FQuat::Identity,
			ObjectQueryParams,
			FCollisionShape::MakeSphere(NearbyInteractRadius),
			QueryParams);
		if (bAnyOverlap)
		{
			for (const FOverlapResult& Overlap : Overlaps)
			{
				AActor* CandidateActor = ResolveActorFromOverlap(Overlap);
				if (!CandidateActor)
				{
					continue;
				}

				FVector InteractionLocation = IAgentInteractable::Execute_GetInteractionLocation(
					CandidateActor,
					const_cast<AActor*>(OwnerActor));
				if (InteractionLocation.IsNearlyZero())
				{
					InteractionLocation = CandidateActor->GetActorLocation();
				}

				AddCandidate(CandidateActor, InteractionLocation, false);
			}
		}
	}

	OutCandidateCount = CandidateMap.Num();
	if (CandidateMap.IsEmpty())
	{
		return nullptr;
	}

	AActor* BestActor = nullptr;
	float BestScore = TNumericLimits<float>::Max();

	for (const TPair<TWeakObjectPtr<AActor>, FInteractCandidate>& Pair : CandidateMap)
	{
		AActor* CandidateActor = Pair.Key.Get();
		const FInteractCandidate& Candidate = Pair.Value;
		if (!CandidateActor || !CanUseInteractableActor(CandidateActor))
		{
			continue;
		}

		if (bRequireLineOfSight)
		{
			FCollisionQueryParams LosParams(SCENE_QUERY_STAT(InteractorLineOfSight), false, OwnerActor);
			LosParams.AddIgnoredActor(OwnerActor);
			FHitResult LosHit;
			const bool bBlocked = World->LineTraceSingleByChannel(
				LosHit,
				ViewLocation,
				Candidate.InteractionLocation,
				ECC_Visibility,
				LosParams);
			if (bBlocked)
			{
				AActor* HitActor = ResolveActorFromHit(LosHit);
				if (HitActor != CandidateActor)
				{
					continue;
				}
			}
		}

		const FVector ToCandidate = (Candidate.InteractionLocation - ViewLocation).GetSafeNormal();
		const float ViewDot = FVector::DotProduct(ViewForward, ToCandidate);
		const float ViewPenalty = bPreferLookAtTarget ? ((1.0f - FMath::Clamp(ViewDot, -1.0f, 1.0f)) * ViewFacingWeight) : 0.0f;
		float Score = Candidate.DistanceSq + ViewPenalty;
		if (Candidate.bFromViewTrace)
		{
			Score *= 0.7f;
		}

		if (Score < BestScore)
		{
			BestScore = Score;
			BestActor = CandidateActor;
		}
	}

	return BestActor;
}

AActor* UAgentInteractorComponent::ResolveActorFromHit(const FHitResult& HitResult) const
{
	AActor* HitActor = HitResult.GetActor();
	if (HitActor && HitActor->GetClass()->ImplementsInterface(UAgentInteractable::StaticClass()))
	{
		return HitActor;
	}

	if (const UPrimitiveComponent* HitComponent = HitResult.GetComponent())
	{
		AActor* OwnerActor = HitComponent->GetOwner();
		if (OwnerActor && OwnerActor->GetClass()->ImplementsInterface(UAgentInteractable::StaticClass()))
		{
			return OwnerActor;
		}
	}

	return nullptr;
}

AActor* UAgentInteractorComponent::ResolveActorFromOverlap(const FOverlapResult& OverlapResult) const
{
	if (AActor* OverlapActor = OverlapResult.GetActor())
	{
		if (OverlapActor->GetClass()->ImplementsInterface(UAgentInteractable::StaticClass()))
		{
			return OverlapActor;
		}
	}

	if (const UPrimitiveComponent* OverlapComponent = OverlapResult.GetComponent())
	{
		AActor* OwnerActor = OverlapComponent->GetOwner();
		if (OwnerActor && OwnerActor->GetClass()->ImplementsInterface(UAgentInteractable::StaticClass()))
		{
			return OwnerActor;
		}
	}

	return nullptr;
}

bool UAgentInteractorComponent::CanUseInteractableActor(AActor* InteractableActor) const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !InteractableActor)
	{
		return false;
	}

	if (!InteractableActor->GetClass()->ImplementsInterface(UAgentInteractable::StaticClass()))
	{
		return false;
	}

	return IAgentInteractable::Execute_CanInteract(InteractableActor, OwnerActor);
}
