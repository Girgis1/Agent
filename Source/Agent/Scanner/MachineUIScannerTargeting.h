// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;
class UMachineComponent;

namespace AgentMachineUITargeting
{
UMachineComponent* ResolveMachineComponentFromCandidate(AActor* CandidateActor);
AActor* ResolveMachineActorFromCandidate(AActor* CandidateActor);
bool IsMachineActorOrChild(AActor* CandidateActor);
bool IsActorPartOfMachine(AActor* CandidateActor, const UMachineComponent* MachineComponent);
bool IsActorPartOfMachineActor(AActor* CandidateActor, const AActor* MachineActor);
}
