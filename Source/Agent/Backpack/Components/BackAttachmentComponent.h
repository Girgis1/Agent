// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BackAttachmentComponent.generated.h"

class ACharacter;
class ABlackHoleBackpackActor;
class AActor;
class USceneComponent;
class USkeletalMeshComponent;

UCLASS(ClassGroup=(BackItem), meta=(BlueprintSpawnableComponent))
class UBackAttachmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBackAttachmentComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category="BackItem")
	ABlackHoleBackpackActor* EnsureBackItem();

	UFUNCTION(BlueprintPure, Category="BackItem|Equip")
	ABlackHoleBackpackActor* FindNearestBackpackInRange(float RangeOverrideCm = -1.0f) const;

	UFUNCTION(BlueprintCallable, Category="BackItem|Equip")
	bool EquipNearestBackpack(float RangeOverrideCm = -1.0f);

	UFUNCTION(BlueprintCallable, Category="BackItem|Equip")
	bool EquipBackpack(ABlackHoleBackpackActor* InBackpack, bool bUseMagnetLerp = true);

	UFUNCTION(BlueprintCallable, Category="BackItem")
	void SetBackItemActor(ABlackHoleBackpackActor* InBackItemActor, bool bAttachImmediately = true);

	UFUNCTION(BlueprintCallable, Category="BackItem")
	void ClearBackItemActor(bool bDestroyActor = false);

	UFUNCTION(BlueprintCallable, Category="BackItem")
	bool ToggleBackItemDeployment();

	UFUNCTION(BlueprintCallable, Category="BackItem")
	bool DeployBackItem();

	UFUNCTION(BlueprintCallable, Category="BackItem")
	bool RecallBackItem();

	UFUNCTION(BlueprintCallable, Category="BackItem")
	void RefreshBackItemAttachment();

	UFUNCTION(BlueprintPure, Category="BackItem")
	ABlackHoleBackpackActor* GetBackItemActor() const { return BackItemActor; }

	UFUNCTION(BlueprintPure, Category="BackItem")
	bool IsBackItemDeployed() const;

	UFUNCTION(BlueprintCallable, Category="BackItem|Tuning")
	void SetAttachFineTune(const FVector& NewLocationOffset, const FRotator& NewRotationOffset);

protected:
	ACharacter* ResolveOwnerCharacter() const;
	USkeletalMeshComponent* ResolveCarrierMesh() const;
	bool ResolveAttachTarget(USceneComponent*& OutAttachParent, FName& OutAttachSocketName) const;
	USceneComponent* ResolveMagnetAttachComponent() const;
	bool DoesActorMatchMagnetHint(const AActor* CandidateActor) const;
	ABlackHoleBackpackActor* FindWorldBackItemCandidate(const ACharacter* OwnerCharacter, float MaxRangeCm = -1.0f) const;
	FTransform BuildAttachFineTuneTransform() const;
	FTransform BuildTargetBackItemWorldTransform(
		const ABlackHoleBackpackActor* BackItem,
		const USceneComponent* AttachParent,
		FName AttachSocketName) const;
	FVector BuildDropImpulse() const;
	bool CanEquipBackItem(const ABlackHoleBackpackActor* CandidateItem, float MaxRangeCm = -1.0f) const;
	void UpdateMagnetRecall(float DeltaTime);
	void StopMagnetRecall(bool bSnapAttach);
	void DrawAttachDebug() const;

protected:
	UPROPERTY(Transient)
	TObjectPtr<ABlackHoleBackpackActor> BackItemActor = nullptr;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BackItem")
	TSubclassOf<ABlackHoleBackpackActor> BackItemClass;

	/** Legacy compatibility flag. Backpacks are now world-persistent and are not auto-spawned by this component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BackItem")
	bool bAutoSpawnBackItem = false;

	/** Legacy compatibility flag retained for existing Blueprints. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BackItem|Spawn")
	bool bClaimExistingWorldBackItemBeforeSpawn = false;

	/** Legacy compatibility value retained for existing Blueprints. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BackItem|Spawn", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bClaimExistingWorldBackItemBeforeSpawn"))
	float ExistingWorldBackItemClaimRadius = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BackItem|Attach")
	FName AttachSocketName = TEXT("spine_03");

	/** Optional custom magnet component or child actor tag to attach to (for example: PlayerMagnet). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BackItem|Attach")
	FName AttachMagnetTag = TEXT("PlayerMagnet");

	/** Name/class hint used to find an attach magnet actor or component when no tag is present. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BackItem|Attach")
	FString AttachMagnetNameHint = TEXT("PlayerMagnet");

	/** If true, search for a magnet first and attach to its transform (no socket offset guessing). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BackItem|Attach")
	bool bPreferAttachMagnet = true;

	/** If true, do not fall back to mesh socket attachment when a magnet cannot be found. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BackItem|Attach", meta=(EditCondition="bPreferAttachMagnet"))
	bool bRequireAttachMagnet = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BackItem|Attach")
	FVector AttachLocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BackItem|Attach")
	FRotator AttachRotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BackItem|Attach")
	FVector AttachScale = FVector::OneVector;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BackItem|Recall|Magnet")
	bool bUseMagnetRecall = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BackItem|Recall|Magnet", meta=(ClampMin="0.01", UIMin="0.01", EditCondition="bUseMagnetRecall"))
	float MagnetMoveInterpSpeed = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BackItem|Recall|Magnet", meta=(ClampMin="0.01", UIMin="0.01", EditCondition="bUseMagnetRecall"))
	float MagnetRotationInterpSpeed = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BackItem|Recall|Magnet", meta=(ClampMin="0.1", UIMin="0.1", EditCondition="bUseMagnetRecall"))
	float MagnetAttachDistance = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BackItem|Recall|Magnet", meta=(ClampMin="0.1", UIMin="0.1", EditCondition="bUseMagnetRecall"))
	float MagnetAttachAngleDegrees = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BackItem|Debug")
	bool bShowAttachDebug = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BackItem|Debug", meta=(ClampMin="1.0", UIMin="1.0"))
	float AttachDebugAxisLength = 18.0f;

protected:
	UPROPERTY(Transient)
	bool bMagnetRecallActive = false;

	UPROPERTY(Transient)
	mutable TWeakObjectPtr<USceneComponent> CachedAttachMagnetComponent = nullptr;
};
