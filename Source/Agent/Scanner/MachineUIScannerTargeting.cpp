// Copyright Epic Games, Inc. All Rights Reserved.

#include "MachineUIScannerTargeting.h"
#include "MachineUIDataProviderInterface.h"
#include "Machine/MachineComponent.h"
#include "GameFramework/Actor.h"

namespace
{
void GatherCandidateActorChain(AActor* CandidateActor, TArray<AActor*>& OutCandidates)
{
	OutCandidates.Reset();
	if (!CandidateActor)
	{
		return;
	}

	TArray<AActor*> PendingActors;
	TSet<TWeakObjectPtr<AActor>> VisitedActors;
	PendingActors.Add(CandidateActor);

	while (PendingActors.Num() > 0)
	{
		AActor* CurrentActor = PendingActors.Pop(EAllowShrinking::No);
		if (!CurrentActor || VisitedActors.Contains(CurrentActor))
		{
			continue;
		}

		VisitedActors.Add(CurrentActor);
		OutCandidates.Add(CurrentActor);

		if (AActor* OwnerActor = CurrentActor->GetOwner())
		{
			PendingActors.Add(OwnerActor);
		}

		if (AActor* ParentActor = CurrentActor->GetAttachParentActor())
		{
			PendingActors.Add(ParentActor);
		}

		TArray<AActor*> AttachedActors;
		CurrentActor->GetAttachedActors(AttachedActors, false, false);
		for (AActor* AttachedActor : AttachedActors)
		{
			PendingActors.Add(AttachedActor);
		}
	}
}
}

UMachineComponent* AgentMachineUITargeting::ResolveMachineComponentFromCandidate(AActor* CandidateActor)
{
	TArray<AActor*> CandidateActors;
	GatherCandidateActorChain(CandidateActor, CandidateActors);

	for (AActor* Candidate : CandidateActors)
	{
		if (!Candidate)
		{
			continue;
		}

		if (UMachineComponent* MachineComponent = Candidate->FindComponentByClass<UMachineComponent>())
		{
			return MachineComponent;
		}
	}

	return nullptr;
}

AActor* AgentMachineUITargeting::ResolveMachineActorFromCandidate(AActor* CandidateActor)
{
	if (UMachineComponent* MachineComponent = ResolveMachineComponentFromCandidate(CandidateActor))
	{
		return MachineComponent->GetOwner();
	}

	TArray<AActor*> CandidateActors;
	GatherCandidateActorChain(CandidateActor, CandidateActors);
	for (AActor* Candidate : CandidateActors)
	{
		if (Candidate && Candidate->GetClass()->ImplementsInterface(UMachineUIDataProviderInterface::StaticClass()))
		{
			return Candidate;
		}
	}

	return nullptr;
}

bool AgentMachineUITargeting::IsMachineActorOrChild(AActor* CandidateActor)
{
	return ResolveMachineActorFromCandidate(CandidateActor) != nullptr;
}

bool AgentMachineUITargeting::IsActorPartOfMachine(AActor* CandidateActor, const UMachineComponent* MachineComponent)
{
	if (!CandidateActor || !MachineComponent)
	{
		return false;
	}

	return ResolveMachineComponentFromCandidate(CandidateActor) == MachineComponent;
}

bool AgentMachineUITargeting::IsActorPartOfMachineActor(AActor* CandidateActor, const AActor* MachineActor)
{
	if (!CandidateActor || !MachineActor)
	{
		return false;
	}

	TArray<AActor*> CandidateActors;
	GatherCandidateActorChain(CandidateActor, CandidateActors);
	for (AActor* Candidate : CandidateActors)
	{
		if (Candidate == MachineActor)
		{
			return true;
		}
	}

	return false;
}
