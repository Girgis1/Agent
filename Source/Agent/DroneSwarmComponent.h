// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DroneSwarmComponent.generated.h"

class ADroneCompanion;

UENUM(BlueprintType)
enum class EDroneSwarmRoleSlot : uint8
{
	None,
	Buddy,
	ThirdPersonCamera,
	MiniMap,
	Pilot,
	Map,
	HookWorker
};

USTRUCT(BlueprintType)
struct FDroneSwarmRecord
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Swarm")
	int32 DroneId = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Swarm")
	TObjectPtr<ADroneCompanion> Drone = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Swarm")
	bool bAvailable = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Swarm")
	bool bManualPowerOff = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Swarm")
	float BatteryPercent = 0.0f;
};

UCLASS(ClassGroup=(Drone), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class UDroneSwarmComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDroneSwarmComponent();

	void SyncFromDrones(const TArray<TObjectPtr<ADroneCompanion>>& Drones);

	UFUNCTION(BlueprintCallable, Category="Drone|Swarm")
	int32 RegisterDrone(ADroneCompanion* Drone);

	UFUNCTION(BlueprintCallable, Category="Drone|Swarm")
	void UnregisterDrone(ADroneCompanion* Drone);

	UFUNCTION(BlueprintPure, Category="Drone|Swarm")
	int32 GetDroneId(const ADroneCompanion* Drone) const;

	UFUNCTION(BlueprintPure, Category="Drone|Swarm")
	ADroneCompanion* GetDroneById(int32 DroneId) const;

	UFUNCTION(BlueprintCallable, Category="Drone|Swarm")
	void SetActiveDrone(ADroneCompanion* Drone);

	UFUNCTION(BlueprintPure, Category="Drone|Swarm")
	ADroneCompanion* GetActiveDrone() const;

	UFUNCTION(BlueprintPure, Category="Drone|Swarm")
	int32 GetActiveDroneId() const { return ActiveDroneId; }

	UFUNCTION(BlueprintCallable, Category="Drone|Swarm")
	void AssignRole(EDroneSwarmRoleSlot Role, ADroneCompanion* Drone);

	UFUNCTION(BlueprintCallable, Category="Drone|Swarm")
	void ClearRole(EDroneSwarmRoleSlot Role);

	UFUNCTION(BlueprintCallable, Category="Drone|Swarm")
	void ClearAllRoles();

	UFUNCTION(BlueprintPure, Category="Drone|Swarm")
	ADroneCompanion* GetRoleDrone(EDroneSwarmRoleSlot Role) const;

	UFUNCTION(BlueprintPure, Category="Drone|Swarm")
	int32 GetRoleDroneId(EDroneSwarmRoleSlot Role) const;

	UFUNCTION(BlueprintPure, Category="Drone|Swarm")
	EDroneSwarmRoleSlot GetAssignedRoleForDrone(const ADroneCompanion* Drone) const;

	UFUNCTION(BlueprintCallable, Category="Drone|Swarm")
	void SetHookWorker(ADroneCompanion* Drone, bool bEnable);

	UFUNCTION(BlueprintCallable, Category="Drone|Swarm")
	void ClearHookWorkers();

	const TArray<FDroneSwarmRecord>& GetDroneRecords() const { return DroneRecords; }

protected:
	virtual void BeginPlay() override;

private:
	int32 RegisterDroneInternal(ADroneCompanion* Drone);
	void RemoveDroneByIdInternal(int32 DroneId);
	void CleanupInvalidEntries();
	void RefreshDroneRecords();
	bool IsDroneAvailable(const ADroneCompanion* Drone) const;
	int32 AllocateDroneId();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Swarm", meta=(AllowPrivateAccess="true"))
	TArray<FDroneSwarmRecord> DroneRecords;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Swarm", meta=(AllowPrivateAccess="true"))
	int32 ActiveDroneId = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Swarm", meta=(AllowPrivateAccess="true"))
	int32 BuddyDroneId = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Swarm", meta=(AllowPrivateAccess="true"))
	int32 ThirdPersonCameraDroneId = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Swarm", meta=(AllowPrivateAccess="true"))
	int32 MiniMapDroneId = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Swarm", meta=(AllowPrivateAccess="true"))
	int32 PilotDroneId = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Swarm", meta=(AllowPrivateAccess="true"))
	int32 MapDroneId = INDEX_NONE;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Drone|Swarm", meta=(AllowPrivateAccess="true"))
	TSet<int32> HookWorkerDroneIds;

	TMap<int32, TWeakObjectPtr<ADroneCompanion>> DroneIdToDrone;
	TMap<FObjectKey, int32> DroneObjectToId;
	int32 NextDroneId = 1;
};
