// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Objects/Types/ObjectDamageTypes.h"
#include "Objects/Types/ObjectHealthTypes.h"
#include "ObjectHealthComponent.generated.h"

class AActor;
class UPrimitiveComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnObjectHealthInitializedSignature, float, MaxHealth, float, CurrentHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnObjectHealthChangedSignature, float, PreviousHealth, float, NewHealth, float, MaxHealth, float, TotalDamagedPenaltyPercent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnObjectDamageAppliedSignature, FObjectDamageSpec, DamageSpec, FObjectDamageResult, DamageResult);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnObjectImpactDamageSignature, float, ImpactVelocity, float, AppliedDamage, float, NewHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnObjectHealthHealedSignature, float, AppliedHealing, float, NewHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnObjectHealthDepletedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnObjectHealthRevivedSignature);

UCLASS(ClassGroup=(Objects), meta=(BlueprintSpawnableComponent))
class UObjectHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UObjectHealthComponent();

	static UObjectHealthComponent* FindObjectHealthComponent(AActor* Actor);
	static float GetActorMaterialRecoveryScalar(const AActor* Actor);
	static float GetActorTotalDamagedPenaltyPercent(const AActor* Actor);
	static bool ApplyDamagedPenaltyToResourceQuantitiesScaled(const AActor* Actor, TMap<FName, int32>& InOutQuantitiesScaled);

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category="Objects|Health")
	bool IsHealthEnabled() const { return bHealthEnabled; }

	UFUNCTION(BlueprintPure, Category="Objects|Health")
	bool IsHealthInitialized() const { return bHealthInitialized; }

	UFUNCTION(BlueprintPure, Category="Objects|Health")
	bool IsDepleted() const;

	UFUNCTION(BlueprintPure, Category="Objects|Health")
	bool IsAlive() const { return !IsDepleted(); }

	UFUNCTION(BlueprintPure, Category="Objects|Health")
	float GetMaxHealth() const { return ResolvedMaxHealth; }

	UFUNCTION(BlueprintPure, Category="Objects|Health")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category="Objects|Health")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintPure, Category="Objects|Health")
	float ResolveCurrentSourceMassKg() const;

	UFUNCTION(BlueprintPure, Category="Objects|Health")
	FObjectHealthState GetHealthState() const;

	UFUNCTION(BlueprintPure, Category="Objects|Health")
	float ComputeMaxHealthFromPrimitive(UPrimitiveComponent* SourcePrimitive) const;

	UFUNCTION(BlueprintPure, Category="Objects|Health|Penalty")
	float GetCurrentPhaseDamagedPenaltyPercent() const { return CurrentPhaseDamagedPenaltyPercent; }

	UFUNCTION(BlueprintPure, Category="Objects|Health|Penalty")
	float GetTotalDamagedPenaltyPercent() const { return TotalDamagedPenaltyPercent; }

	UFUNCTION(BlueprintPure, Category="Objects|Health|Penalty")
	float GetMaterialRecoveryScalar() const;

	UFUNCTION(BlueprintCallable, Category="Objects|Health|Penalty")
	void SetInheritedDamagedPenaltyPercent(float NewPenaltyPercent);

	UFUNCTION(BlueprintCallable, Category="Objects|Health")
	void InitializeHealth();

	UFUNCTION(BlueprintCallable, Category="Objects|Health")
	void InitializeHealthFromPrimitive(UPrimitiveComponent* SourcePrimitive);

	UFUNCTION(BlueprintCallable, Category="Objects|Health")
	bool RefreshDerivedMaxHealth(bool bPreserveHealthPercent = true);

	UFUNCTION(BlueprintCallable, Category="Objects|Health")
	void SetCurrentHealth(float NewCurrentHealth);

	UFUNCTION(BlueprintCallable, Category="Objects|Health")
	void SetManualMaxHealth(float NewManualMaxHealth);

	UFUNCTION(BlueprintCallable, Category="Objects|Health", meta=(AdvancedDisplay="DamageCauser,DamageLocation,DamageImpulse,SourceName"))
	FObjectDamageResult ApplyDamage(
		float Damage,
		EObjectDamageSourceKind SourceKind,
		AActor* DamageCauser,
		FVector DamageLocation,
		FVector DamageImpulse,
		FName SourceName);

	UFUNCTION(BlueprintCallable, Category="Objects|Health", meta=(AdvancedDisplay="DamageCauser,DamageLocation,DamageImpulse,SourceName"))
	FObjectDamageResult ApplyDamagePerSecond(
		float DamagePerSecond,
		float DeltaTime,
		EObjectDamageSourceKind SourceKind,
		AActor* DamageCauser,
		FVector DamageLocation,
		FVector DamageImpulse,
		FName SourceName);

	UFUNCTION(BlueprintCallable, Category="Objects|Health")
	float ApplyHealing(float HealingAmount, AActor* Healer);

	FObjectDamageResult ApplyDamageFromSpec(const FObjectDamageSpec& DamageSpec);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Health")
	bool bHealthEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Health")
	bool bCanTakeDamage = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Health")
	bool bCanBeHealed = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Health")
	bool bCanDie = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Health")
	bool bDestroyOwnerWhenDepleted = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Health")
	bool bAutoInitializeOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Health")
	bool bDeferMassInitializationToNextTick = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Health")
	bool bStartAtFullHealth = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Health")
	EObjectHealthInitializationMode InitializationMode = EObjectHealthInitializationMode::Manual;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Health", meta=(ClampMin="0.01", UIMin="0.01"))
	float ManualMaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Health", meta=(ClampMin="0.0", UIMin="0.0"))
	float CurrentHealth = 100.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|Health")
	float ResolvedMaxHealth = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Health|Derived", meta=(ClampMin="0.0", UIMin="0.0"))
	float HealthPerKg = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Health|Derived", meta=(ClampMin="0.0", UIMin="0.0", Units="%", DisplayName="Mass Health Percent", ToolTip="Only used when InitializationMode is FromCurrentMass. 100 keeps current behavior, 10 = 10% of mass-derived max health."))
	float MassHealthPercent = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Health|Derived")
	float BaseHealthOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Health|Derived", meta=(ClampMin="0.01", UIMin="0.01"))
	float MinimumResolvedMaxHealth = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Health|Derived", meta=(ClampMin="0.0", UIMin="0.0", ToolTip="0 means no upper cap."))
	float MaximumResolvedMaxHealth = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Health|Impact")
	bool bEnableImpactDamage = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Health|Impact")
	FComponentReference ImpactSourceComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Health|Impact", meta=(ClampMin="0.0", UIMin="0.0", Units="cm/s"))
	float ImpactDamageVelocity = 600.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Health|Impact", meta=(ClampMin="0.0", UIMin="0.0", Units="cm/s"))
	float MaxImpactDamageVelocity = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Health|Impact", meta=(ClampMin="0.0", UIMin="0.0"))
	float MaxImpactDamage = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Health|Impact", meta=(ClampMin="0.0", UIMin="0.0", Units="s"))
	float ImpactDamageCooldown = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Health|Impact")
	bool bUseClosingSpeedOnly = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Health|Penalty", meta=(ClampMin="0.0", ClampMax="100.0", UIMin="0.0", UIMax="100.0", Units="%"))
	float DamagedPenaltyPercent = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Health|Debug")
	bool bShowDamageDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Health|Debug", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bShowDamageDebug", Units="s"))
	float DamageDebugDurationSeconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Health|Debug", meta=(EditCondition="bShowDamageDebug"))
	FColor DamageDebugColor = FColor::Cyan;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|Health|Penalty", meta=(ClampMin="0.0", ClampMax="100.0", Units="%"))
	float InheritedDamagedPenaltyPercent = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|Health|Penalty", meta=(ClampMin="0.0", ClampMax="100.0", Units="%"))
	float CurrentPhaseDamagedPenaltyPercent = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|Health|Penalty", meta=(ClampMin="0.0", ClampMax="100.0", Units="%"))
	float TotalDamagedPenaltyPercent = 0.0f;

	UPROPERTY(BlueprintAssignable, Category="Objects|Health")
	FOnObjectHealthInitializedSignature OnHealthInitialized;

	UPROPERTY(BlueprintAssignable, Category="Objects|Health")
	FOnObjectHealthChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category="Objects|Health")
	FOnObjectDamageAppliedSignature OnDamageApplied;

	UPROPERTY(BlueprintAssignable, Category="Objects|Health")
	FOnObjectImpactDamageSignature OnImpactDamageApplied;

	UPROPERTY(BlueprintAssignable, Category="Objects|Health")
	FOnObjectHealthHealedSignature OnHealed;

	UPROPERTY(BlueprintAssignable, Category="Objects|Health")
	FOnObjectHealthDepletedSignature OnDepleted;

	UPROPERTY(BlueprintAssignable, Category="Objects|Health")
	FOnObjectHealthRevivedSignature OnRevived;

protected:
	void HandleDeferredAutoInitialization();
	void EnsureInitializedForRuntimeMutation();
	void InitializeHealthInternal(float DesiredMaxHealth, bool bResetToFullHealth);
	void ClampConfiguredValues();
	float ClampResolvedMaxHealth(float DesiredMaxHealth) const;
	float GetMinimumAllowedHealth() const;
	float ClampHealthValue(float DesiredHealth) const;
	void UpdateDamagedPenaltyCache();
	void BroadcastHealthChangedIfNeeded(float PreviousHealth, float PreviousMaxHealth, float PreviousTotalDamagedPenaltyPercent, bool bAllowStateTransitionEvents);
	void ShowDamageDebugMessage(const FObjectDamageSpec& DamageSpec, const FObjectDamageResult& DamageResult, const FString& ContextLabel) const;
	UPrimitiveComponent* ResolveSourcePrimitive() const;
	UPrimitiveComponent* ResolveImpactPrimitive() const;
	void BindImpactSourceIfNeeded();

	UFUNCTION()
	void HandleImpactHit(
		UPrimitiveComponent* HitComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& Hit);

	UPROPERTY(Transient)
	bool bHealthInitialized = false;

	UPROPERTY(Transient)
	bool bWasDepleted = false;

	UPROPERTY(Transient)
	float LastImpactDamageTimeSeconds = -1000.0f;

	UPROPERTY(Transient)
	TWeakObjectPtr<UPrimitiveComponent> BoundImpactPrimitive;
};
