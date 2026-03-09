// Copyright Epic Games, Inc. All Rights Reserved.

#include "DroneSwarmComponent.h"
#include "DroneCompanion.h"
#include "UObject/ObjectKey.h"

UDroneSwarmComponent::UDroneSwarmComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDroneSwarmComponent::BeginPlay()
{
	Super::BeginPlay();
	CleanupInvalidEntries();
	RefreshDroneRecords();
}

void UDroneSwarmComponent::SyncFromDrones(const TArray<TObjectPtr<ADroneCompanion>>& Drones)
{
	TSet<FObjectKey> SeenDroneKeys;
	for (ADroneCompanion* Drone : Drones)
	{
		if (!IsValid(Drone))
		{
			continue;
		}

		const FObjectKey DroneKey(Drone);
		SeenDroneKeys.Add(DroneKey);
		RegisterDroneInternal(Drone);
	}

	TArray<FObjectKey> ExistingKeys;
	DroneObjectToId.GetKeys(ExistingKeys);
	for (const FObjectKey& ExistingKey : ExistingKeys)
	{
		if (SeenDroneKeys.Contains(ExistingKey))
		{
			continue;
		}

		if (const int32* DroneId = DroneObjectToId.Find(ExistingKey))
		{
			RemoveDroneByIdInternal(*DroneId);
		}
	}

	CleanupInvalidEntries();
	RefreshDroneRecords();
}

int32 UDroneSwarmComponent::RegisterDrone(ADroneCompanion* Drone)
{
	const int32 DroneId = RegisterDroneInternal(Drone);
	RefreshDroneRecords();
	return DroneId;
}

void UDroneSwarmComponent::UnregisterDrone(ADroneCompanion* Drone)
{
	if (!IsValid(Drone))
	{
		return;
	}

	const FObjectKey DroneKey(Drone);
	if (const int32* DroneId = DroneObjectToId.Find(DroneKey))
	{
		RemoveDroneByIdInternal(*DroneId);
		RefreshDroneRecords();
	}
}

int32 UDroneSwarmComponent::GetDroneId(const ADroneCompanion* Drone) const
{
	if (!IsValid(Drone))
	{
		return INDEX_NONE;
	}

	const int32* DroneId = DroneObjectToId.Find(FObjectKey(Drone));
	return DroneId ? *DroneId : INDEX_NONE;
}

ADroneCompanion* UDroneSwarmComponent::GetDroneById(int32 DroneId) const
{
	if (const TWeakObjectPtr<ADroneCompanion>* DronePtr = DroneIdToDrone.Find(DroneId))
	{
		return DronePtr->Get();
	}

	return nullptr;
}

void UDroneSwarmComponent::SetActiveDrone(ADroneCompanion* Drone)
{
	const int32 NewDroneId = RegisterDroneInternal(Drone);
	if (ActiveDroneId == NewDroneId)
	{
		return;
	}

	ActiveDroneId = NewDroneId;
	RefreshDroneRecords();
}

ADroneCompanion* UDroneSwarmComponent::GetActiveDrone() const
{
	return GetDroneById(ActiveDroneId);
}

void UDroneSwarmComponent::AssignRole(EDroneSwarmRoleSlot Role, ADroneCompanion* Drone)
{
	const int32 DroneId = RegisterDroneInternal(Drone);
	switch (Role)
	{
	case EDroneSwarmRoleSlot::Buddy:
		BuddyDroneId = DroneId;
		break;
	case EDroneSwarmRoleSlot::ThirdPersonCamera:
		ThirdPersonCameraDroneId = DroneId;
		break;
	case EDroneSwarmRoleSlot::MiniMap:
		MiniMapDroneId = DroneId;
		break;
	case EDroneSwarmRoleSlot::Pilot:
		PilotDroneId = DroneId;
		break;
	case EDroneSwarmRoleSlot::Map:
		MapDroneId = DroneId;
		break;
	case EDroneSwarmRoleSlot::HookWorker:
		if (DroneId != INDEX_NONE)
		{
			HookWorkerDroneIds.Add(DroneId);
		}
		break;
	case EDroneSwarmRoleSlot::None:
	default:
		break;
	}

	RefreshDroneRecords();
}

void UDroneSwarmComponent::ClearRole(EDroneSwarmRoleSlot Role)
{
	switch (Role)
	{
	case EDroneSwarmRoleSlot::Buddy:
		BuddyDroneId = INDEX_NONE;
		break;
	case EDroneSwarmRoleSlot::ThirdPersonCamera:
		ThirdPersonCameraDroneId = INDEX_NONE;
		break;
	case EDroneSwarmRoleSlot::MiniMap:
		MiniMapDroneId = INDEX_NONE;
		break;
	case EDroneSwarmRoleSlot::Pilot:
		PilotDroneId = INDEX_NONE;
		break;
	case EDroneSwarmRoleSlot::Map:
		MapDroneId = INDEX_NONE;
		break;
	case EDroneSwarmRoleSlot::HookWorker:
		HookWorkerDroneIds.Reset();
		break;
	case EDroneSwarmRoleSlot::None:
	default:
		break;
	}

	RefreshDroneRecords();
}

void UDroneSwarmComponent::ClearAllRoles()
{
	BuddyDroneId = INDEX_NONE;
	ThirdPersonCameraDroneId = INDEX_NONE;
	MiniMapDroneId = INDEX_NONE;
	PilotDroneId = INDEX_NONE;
	MapDroneId = INDEX_NONE;
	HookWorkerDroneIds.Reset();
	RefreshDroneRecords();
}

ADroneCompanion* UDroneSwarmComponent::GetRoleDrone(EDroneSwarmRoleSlot Role) const
{
	return GetDroneById(GetRoleDroneId(Role));
}

int32 UDroneSwarmComponent::GetRoleDroneId(EDroneSwarmRoleSlot Role) const
{
	switch (Role)
	{
	case EDroneSwarmRoleSlot::Buddy:
		return BuddyDroneId;
	case EDroneSwarmRoleSlot::ThirdPersonCamera:
		return ThirdPersonCameraDroneId;
	case EDroneSwarmRoleSlot::MiniMap:
		return MiniMapDroneId;
	case EDroneSwarmRoleSlot::Pilot:
		return PilotDroneId;
	case EDroneSwarmRoleSlot::Map:
		return MapDroneId;
	case EDroneSwarmRoleSlot::HookWorker:
		return HookWorkerDroneIds.Num() > 0 ? *HookWorkerDroneIds.CreateConstIterator() : INDEX_NONE;
	case EDroneSwarmRoleSlot::None:
	default:
		return INDEX_NONE;
	}
}

EDroneSwarmRoleSlot UDroneSwarmComponent::GetAssignedRoleForDrone(const ADroneCompanion* Drone) const
{
	const int32 DroneId = GetDroneId(Drone);
	if (DroneId == INDEX_NONE)
	{
		return EDroneSwarmRoleSlot::None;
	}

	if (DroneId == ThirdPersonCameraDroneId)
	{
		return EDroneSwarmRoleSlot::ThirdPersonCamera;
	}

	if (DroneId == MiniMapDroneId)
	{
		return EDroneSwarmRoleSlot::MiniMap;
	}

	if (DroneId == PilotDroneId)
	{
		return EDroneSwarmRoleSlot::Pilot;
	}

	if (DroneId == MapDroneId)
	{
		return EDroneSwarmRoleSlot::Map;
	}

	if (DroneId == BuddyDroneId)
	{
		return EDroneSwarmRoleSlot::Buddy;
	}

	if (HookWorkerDroneIds.Contains(DroneId))
	{
		return EDroneSwarmRoleSlot::HookWorker;
	}

	return EDroneSwarmRoleSlot::None;
}

void UDroneSwarmComponent::SetHookWorker(ADroneCompanion* Drone, bool bEnable)
{
	const int32 DroneId = RegisterDroneInternal(Drone);
	if (DroneId == INDEX_NONE)
	{
		return;
	}

	if (bEnable)
	{
		HookWorkerDroneIds.Add(DroneId);
	}
	else
	{
		HookWorkerDroneIds.Remove(DroneId);
	}

	RefreshDroneRecords();
}

void UDroneSwarmComponent::ClearHookWorkers()
{
	HookWorkerDroneIds.Reset();
	RefreshDroneRecords();
}

int32 UDroneSwarmComponent::RegisterDroneInternal(ADroneCompanion* Drone)
{
	if (!IsValid(Drone))
	{
		return INDEX_NONE;
	}

	const FObjectKey DroneKey(Drone);
	if (const int32* ExistingId = DroneObjectToId.Find(DroneKey))
	{
		if (const TWeakObjectPtr<ADroneCompanion>* ExistingDrone = DroneIdToDrone.Find(*ExistingId))
		{
			if (ExistingDrone->Get() == Drone)
			{
				return *ExistingId;
			}
		}
	}

	const int32 NewDroneId = AllocateDroneId();
	DroneObjectToId.Add(DroneKey, NewDroneId);
	DroneIdToDrone.Add(NewDroneId, Drone);
	return NewDroneId;
}

void UDroneSwarmComponent::RemoveDroneByIdInternal(int32 DroneId)
{
	if (DroneId == INDEX_NONE)
	{
		return;
	}

	if (const TWeakObjectPtr<ADroneCompanion>* DronePtr = DroneIdToDrone.Find(DroneId))
	{
		if (ADroneCompanion* Drone = DronePtr->Get())
		{
			DroneObjectToId.Remove(FObjectKey(Drone));
		}
	}

	DroneIdToDrone.Remove(DroneId);
	HookWorkerDroneIds.Remove(DroneId);
	if (ActiveDroneId == DroneId)
	{
		ActiveDroneId = INDEX_NONE;
	}

	if (BuddyDroneId == DroneId)
	{
		BuddyDroneId = INDEX_NONE;
	}
	if (ThirdPersonCameraDroneId == DroneId)
	{
		ThirdPersonCameraDroneId = INDEX_NONE;
	}
	if (MiniMapDroneId == DroneId)
	{
		MiniMapDroneId = INDEX_NONE;
	}
	if (PilotDroneId == DroneId)
	{
		PilotDroneId = INDEX_NONE;
	}
	if (MapDroneId == DroneId)
	{
		MapDroneId = INDEX_NONE;
	}
}

void UDroneSwarmComponent::CleanupInvalidEntries()
{
	TArray<int32> InvalidIds;
	for (const TPair<int32, TWeakObjectPtr<ADroneCompanion>>& Pair : DroneIdToDrone)
	{
		if (!Pair.Value.IsValid())
		{
			InvalidIds.Add(Pair.Key);
		}
	}

	for (const int32 InvalidId : InvalidIds)
	{
		RemoveDroneByIdInternal(InvalidId);
	}
}

void UDroneSwarmComponent::RefreshDroneRecords()
{
	CleanupInvalidEntries();

	DroneRecords.Reset();
	DroneRecords.Reserve(DroneIdToDrone.Num());
	for (const TPair<int32, TWeakObjectPtr<ADroneCompanion>>& Pair : DroneIdToDrone)
	{
		ADroneCompanion* Drone = Pair.Value.Get();
		if (!IsValid(Drone))
		{
			continue;
		}

		FDroneSwarmRecord Record;
		Record.DroneId = Pair.Key;
		Record.Drone = Drone;
		Record.bAvailable = IsDroneAvailable(Drone);
		Record.bManualPowerOff = Drone->IsManualPowerOffRequested();
		Record.BatteryPercent = Drone->GetBatteryPercent();
		DroneRecords.Add(Record);
	}

	DroneRecords.Sort(
		[](const FDroneSwarmRecord& Left, const FDroneSwarmRecord& Right)
		{
			return Left.DroneId < Right.DroneId;
		});
}

bool UDroneSwarmComponent::IsDroneAvailable(const ADroneCompanion* Drone) const
{
	return IsValid(Drone)
		&& !Drone->IsBatteryDepleted()
		&& !Drone->IsManualPowerOffRequested();
}

int32 UDroneSwarmComponent::AllocateDroneId()
{
	const int32 NewDroneId = NextDroneId;
	NextDroneId = FMath::Max(1, NextDroneId + 1);
	return NewDroneId;
}
