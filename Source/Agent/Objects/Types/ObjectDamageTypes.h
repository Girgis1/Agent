// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ObjectDamageTypes.generated.h"

class AActor;
class UPrimitiveComponent;

UENUM(BlueprintType)
enum class EObjectDamageSourceKind : uint8
{
	Generic UMETA(DisplayName="Generic"),
	Impact UMETA(DisplayName="Impact"),
	Tool UMETA(DisplayName="Tool"),
	Shredder UMETA(DisplayName="Shredder"),
	Hazard UMETA(DisplayName="Hazard")
};

UENUM(BlueprintType)
enum class EObjectDamageApplicationMode : uint8
{
	Instant UMETA(DisplayName="Instant"),
	PerSecond UMETA(DisplayName="Per Second")
};

USTRUCT(BlueprintType)
struct FObjectDamageSpec
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Damage")
	float Damage = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Damage")
	EObjectDamageSourceKind SourceKind = EObjectDamageSourceKind::Generic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Damage")
	FName SourceName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Damage")
	FVector DamageLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Damage")
	FVector DamageImpulse = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Damage", meta=(ClampMin="0.0", UIMin="0.0"))
	float ImpactVelocity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Damage")
	TObjectPtr<AActor> DamageCauserActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Damage")
	TObjectPtr<UPrimitiveComponent> SourceComponent = nullptr;
};

USTRUCT(BlueprintType)
struct FObjectDamageResult
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|Damage")
	float RequestedDamage = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|Damage")
	float AppliedDamage = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|Damage")
	float PreviousHealth = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|Damage")
	float NewHealth = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|Damage")
	float MaxHealth = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|Damage")
	float CurrentPhaseDamagedPenaltyPercent = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|Damage")
	float TotalDamagedPenaltyPercent = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|Damage")
	bool bApplied = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|Damage")
	bool bDepleted = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|Damage")
	bool bKilled = false;
};
