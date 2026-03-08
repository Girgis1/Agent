// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Material/AgentResourceTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "FactoryResourceBankSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnFactoryResourceBankChanged);

UCLASS()
class UFactoryResourceBankSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category="Factory|ResourceBank")
	bool IsInfiniteStorage() const;

	UFUNCTION(BlueprintCallable, Category="Factory|ResourceBank")
	void SetInfiniteStorage(bool bInInfiniteStorage);

	UFUNCTION(BlueprintPure, Category="Factory|ResourceBank")
	int32 GetCapacityUnits() const;

	UFUNCTION(BlueprintCallable, Category="Factory|ResourceBank")
	void SetCapacityUnits(int32 InCapacityUnits);

	UFUNCTION(BlueprintPure, Category="Factory|ResourceBank")
	float GetStoredUnits(FName ResourceId) const;

	UFUNCTION(BlueprintPure, Category="Factory|ResourceBank")
	float GetTotalStoredUnits() const;

	UFUNCTION(BlueprintPure, Category="Factory|ResourceBank")
	int32 GetStoredScaled(FName ResourceId) const;

	UFUNCTION(BlueprintPure, Category="Factory|ResourceBank")
	int32 GetTotalStoredScaled() const;

	UFUNCTION(BlueprintPure, Category="Factory|ResourceBank")
	float GetCapacityUnitsAsFloat() const;

	UFUNCTION(BlueprintPure, Category="Factory|ResourceBank")
	float GetRemainingCapacityUnits() const;

	UFUNCTION(BlueprintPure, Category="Factory|ResourceBank")
	int32 GetRemainingCapacityScaled() const;

	UFUNCTION(BlueprintPure, Category="Factory|ResourceBank")
	bool HasResourceQuantityUnits(FName ResourceId, int32 QuantityUnits) const;

	UFUNCTION(BlueprintPure, Category="Factory|ResourceBank")
	bool HasResourceQuantityScaled(FName ResourceId, int32 QuantityScaled) const;

	UFUNCTION(BlueprintCallable, Category="Factory|ResourceBank")
	bool ConsumeResourceQuantityUnits(FName ResourceId, int32 QuantityUnits);

	UFUNCTION(BlueprintCallable, Category="Factory|ResourceBank")
	bool ConsumeResourceQuantityScaled(FName ResourceId, int32 QuantityScaled);

	UFUNCTION(BlueprintCallable, Category="Factory|ResourceBank")
	bool ConsumeResourcesScaledAtomic(const TMap<FName, int32>& ResourceQuantitiesScaled);

	UFUNCTION(BlueprintCallable, Category="Factory|ResourceBank")
	bool AddResourcesScaledAtomic(const TMap<FName, int32>& ResourceQuantitiesScaled);

	UFUNCTION(BlueprintCallable, Category="Factory|ResourceBank")
	void ClearStorage();

	UFUNCTION(BlueprintPure, Category="Factory|ResourceBank")
	void GetStorageSnapshot(TArray<FResourceStorageEntry>& OutEntries) const;

	UPROPERTY(BlueprintAssignable, Category="Factory|ResourceBank")
	FOnFactoryResourceBankChanged OnStorageChanged;

protected:
	void ApplyWorldDefaultsIfNeeded();
	void BroadcastStorageChanged();
	int32 GetCapacityScaled() const;
	int32 SanitizeCapacityUnits(int32 InCapacityUnits) const;
	bool CanAcceptResourcesAtomic(const TMap<FName, int32>& ResourceQuantitiesScaled) const;

protected:
	UPROPERTY(VisibleAnywhere, Category="Factory|ResourceBank|Storage")
	TMap<FName, int32> StoredResourcesScaled;

	UPROPERTY(VisibleAnywhere, Category="Factory|ResourceBank|Storage")
	int32 TotalStoredScaled = 0;

	UPROPERTY(VisibleAnywhere, Category="Factory|ResourceBank|Storage")
	bool bInfiniteStorage = true;

	UPROPERTY(VisibleAnywhere, Category="Factory|ResourceBank|Storage", meta=(ClampMin="0"))
	int32 CapacityUnits = 0;

	UPROPERTY(Transient)
	bool bWorldDefaultsApplied = false;
};
