// Copyright Epic Games, Inc. All Rights Reserved.

#include "Machine/DroneChargerVolume.h"
#include "Components/PrimitiveComponent.h"
#include "DroneCompanion.h"

UDroneChargerVolume::UDroneChargerVolume()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	InitBoxExtent(FVector(40.0f, 40.0f, 40.0f));

	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionObjectType(ECC_WorldDynamic);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	SetGenerateOverlapEvents(true);
	SetCanEverAffectNavigation(false);
}

void UDroneChargerVolume::BeginPlay()
{
	Super::BeginPlay();

	OnComponentBeginOverlap.AddDynamic(this, &UDroneChargerVolume::HandleBeginOverlap);
	OnComponentEndOverlap.AddDynamic(this, &UDroneChargerVolume::HandleEndOverlap);
}

void UDroneChargerVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (const TWeakObjectPtr<ADroneCompanion>& DronePtr : OverlappingDrones)
	{
		if (ADroneCompanion* DroneCompanion = DronePtr.Get())
		{
			DroneCompanion->NotifyExitedChargerVolume();
		}
	}

	OverlappingDrones.Reset();
	Super::EndPlay(EndPlayReason);
}

void UDroneChargerVolume::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEnabled || ChargeRatePercentPerSecond <= 0.0f || OverlappingDrones.Num() == 0)
	{
		return;
	}

	const float ChargeAmount = FMath::Max(0.0f, ChargeRatePercentPerSecond) * FMath::Max(0.0f, DeltaTime);
	if (ChargeAmount <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	for (auto It = OverlappingDrones.CreateIterator(); It; ++It)
	{
		ADroneCompanion* DroneCompanion = It->Get();
		if (!IsValid(DroneCompanion))
		{
			It.RemoveCurrent();
			continue;
		}

		DroneCompanion->ChargeBatteryPercent(ChargeAmount);
	}
}

void UDroneChargerVolume::HandleBeginOverlap(
	UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	(void)OverlappedComp;
	(void)OtherComp;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;

	if (!bEnabled)
	{
		return;
	}

	TrackDroneOverlap(Cast<ADroneCompanion>(OtherActor), true);
}

void UDroneChargerVolume::HandleEndOverlap(
	UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	(void)OverlappedComp;
	(void)OtherComp;
	(void)OtherBodyIndex;

	TrackDroneOverlap(Cast<ADroneCompanion>(OtherActor), false);
}

void UDroneChargerVolume::TrackDroneOverlap(ADroneCompanion* DroneCompanion, bool bIsEntering)
{
	if (!DroneCompanion)
	{
		return;
	}

	const TWeakObjectPtr<ADroneCompanion> DronePtr(DroneCompanion);
	if (bIsEntering)
	{
		if (!OverlappingDrones.Contains(DronePtr))
		{
			OverlappingDrones.Add(DronePtr);
			DroneCompanion->NotifyEnteredChargerVolume();
		}

		return;
	}

	if (OverlappingDrones.Remove(DronePtr) > 0)
	{
		DroneCompanion->NotifyExitedChargerVolume();
	}
}
