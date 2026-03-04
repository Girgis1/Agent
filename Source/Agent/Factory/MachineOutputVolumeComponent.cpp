// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/MachineOutputVolumeComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Factory/FactoryPayloadActor.h"
#include "Factory/ResourceTypes.h"

UMachineOutputVolumeComponent::UMachineOutputVolumeComponent()
{
	DebugName = TEXT("MachineOutput");
}

void UMachineOutputVolumeComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEnabled || !bAutoEject)
	{
		return;
	}

	if (TimeUntilNextProcess > 0.0f)
	{
		return;
	}

	if (TryEmitOnePayload())
	{
		TimeUntilNextProcess = FMath::Max(0.0f, ProcessingInterval);
	}
}

void UMachineOutputVolumeComponent::QueueResourceAmount(const FResourceAmount& ResourceAmount)
{
	QueueResourceScaled(ResourceAmount.ResourceId, ResourceAmount.GetScaledQuantity());
}

void UMachineOutputVolumeComponent::QueueWholeUnits(FName ResourceId, int32 QuantityUnits)
{
	QueueResourceScaled(ResourceId, AgentResource::WholeUnitsToScaled(QuantityUnits));
}

int32 UMachineOutputVolumeComponent::QueueResourceScaled(FName ResourceId, int32 QuantityScaled)
{
	if (ResourceId.IsNone() || QuantityScaled <= 0 || IsResourceBlocked(ResourceId))
	{
		return 0;
	}

	int32 QuantityToAdd = QuantityScaled;
	const int32 MaxQuantityScaled = GetMaxQuantityScaled();
	if (MaxQuantityScaled > 0)
	{
		const int32 RemainingCapacity = FMath::Max(0, MaxQuantityScaled - GetCurrentStoredQuantityScaled());
		QuantityToAdd = FMath::Min(QuantityToAdd, RemainingCapacity);
	}

	if (QuantityToAdd <= 0)
	{
		return 0;
	}

	PendingOutputQuantitiesScaled.FindOrAdd(ResourceId) += QuantityToAdd;
	return QuantityToAdd;
}

float UMachineOutputVolumeComponent::GetBufferedUnits(FName ResourceId) const
{
	if (ResourceId.IsNone())
	{
		return AgentResource::ScaledToUnits(GetCurrentStoredQuantityScaled());
	}

	if (const int32* FoundQuantity = PendingOutputQuantitiesScaled.Find(ResourceId))
	{
		return AgentResource::ScaledToUnits(*FoundQuantity);
	}

	return 0.0f;
}

bool UMachineOutputVolumeComponent::TryProcessOverlappingActor(AActor* OverlappingActor)
{
	(void)OverlappingActor;
	return false;
}

ECollisionEnabled::Type UMachineOutputVolumeComponent::GetConfiguredCollisionEnabled() const
{
	return ECollisionEnabled::NoCollision;
}

bool UMachineOutputVolumeComponent::GetConfiguredGenerateOverlapEvents() const
{
	return false;
}

int32 UMachineOutputVolumeComponent::GetCurrentStoredQuantityScaled() const
{
	int32 TotalQuantityScaled = 0;
	for (const TPair<FName, int32>& Pair : PendingOutputQuantitiesScaled)
	{
		TotalQuantityScaled += FMath::Max(0, Pair.Value);
	}

	return TotalQuantityScaled;
}

bool UMachineOutputVolumeComponent::CanSpawnPayload() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MachineOutputClearance), false);
	QueryParams.AddIgnoredActor(GetOwner());

	return !World->OverlapAnyTestByObjectType(
		GetComponentLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(FMath::Max(1.0f, OutputClearanceRadius)),
		QueryParams);
}

bool UMachineOutputVolumeComponent::TryEmitOnePayload()
{
	if (!CanSpawnPayload())
	{
		return false;
	}

	TArray<FName> ResourceIds;
	PendingOutputQuantitiesScaled.GetKeys(ResourceIds);
	ResourceIds.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});

	for (const FName& ResourceId : ResourceIds)
	{
		int32* FoundQuantity = PendingOutputQuantitiesScaled.Find(ResourceId);
		if (!FoundQuantity)
		{
			continue;
		}

		const int32 AvailableQuantityScaled = FMath::Max(0, *FoundQuantity);
		if (AvailableQuantityScaled <= 0)
		{
			PendingOutputQuantitiesScaled.Remove(ResourceId);
			continue;
		}

		const int32 ChunkSizeScaled = FMath::Max(1, AgentResource::WholeUnitsToScaled(OutputChunkSizeUnits));
		const int32 EmitQuantityScaled = FMath::Min(AvailableQuantityScaled, ChunkSizeScaled);
		if (EmitQuantityScaled <= 0)
		{
			continue;
		}

		UWorld* World = GetWorld();
		if (!World)
		{
			return false;
		}

		UClass* PayloadClass = PayloadActorClass.Get();
		if (!PayloadClass)
		{
			PayloadClass = AFactoryPayloadActor::StaticClass();
		}

		FActorSpawnParameters SpawnParameters{};
		SpawnParameters.Owner = GetOwner();
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;

		AFactoryPayloadActor* PayloadActor = World->SpawnActor<AFactoryPayloadActor>(
			PayloadClass,
			GetComponentLocation(),
			GetComponentRotation(),
			SpawnParameters);

		if (!PayloadActor)
		{
			return false;
		}

		FResourceAmount SpawnedAmount;
		SpawnedAmount.ResourceId = ResourceId;
		SpawnedAmount.SetScaledQuantity(EmitQuantityScaled);
		PayloadActor->SetPayloadResource(SpawnedAmount);

		if (UPrimitiveComponent* PayloadPrimitive = Cast<UPrimitiveComponent>(PayloadActor->GetRootComponent()))
		{
			PayloadPrimitive->AddImpulse(GetForwardVector() * FMath::Max(0.0f, OutputImpulse), NAME_None, true);
		}

		*FoundQuantity = FMath::Max(0, AvailableQuantityScaled - EmitQuantityScaled);
		if (*FoundQuantity == 0)
		{
			PendingOutputQuantitiesScaled.Remove(ResourceId);
		}

		if (GEngine)
		{
			const FString DebugLabel = DebugName.ToString();
			const FString ResourceLabel = ResourceId.ToString();
			GEngine->AddOnScreenDebugMessage(
				static_cast<uint64>(GetUniqueID()) + 22000ULL,
				1.0f,
				FColor::Orange,
				FString::Printf(TEXT("%s Output %s: %.2f"), *DebugLabel, *ResourceLabel, SpawnedAmount.GetUnits()));
		}

		return true;
	}

	return false;
}
