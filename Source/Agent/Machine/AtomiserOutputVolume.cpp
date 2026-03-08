// Copyright Epic Games, Inc. All Rights Reserved.

#include "Machine/AtomiserOutputVolume.h"
#include "Components/ArrowComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Factory/FactoryPayloadActor.h"
#include "Factory/FactoryResourceBankSubsystem.h"
#include "Material/AgentResourceTypes.h"
#include "Material/MaterialComponent.h"
#include "Material/MaterialDefinitionAsset.h"
#include "UObject/UObjectIterator.h"

UAtomiserOutputVolume::UAtomiserOutputVolume()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	InitBoxExtent(FVector(24.0f, 24.0f, 24.0f));

	SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetGenerateOverlapEvents(false);
	SetCanEverAffectNavigation(false);
}

void UAtomiserOutputVolume::BeginPlay()
{
	Super::BeginPlay();
	ResolveOutputArrow();
	RebuildResourceOutputClassLookup();
}

void UAtomiserOutputVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void UAtomiserOutputVolume::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEnabled || !bAutoOutput)
	{
		return;
	}

	TimeUntilNextSpawn = FMath::Max(0.0f, TimeUntilNextSpawn - FMath::Max(0.0f, DeltaTime));
	if (TimeUntilNextSpawn > 0.0f)
	{
		return;
	}

	if (TryEmitOnePayload())
	{
		TimeUntilNextSpawn = FMath::Max(0.0f, SpawnInterval);
	}
}

void UAtomiserOutputVolume::SetOutputArrow(UArrowComponent* InOutputArrow)
{
	OutputArrow = InOutputArrow;
}

bool UAtomiserOutputVolume::EmitOnce()
{
	if (!bEnabled)
	{
		return false;
	}

	return TryEmitOnePayload();
}

float UAtomiserOutputVolume::GetAvailableUnits(FName ResourceId) const
{
	const UFactoryResourceBankSubsystem* ResourceBank = ResolveSharedResourceBank();
	if (!ResourceBank)
	{
		return 0.0f;
	}

	if (ResourceId.IsNone())
	{
		return ResourceBank->GetTotalStoredUnits();
	}

	return ResourceBank->GetStoredUnits(ResourceId);
}

UFactoryResourceBankSubsystem* UAtomiserOutputVolume::ResolveSharedResourceBank() const
{
	if (UWorld* World = GetWorld())
	{
		return World->GetSubsystem<UFactoryResourceBankSubsystem>();
	}

	return nullptr;
}

bool UAtomiserOutputVolume::ResolveOutputArrow()
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

bool UAtomiserOutputVolume::CanSpawnPayload() const
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

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(AtomiserOutputClearance), false);
	QueryParams.AddIgnoredActor(GetOwner());

	return !World->OverlapAnyTestByObjectType(
		GetSpawnLocation(),
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(FMath::Max(1.0f, OutputClearanceRadius)),
		QueryParams);
}

bool UAtomiserOutputVolume::TryEmitOnePayload()
{
	UWorld* World = GetWorld();
	UFactoryResourceBankSubsystem* ResourceBank = ResolveSharedResourceBank();
	if (!World || !ResourceBank)
	{
		return false;
	}

	if (!CanSpawnPayload())
	{
		return false;
	}

	ResolveOutputArrow();

	FName ResourceId = NAME_None;
	int32 EmitQuantityScaled = 0;
	if (!TrySelectResourceToEmit(ResourceBank, ResourceId, EmitQuantityScaled))
	{
		return false;
	}

	if (!ResourceBank->ConsumeResourceQuantityScaled(ResourceId, EmitQuantityScaled))
	{
		return false;
	}

	auto RefundQuantity = [ResourceBank, ResourceId, EmitQuantityScaled]()
	{
		TMap<FName, int32> RefundMap;
		RefundMap.Add(ResourceId, EmitQuantityScaled);
		ResourceBank->AddResourcesScaledAtomic(RefundMap);
	};

	TSubclassOf<AActor> SpawnClassType = ResolveSpawnClassForResource(ResourceId);
	UClass* SpawnClass = SpawnClassType.Get() ? SpawnClassType.Get() : (PayloadActorClass.Get() ? PayloadActorClass.Get() : AFactoryPayloadActor::StaticClass());

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwner();
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;

	AActor* SpawnedActor = World->SpawnActor<AActor>(
		SpawnClass,
		GetSpawnLocation(),
		GetSpawnRotation(),
		SpawnParameters);

	if (!SpawnedActor)
	{
		RefundQuantity();
		return false;
	}

	bool bConfiguredResource = false;
	FResourceAmount OutputAmount;
	OutputAmount.ResourceId = ResourceId;
	OutputAmount.SetScaledQuantity(EmitQuantityScaled);

	if (AFactoryPayloadActor* PayloadActor = Cast<AFactoryPayloadActor>(SpawnedActor))
	{
		PayloadActor->SetPayloadResource(OutputAmount);
		bConfiguredResource = true;
	}

	if (!bConfiguredResource)
	{
		if (UMaterialComponent* MaterialComponent = SpawnedActor->FindComponentByClass<UMaterialComponent>())
		{
			bConfiguredResource = MaterialComponent->ConfigureSingleResourceById(ResourceId, EmitQuantityScaled);
		}
	}

	if (!bConfiguredResource && SpawnClassType.Get() && bTreatMaterialOutputClassAsSelfContained)
	{
		bConfiguredResource = true;
	}

	if (!bConfiguredResource)
	{
		SpawnedActor->Destroy();

		UClass* FallbackClass = PayloadActorClass.Get() ? PayloadActorClass.Get() : AFactoryPayloadActor::StaticClass();
		AFactoryPayloadActor* FallbackPayload = World->SpawnActor<AFactoryPayloadActor>(
			FallbackClass,
			GetSpawnLocation(),
			GetSpawnRotation(),
			SpawnParameters);

		if (!FallbackPayload)
		{
			RefundQuantity();
			return false;
		}

		FallbackPayload->SetPayloadResource(OutputAmount);
		SpawnedActor = FallbackPayload;
	}

	if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(SpawnedActor->GetRootComponent()))
	{
		RootPrimitive->AddImpulse(GetSpawnRotation().Vector() * FMath::Max(0.0f, SpawnImpulse), NAME_None, true);
	}

	if (bShowDebugMessages && GEngine)
	{
		const FString Prefix = DebugName.IsNone() ? TEXT("AtomiserOutput") : DebugName.ToString();
		GEngine->AddOnScreenDebugMessage(
			static_cast<uint64>(GetUniqueID()) + 27000ULL,
			1.0f,
			FColor::Orange,
			FString::Printf(TEXT("%s: Output %s %.2f"), *Prefix, *ResourceId.ToString(), OutputAmount.GetUnits()));
	}

	return true;
}

bool UAtomiserOutputVolume::TrySelectResourceToEmit(const UFactoryResourceBankSubsystem* ResourceBank, FName& OutResourceId, int32& OutEmitQuantityScaled) const
{
	OutResourceId = NAME_None;
	OutEmitQuantityScaled = 0;
	if (!ResourceBank)
	{
		return false;
	}

	const int32 ChunkSizeScaled = AgentResource::WholeUnitsToScaled(FMath::Max(1, OutputChunkUnits));
	if (ChunkSizeScaled <= 0)
	{
		return false;
	}

	auto IsResourceAllowed = [this](FName ResourceId)
	{
		if (ResourceId.IsNone())
		{
			return false;
		}

		if (!bUseResourceWhitelist)
		{
			return true;
		}

		return AllowedResourceIds.Contains(ResourceId);
	};

	if (bPreferSingleResource && !PreferredResourceId.IsNone() && IsResourceAllowed(PreferredResourceId))
	{
		const int32 AvailableScaled = ResourceBank->GetStoredScaled(PreferredResourceId);
		if (AvailableScaled > 0)
		{
			OutResourceId = PreferredResourceId;
			OutEmitQuantityScaled = FMath::Min(AvailableScaled, ChunkSizeScaled);
			return OutEmitQuantityScaled > 0;
		}
	}

	TArray<FResourceStorageEntry> SnapshotEntries;
	ResourceBank->GetStorageSnapshot(SnapshotEntries);
	for (const FResourceStorageEntry& Entry : SnapshotEntries)
	{
		if (!IsResourceAllowed(Entry.ResourceId))
		{
			continue;
		}

		const int32 AvailableScaled = FMath::Max(0, Entry.QuantityScaled);
		if (AvailableScaled <= 0)
		{
			continue;
		}

		OutResourceId = Entry.ResourceId;
		OutEmitQuantityScaled = FMath::Min(AvailableScaled, ChunkSizeScaled);
		return OutEmitQuantityScaled > 0;
	}

	return false;
}

TSubclassOf<AActor> UAtomiserOutputVolume::ResolveSpawnClassForResource(FName ResourceId)
{
	if (!bPreferMaterialOutputActorClass || ResourceId.IsNone())
	{
		return nullptr;
	}

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

	return nullptr;
}

void UAtomiserOutputVolume::RebuildResourceOutputClassLookup()
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

FVector UAtomiserOutputVolume::GetSpawnLocation() const
{
	if (OutputArrow)
	{
		return OutputArrow->GetComponentLocation() + OutputArrow->GetComponentTransform().TransformVectorNoScale(LocalSpawnOffset);
	}

	return GetComponentLocation() + GetComponentTransform().TransformVectorNoScale(LocalSpawnOffset);
}

FRotator UAtomiserOutputVolume::GetSpawnRotation() const
{
	if (OutputArrow)
	{
		return OutputArrow->GetComponentRotation();
	}

	return GetComponentRotation();
}
