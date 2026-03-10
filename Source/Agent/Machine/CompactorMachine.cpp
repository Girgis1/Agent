// Copyright Epic Games, Inc. All Rights Reserved.

#include "Machine/CompactorMachine.h"
#include "Machine/CompactorComponent.h"

ACompactorMachine::ACompactorMachine()
{
	CompactorComponent = CreateDefaultSubobject<UCompactorComponent>(TEXT("CompactorComponent"));
}

void ACompactorMachine::BeginPlay()
{
	Super::BeginPlay();

	if (CompactorComponent)
	{
		CompactorComponent->SetMachineComponent(MachineComponent);
		CompactorComponent->SetOutputVolume(OutputVolume);
	}
}

