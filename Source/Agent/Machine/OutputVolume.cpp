// Copyright Epic Games, Inc. All Rights Reserved.

#include "Machine/OutputVolume.h"
#include "Components/ArrowComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"
#include "Factory/FactoryPayloadActor.h"
#include "Factory/StorageVolumeComponent.h"
#include "GameFramework/Actor.h"
#include "Material/MaterialComponent.h"
#include "Material/MaterialDefinitionAsset.h"
#include "Material/MixedMaterialActor.h"
#include "Material/AgentResourceTypes.h"
#include "UObject/UObjectIterator.h"

namespace
{
const TCHAR* OutputVolumeRecipeClassOutputPrefix = TEXT("__RecipeClassOutput__:");

bool IsRecipeClassOnlyOutputKey(const FName& ResourceId)
{
	if (ResourceId.IsNone())
	{
		return false;
	}

	return ResourceId.ToString().StartsWith(OutputVolumeRecipeClassOutputPrefix);
}

TSubclassOf<AActor> ResolveRecipeClassOutputActor(FName ResourceId)
{
	if (!IsRecipeClassOnlyOutputKey(ResourceId))
	{
		return nullptr;
	}

	FString ResourceKey = ResourceId.ToString();
	if (!ResourceKey.RemoveFromStart(OutputVolumeRecipeClassOutputPrefix) || ResourceKey.IsEmpty())
	{
		return nullptr;
	}

	UClass* LoadedClass = FindObject<UClass>(nullptr, *ResourceKey);
	if (!LoadedClass)
	{
		LoadedClass = LoadObject<UClass>(nullptr, *ResourceKey);
	}

	if (!LoadedClass || !LoadedClass->IsChildOf(AActor::StaticClass()))
	{
		return nullptr;
	}

	return LoadedClass;
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

	if (!bEnabled || !bAutoOutput)
	{
		return;
	}

	if (PendingTeleportOutputs.Num() > 0)
	{
		if (!bTeleportMachineActivated)
		{
			return;
		}

		TimeUntilNextSpawn = FMath::Max(0.0f, TimeUntilNextSpawn - FMath::Max(0.0f, DeltaTime));
		if (TimeUntilNextSpawn > 0.0f)
		{
			return;
		}

		if (TryEmitOneTeleportItem())
		{
			TimeUntilNextSpawn = FMath::Max(0.0f, SpawnInterval);
		}
		return;
	}

	if (PendingOutputScaled.Num() == 0 && PendingMixedMaterialOutputs.Num() == 0)
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

void UOutputVolume::SetOutputPureMaterials(bool bInOutputPureMaterials)
{
	if (bOutputPureMaterials == bInOutputPureMaterials)
	{
		return;
	}

	bOutputPureMaterials = bInOutputPureMaterials;
	RebuildResourceOutputClassLookup();
}

void UOutputVolume::SetAttachedStorageVolume(UStorageVolumeComponent* InStorageVolume)
{
	AttachedStorageVolume = InStorageVolume;
}

bool UOutputVolume::QueueTeleportActor(TSubclassOf<AActor> ActorClass, const TMap<FName, int32>& ResourceQuantitiesScaled, float StoredMassKg)
{
	if (!ActorClass.Get() || bTeleportMachineActivated)
	{
		return false;
	}

	FTeleportOutputRequest& PendingTeleport = PendingTeleportOutputs.Emplace_GetRef();
	PendingTeleport.ActorClass = ActorClass;
	PendingTeleport.StoredMassKg = FMath::Max(0.0f, StoredMassKg);
	PendingTeleport.ResourceQuantitiesScaled.Reset();
	for (const TPair<FName, int32>& Pair : ResourceQuantitiesScaled)
	{
		const FName ResourceId = Pair.Key;
		const int32 QuantityScaled = FMath::Max(0, Pair.Value);
		if (ResourceId.IsNone() || QuantityScaled <= 0)
		{
			continue;
		}

		PendingTeleport.ResourceQuantitiesScaled.FindOrAdd(ResourceId) += QuantityScaled;
	}

	return true;
}

void UOutputVolume::SetTeleportMachineActivated(bool bInActivated)
{
	bTeleportMachineActivated = bInActivated;
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

bool UOutputVolume::QueueMixedMaterialScaled(
	const TMap<FName, int32>& CompositionScaled,
	TSubclassOf<AActor> OutputActorClassOverride,
	float MinUnitsForScale,
	float MaxUnitsForScale,
	float MinVisualScale,
	float MaxVisualScale)
{
	TMap<FName, int32> SanitizedCompositionScaled;
	for (const TPair<FName, int32>& Pair : CompositionScaled)
	{
		const FName ResourceId = Pair.Key;
		const int32 QuantityScaled = FMath::Max(0, Pair.Value);
		if (ResourceId.IsNone() || QuantityScaled <= 0)
		{
			continue;
		}

		SanitizedCompositionScaled.FindOrAdd(ResourceId) += QuantityScaled;
	}

	if (SanitizedCompositionScaled.Num() == 0)
	{
		return false;
	}

	FMixedMaterialOutputRequest& Request = PendingMixedMaterialOutputs.Emplace_GetRef();
	Request.CompositionScaled = MoveTemp(SanitizedCompositionScaled);
	Request.OutputActorClassOverride = OutputActorClassOverride;
	Request.MinUnitsForScale = FMath::Max(0.01f, MinUnitsForScale);
	Request.MaxUnitsForScale = FMath::Max(Request.MinUnitsForScale, MaxUnitsForScale);
	Request.MinVisualScale = FMath::Max(0.05f, MinVisualScale);
	Request.MaxVisualScale = FMath::Max(Request.MinVisualScale, MaxVisualScale);
	return true;
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

UStorageVolumeComponent* UOutputVolume::ResolveAttachedStorageVolume()
{
	if (IsValid(AttachedStorageVolume.Get()))
	{
		return AttachedStorageVolume.Get();
	}

	AttachedStorageVolume = nullptr;
	if (!bAutoResolveAttachedStorage)
	{
		return nullptr;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}

	if (UStorageVolumeComponent* LocalStorageVolume = OwnerActor->FindComponentByClass<UStorageVolumeComponent>())
	{
		AttachedStorageVolume = LocalStorageVolume;
		return AttachedStorageVolume.Get();
	}

	TArray<AActor*> AttachedActors;
	OwnerActor->GetAttachedActors(AttachedActors, true);
	for (AActor* AttachedActor : AttachedActors)
	{
		if (!AttachedActor)
		{
			continue;
		}

		if (UStorageVolumeComponent* AttachedStorage = AttachedActor->FindComponentByClass<UStorageVolumeComponent>())
		{
			AttachedStorageVolume = AttachedStorage;
			return AttachedStorageVolume.Get();
		}
	}

	if (AActor* ParentActor = OwnerActor->GetAttachParentActor())
	{
		if (UStorageVolumeComponent* ParentStorageVolume = ParentActor->FindComponentByClass<UStorageVolumeComponent>())
		{
			AttachedStorageVolume = ParentStorageVolume;
			return AttachedStorageVolume.Get();
		}
	}

	return nullptr;
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
		return ResolveRecipeClassOutputActor(ResourceId);
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
			if (!ResourceDefinition || ResourceDefinition->GetResolvedMaterialId() != ResourceId)
			{
				continue;
			}

			if (const TSubclassOf<AActor> ResourceOutputClass = ResourceDefinition->ResolveOutputActorClass(bOutputPureMaterials ? EMaterialOutputForm::Pure : EMaterialOutputForm::Raw))
			{
				ResourceOutputActorClassById.Add(ResourceId, ResourceOutputClass);
				return ResourceOutputClass;
			}

			break;
		}
	}

	return nullptr;
}

int32 UOutputVolume::TryStoreResourceInAttachedStorage(FName ResourceId, int32 QuantityScaled)
{
	if (!bRouteOutputToAttachedStorage || ResourceId.IsNone() || QuantityScaled <= 0 || IsRecipeClassOnlyOutputKey(ResourceId))
	{
		return 0;
	}

	UStorageVolumeComponent* StorageVolume = ResolveAttachedStorageVolume();
	if (!StorageVolume)
	{
		return 0;
	}

	return StorageVolume->StoreResourceScaledDirect(ResourceId, QuantityScaled, 1);
}

bool UOutputVolume::TryStoreMixedOutputInAttachedStorage(const FMixedMaterialOutputRequest& MixedRequest)
{
	if (!bRouteOutputToAttachedStorage || MixedRequest.CompositionScaled.Num() == 0)
	{
		return false;
	}

	UStorageVolumeComponent* StorageVolume = ResolveAttachedStorageVolume();
	if (!StorageVolume)
	{
		return false;
	}

	return StorageVolume->StoreResourcesScaledDirect(MixedRequest.CompositionScaled, 1);
}

bool UOutputVolume::TryEmitOneTeleportItem()
{
	if (PendingTeleportOutputs.Num() == 0)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	ResolveOutputArrow();

	const FTeleportOutputRequest& TeleportRequest = PendingTeleportOutputs[0];
	if (!TeleportRequest.ActorClass.Get())
	{
		PendingTeleportOutputs.RemoveAt(0);
		return true;
	}

	if (bRouteOutputToAttachedStorage)
	{
		UStorageVolumeComponent* StorageVolume = ResolveAttachedStorageVolume();
		if (StorageVolume && TeleportRequest.ResourceQuantitiesScaled.Num() > 0)
		{
			if (StorageVolume->StoreResourcesScaledDirect(TeleportRequest.ResourceQuantitiesScaled, 1))
			{
				PendingTeleportOutputs.RemoveAt(0);
				return true;
			}

			if (!bAllowWorldSpawnWhenStorageUnavailable)
			{
				return false;
			}
		}
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwner();
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* SpawnedActor = World->SpawnActor<AActor>(
		TeleportRequest.ActorClass.Get(),
		GetSpawnLocation(),
		GetSpawnRotation(),
		SpawnParameters);

	if (!SpawnedActor)
	{
		return false;
	}

	if (TeleportRequest.ResourceQuantitiesScaled.Num() > 0)
	{
		if (UMaterialComponent* MaterialComponent = SpawnedActor->FindComponentByClass<UMaterialComponent>())
		{
			MaterialComponent->ConfigureResourcesById(TeleportRequest.ResourceQuantitiesScaled);
		}
		else if (AFactoryPayloadActor* PayloadActor = Cast<AFactoryPayloadActor>(SpawnedActor))
		{
			if (TeleportRequest.ResourceQuantitiesScaled.Num() == 1)
			{
				const auto ResourceIterator = TeleportRequest.ResourceQuantitiesScaled.CreateConstIterator();
				const FName ResourceId = ResourceIterator->Key;
				const int32 QuantityScaled = ResourceIterator->Value;
				FResourceAmount PayloadResource;
				PayloadResource.ResourceId = ResourceId;
				PayloadResource.SetScaledQuantity(QuantityScaled);
				PayloadActor->SetPayloadResource(PayloadResource);
			}
		}
	}

	if (UPrimitiveComponent* PayloadPrimitive = Cast<UPrimitiveComponent>(SpawnedActor->GetRootComponent()))
	{
		PayloadPrimitive->AddImpulse(GetSpawnRotation().Vector() * FMath::Max(0.0f, SpawnImpulse), NAME_None, true);
	}

	PendingTeleportOutputs.RemoveAt(0);
	return true;
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

		const FName ResourceId = ResourceDefinition->GetResolvedMaterialId();
		if (ResourceId.IsNone())
		{
			continue;
		}

		if (const TSubclassOf<AActor> ResourceOutputClass = ResourceDefinition->ResolveOutputActorClass(bOutputPureMaterials ? EMaterialOutputForm::Pure : EMaterialOutputForm::Raw))
		{
			ResourceOutputActorClassById.Add(ResourceId, ResourceOutputClass);
		}
	}
}

bool UOutputVolume::TrySpawnOnePayload()
{
	UWorld* World = GetWorld();
	if (!World || (PendingOutputScaled.Num() == 0 && PendingMixedMaterialOutputs.Num() == 0))
	{
		return false;
	}

	ResolveOutputArrow();

	if (PendingMixedMaterialOutputs.Num() > 0)
	{
		const FMixedMaterialOutputRequest& MixedRequest = PendingMixedMaterialOutputs[0];
		if (bRouteOutputToAttachedStorage)
		{
			UStorageVolumeComponent* StorageVolume = ResolveAttachedStorageVolume();
			if (StorageVolume && TryStoreMixedOutputInAttachedStorage(MixedRequest))
			{
				PendingMixedMaterialOutputs.RemoveAt(0);
				return true;
			}

			if (StorageVolume && !bAllowWorldSpawnWhenStorageUnavailable)
			{
				return false;
			}
		}

		TSubclassOf<AActor> SpawnClassType = MixedRequest.OutputActorClassOverride.Get()
			? MixedRequest.OutputActorClassOverride
			: TSubclassOf<AActor>(AMixedMaterialActor::StaticClass());
		UClass* SpawnClass = SpawnClassType.Get() ? SpawnClassType.Get() : AMixedMaterialActor::StaticClass();

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

		bool bConfiguredMixedMaterial = false;
		if (AMixedMaterialActor* MixedMaterialActor = Cast<AMixedMaterialActor>(SpawnedActor))
		{
			bConfiguredMixedMaterial = MixedMaterialActor->ConfigureMixedMaterialPayload(
				MixedRequest.CompositionScaled,
				MixedRequest.MinUnitsForScale,
				MixedRequest.MaxUnitsForScale,
				MixedRequest.MinVisualScale,
				MixedRequest.MaxVisualScale);
		}

		if (!bConfiguredMixedMaterial)
		{
			if (UMaterialComponent* MaterialComponent = SpawnedActor->FindComponentByClass<UMaterialComponent>())
			{
				bConfiguredMixedMaterial = MaterialComponent->ConfigureResourcesById(MixedRequest.CompositionScaled);
			}
		}

		if (!bConfiguredMixedMaterial)
		{
			SpawnedActor->Destroy();

			AMixedMaterialActor* FallbackMixedMaterialActor = World->SpawnActor<AMixedMaterialActor>(
				AMixedMaterialActor::StaticClass(),
				GetSpawnLocation(),
				GetSpawnRotation(),
				SpawnParameters);

			if (!FallbackMixedMaterialActor)
			{
				return false;
			}

			bConfiguredMixedMaterial = FallbackMixedMaterialActor->ConfigureMixedMaterialPayload(
				MixedRequest.CompositionScaled,
				MixedRequest.MinUnitsForScale,
				MixedRequest.MaxUnitsForScale,
				MixedRequest.MinVisualScale,
				MixedRequest.MaxVisualScale);
			SpawnedActor = FallbackMixedMaterialActor;
		}

		if (!bConfiguredMixedMaterial)
		{
			SpawnedActor->Destroy();
			return false;
		}

		if (UPrimitiveComponent* PayloadPrimitive = Cast<UPrimitiveComponent>(SpawnedActor->GetRootComponent()))
		{
			PayloadPrimitive->AddImpulse(GetSpawnRotation().Vector() * FMath::Max(0.0f, SpawnImpulse), NAME_None, true);
		}

		PendingMixedMaterialOutputs.RemoveAt(0);
		return true;
	}

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

		const int32 RequestedSpawnQuantityScaled = FMath::Min(PendingScaled, FMath::Max(1, ChunkSizeScaled));
		int32 SpawnQuantityScaled = RequestedSpawnQuantityScaled;
		if (bRouteOutputToAttachedStorage)
		{
			UStorageVolumeComponent* StorageVolume = ResolveAttachedStorageVolume();
			if (StorageVolume && !IsRecipeClassOnlyOutputKey(ResourceId))
			{
				const int32 StoredScaled = TryStoreResourceInAttachedStorage(ResourceId, RequestedSpawnQuantityScaled);
				if (StoredScaled > 0)
				{
					const int32 RemainingAfterStoreScaled = FMath::Max(0, PendingScaled - StoredScaled);
					*FoundQuantityScaled = RemainingAfterStoreScaled;
					if (RemainingAfterStoreScaled == 0)
					{
						PendingOutputScaled.Remove(ResourceId);
						PendingOutputActorClassOverrideById.Remove(ResourceId);
						return true;
					}

					SpawnQuantityScaled = FMath::Max(0, RequestedSpawnQuantityScaled - StoredScaled);
				}

				if (SpawnQuantityScaled <= 0)
				{
					return true;
				}

				if (!bAllowWorldSpawnWhenStorageUnavailable)
				{
					return StoredScaled > 0;
				}
			}
		}

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

		*FoundQuantityScaled = FMath::Max(0, *FoundQuantityScaled - SpawnQuantityScaled);
		if (*FoundQuantityScaled == 0)
		{
			PendingOutputScaled.Remove(ResourceId);
			PendingOutputActorClassOverrideById.Remove(ResourceId);
		}

		return true;
	}

	return false;
}

