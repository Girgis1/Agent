// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Machine/MachineActor.h"
#include "CompactorMachine.generated.h"

class UCompactorComponent;

UCLASS()
class ACompactorMachine : public AMachineActor
{
	GENERATED_BODY()

public:
	ACompactorMachine();

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Machine|Compactor")
	TObjectPtr<UCompactorComponent> CompactorComponent = nullptr;
};

