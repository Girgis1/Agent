// Copyright Epic Games, Inc. All Rights Reserved.

#include "Objects/Components/ObjectHealthComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "PhysicsEngine/BodyInstance.h"
#include "TimerManager.h"

namespace
{
UPrimitiveComponent* ResolveOwnerHealthSourcePrimitive(AActor* OwnerActor)
{
	if (!OwnerActor)
	{
		return nullptr;
	}

	if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(OwnerActor->GetRootComponent()))
	{
		return RootPrimitive;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	OwnerActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	UPrimitiveComponent* FirstUsablePrimitive = nullptr;
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent || PrimitiveComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
		{
			continue;
		}

		if (!FirstUsablePrimitive)
		{
			FirstUsablePrimitive = PrimitiveComponent;
		}

		if (PrimitiveComponent->IsSimulatingPhysics())
		{
			return PrimitiveComponent;
		}
	}

	return FirstUsablePrimitive;
}

float ResolvePrimitiveMassKgWithoutWarning(const UPrimitiveComponent* PrimitiveComponent)
{
	if (!PrimitiveComponent)
	{
		return 0.0f;
	}

	if (const FBodyInstance* BodyInstance = PrimitiveComponent->GetBodyInstance())
	{
		const float BodyMassKg = BodyInstance->GetBodyMass();
		if (BodyMassKg > KINDA_SMALL_NUMBER)
		{
			return FMath::Max(0.0f, BodyMassKg);
		}
	}

	if (PrimitiveComponent->IsSimulatingPhysics())
	{
		return FMath::Max(0.0f, PrimitiveComponent->GetMass());
	}

	return 0.0f;
}

UPrimitiveComponent* ResolveComponentReferencePrimitive(const FComponentReference& ComponentReference, const AActor* OwnerActor)
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

UObjectHealthComponent::UObjectHealthComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UObjectHealthComponent* UObjectHealthComponent::FindObjectHealthComponent(AActor* Actor)
{
	if (!Actor)
	{
		return nullptr;
	}

	if (UObjectHealthComponent* HealthComponent = Actor->FindComponentByClass<UObjectHealthComponent>())
	{
		return HealthComponent->IsHealthEnabled() ? HealthComponent : nullptr;
	}

	return nullptr;
}

float UObjectHealthComponent::GetActorMaterialRecoveryScalar(const AActor* Actor)
{
	if (const UObjectHealthComponent* HealthComponent = FindObjectHealthComponent(const_cast<AActor*>(Actor)))
	{
		return HealthComponent->GetMaterialRecoveryScalar();
	}

	return 1.0f;
}

float UObjectHealthComponent::GetActorTotalDamagedPenaltyPercent(const AActor* Actor)
{
	if (const UObjectHealthComponent* HealthComponent = FindObjectHealthComponent(const_cast<AActor*>(Actor)))
	{
		return HealthComponent->GetTotalDamagedPenaltyPercent();
	}

	return 0.0f;
}

bool UObjectHealthComponent::ApplyDamagedPenaltyToResourceQuantitiesScaled(const AActor* Actor, TMap<FName, int32>& InOutQuantitiesScaled)
{
	const float RecoveryScalar = FMath::Clamp(GetActorMaterialRecoveryScalar(Actor), 0.0f, 1.0f);
	if (RecoveryScalar >= 1.0f - KINDA_SMALL_NUMBER)
	{
		return false;
	}

	for (auto It = InOutQuantitiesScaled.CreateIterator(); It; ++It)
	{
		const int32 OriginalQuantityScaled = FMath::Max(0, It.Value());
		const int32 AdjustedQuantityScaled = FMath::Max(0, FMath::RoundToInt(static_cast<float>(OriginalQuantityScaled) * RecoveryScalar));
		if (AdjustedQuantityScaled <= 0 || It.Key().IsNone())
		{
			It.RemoveCurrent();
			continue;
		}

		It.Value() = AdjustedQuantityScaled;
	}

	return true;
}

void UObjectHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	ClampConfiguredValues();

	if (!bHealthEnabled)
	{
		UpdateDamagedPenaltyCache();
		return;
	}

	if (bEnableImpactDamage)
	{
		BindImpactSourceIfNeeded();
	}

	if (!bAutoInitializeOnBeginPlay)
	{
		UpdateDamagedPenaltyCache();
		return;
	}

	if (InitializationMode == EObjectHealthInitializationMode::FromCurrentMass
		&& bDeferMassInitializationToNextTick
		&& GetWorld())
	{
		GetWorld()->GetTimerManager().SetTimerForNextTick(this, &UObjectHealthComponent::HandleDeferredAutoInitialization);
		return;
	}

	InitializeHealth();
}

bool UObjectHealthComponent::IsDepleted() const
{
	return bHealthEnabled && bHealthInitialized && CurrentHealth <= KINDA_SMALL_NUMBER;
}

float UObjectHealthComponent::GetHealthPercent() const
{
	const float SafeMaxHealth = FMath::Max(0.01f, ResolvedMaxHealth);
	return (CurrentHealth / SafeMaxHealth) * 100.0f;
}

float UObjectHealthComponent::ResolveCurrentSourceMassKg() const
{
	return ResolvePrimitiveMassKgWithoutWarning(ResolveSourcePrimitive());
}

FObjectHealthState UObjectHealthComponent::GetHealthState() const
{
	FObjectHealthState HealthState;
	HealthState.MaxHealth = ResolvedMaxHealth;
	HealthState.CurrentHealth = CurrentHealth;
	HealthState.CurrentPhaseDamagedPenaltyPercent = CurrentPhaseDamagedPenaltyPercent;
	HealthState.TotalDamagedPenaltyPercent = TotalDamagedPenaltyPercent;
	HealthState.bInitialized = bHealthInitialized;
	HealthState.bDepleted = IsDepleted();
	return HealthState;
}

float UObjectHealthComponent::ComputeMaxHealthFromPrimitive(UPrimitiveComponent* SourcePrimitive) const
{
	if (InitializationMode != EObjectHealthInitializationMode::FromCurrentMass)
	{
		return ClampResolvedMaxHealth(ManualMaxHealth);
	}

	if (!SourcePrimitive)
	{
		return ClampResolvedMaxHealth(ManualMaxHealth);
	}

	const float SourceMassKg = ResolvePrimitiveMassKgWithoutWarning(SourcePrimitive);
	const float DesiredMaxHealth = BaseHealthOffset + (SourceMassKg * FMath::Max(0.0f, HealthPerKg));
	return ClampResolvedMaxHealth(DesiredMaxHealth);
}

float UObjectHealthComponent::GetMaterialRecoveryScalar() const
{
	return 1.0f - FMath::Clamp(TotalDamagedPenaltyPercent, 0.0f, 100.0f) / 100.0f;
}

void UObjectHealthComponent::SetInheritedDamagedPenaltyPercent(float NewPenaltyPercent)
{
	ClampConfiguredValues();

	const float PreviousHealth = CurrentHealth;
	const float PreviousMaxHealth = ResolvedMaxHealth;
	const float PreviousTotalPenalty = TotalDamagedPenaltyPercent;

	InheritedDamagedPenaltyPercent = FMath::Clamp(NewPenaltyPercent, 0.0f, 100.0f);
	UpdateDamagedPenaltyCache();
	BroadcastHealthChangedIfNeeded(PreviousHealth, PreviousMaxHealth, PreviousTotalPenalty, false);
}

void UObjectHealthComponent::InitializeHealth()
{
	if (!bHealthEnabled)
	{
		UpdateDamagedPenaltyCache();
		return;
	}

	ClampConfiguredValues();

	if (InitializationMode == EObjectHealthInitializationMode::FromCurrentMass)
	{
		InitializeHealthFromPrimitive(ResolveSourcePrimitive());
		return;
	}

	InitializeHealthInternal(ManualMaxHealth, bStartAtFullHealth);
}

void UObjectHealthComponent::InitializeHealthFromPrimitive(UPrimitiveComponent* SourcePrimitive)
{
	if (!bHealthEnabled)
	{
		UpdateDamagedPenaltyCache();
		return;
	}

	ClampConfiguredValues();

	const float DesiredMaxHealth = ComputeMaxHealthFromPrimitive(SourcePrimitive);
	InitializeHealthInternal(DesiredMaxHealth, bStartAtFullHealth);
}

bool UObjectHealthComponent::RefreshDerivedMaxHealth(bool bPreserveHealthPercent)
{
	if (!bHealthEnabled)
	{
		return false;
	}

	ClampConfiguredValues();

	if (InitializationMode != EObjectHealthInitializationMode::FromCurrentMass)
	{
		return false;
	}

	EnsureInitializedForRuntimeMutation();

	UPrimitiveComponent* SourcePrimitive = ResolveSourcePrimitive();
	const float PreviousHealth = CurrentHealth;
	const float PreviousMaxHealth = ResolvedMaxHealth;
	const float PreviousTotalPenalty = TotalDamagedPenaltyPercent;
	const float PreviousPercent = PreviousMaxHealth > KINDA_SMALL_NUMBER
		? (PreviousHealth / PreviousMaxHealth)
		: 0.0f;

	ResolvedMaxHealth = ComputeMaxHealthFromPrimitive(SourcePrimitive);
	CurrentHealth = bPreserveHealthPercent
		? ClampHealthValue(ResolvedMaxHealth * PreviousPercent)
		: ClampHealthValue(CurrentHealth);
	UpdateDamagedPenaltyCache();

	BroadcastHealthChangedIfNeeded(PreviousHealth, PreviousMaxHealth, PreviousTotalPenalty, true);
	return !FMath::IsNearlyEqual(PreviousHealth, CurrentHealth)
		|| !FMath::IsNearlyEqual(PreviousMaxHealth, ResolvedMaxHealth)
		|| !FMath::IsNearlyEqual(PreviousTotalPenalty, TotalDamagedPenaltyPercent);
}

void UObjectHealthComponent::SetCurrentHealth(float NewCurrentHealth)
{
	if (!bHealthEnabled)
	{
		return;
	}

	ClampConfiguredValues();
	EnsureInitializedForRuntimeMutation();

	const float PreviousHealth = CurrentHealth;
	const float PreviousMaxHealth = ResolvedMaxHealth;
	const float PreviousTotalPenalty = TotalDamagedPenaltyPercent;

	CurrentHealth = ClampHealthValue(NewCurrentHealth);
	UpdateDamagedPenaltyCache();
	BroadcastHealthChangedIfNeeded(PreviousHealth, PreviousMaxHealth, PreviousTotalPenalty, true);
}

void UObjectHealthComponent::SetManualMaxHealth(float NewManualMaxHealth)
{
	ManualMaxHealth = NewManualMaxHealth;
	ClampConfiguredValues();

	if (!bHealthEnabled)
	{
		return;
	}

	if (InitializationMode != EObjectHealthInitializationMode::Manual)
	{
		return;
	}

	EnsureInitializedForRuntimeMutation();

	const float PreviousHealth = CurrentHealth;
	const float PreviousMaxHealth = ResolvedMaxHealth;
	const float PreviousTotalPenalty = TotalDamagedPenaltyPercent;

	ResolvedMaxHealth = ClampResolvedMaxHealth(ManualMaxHealth);
	CurrentHealth = ClampHealthValue(CurrentHealth);
	UpdateDamagedPenaltyCache();
	BroadcastHealthChangedIfNeeded(PreviousHealth, PreviousMaxHealth, PreviousTotalPenalty, true);
}

FObjectDamageResult UObjectHealthComponent::ApplyDamage(
	float Damage,
	EObjectDamageSourceKind SourceKind,
	AActor* DamageCauser,
	FVector DamageLocation,
	FVector DamageImpulse,
	FName SourceName)
{
	FObjectDamageSpec DamageSpec;
	DamageSpec.Damage = Damage;
	DamageSpec.SourceKind = SourceKind;
	DamageSpec.SourceName = SourceName;
	DamageSpec.DamageLocation = DamageLocation;
	DamageSpec.DamageImpulse = DamageImpulse;
	DamageSpec.DamageCauserActor = DamageCauser;
	return ApplyDamageFromSpec(DamageSpec);
}

FObjectDamageResult UObjectHealthComponent::ApplyDamagePerSecond(
	float DamagePerSecond,
	float DeltaTime,
	EObjectDamageSourceKind SourceKind,
	AActor* DamageCauser,
	FVector DamageLocation,
	FVector DamageImpulse,
	FName SourceName)
{
	return ApplyDamage(
		FMath::Max(0.0f, DamagePerSecond) * FMath::Max(0.0f, DeltaTime),
		SourceKind,
		DamageCauser,
		DamageLocation,
		DamageImpulse,
		SourceName);
}

float UObjectHealthComponent::ApplyHealing(float HealingAmount, AActor* Healer)
{
	(void)Healer;

	if (!bHealthEnabled)
	{
		return 0.0f;
	}

	ClampConfiguredValues();
	EnsureInitializedForRuntimeMutation();

	if (!bCanBeHealed || HealingAmount <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	const float PreviousHealth = CurrentHealth;
	const float PreviousMaxHealth = ResolvedMaxHealth;
	const float PreviousTotalPenalty = TotalDamagedPenaltyPercent;

	CurrentHealth = ClampHealthValue(CurrentHealth + HealingAmount);
	UpdateDamagedPenaltyCache();

	const float AppliedHealing = FMath::Max(0.0f, CurrentHealth - PreviousHealth);
	if (AppliedHealing <= KINDA_SMALL_NUMBER)
	{
		return 0.0f;
	}

	BroadcastHealthChangedIfNeeded(PreviousHealth, PreviousMaxHealth, PreviousTotalPenalty, true);
	OnHealed.Broadcast(AppliedHealing, CurrentHealth);
	return AppliedHealing;
}

FObjectDamageResult UObjectHealthComponent::ApplyDamageFromSpec(const FObjectDamageSpec& DamageSpec)
{
	if (!bHealthEnabled)
	{
		return FObjectDamageResult();
	}

	ClampConfiguredValues();
	EnsureInitializedForRuntimeMutation();

	FObjectDamageResult DamageResult;
	DamageResult.RequestedDamage = FMath::Max(0.0f, DamageSpec.Damage);
	DamageResult.PreviousHealth = CurrentHealth;
	DamageResult.NewHealth = CurrentHealth;
	DamageResult.MaxHealth = ResolvedMaxHealth;
	DamageResult.CurrentPhaseDamagedPenaltyPercent = CurrentPhaseDamagedPenaltyPercent;
	DamageResult.TotalDamagedPenaltyPercent = TotalDamagedPenaltyPercent;

	if (!bCanTakeDamage || DamageResult.RequestedDamage <= KINDA_SMALL_NUMBER)
	{
		return DamageResult;
	}

	const bool bWasDepletedBeforeDamage = IsDepleted();
	const float PreviousHealth = CurrentHealth;
	const float PreviousMaxHealth = ResolvedMaxHealth;
	const float PreviousTotalPenalty = TotalDamagedPenaltyPercent;

	const float DesiredHealth = CurrentHealth - DamageResult.RequestedDamage;
	CurrentHealth = ClampHealthValue(DesiredHealth);
	UpdateDamagedPenaltyCache();

	DamageResult.NewHealth = CurrentHealth;
	DamageResult.AppliedDamage = FMath::Max(0.0f, DamageResult.PreviousHealth - DamageResult.NewHealth);
	DamageResult.CurrentPhaseDamagedPenaltyPercent = CurrentPhaseDamagedPenaltyPercent;
	DamageResult.TotalDamagedPenaltyPercent = TotalDamagedPenaltyPercent;
	DamageResult.bApplied = DamageResult.AppliedDamage > KINDA_SMALL_NUMBER;
	DamageResult.bDepleted = IsDepleted();
	DamageResult.bKilled = !bWasDepletedBeforeDamage && DamageResult.bDepleted;

	if (!DamageResult.bApplied)
	{
		if (DamageSpec.SourceKind == EObjectDamageSourceKind::Impact && bShowDamageDebug)
		{
			ShowDamageDebugMessage(DamageSpec, DamageResult, TEXT("Impact ignored"));
		}

		return DamageResult;
	}

	BroadcastHealthChangedIfNeeded(PreviousHealth, PreviousMaxHealth, PreviousTotalPenalty, true);
	OnDamageApplied.Broadcast(DamageSpec, DamageResult);
	ShowDamageDebugMessage(DamageSpec, DamageResult, TEXT("Damage applied"));

	if (DamageSpec.SourceKind == EObjectDamageSourceKind::Impact)
	{
		OnImpactDamageApplied.Broadcast(DamageSpec.ImpactVelocity, DamageResult.AppliedDamage, DamageResult.NewHealth);
	}

	if (DamageResult.bKilled && bDestroyOwnerWhenDepleted)
	{
		if (AActor* OwnerActor = GetOwner())
		{
			OwnerActor->Destroy();
		}
	}

	return DamageResult;
}

void UObjectHealthComponent::HandleDeferredAutoInitialization()
{
	if (!IsValid(this))
	{
		return;
	}

	if (!bHealthEnabled)
	{
		return;
	}

	InitializeHealth();
}

void UObjectHealthComponent::EnsureInitializedForRuntimeMutation()
{
	if (!bHealthEnabled)
	{
		return;
	}

	if (bHealthInitialized)
	{
		return;
	}

	ClampConfiguredValues();

	const float DesiredMaxHealth = (InitializationMode == EObjectHealthInitializationMode::FromCurrentMass)
		? ComputeMaxHealthFromPrimitive(ResolveSourcePrimitive())
		: ClampResolvedMaxHealth(ManualMaxHealth);

	const float PreviousHealth = CurrentHealth;
	const float PreviousMaxHealth = ResolvedMaxHealth;
	const float PreviousTotalPenalty = TotalDamagedPenaltyPercent;

	ResolvedMaxHealth = DesiredMaxHealth;
	CurrentHealth = ClampHealthValue(CurrentHealth);
	bHealthInitialized = true;
	UpdateDamagedPenaltyCache();
	bWasDepleted = IsDepleted();

	OnHealthInitialized.Broadcast(ResolvedMaxHealth, CurrentHealth);
	BroadcastHealthChangedIfNeeded(PreviousHealth, PreviousMaxHealth, PreviousTotalPenalty, false);
}

void UObjectHealthComponent::InitializeHealthInternal(float DesiredMaxHealth, bool bResetToFullHealth)
{
	const float PreviousHealth = CurrentHealth;
	const float PreviousMaxHealth = ResolvedMaxHealth;
	const float PreviousTotalPenalty = TotalDamagedPenaltyPercent;

	ResolvedMaxHealth = ClampResolvedMaxHealth(DesiredMaxHealth);
	if (bResetToFullHealth)
	{
		CurrentHealth = ResolvedMaxHealth;
	}
	else
	{
		CurrentHealth = ClampHealthValue(CurrentHealth);
	}

	bHealthInitialized = true;
	UpdateDamagedPenaltyCache();
	bWasDepleted = IsDepleted();

	OnHealthInitialized.Broadcast(ResolvedMaxHealth, CurrentHealth);
	BroadcastHealthChangedIfNeeded(PreviousHealth, PreviousMaxHealth, PreviousTotalPenalty, false);
}

void UObjectHealthComponent::ClampConfiguredValues()
{
	ManualMaxHealth = FMath::Max(0.01f, ManualMaxHealth);
	ResolvedMaxHealth = FMath::Max(0.01f, ResolvedMaxHealth);
	CurrentHealth = FMath::Max(0.0f, CurrentHealth);
	HealthPerKg = FMath::Max(0.0f, HealthPerKg);
	MinimumResolvedMaxHealth = FMath::Max(0.01f, MinimumResolvedMaxHealth);
	ImpactDamageVelocity = FMath::Max(0.0f, ImpactDamageVelocity);
	MaxImpactDamageVelocity = FMath::Max(ImpactDamageVelocity, MaxImpactDamageVelocity);
	MaxImpactDamage = FMath::Max(0.0f, MaxImpactDamage);
	ImpactDamageCooldown = FMath::Max(0.0f, ImpactDamageCooldown);
	DamagedPenaltyPercent = FMath::Clamp(DamagedPenaltyPercent, 0.0f, 100.0f);
	InheritedDamagedPenaltyPercent = FMath::Clamp(InheritedDamagedPenaltyPercent, 0.0f, 100.0f);

	if (MaximumResolvedMaxHealth > 0.0f)
	{
		MaximumResolvedMaxHealth = FMath::Max(MinimumResolvedMaxHealth, MaximumResolvedMaxHealth);
	}
}

float UObjectHealthComponent::ClampResolvedMaxHealth(float DesiredMaxHealth) const
{
	float ClampedMaxHealth = FMath::Max(MinimumResolvedMaxHealth, DesiredMaxHealth);
	if (MaximumResolvedMaxHealth > 0.0f)
	{
		ClampedMaxHealth = FMath::Min(ClampedMaxHealth, MaximumResolvedMaxHealth);
	}

	return FMath::Max(0.01f, ClampedMaxHealth);
}

float UObjectHealthComponent::GetMinimumAllowedHealth() const
{
	return bCanDie ? 0.0f : FMath::Min(ResolvedMaxHealth, 0.01f);
}

float UObjectHealthComponent::ClampHealthValue(float DesiredHealth) const
{
	return FMath::Clamp(DesiredHealth, GetMinimumAllowedHealth(), ResolvedMaxHealth);
}

void UObjectHealthComponent::UpdateDamagedPenaltyCache()
{
	if (!bHealthEnabled)
	{
		CurrentPhaseDamagedPenaltyPercent = 0.0f;
		TotalDamagedPenaltyPercent = FMath::Clamp(InheritedDamagedPenaltyPercent, 0.0f, 100.0f);
		return;
	}

	const float SafeMaxHealth = FMath::Max(0.01f, ResolvedMaxHealth);
	const float MissingHealthFraction = FMath::Clamp(1.0f - (CurrentHealth / SafeMaxHealth), 0.0f, 1.0f);
	CurrentPhaseDamagedPenaltyPercent = MissingHealthFraction * DamagedPenaltyPercent;
	TotalDamagedPenaltyPercent = FMath::Clamp(InheritedDamagedPenaltyPercent + CurrentPhaseDamagedPenaltyPercent, 0.0f, 100.0f);
}

void UObjectHealthComponent::ShowDamageDebugMessage(
	const FObjectDamageSpec& DamageSpec,
	const FObjectDamageResult& DamageResult,
	const FString& ContextLabel) const
{
	if (!bShowDamageDebug || !GEngine)
	{
		return;
	}

	const UEnum* SourceKindEnum = StaticEnum<EObjectDamageSourceKind>();
	const FString SourceKindLabel = SourceKindEnum
		? SourceKindEnum->GetDisplayNameTextByValue(static_cast<int64>(DamageSpec.SourceKind)).ToString()
		: TEXT("Unknown");

	const FString OwnerLabel = GetOwner() ? GetOwner()->GetName() : GetName();
	const FString Message = FString::Printf(
		TEXT("%s | %s | Type: %s | Damage: %.2f -> %.2f | Health: %.2f / %.2f | Velocity: %.2f | Penalty: %.1f%%"),
		*OwnerLabel,
		*ContextLabel,
		*SourceKindLabel,
		DamageResult.RequestedDamage,
		DamageResult.AppliedDamage,
		DamageResult.NewHealth,
		DamageResult.MaxHealth,
		DamageSpec.ImpactVelocity,
		DamageResult.TotalDamagedPenaltyPercent);

	GEngine->AddOnScreenDebugMessage(
		static_cast<uint64>(GetUniqueID()) + 31000ULL,
		FMath::Max(0.0f, DamageDebugDurationSeconds),
		DamageDebugColor,
		Message);
}

void UObjectHealthComponent::BroadcastHealthChangedIfNeeded(
	float PreviousHealth,
	float PreviousMaxHealth,
	float PreviousTotalDamagedPenaltyPercent,
	bool bAllowStateTransitionEvents)
{
	const bool bHealthChanged = !FMath::IsNearlyEqual(PreviousHealth, CurrentHealth);
	const bool bMaxHealthChanged = !FMath::IsNearlyEqual(PreviousMaxHealth, ResolvedMaxHealth);
	const bool bPenaltyChanged = !FMath::IsNearlyEqual(PreviousTotalDamagedPenaltyPercent, TotalDamagedPenaltyPercent);
	if (bHealthChanged || bMaxHealthChanged || bPenaltyChanged)
	{
		OnHealthChanged.Broadcast(PreviousHealth, CurrentHealth, ResolvedMaxHealth, TotalDamagedPenaltyPercent);
	}

	if (!bAllowStateTransitionEvents)
	{
		return;
	}

	const bool bIsDepletedNow = IsDepleted();
	if (bIsDepletedNow == bWasDepleted)
	{
		return;
	}

	if (bIsDepletedNow)
	{
		OnDepleted.Broadcast();
	}
	else
	{
		OnRevived.Broadcast();
	}

	bWasDepleted = bIsDepletedNow;
}

UPrimitiveComponent* UObjectHealthComponent::ResolveSourcePrimitive() const
{
	return ResolveOwnerHealthSourcePrimitive(GetOwner());
}

UPrimitiveComponent* UObjectHealthComponent::ResolveImpactPrimitive() const
{
	if (UPrimitiveComponent* ExplicitImpactPrimitive = ResolveComponentReferencePrimitive(ImpactSourceComponent, GetOwner()))
	{
		return ExplicitImpactPrimitive;
	}

	return ResolveSourcePrimitive();
}

void UObjectHealthComponent::BindImpactSourceIfNeeded()
{
	if (!bHealthEnabled || !bEnableImpactDamage)
	{
		return;
	}

	UPrimitiveComponent* ImpactPrimitive = ResolveImpactPrimitive();
	if (!ImpactPrimitive)
	{
		return;
	}

	if (BoundImpactPrimitive.Get() == ImpactPrimitive)
	{
		return;
	}

	if (UPrimitiveComponent* ExistingPrimitive = BoundImpactPrimitive.Get())
	{
		ExistingPrimitive->OnComponentHit.RemoveDynamic(this, &UObjectHealthComponent::HandleImpactHit);
	}

	BoundImpactPrimitive = ImpactPrimitive;
	ImpactPrimitive->SetNotifyRigidBodyCollision(true);
	ImpactPrimitive->OnComponentHit.AddUniqueDynamic(this, &UObjectHealthComponent::HandleImpactHit);
}

void UObjectHealthComponent::HandleImpactHit(
	UPrimitiveComponent* HitComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (!bHealthEnabled || !bEnableImpactDamage || !bCanTakeDamage || !HitComponent)
	{
		return;
	}

	const float CurrentTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if ((CurrentTimeSeconds - LastImpactDamageTimeSeconds) < ImpactDamageCooldown)
	{
		return;
	}

	const FVector OtherVelocity = OtherComp ? OtherComp->GetComponentVelocity() : FVector::ZeroVector;
	const FVector RelativeVelocity = HitComponent->GetComponentVelocity() - OtherVelocity;

	float MeasuredImpactVelocity = RelativeVelocity.Size();
	if (bUseClosingSpeedOnly)
	{
		const FVector ImpactNormal = Hit.ImpactNormal.GetSafeNormal();
		if (!ImpactNormal.IsNearlyZero())
		{
			MeasuredImpactVelocity = FMath::Max(0.0f, FVector::DotProduct(RelativeVelocity, -ImpactNormal));
		}
	}

	FObjectDamageSpec DamageSpec;
	DamageSpec.SourceKind = EObjectDamageSourceKind::Impact;
	DamageSpec.SourceName = TEXT("Impact");
	DamageSpec.DamageLocation = Hit.ImpactPoint;
	DamageSpec.DamageImpulse = NormalImpulse;
	DamageSpec.ImpactVelocity = MeasuredImpactVelocity;
	DamageSpec.DamageCauserActor = OtherActor;
	DamageSpec.SourceComponent = OtherComp;

	if (MeasuredImpactVelocity < ImpactDamageVelocity || MaxImpactDamage <= KINDA_SMALL_NUMBER)
	{
		if (bShowDamageDebug)
		{
			FObjectDamageResult DamageResult;
			DamageResult.RequestedDamage = 0.0f;
			DamageResult.AppliedDamage = 0.0f;
			DamageResult.PreviousHealth = CurrentHealth;
			DamageResult.NewHealth = CurrentHealth;
			DamageResult.MaxHealth = ResolvedMaxHealth;
			DamageResult.CurrentPhaseDamagedPenaltyPercent = CurrentPhaseDamagedPenaltyPercent;
			DamageResult.TotalDamagedPenaltyPercent = TotalDamagedPenaltyPercent;
			ShowDamageDebugMessage(DamageSpec, DamageResult, TEXT("Impact below threshold"));
		}

		return;
	}

	float DamageAlpha = 1.0f;
	if (MaxImpactDamageVelocity > ImpactDamageVelocity + KINDA_SMALL_NUMBER)
	{
		DamageAlpha = FMath::Clamp(
			(MeasuredImpactVelocity - ImpactDamageVelocity) / (MaxImpactDamageVelocity - ImpactDamageVelocity),
			0.0f,
			1.0f);
	}

	const float ImpactDamage = FMath::Lerp(0.0f, MaxImpactDamage, DamageAlpha);
	if (ImpactDamage <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	LastImpactDamageTimeSeconds = CurrentTimeSeconds;

	DamageSpec.Damage = ImpactDamage;
	ApplyDamageFromSpec(DamageSpec);
}
