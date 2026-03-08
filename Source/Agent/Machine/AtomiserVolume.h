// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "Material/AgentResourceTypes.h"
#include "AtomiserVolume.generated.h"

class UMaterialComponent;
class UPrimitiveComponent;
class AActor;
class UFactoryResourceBankSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAtomiserStorageChanged);

UCLASS(ClassGroup=(Machine), meta=(BlueprintSpawnableComponent))
class UAtomiserVolume : public UBoxComponent
{
	GENERATED_BODY()

public:
	UAtomiserVolume();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintPure, Category="Machine|Atomiser")
	bool IsActorAcceptedByWhitelist(AActor* Actor) const;

	UFUNCTION(BlueprintPure, Category="Machine|Atomiser")
	float GetStoredUnits(FName ResourceId) const;

	UFUNCTION(BlueprintPure, Category="Machine|Atomiser")
	float GetTotalStoredUnits() const;

	UFUNCTION(BlueprintPure, Category="Machine|Atomiser")
	int32 GetTotalStoredScaled() const;

	UFUNCTION(BlueprintPure, Category="Machine|Atomiser")
	int32 GetStoredScaled(FName ResourceId) const;

	UFUNCTION(BlueprintPure, Category="Machine|Atomiser")
	float GetCapacityUnits() const;

	UFUNCTION(BlueprintPure, Category="Machine|Atomiser")
	float GetRemainingCapacityUnits() const;

	UFUNCTION(BlueprintPure, Category="Machine|Atomiser")
	int32 GetRemainingCapacityScaled() const;

	UFUNCTION(BlueprintPure, Category="Machine|Atomiser")
	bool HasResourceQuantityUnits(FName ResourceId, int32 QuantityUnits) const;

	UFUNCTION(BlueprintPure, Category="Machine|Atomiser")
	bool HasResourceQuantityScaled(FName ResourceId, int32 QuantityScaled) const;

	UFUNCTION(BlueprintCallable, Category="Machine|Atomiser")
	bool ConsumeResourceQuantityUnits(FName ResourceId, int32 QuantityUnits);

	UFUNCTION(BlueprintCallable, Category="Machine|Atomiser")
	bool ConsumeResourceQuantityScaled(FName ResourceId, int32 QuantityScaled);

	UFUNCTION(BlueprintCallable, Category="Machine|Atomiser")
	bool ConsumeResourcesScaledAtomic(const TMap<FName, int32>& ResourceQuantitiesScaled);

	UFUNCTION(BlueprintCallable, Category="Machine|Atomiser")
	bool AddResourcesScaledAtomic(const TMap<FName, int32>& ResourceQuantitiesScaled);

	UFUNCTION(BlueprintCallable, Category="Machine|Atomiser")
	void ClearStoredResources();

	UFUNCTION(BlueprintPure, Category="Machine|Atomiser")
	void GetStorageSnapshot(TArray<FResourceStorageEntry>& OutEntries) const;

protected:
	UFUNCTION()
	void HandleSharedResourceBankChanged();

	UFactoryResourceBankSubsystem* ResolveSharedResourceBank() const;
	void BindToSharedResourceBank();
	void UnbindFromSharedResourceBank();
	void SyncCachedStorageFromSharedBank();

	bool TryConsumeOverlappingActor(AActor* OverlappingActor);
	bool TryBuildActorResourceMap(const AActor* SourceActor, TMap<FName, int32>& OutResourceQuantitiesScaled) const;
	bool IsActorEligibleForAtomiser(const AActor* SourceActor) const;
	bool CanAcceptResourcesAtomic(const TMap<FName, int32>& ResourceQuantitiesScaled) const;
	void ShowDebugMessage(const FString& Message, FColor Color) const;
	void BroadcastStorageChanged();

	static UPrimitiveComponent* ResolveSourcePrimitive(const AActor* Actor);

	int32 GetCapacityScaled() const;

	float TimeUntilNextIntake = 0.0f;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Atomiser")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Atomiser", meta=(ClampMin="0.0", UIMin="0.0"))
	float IntakeInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Atomiser|Intake")
	bool bUseBlueprintWhitelist = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Atomiser|Intake", meta=(EditCondition="bUseBlueprintWhitelist"))
	TArray<TSoftClassPtr<AActor>> AllowedBlueprintClasses;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Atomiser|Intake")
	bool bRequireMaterialComponent = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Atomiser|Intake")
	bool bRejectFactoryPayloadActors = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Atomiser|Storage")
	bool bInfiniteStorage = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Atomiser|Storage", meta=(ClampMin="0", UIMin="0", EditCondition="!bInfiniteStorage"))
	int32 CapacityUnits = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Atomiser|Storage")
	bool bUseSharedResourceBank = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Atomiser|Debug")
	bool bShowDebugMessages = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Atomiser|Debug")
	FName DebugName = TEXT("Atomiser");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Machine|Atomiser|Storage")
	TMap<FName, int32> StoredResourcesScaled;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Machine|Atomiser|Storage")
	int32 TotalStoredScaled = 0;

	UPROPERTY(BlueprintAssignable, Category="Machine|Atomiser")
	FOnAtomiserStorageChanged OnStorageChanged;

protected:
	UPROPERTY(Transient)
	TObjectPtr<UFactoryResourceBankSubsystem> BoundSharedResourceBank = nullptr;
};
