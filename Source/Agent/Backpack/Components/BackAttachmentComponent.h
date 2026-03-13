// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "UObject/ObjectKey.h"
#include "BackAttachmentComponent.generated.h"

class AActor;
class ACharacter;
class ABlackHoleBackpackActor;
class UBackpackMagnetComponent;
class UPlayerMagnetComponent;
class UPrimitiveComponent;
class USceneComponent;
class UShapeComponent;

UCLASS(ClassGroup=(BackItem), meta=(BlueprintSpawnableComponent))
class AGENT_API UBackAttachmentComponent : public UActorComponent
{
	GENERATED_BODY()

private:
	struct FTrackedMagnetItem;

public:
	UBackAttachmentComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category="BackItem")
	AActor* GetBackItemActorRaw() const { return IsValid(BackItemActor.Get()) ? BackItemActor.Get() : nullptr; }

	/** Legacy compatibility accessor for existing Blueprint graphs. */
	UFUNCTION(BlueprintPure, Category="BackItem")
	ABlackHoleBackpackActor* GetBackItemActor() const;

	UFUNCTION(BlueprintPure, Category="BackItem")
	bool IsBackItemDeployed() const;

	UFUNCTION(BlueprintPure, Category="BackItem|Equip")
	AActor* FindNearestBackItemInRange(float RangeOverrideCm = -1.0f) const;

	/** Legacy compatibility wrapper for existing Blueprint graphs. */
	UFUNCTION(BlueprintPure, Category="BackItem|Equip")
	ABlackHoleBackpackActor* FindNearestBackpackInRange(float RangeOverrideCm = -1.0f) const;

	UFUNCTION(BlueprintCallable, Category="BackItem|Equip")
	bool EquipBackItemActor(AActor* InBackItemActor, bool bRequireManualHold = false);

	/** Legacy compatibility wrapper for existing Blueprint graphs. */
	UFUNCTION(BlueprintCallable, Category="BackItem|Equip")
	bool EquipBackpack(ABlackHoleBackpackActor* InBackpack, bool bUseMagnetLerp = true);

	/** Legacy compatibility wrapper for existing Blueprint graphs. */
	UFUNCTION(BlueprintCallable, Category="BackItem|Equip")
	bool EquipNearestBackpack(float RangeOverrideCm = -1.0f);

	/** Legacy compatibility wrapper for existing Blueprint graphs. */
	UFUNCTION(BlueprintCallable, Category="BackItem")
	void SetBackItemActor(ABlackHoleBackpackActor* InBackItemActor, bool bAttachImmediately = true);

	UFUNCTION(BlueprintCallable, Category="BackItem|Deploy")
	bool DeployBackItem();

	/** Legacy compatibility wrapper for existing Blueprint graphs. */
	UFUNCTION(BlueprintCallable, Category="BackItem")
	bool ToggleBackItemDeployment();

	/** Legacy compatibility wrapper for existing Blueprint graphs. */
	UFUNCTION(BlueprintCallable, Category="BackItem")
	bool RecallBackItem();

	/** Legacy compatibility no-op for existing Blueprint graphs. */
	UFUNCTION(BlueprintCallable, Category="BackItem")
	void RefreshBackItemAttachment() {}

	/** Legacy compatibility no-op for existing Blueprint graphs. */
	UFUNCTION(BlueprintCallable, Category="BackItem|Tuning")
	void SetAttachFineTune(const FVector& NewLocationOffset, const FRotator& NewRotationOffset) { (void)NewLocationOffset; (void)NewRotationOffset; }

	UFUNCTION(BlueprintCallable, Category="BackItem")
	void ClearBackItemActor(bool bDestroyActor = false);

	UFUNCTION(BlueprintCallable, Category="BackItem|Magnet")
	void SetManualMagnetRequested(bool bInManualMagnetRequested);

	UFUNCTION(BlueprintCallable, Category="BackItem|Magnet")
	void SetManualMagnetChargeAlpha(float InChargeAlpha);

	UFUNCTION(BlueprintPure, Category="BackItem|Magnet")
	bool IsManualMagnetRequested() const { return bManualMagnetRequested; }

	UFUNCTION(BlueprintPure, Category="BackItem|Magnet")
	float GetManualMagnetChargeAlpha() const { return ManualMagnetChargeAlpha; }

	UFUNCTION(BlueprintPure, Category="BackItem|Magnet")
	bool HasLockedItems() const { return LockedItemKeys.Num() > 0; }

	UFUNCTION(BlueprintCallable, Category="BackItem|Ragdoll")
	void SetOwnerRagdolling(bool bInOwnerRagdolling);

protected:
	ACharacter* ResolveOwnerCharacter() const;
	UPlayerMagnetComponent* ResolvePlayerMagnetComponent() const;
	bool ResolveAttachTarget(USceneComponent*& OutAttachParent, FName& OutAttachSocketName) const;
	bool ResolveMagLockAnchor(USceneComponent*& OutAnchorComponent, FName& OutAnchorSocketName) const;
	UShapeComponent* ResolveMagLockZone() const;

	AActor* FindWorldBackItemCandidate(
		const ACharacter* OwnerCharacter,
		const FVector& RangeOrigin,
		float MaxRangeCm = -1.0f) const;
	bool CanEquipBackItem(
		const AActor* CandidateItem,
		const FVector& RangeOrigin,
		float MaxRangeCm = -1.0f) const;

	UPrimitiveComponent* ResolveBackItemPrimitive(AActor* BackItem) const;
	UPrimitiveComponent* ResolveBackItemPrimitive(const AActor* BackItem) const;
	UBackpackMagnetComponent* ResolveBackItemMagnet(AActor* BackItem) const;
	const UBackpackMagnetComponent* ResolveBackItemMagnet(const AActor* BackItem) const;

	bool IsBackItemDeployedState(const AActor* BackItem) const;
	bool IsActorTrackedAndLocked(const AActor* CandidateActor) const;
	bool IsActorMagneticEligible(
		const AActor* CandidateItem,
		const UPrimitiveComponent* CandidatePrimitive,
		const UBackpackMagnetComponent* CandidateMagnet,
		const UPlayerMagnetComponent* PlayerMagnet) const;
	bool HasAnyConfiguredTag(const AActor* Actor, const UActorComponent* Component, const TArray<FName>& RequiredTags) const;
	float ResolveItemMagnetStrengthMultiplier(const UBackpackMagnetComponent* BackItemMagnet) const;
	float ResolveMagnetActivationAlpha() const;
	bool DoesBackItemHaveMagLockAnchor(const AActor* BackItem) const;

	FTransform ResolveBackItemAttachPointWorldTransform(
		const AActor* BackItem,
		const UBackpackMagnetComponent* BackItemMagnet) const;
	FTransform BuildTargetBackItemWorldTransform(
		const AActor* BackItem,
		const UBackpackMagnetComponent* BackItemMagnet,
		const FTransform& TargetAttachPointWorldTransform) const;
	FVector BuildDropImpulse() const;

	void UpdateMagnetSimulation(float DeltaTime);
	void ScanMagnetCandidates(const FVector& MagnetLocation, float FieldRangeCm);
	void RegisterTrackedItem(
		AActor* BackItemActor,
		UPrimitiveComponent* BackItemPrimitive,
		const UBackpackMagnetComponent* BackItemMagnet);
	void PruneTrackedItems(float CurrentWorldTime, bool bMagnetActive);
	void ApplyMagnetForces(
		float DeltaTime,
		float MagnetAlpha,
		USceneComponent* MagnetAnchorComponent,
		FName MagnetAnchorSocketName,
		const FTransform& MagnetAnchorTransform,
		const FVector& MagnetAnchorVelocity,
		UShapeComponent* MagLockZone);

	bool IsInsideMagLockZone(const UShapeComponent* MagLockZone, const FVector& WorldLocation) const;
	void SetItemLocked(FObjectKey ItemKey, bool bLocked, float CurrentWorldTime, float ReacquireDelay);
	void ReleaseAllLockedItems(bool bSetReacquireCooldown, bool bApplyDropImpulse = false);

	void ApplyAttractionForce(
		UPrimitiveComponent* BackItemPrimitive,
		float ItemMassKg,
		const FVector& CurrentAttachPointLocation,
		const FVector& MagnetAnchorLocation,
		float DistanceToAnchor,
		float MagnetAlpha,
		float ItemStrengthMultiplier,
		const UPlayerMagnetComponent* PlayerMagnet) const;
	void ApplyMagLockForce(
		UPrimitiveComponent* BackItemPrimitive,
		float ItemMassKg,
		const FVector& CurrentAttachPointLocation,
		const FVector& CurrentAttachPointVelocity,
		const FVector& MagnetAnchorLocation,
		const FVector& MagnetAnchorVelocity,
		float MagnetAlpha,
		float ItemStrengthMultiplier,
		const UPlayerMagnetComponent* PlayerMagnet) const;
	void ApplyAlignmentTorque(
		UPrimitiveComponent* BackItemPrimitive,
		float ItemMassKg,
		const FQuat& TargetRotation,
		float MagnetAlpha,
		float ItemStrengthMultiplier,
		const UPlayerMagnetComponent* PlayerMagnet) const;
	void ApplyMagnetCollisionOverrides(
		FTrackedMagnetItem& TrackedItem,
		const UPlayerMagnetComponent* PlayerMagnet,
		float CurrentWorldTime) const;
	void RestoreMagnetCollisionOverrides(FTrackedMagnetItem& TrackedItem, float CurrentWorldTime = -1.0f) const;
	void EnsureMagnetKinematicState(FTrackedMagnetItem& TrackedItem) const;
	void RestoreMagnetKinematicState(FTrackedMagnetItem& TrackedItem, bool bWakePhysics = true) const;
	void WeldLockedItemToMagnet(
		FTrackedMagnetItem& TrackedItem,
		USceneComponent* MagnetAnchorComponent,
		FName MagnetAnchorSocketName) const;
	void UnweldLockedItemFromMagnet(FTrackedMagnetItem& TrackedItem) const;
	void ApplyStageOneWobble(
		UPrimitiveComponent* BackItemPrimitive,
		float ItemMassKg,
		float CurrentWorldTime,
		float StageAlpha,
		const UPlayerMagnetComponent* PlayerMagnet) const;
	void ApplyStageOneLiftForce(
		FTrackedMagnetItem& TrackedItem,
		const FVector& CurrentAttachPointLocation,
		const FVector& CurrentAttachPointVelocity,
		const UPlayerMagnetComponent* PlayerMagnet) const;
	void TryTriggerFrontMagnetImpactKnockdown(
		const FTrackedMagnetItem& TrackedItem,
		const UPlayerMagnetComponent* PlayerMagnet,
		float CurrentWorldTime);
	void MoveMagnetItemKinematic(
		float DeltaTime,
		FTrackedMagnetItem& TrackedItem,
		const FTransform& TargetBackItemTransform,
		float DistanceToAnchor,
		float MagnetAlpha,
		float ItemStrengthMultiplier,
		const UPlayerMagnetComponent* PlayerMagnet,
		bool bAlignRotation,
		float LocationSpeedScale = 1.0f,
		float RotationSpeedScale = 1.0f,
		float FinalSnapDistance = 0.0f) const;

	void SetCapsuleBackItemCollisionIgnored(UPrimitiveComponent* BackItemPrimitive, bool bIgnore);
	void ClearCapsuleBackItemCollisionIgnore();
	void DrawAttachDebug() const;

protected:
	UPROPERTY(Transient)
	TObjectPtr<AActor> BackItemActor = nullptr;

public:
	/** When enabled, only actors matching BackItemClass can be magnet-managed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BackItem|Filter")
	bool bRestrictToBackItemClass = false;

	/** Optional class filter for valid magnet items when bRestrictToBackItemClass is enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BackItem|Filter", meta=(EditCondition="bRestrictToBackItemClass"))
	TSubclassOf<AActor> BackItemClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BackItem|Deploy")
	float DropForwardImpulse = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BackItem|Deploy")
	float DropRightImpulse = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BackItem|Deploy")
	float DropUpImpulse = 80.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BackItem|Equip", meta=(ClampMin="0.0", UIMin="0.0"))
	float RecallMaxDistance = 1000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BackItem|Equip")
	bool bRequireLineOfSightForRecall = false;

	/** Ignore magnet-managed items against the player capsule while magnet is active; mesh collision remains intact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BackItem|Recall|Collision")
	bool bIgnoreOwnerCapsuleCollisionWhileRecalling = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BackItem|Debug")
	bool bShowAttachDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BackItem|Debug", meta=(ClampMin="1.0", UIMin="1.0"))
	float AttachDebugAxisLength = 18.0f;

protected:
	UPROPERTY(Transient)
	bool bOwnerRagdolling = false;

	UPROPERTY(Transient)
	bool bManualMagnetRequested = false;

	UPROPERTY(Transient)
	float ManualMagnetChargeAlpha = 0.0f;

	UPROPERTY(Transient)
	float ManualMagnetHeldDuration = 0.0f;

private:
	struct FTrackedMagnetItem
	{
		TWeakObjectPtr<AActor> Actor = nullptr;
		TWeakObjectPtr<UPrimitiveComponent> Primitive = nullptr;
		TWeakObjectPtr<UBackpackMagnetComponent> BackpackMagnet = nullptr;
		float LastSeenTime = 0.0f;
		float LockReacquireTime = 0.0f;
		bool bLocked = false;
		bool bCollisionResponsesCaptured = false;
		TEnumAsByte<ECollisionResponse> CachedCameraResponse = ECR_Block;
		TEnumAsByte<ECollisionResponse> CachedPawnResponse = ECR_Block;
		float CameraIgnoreUntilTime = 0.0f;
		bool bPhysicsStateCaptured = false;
		bool bWasSimulatingPhysics = false;
		TEnumAsByte<ECollisionEnabled::Type> CachedCollisionEnabled = ECollisionEnabled::QueryAndPhysics;
		bool bWeldedToMagnet = false;
		bool bHasMagLockAnchor = false;
		bool bStageOneLiftInitialized = false;
		FVector StageOneLiftBaseLocation = FVector::ZeroVector;
		float StageOneLiftTargetCm = 0.0f;
		bool bStageOneSnapDelayInitialized = false;
		float StageOneSnapDelaySeconds = 0.0f;
		float MagLockVolumeEnterTime = -1.0f;
	};

	TMap<FObjectKey, FTrackedMagnetItem> TrackedItems;
	TSet<FObjectKey> LockedItemKeys;
	TSet<TWeakObjectPtr<UPrimitiveComponent>> CapsuleIgnoredBackItemPrimitives;
	float CandidateScanTimer = 0.0f;
	float LastMagnetAlpha = 0.0f;
	float NextFrontMagnetImpactKnockdownTime = 0.0f;
	bool bCapsulePhysicsBodyResponseOverridden = false;
	TEnumAsByte<ECollisionResponse> CachedCapsulePhysicsBodyResponse = ECR_Block;
};
