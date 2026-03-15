// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Objects/Types/ObjectDamageTypes.h"
#include "ObjectDamageSourceComponent.generated.h"

class AActor;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnObjectDamageSourceAppliedSignature, AActor*, TargetActor, FObjectDamageResult, DamageResult);

UCLASS(ClassGroup=(Objects), meta=(BlueprintSpawnableComponent))
class UObjectDamageSourceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UObjectDamageSourceComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category="Objects|Damage Source")
	bool HasValidTargetVolume() const;

	UFUNCTION(BlueprintPure, Category="Objects|Damage Source")
	float GetConfiguredDamageAmountForDeltaTime(float DeltaTime) const;

	UFUNCTION(BlueprintCallable, Category="Objects|Damage Source")
	FObjectDamageResult ApplyConfiguredDamageToActor(AActor* TargetActor);

	UFUNCTION(BlueprintCallable, Category="Objects|Damage Source", meta=(AdvancedDisplay="DamageLocation,DamageImpulse,DeltaTime"))
	FObjectDamageResult ApplyConfiguredDamageToActorWithContext(
		AActor* TargetActor,
		FVector DamageLocation,
		FVector DamageImpulse,
		float DeltaTime);

	UFUNCTION(BlueprintCallable, Category="Objects|Damage Source")
	int32 ApplyConfiguredDamageToCurrentOverlaps(float DeltaTime);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Damage Source")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Damage Source")
	EObjectDamageSourceKind SourceKind = EObjectDamageSourceKind::Generic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Damage Source")
	FName SourceName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Damage Source")
	EObjectDamageApplicationMode ApplicationMode = EObjectDamageApplicationMode::Instant;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Damage Source", meta=(ClampMin="0.0", UIMin="0.0", ToolTip="Instant mode: damage per application. Per Second mode: damage per second."))
	float DamageAmount = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Damage Source")
	FVector DamageImpulse = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Damage Source|Overlap")
	bool bAutoApplyToOverlaps = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Damage Source|Overlap")
	FComponentReference TargetVolumeComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Damage Source|Overlap", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bAutoApplyToOverlaps", ToolTip="0 means apply every tick."))
	float AutoApplyInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Damage Source|Overlap")
	bool bIgnoreOwner = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Damage Source|Overlap")
	bool bOnlyAffectActorsWithHealth = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Damage Source|Overlap", meta=(ToolTip="If enabled, overlapping AgentCharacter pawns are forced into ragdoll as soon as they enter the target overlap volume."))
	bool bRagdollAgentCharactersOnOverlap = false;

	UPROPERTY(BlueprintAssignable, Category="Objects|Damage Source")
	FOnObjectDamageSourceAppliedSignature OnConfiguredDamageApplied;

protected:
	UFUNCTION()
	void HandleTargetPrimitiveBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	void RefreshOverlapBinding();
	void UnbindFromTargetPrimitiveOverlap();
	bool TryTriggerConfiguredRagdoll(AActor* TargetActor) const;
	UPrimitiveComponent* ResolveTargetPrimitive() const;
	float ResolveEffectiveDeltaTime(float RequestedDeltaTime) const;
	FName ResolveEffectiveSourceName() const;

	UPROPERTY(Transient)
	float AutoApplyAccumulatorSeconds = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UPrimitiveComponent> BoundTargetOverlapPrimitive = nullptr;
};
