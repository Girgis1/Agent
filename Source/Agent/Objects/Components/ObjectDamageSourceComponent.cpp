// Copyright Epic Games, Inc. All Rights Reserved.

#include "Objects/Components/ObjectDamageSourceComponent.h"
#include "AgentCharacter.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Objects/Components/ObjectHealthComponent.h"

namespace
{
UPrimitiveComponent* ResolveComponentReferencePrimitiveForDamageSource(const FComponentReference& ComponentReference, const AActor* OwnerActor)
{
	if (!OwnerActor)
	{
		return nullptr;
	}

	if (UActorComponent* ResolvedComponent = ComponentReference.GetComponent(const_cast<AActor*>(OwnerActor)))
	{
		return Cast<UPrimitiveComponent>(ResolvedComponent);
	}

	return nullptr;
}
}

UObjectDamageSourceComponent::UObjectDamageSourceComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UObjectDamageSourceComponent::BeginPlay()
{
	Super::BeginPlay();
	AutoApplyAccumulatorSeconds = 0.0f;
	RefreshOverlapBinding();
}

void UObjectDamageSourceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromTargetPrimitiveOverlap();
	Super::EndPlay(EndPlayReason);
}

void UObjectDamageSourceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bRagdollAgentCharactersOnOverlap)
	{
		RefreshOverlapBinding();
	}
	else
	{
		UnbindFromTargetPrimitiveOverlap();
	}

	if (!bEnabled || !bAutoApplyToOverlaps)
	{
		AutoApplyAccumulatorSeconds = 0.0f;
		return;
	}

	const float SafeDeltaTime = FMath::Max(0.0f, DeltaTime);
	const float SafeInterval = FMath::Max(0.0f, AutoApplyInterval);
	if (SafeInterval <= KINDA_SMALL_NUMBER)
	{
		ApplyConfiguredDamageToCurrentOverlaps(SafeDeltaTime);
		return;
	}

	AutoApplyAccumulatorSeconds += SafeDeltaTime;
	while (AutoApplyAccumulatorSeconds + KINDA_SMALL_NUMBER >= SafeInterval)
	{
		ApplyConfiguredDamageToCurrentOverlaps(SafeInterval);
		AutoApplyAccumulatorSeconds = FMath::Max(0.0f, AutoApplyAccumulatorSeconds - SafeInterval);
	}
}

bool UObjectDamageSourceComponent::HasValidTargetVolume() const
{
	return ResolveTargetPrimitive() != nullptr;
}

float UObjectDamageSourceComponent::GetConfiguredDamageAmountForDeltaTime(float DeltaTime) const
{
	const float SafeDamageAmount = FMath::Max(0.0f, DamageAmount);
	if (ApplicationMode == EObjectDamageApplicationMode::PerSecond)
	{
		return SafeDamageAmount * FMath::Max(0.0f, DeltaTime);
	}

	return SafeDamageAmount;
}

FObjectDamageResult UObjectDamageSourceComponent::ApplyConfiguredDamageToActor(AActor* TargetActor)
{
	return ApplyConfiguredDamageToActorWithContext(TargetActor, FVector::ZeroVector, FVector::ZeroVector, 0.0f);
}

FObjectDamageResult UObjectDamageSourceComponent::ApplyConfiguredDamageToActorWithContext(
	AActor* TargetActor,
	FVector DamageLocation,
	FVector DamageImpulseOverride,
	float DeltaTime)
{
	FObjectDamageResult DamageResult;
	if (!bEnabled || !IsValid(TargetActor))
	{
		return DamageResult;
	}

	if (bIgnoreOwner && TargetActor == GetOwner())
	{
		return DamageResult;
	}

	UObjectHealthComponent* HealthComponent = UObjectHealthComponent::FindObjectHealthComponent(TargetActor);
	if (!HealthComponent)
	{
		return DamageResult;
	}

	if (bOnlyAffectActorsWithHealth && !HealthComponent->IsHealthInitialized())
	{
		HealthComponent->InitializeHealth();
	}

	const float EffectiveDeltaTime = ResolveEffectiveDeltaTime(DeltaTime);
	const float AppliedDamageAmount = GetConfiguredDamageAmountForDeltaTime(EffectiveDeltaTime);
	if (AppliedDamageAmount <= KINDA_SMALL_NUMBER)
	{
		return DamageResult;
	}

	const FVector EffectiveDamageLocation = DamageLocation.IsNearlyZero() ? TargetActor->GetActorLocation() : DamageLocation;
	const FVector EffectiveDamageImpulse = DamageImpulseOverride.IsNearlyZero() ? DamageImpulse : DamageImpulseOverride;

	DamageResult = HealthComponent->ApplyDamage(
		AppliedDamageAmount,
		SourceKind,
		GetOwner(),
		EffectiveDamageLocation,
		EffectiveDamageImpulse,
		ResolveEffectiveSourceName());

	if (DamageResult.bApplied)
	{
		OnConfiguredDamageApplied.Broadcast(TargetActor, DamageResult);
	}

	return DamageResult;
}

int32 UObjectDamageSourceComponent::ApplyConfiguredDamageToCurrentOverlaps(float DeltaTime)
{
	if (!bEnabled)
	{
		return 0;
	}

	UPrimitiveComponent* TargetPrimitive = ResolveTargetPrimitive();
	if (!TargetPrimitive)
	{
		return 0;
	}

	TArray<AActor*> OverlappingActors;
	TargetPrimitive->GetOverlappingActors(OverlappingActors);

	int32 DamagedActorCount = 0;
	for (AActor* OverlappingActor : OverlappingActors)
	{
		if (!IsValid(OverlappingActor))
		{
			continue;
		}

		if (bIgnoreOwner && OverlappingActor == GetOwner())
		{
			continue;
		}

		TryTriggerConfiguredRagdoll(OverlappingActor);

		if (bOnlyAffectActorsWithHealth && !UObjectHealthComponent::FindObjectHealthComponent(OverlappingActor))
		{
			continue;
		}

		const FObjectDamageResult DamageResult = ApplyConfiguredDamageToActorWithContext(
			OverlappingActor,
			OverlappingActor->GetActorLocation(),
			DamageImpulse,
			DeltaTime);

		if (DamageResult.bApplied)
		{
			++DamagedActorCount;
		}
	}

	return DamagedActorCount;
}

void UObjectDamageSourceComponent::HandleTargetPrimitiveBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	TryTriggerConfiguredRagdoll(OtherActor);
}

void UObjectDamageSourceComponent::RefreshOverlapBinding()
{
	if (!bRagdollAgentCharactersOnOverlap)
	{
		UnbindFromTargetPrimitiveOverlap();
		return;
	}

	UPrimitiveComponent* ResolvedPrimitive = ResolveTargetPrimitive();
	if (BoundTargetOverlapPrimitive == ResolvedPrimitive)
	{
		return;
	}

	UnbindFromTargetPrimitiveOverlap();
	BoundTargetOverlapPrimitive = ResolvedPrimitive;
	if (BoundTargetOverlapPrimitive)
	{
		BoundTargetOverlapPrimitive->OnComponentBeginOverlap.AddUniqueDynamic(
			this,
			&UObjectDamageSourceComponent::HandleTargetPrimitiveBeginOverlap);
	}
}

void UObjectDamageSourceComponent::UnbindFromTargetPrimitiveOverlap()
{
	if (!BoundTargetOverlapPrimitive)
	{
		return;
	}

	BoundTargetOverlapPrimitive->OnComponentBeginOverlap.RemoveDynamic(
		this,
		&UObjectDamageSourceComponent::HandleTargetPrimitiveBeginOverlap);
	BoundTargetOverlapPrimitive = nullptr;
}

bool UObjectDamageSourceComponent::TryTriggerConfiguredRagdoll(AActor* TargetActor) const
{
	if (!bEnabled || !bRagdollAgentCharactersOnOverlap || !IsValid(TargetActor))
	{
		return false;
	}

	if (bIgnoreOwner && TargetActor == GetOwner())
	{
		return false;
	}

	AAgentCharacter* AgentCharacter = Cast<AAgentCharacter>(TargetActor);
	if (!AgentCharacter)
	{
		return false;
	}

	return AgentCharacter->RequestEnterRagdoll(EAgentRagdollReason::Scripted);
}

UPrimitiveComponent* UObjectDamageSourceComponent::ResolveTargetPrimitive() const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}

	if (UPrimitiveComponent* ExplicitTarget = ResolveComponentReferencePrimitiveForDamageSource(TargetVolumeComponent, OwnerActor))
	{
		return ExplicitTarget;
	}

	if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent()))
	{
		if (RootPrimitive->GetGenerateOverlapEvents())
		{
			return RootPrimitive;
		}
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	OwnerActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (PrimitiveComponent && PrimitiveComponent->GetGenerateOverlapEvents())
		{
			return PrimitiveComponent;
		}
	}

	return Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent());
}

float UObjectDamageSourceComponent::ResolveEffectiveDeltaTime(float RequestedDeltaTime) const
{
	if (ApplicationMode != EObjectDamageApplicationMode::PerSecond)
	{
		return 0.0f;
	}

	if (RequestedDeltaTime > KINDA_SMALL_NUMBER)
	{
		return RequestedDeltaTime;
	}

	if (AutoApplyInterval > KINDA_SMALL_NUMBER)
	{
		return AutoApplyInterval;
	}

	if (const UWorld* World = GetWorld())
	{
		return World->GetDeltaSeconds();
	}

	return 0.0f;
}

FName UObjectDamageSourceComponent::ResolveEffectiveSourceName() const
{
	if (!SourceName.IsNone())
	{
		return SourceName;
	}

	if (const AActor* OwnerActor = GetOwner())
	{
		return OwnerActor->GetFName();
	}

	return NAME_None;
}
