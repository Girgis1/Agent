// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/EngineTypes.h"
#include "ToolDefinition.generated.h"

UCLASS(BlueprintType, Abstract)
class AGENT_API UToolDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tool|Equip", meta=(ClampMin="1.0", UIMin="1.0", Units="cm"))
	float MaxEquipDistance = 260.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tool|Equip")
	FName HandSocketName = FName(TEXT("HandGrip_R"));

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tool|State")
	bool bDisableCollisionWhileEquipped = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tool|State")
	TEnumAsByte<ECollisionEnabled::Type> EquippedCollisionEnabled = ECollisionEnabled::QueryOnly;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tool|Drop", meta=(ClampMin="0.0", UIMin="0.0", Units="cm/s"))
	float DropForwardBoostSpeed = 120.0f;
};

