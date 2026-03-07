// Copyright Epic Games, Inc. All Rights Reserved.

#include "Machine/OutputVolume.h"
#include "Components/ArrowComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Factory/FactoryPayloadActor.h"
#include "Material/MaterialComponent.h"
#include "Material/MaterialDefinitionAsset.h"
#include "Material/MaterialTypes.h"
#include "UObject/UObjectIterator.h"

namespace
{
const TCHAR* RecipeClassOutputPrefix = TEXT("__RecipeClassOutput__:");

bool IsRecipeClassOnlyOutputKey(const FName& ResourceId)
{
	if (ResourceId.IsNone())
	{
		return false;
	}

	return ResourceId.ToString().StartsWith(RecipeClassOutputPrefix);
}
}

UOutputVolume::UOutputVolume()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	InitBoxExtent(FVector(24.0f, 24.0f, 24.0f));

	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
}

void UOutputVolume::BeginPlay()
{
	Super::BeginPlay();
	ResolveOutputArrow();
	RebuildResourceOutputClassLookup();
}

void UOutputVolume::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEnabled || !bAutoOutput || PendingOutputScaled.Num() == 0)
	{
		return;
	}

	TimeUntilNextSpawn = FMath::Max(0.0f, TimeUntilNextSpawn - FMath::Max(0.0f, DeltaTime));
	if (TimeUntilNextSpawn > 0.0f)
	{
		return;
	}

	if (TrySpawnOnePayload())
	{
		TimeUntilNextSpawn = FMath::Max(0.0f, SpawnInterval);
	}
}

void UOutputVolume::SetOutputArrow(UArrowComponent* InOutputArrow)
{
	OutputArrow = InOutputArrow;
}

int32 UOutputVolume::QueueResourceScaled(FName ResourceId, int32 QuantityScaled, TSubclassOf<AActor> OutputActorClassOverride)
{
	if (ResourceId.IsNone() || QuantityScaled <= 0)
	{
		return 0;
	}

	const int32 SafeQuantityScaled = FMath::Max(0, QuantityScaled);
	PendingOutputScaled.FindOrAdd(ResourceId) += SafeQuantityScaled;
	if (OutputActorClassOverride.Get())
	{
		PendingOutputActorClassOverrideById.FindOrAdd(ResourceId) = OutputActorClassOverride;
	}

	return SafeQuantityScaled;
}

int32 UOutputVolume::QueueResourceScaled(FName ResourceId, int32 QuantityScaled)
{
	return QueueResourceScaled(ResourceId, QuantityScaled, nullptr);
}

void UOutputVolume::QueueResourceUnits(FName ResourceId, int32 QuantityUnits)
{
	QueueResourceScaled(ResourceId, AgentResource::WholeUnitsToScaled(QuantityUnits));
}

float UOutputVolume::GetPendingUnits(FName ResourceId) const
{
	const int32* FoundQuantityScaled = PendingOutputScaled.Find(ResourceId);
	return FoundQuantityScaled ? AgentResource::ScaledToUnits(*FoundQuantityScaled) : 0.0f;
}

bool UOutputVolume::ResolveOutputArrow()
{
	if (OutputArrow)
	{
		return true;
	}

	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return false;
	}

	TArray<UArrowComponent*> ArrowComponents;
	OwnerActor->GetComponents<UArrowComponent>(ArrowComponents);
	for (UArrowComponent* ArrowComponent : ArrowComponents)
	{
		if (!ArrowComponent)
		{
			continue;
		}

		if (!OutputArrowComponentName.IsNone() && ArrowComponent->GetFName() != OutputArrowComponentName)
		{
			continue;
		}

		OutputArrow = ArrowComponent;
		return true;
	}

	if (ArrowComponents.Num() > 0)
	{
		OutputArrow = ArrowComponents[0];
		return true;
	}

	return false;
}

FVector UOutputVolume::GetSpawnLocation() const
{
	if (OutputArrow)
	{
		return OutputArrow->GetComponentLocation() + OutputArrow->GetComponentTransform().TransformVectorNoScale(LocalSpawnOffset);
	}

	return GetComponentLocation() + GetComponentTransform().TransformVectorNoScale(LocalSpawnOffset);
}

FRotator UOutputVolume::GetSpawnRotation() const
{
	if (OutputArrow)
	{
		return OutputArrow->GetComponentRotation();
	}

	return GetComponentRotation();
}

TSubclassOf<AActor> UOutputVolume::ResolveSpawnClassForResource(FName ResourceId)
{
	if (IsRecipeClassOnlyOutputKey(ResourceId))
	{
		return nullptr;
	}

	if (!ResourceId.IsNone())
	{
		if (const TSubclassOf<AActor>* FoundClass = ResourceOutputActorClassById.Find(ResourceId))
		{
			return *FoundClass;
		}

		for (TObjectIterator<UMaterialDefinitionAsset> It; It; ++It)
		{
			const UMaterialDefinitionAsset* ResourceDefinition = *It;
			if (!ResourceDefinition || ResourceDefinition->GetResolvedResourceId() != ResourceId)
			{
				continue;
			}

			if (const TSubclassOf<AActor> ResourceOutputClass = ResourceDefinition->ResolveOutputActorClass())
			{
				ResourceOutputActorClassById.Add(ResourceId, ResourceOutputClass);
				return ResourceOutputClass;
			}

			break;
		}
	}

	return nullptr;
}

void UOutputVolume::RebuildResourceOutputClassLookup()
{
	ResourceOutputActorClassById.Reset();

	for (TObjectIterator<UMaterialDefinitionAsset> It; It; ++It)
	{
		const UMaterialDefinitionAsset* ResourceDefinition = *It;
		if (!ResourceDefinition)
		{
			continue;
		}

		const FName ResourceId = ResourceDefinition->GetResolvedResourceId();
		if (ResourceId.IsNone())
		{
			continue;
		}

		if (const TSubclassOf<AActor> ResourceOutputClass = ResourceDefinition->ResolveOutputActorClass())
		{
			ResourceOutputActorClassById.Add(ResourceId, ResourceOutputClass);
		}
	}
}

bool UOutputVolume::TrySpawnOnePayload()
{
	UWorld* World = GetWorld();
	if (!World || PendingOutputScaled.Num() == 0)
	{
		return false;
	}

	ResolveOutputArrow();

	TArray<FName> ResourceIds;
	PendingOutputScaled.GetKeys(ResourceIds);
	ResourceIds.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});

	const int32 ChunkSizeScaled = AgentResource::WholeUnitsToScaled(FMath::Max(1, OutputChunkUnits));

	for (const FName& ResourceId : ResourceIds)
	{
		int32* FoundQuantityScaled = PendingOutputScaled.Find(ResourceId);
		if (!FoundQuantityScaled)
		{
			continue;
		}

		const int32 PendingScaled = FMath::Max(0, *FoundQuantityScaled);
		if (PendingScaled <= 0)
		{
			PendingOutputScaled.Remove(ResourceId);
			PendingOutputActorClassOverrideById.Remove(ResourceId);
			continue;
		}

		const int32 SpawnQuantityScaled = FMath::Min(PendingScaled, FMath::Max(1, ChunkSizeScaled));
		TSubclassOf<AActor> SpawnClassType = PendingOutputActorClassOverrideById.FindRef(ResourceId);
		if (!SpawnClassType.Get())
		{
			SpawnClassType = ResolveSpawnClassForResource(ResourceId);
		}

		UClass* SpawnClass = SpawnClassType.Get() ? SpawnClassType.Get() : (PayloadActorClass.Get() ? PayloadActorClass.Get() : AFactoryPayloadActor::StaticClass());

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = GetOwner();
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* SpawnedActor = World->SpawnActor<AActor>(
			SpawnClass,
			GetSpawnLocation(),
			GetSpawnRotation(),
			SpawnParameters);

		if (!SpawnedActor)
		{
			return false;
		}

		bool bConfiguredResource = IsRecipeClassOnlyOutputKey(ResourceId);
		if (!bConfiguredResource)
		{
			if (AFactoryPayloadActor* SpawnedPayloadActor = Cast<AFactoryPayloadActor>(SpawnedActor))
			{
				FResourceAmount PayloadResource;
				PayloadResource.ResourceId = ResourceId;
				PayloadResource.SetScaledQuantity(SpawnQuantityScaled);
				SpawnedPayloadActor->SetPayloadResource(PayloadResource);
				bConfiguredResource = true;
			}
		}

		if (!bConfiguredResource)
		{
			if (UMaterialComponent* ResourceComponent = SpawnedActor->FindComponentByClass<UMaterialComponent>())
			{
				bConfiguredResource = ResourceComponent->ConfigureSingleResourceById(ResourceId, SpawnQuantityScaled);
			}
		}

		if (!bConfiguredResource)
		{
			SpawnedActor->Destroy();

			UClass* FallbackClass = PayloadActorClass.Get() ? PayloadActorClass.Get() : AFactoryPayloadActor::StaticClass();
			AFactoryPayloadActor* FallbackPayloadActor = World->SpawnActor<AFactoryPayloadActor>(
				FallbackClass,
				GetSpawnLocation(),
				GetSpawnRotation(),
				SpawnParameters);

			if (!FallbackPayloadActor)
			{
				return false;
			}

			FResourceAmount PayloadResource;
			PayloadResource.ResourceId = ResourceId;
			PayloadResource.SetScaledQuantity(SpawnQuantityScaled);
			FallbackPayloadActor->SetPayloadResource(PayloadResource);
			SpawnedActor = FallbackPayloadActor;
		}

		if (UPrimitiveComponent* PayloadPrimitive = Cast<UPrimitiveComponent>(SpawnedActor->GetRootComponent()))
		{
			PayloadPrimitive->AddImpulse(GetSpawnRotation().Vector() * FMath::Max(0.0f, SpawnImpulse), NAME_None, true);
		}

		*FoundQuantityScaled = FMath::Max(0, PendingScaled - SpawnQuantityScaled);
		if (*FoundQuantityScaled == 0)
		{
			PendingOutputScaled.Remove(ResourceId);
			PendingOutputActorClassOverrideById.Remove(ResourceId);
		}

		return true;
	}

	return false;
}

