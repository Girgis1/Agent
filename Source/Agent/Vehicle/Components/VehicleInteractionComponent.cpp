// Copyright Epic Games, Inc. All Rights Reserved.

#include "Vehicle/Components/VehicleInteractionComponent.h"
#include "Vehicle/Interfaces/VehicleInteractable.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"

UVehicleInteractionComponent::UVehicleInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UVehicleInteractionComponent::TickComponent(
	float DeltaTime,
	ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!IsVehicleStillControlled())
	{
		ActiveVehicleActor.Reset();
	}
}

bool UVehicleInteractionComponent::TryEnterNearestVehicle()
{
	if (IsVehicleStillControlled())
	{
		return true;
	}

	int32 CandidateCount = 0;
	AActor* VehicleActor = FindBestVehicleActor(CandidateCount);
	LastCandidateCount = CandidateCount;
	if (!VehicleActor)
	{
		return false;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !CanUseVehicleActor(VehicleActor))
	{
		return false;
	}

	const bool bEnteredVehicle = IVehicleInteractable::Execute_EnterVehicle(VehicleActor, OwnerActor);
	if (bEnteredVehicle)
	{
		ActiveVehicleActor = VehicleActor;
	}

	return bEnteredVehicle;
}

bool UVehicleInteractionComponent::TryExitCurrentVehicle()
{
	AActor* OwnerActor = GetOwner();
	AActor* VehicleActor = ActiveVehicleActor.Get();
	if (!OwnerActor || !VehicleActor)
	{
		ActiveVehicleActor.Reset();
		return false;
	}

	if (!VehicleActor->GetClass()->ImplementsInterface(UVehicleInteractable::StaticClass()))
	{
		ActiveVehicleActor.Reset();
		return false;
	}

	const bool bExitedVehicle = IVehicleInteractable::Execute_ExitVehicle(VehicleActor, OwnerActor);
	if (bExitedVehicle || !IsVehicleStillControlled())
	{
		ActiveVehicleActor.Reset();
	}

	return bExitedVehicle;
}

bool UVehicleInteractionComponent::IsControllingVehicle() const
{
	return IsVehicleStillControlled();
}

AActor* UVehicleInteractionComponent::GetControlledVehicle() const
{
	return IsVehicleStillControlled() ? ActiveVehicleActor.Get() : nullptr;
}

bool UVehicleInteractionComponent::ApplyVehicleMoveInput(float ForwardInput, float RightInput)
{
	AActor* OwnerActor = GetOwner();
	AActor* VehicleActor = ActiveVehicleActor.Get();
	if (!OwnerActor || !VehicleActor || !IsVehicleStillControlled())
	{
		return false;
	}

	IVehicleInteractable::Execute_SetVehicleMoveInput(VehicleActor, OwnerActor, ForwardInput, RightInput);
	return true;
}

void UVehicleInteractionComponent::SetVehicleHandbrakeInput(bool bHandbrakeActive)
{
	AActor* OwnerActor = GetOwner();
	AActor* VehicleActor = ActiveVehicleActor.Get();
	if (!OwnerActor || !VehicleActor || !IsVehicleStillControlled())
	{
		return;
	}

	IVehicleInteractable::Execute_SetVehicleHandbrakeInput(VehicleActor, OwnerActor, bHandbrakeActive);
}

AActor* UVehicleInteractionComponent::FindBestVehicleActor(int32& OutCandidateCount) const
{
	OutCandidateCount = 0;

	AActor* OwnerActor = GetOwner();
	UWorld* World = GetWorld();
	if (!OwnerActor || !World)
	{
		return nullptr;
	}

	FVector ViewLocation = OwnerActor->GetActorLocation();
	FRotator ViewRotation = OwnerActor->GetActorRotation();
	if (const APawn* OwnerPawn = Cast<APawn>(OwnerActor))
	{
		if (const AController* OwnerController = OwnerPawn->GetController())
		{
			OwnerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
		}
	}

	const FVector ViewForward = ViewRotation.Vector();
	const FVector TraceEnd = ViewLocation + (ViewForward * FMath::Max(0.0f, InteractionTraceDistance));
	const float MaxDistanceSq = FMath::Square(FMath::Max(0.0f, MaxEnterDistance));
	const float FacingBias = FMath::Clamp(NearbyCandidateFacingWeight, 0.0f, 1.0f);

	TSet<TWeakObjectPtr<AActor>> CandidateSet;
	AActor* BestActor = nullptr;
	float BestScore = TNumericLimits<float>::Lowest();

	auto ScoreCandidate = [&](AActor* CandidateActor, const FVector& CandidateLocation, bool bFromTrace)
	{
		AActor* VehicleActor = ResolveVehicleActor(CandidateActor);
		if (!VehicleActor || CandidateSet.Contains(VehicleActor))
		{
			return;
		}

		if (!CanUseVehicleActor(VehicleActor))
		{
			return;
		}

		CandidateSet.Add(VehicleActor);
		++OutCandidateCount;

		const FVector OwnerLocation = OwnerActor->GetActorLocation();
		const FVector EffectiveLocation = CandidateLocation.IsNearlyZero()
			? IVehicleInteractable::Execute_GetVehicleInteractionLocation(VehicleActor, OwnerActor)
			: CandidateLocation;
		const float DistanceSq = FVector::DistSquared(OwnerLocation, EffectiveLocation);
		if (MaxDistanceSq > 0.0f && DistanceSq > MaxDistanceSq)
		{
			return;
		}

		const FVector ToCandidate = (EffectiveLocation - ViewLocation).GetSafeNormal();
		const float FacingDot = FVector::DotProduct(ViewForward, ToCandidate);
		const float DistanceWeight = 1.0f - FMath::Clamp(
			FMath::Sqrt(DistanceSq) / FMath::Max(1.0f, MaxEnterDistance),
			0.0f,
			1.0f);
		float Score = DistanceWeight + (FacingDot * FacingBias);
		if (bFromTrace)
		{
			Score += 1.0f;
		}

		if (Score > BestScore)
		{
			BestScore = Score;
			BestActor = VehicleActor;
		}
	};

	{
		TArray<FHitResult> Hits;
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VehicleInteractionTrace), false, OwnerActor);
		World->SweepMultiByChannel(
			Hits,
			ViewLocation,
			TraceEnd,
			FQuat::Identity,
			TraceChannel,
			FCollisionShape::MakeSphere(FMath::Max(0.0f, InteractionTraceRadius)),
			QueryParams);

		for (const FHitResult& Hit : Hits)
		{
			AActor* HitActor = Hit.GetActor();
			if (!HitActor)
			{
				continue;
			}

			ScoreCandidate(HitActor, Hit.ImpactPoint, true);
		}
	}

	{
		TArray<FOverlapResult> Overlaps;
		FCollisionObjectQueryParams ObjectQueryParams;
		ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
		ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
		ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);

		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(VehicleInteractionOverlap), false, OwnerActor);
		World->OverlapMultiByObjectType(
			Overlaps,
			OwnerActor->GetActorLocation(),
			FQuat::Identity,
			ObjectQueryParams,
			FCollisionShape::MakeSphere(FMath::Max(0.0f, NearbySearchRadius)),
			QueryParams);

		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* CandidateActor = Overlap.GetActor();
			if (!CandidateActor)
			{
				continue;
			}

			ScoreCandidate(CandidateActor, CandidateActor->GetActorLocation(), false);
		}
	}

	return BestActor;
}

int32 UVehicleInteractionComponent::GetLastCandidateCount() const
{
	return LastCandidateCount;
}

bool UVehicleInteractionComponent::IsVehicleStillControlled() const
{
	AActor* OwnerActor = GetOwner();
	AActor* VehicleActor = ActiveVehicleActor.Get();
	if (!OwnerActor || !VehicleActor)
	{
		return false;
	}

	if (!VehicleActor->GetClass()->ImplementsInterface(UVehicleInteractable::StaticClass()))
	{
		return false;
	}

	return IVehicleInteractable::Execute_IsVehicleControlledBy(VehicleActor, OwnerActor);
}

bool UVehicleInteractionComponent::CanUseVehicleActor(AActor* VehicleActor) const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor || !VehicleActor)
	{
		return false;
	}

	if (!VehicleActor->GetClass()->ImplementsInterface(UVehicleInteractable::StaticClass()))
	{
		return false;
	}

	return IVehicleInteractable::Execute_CanEnterVehicle(VehicleActor, OwnerActor);
}

AActor* UVehicleInteractionComponent::ResolveVehicleActor(AActor* CandidateActor) const
{
	if (!CandidateActor)
	{
		return nullptr;
	}

	if (CandidateActor->GetClass()->ImplementsInterface(UVehicleInteractable::StaticClass()))
	{
		return CandidateActor;
	}

	AActor* ParentActor = CandidateActor->GetAttachParentActor();
	if (ParentActor && ParentActor->GetClass()->ImplementsInterface(UVehicleInteractable::StaticClass()))
	{
		return ParentActor;
	}

	return nullptr;
}
