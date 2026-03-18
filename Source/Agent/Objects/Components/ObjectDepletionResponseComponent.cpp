// Copyright Epic Games, Inc. All Rights Reserved.

#include "Objects/Components/ObjectDepletionResponseComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "Factory/FactoryPayloadActor.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Material/AgentResourceTypes.h"
#include "Material/MaterialComponent.h"
#include "Material/MaterialDefinitionAsset.h"
#include "Math/RotationMatrix.h"
#include "Objects/Components/ObjectHealthComponent.h"
#include "Objects/Types/ObjectBreakUtilities.h"
#include "Particles/ParticleSystem.h"
#include "UObject/UObjectIterator.h"

bool FObjectDepletionBlueprintDropEntry::IsUsable() const
{
	if (!DropActorClass.Get())
	{
		return false;
	}

	if (!bUseRange)
	{
		return Amount > 0;
	}

	return FMath::Max(0, FMath::Max(MinAmount, MaxAmount)) > 0;
}

int32 FObjectDepletionBlueprintDropEntry::ResolveAmount(bool bAllowRangeRoll) const
{
	if (!IsUsable())
	{
		return 0;
	}

	if (!bUseRange || !bAllowRangeRoll)
	{
		return FMath::Max(0, Amount);
	}

	const int32 SafeMin = FMath::Max(0, MinAmount);
	const int32 SafeMax = FMath::Max(SafeMin, MaxAmount);
	return FMath::RandRange(SafeMin, SafeMax);
}

bool FObjectDepletionRawDropClassOverride::IsUsable() const
{
	return !MaterialId.IsNone() && DropActorClass.Get() != nullptr;
}

UObjectDepletionResponseComponent::UObjectDepletionResponseComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UObjectDepletionResponseComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!bResponseEnabled)
	{
		return;
	}

	CachedHealthComponent = ResolveHealthComponent();
	if (!CachedHealthComponent)
	{
		return;
	}

	CachedHealthComponent->OnDepleted.AddUniqueDynamic(this, &UObjectDepletionResponseComponent::HandleOwnerDepleted);

	if (bTakeOwnershipOfOwnerDepletionLifecycle)
	{
		CachedHealthComponent->bDestroyOwnerWhenDepleted = false;
	}
}

void UObjectDepletionResponseComponent::TriggerDepletionResponses()
{
	if (!bResponseEnabled)
	{
		return;
	}

	if (bHandleOnlyFirstDepletion && bHasHandledDepletion)
	{
		return;
	}

	bHasHandledDepletion = true;
	ExecuteDepletionResponses();
}

void UObjectDepletionResponseComponent::HandleOwnerDepleted()
{
	TriggerDepletionResponses();
}

void UObjectDepletionResponseComponent::ExecuteDepletionResponses()
{
	AActor* OwnerActor = GetOwner();
	if (OwnerActor && !OwnerActor->HasAuthority())
	{
		return;
	}

	FHitResult GroundHit;
	const bool bHasGroundHit = ResolveGroundHit(GroundHit);

	if (bSpawnEmitterOnDepleted)
	{
		SpawnEmitterResponse(bHasGroundHit ? &GroundHit : nullptr);
	}

	if (bSpawnGroundDecalOnDepleted && bHasGroundHit)
	{
		SpawnGroundDecalResponse(GroundHit);
	}

	if (bDropRawMaterialsOnDepleted)
	{
		SpawnRawMaterialDrops();
	}

	if (bSpawnBlueprintDropsOnDepleted)
	{
		SpawnBlueprintDrops();
	}

	if (bTakeOwnershipOfOwnerDepletionLifecycle)
	{
		ApplyOwnerPostDepletionState();
	}
}

void UObjectDepletionResponseComponent::SpawnEmitterResponse(const FHitResult* GroundHit)
{
	if (!DepletionEmitterTemplate)
	{
		return;
	}

	UWorld* World = GetWorld();
	AActor* OwnerActor = GetOwner();
	if (!World || !OwnerActor)
	{
		return;
	}

	FTransform SpawnTransform = OwnerActor->GetActorTransform();
	SpawnTransform.SetLocation(OwnerActor->GetActorLocation() + EmitterWorldOffset);

	if (bAlignEmitterToGroundNormal && GroundHit)
	{
		SpawnTransform = BuildGroundAlignedTransform(*GroundHit, EmitterGroundOffsetCm, 0.0f);
		SpawnTransform.SetLocation(SpawnTransform.GetLocation() + EmitterWorldOffset);
	}

	UGameplayStatics::SpawnEmitterAtLocation(
		World,
		DepletionEmitterTemplate,
		SpawnTransform.GetLocation(),
		SpawnTransform.GetRotation().Rotator(),
		SpawnTransform.GetScale3D(),
		true);
}

void UObjectDepletionResponseComponent::SpawnGroundDecalResponse(const FHitResult& GroundHit)
{
	if (!GroundDecalActorClass.Get())
	{
		return;
	}

	const FTransform SpawnTransform = BuildGroundAlignedTransform(GroundHit, GroundDecalNormalOffsetCm, GroundDecalRandomYawDegrees);
	SpawnActorAtWorldTransform(GroundDecalActorClass, SpawnTransform);
}

void UObjectDepletionResponseComponent::SpawnRawMaterialDrops()
{
	AActor* OwnerActor = GetOwner();
	UMaterialComponent* MaterialComponent = ResolveMaterialComponent();
	UPrimitiveComponent* SourcePrimitive = ResolveOwnerPrimitive();
	if (!OwnerActor || !MaterialComponent || !SourcePrimitive)
	{
		return;
	}

	TMap<FName, int32> ResourceQuantitiesScaled;
	if (!MaterialComponent->BuildResolvedResourceQuantitiesScaled(SourcePrimitive, ResourceQuantitiesScaled))
	{
		return;
	}

	if (bApplyDamagePenaltyToRawDrops)
	{
		UObjectHealthComponent::ApplyDamagedPenaltyToResourceQuantitiesScaled(OwnerActor, ResourceQuantitiesScaled);
	}

	if (ResourceQuantitiesScaled.Num() == 0)
	{
		return;
	}

	const int32 ChunkSizeScaled = AgentResource::WholeUnitsToScaled(FMath::Max(1, RawDropChunkUnits));
	const FVector BaseLocation = OwnerActor->GetActorLocation() + FVector(0.0f, 0.0f, RawDropHeightOffsetCm);

	TArray<FName> ResourceIds;
	ResourceQuantitiesScaled.GetKeys(ResourceIds);
	ResourceIds.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});

	int32 TotalSpawnedRawDropActors = 0;
	for (const FName& ResourceId : ResourceIds)
	{
		int32 RemainingQuantityScaled = FMath::Max(0, ResourceQuantitiesScaled.FindRef(ResourceId));
		while (RemainingQuantityScaled > 0)
		{
			if (TotalSpawnedRawDropActors >= FMath::Max(1, MaxRawDropActorSpawns))
			{
				return;
			}

			const int32 SpawnQuantityScaled = ChunkSizeScaled > 0
				? FMath::Min(RemainingQuantityScaled, ChunkSizeScaled)
				: RemainingQuantityScaled;
			if (SpawnQuantityScaled <= 0)
			{
				break;
			}

			const TSubclassOf<AActor> DropActorClass = ResolveRawDropActorClass(ResourceId);
			const FVector SpawnLocation = BaseLocation + BuildRandomHorizontalOffset(RawDropScatterRadiusCm);
			const FTransform SpawnTransform(FRotator::ZeroRotator, SpawnLocation);

			AActor* SpawnedActor = SpawnActorAtWorldTransform(DropActorClass, SpawnTransform);
			bool bConfiguredResource = false;

			if (AFactoryPayloadActor* PayloadActor = Cast<AFactoryPayloadActor>(SpawnedActor))
			{
				FResourceAmount PayloadResource;
				PayloadResource.ResourceId = ResourceId;
				PayloadResource.SetScaledQuantity(SpawnQuantityScaled);
				PayloadActor->SetPayloadResource(PayloadResource);
				bConfiguredResource = true;
			}

			if (!bConfiguredResource && SpawnedActor)
			{
				if (UMaterialComponent* DropMaterialComponent = SpawnedActor->FindComponentByClass<UMaterialComponent>())
				{
					bConfiguredResource = DropMaterialComponent->ConfigureSingleResourceById(ResourceId, SpawnQuantityScaled);
				}
			}

			if (!bConfiguredResource)
			{
				if (SpawnedActor)
				{
					SpawnedActor->Destroy();
				}

				TSubclassOf<AActor> FallbackClass = RawDropFallbackPayloadActorClass.Get()
					? TSubclassOf<AActor>(RawDropFallbackPayloadActorClass.Get())
					: TSubclassOf<AActor>(AFactoryPayloadActor::StaticClass());
				AFactoryPayloadActor* FallbackActor = Cast<AFactoryPayloadActor>(SpawnActorAtWorldTransform(FallbackClass, SpawnTransform));
				if (FallbackActor)
				{
					FResourceAmount PayloadResource;
					PayloadResource.ResourceId = ResourceId;
					PayloadResource.SetScaledQuantity(SpawnQuantityScaled);
					FallbackActor->SetPayloadResource(PayloadResource);
					SpawnedActor = FallbackActor;
					bConfiguredResource = true;
				}
			}

			if (bConfiguredResource && SpawnedActor)
			{
				ApplyImpulseToActor(SpawnedActor, RawDropImpulseMin, RawDropImpulseMax);
				++TotalSpawnedRawDropActors;
			}

			RemainingQuantityScaled = FMath::Max(0, RemainingQuantityScaled - SpawnQuantityScaled);
		}
	}
}

void UObjectDepletionResponseComponent::SpawnBlueprintDrops()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	TArray<int32> UsableEntryIndices;
	UsableEntryIndices.Reserve(BlueprintDrops.Num());
	for (int32 EntryIndex = 0; EntryIndex < BlueprintDrops.Num(); ++EntryIndex)
	{
		if (BlueprintDrops[EntryIndex].IsUsable())
		{
			UsableEntryIndices.Add(EntryIndex);
		}
	}

	if (UsableEntryIndices.Num() == 0)
	{
		return;
	}

	TArray<int32> SelectedEntryIndices;
	if (!bRandomizeBlueprintDrops)
	{
		SelectedEntryIndices = UsableEntryIndices;
	}
	else
	{
		const int32 SafeMinSelections = FMath::Max(1, ChosenBlueprintDropCountMin);
		const int32 SafeMaxSelections = FMath::Max(SafeMinSelections, ChosenBlueprintDropCountMax);
		const int32 SelectionCount = FMath::Clamp(FMath::RandRange(SafeMinSelections, SafeMaxSelections), 1, UsableEntryIndices.Num());

		TArray<int32> RemainingEntryIndices = UsableEntryIndices;
		SelectedEntryIndices.Reserve(SelectionCount);

		while (SelectedEntryIndices.Num() < SelectionCount && RemainingEntryIndices.Num() > 0)
		{
			double TotalWeight = 0.0;
			for (const int32 EntryIndex : RemainingEntryIndices)
			{
				TotalWeight += FMath::Max(0.0, static_cast<double>(BlueprintDrops[EntryIndex].SelectionWeight));
			}

			int32 SelectedPosition = 0;
			if (TotalWeight <= KINDA_SMALL_NUMBER)
			{
				SelectedPosition = FMath::RandRange(0, RemainingEntryIndices.Num() - 1);
			}
			else
			{
				const double RandomWeight = FMath::FRandRange(0.0, TotalWeight);
				double RunningWeight = 0.0;
				for (int32 Position = 0; Position < RemainingEntryIndices.Num(); ++Position)
				{
					const int32 EntryIndex = RemainingEntryIndices[Position];
					RunningWeight += FMath::Max(0.0, static_cast<double>(BlueprintDrops[EntryIndex].SelectionWeight));
					if (RandomWeight <= RunningWeight)
					{
						SelectedPosition = Position;
						break;
					}
				}
			}

			SelectedEntryIndices.Add(RemainingEntryIndices[SelectedPosition]);
			RemainingEntryIndices.RemoveAtSwap(SelectedPosition);
		}
	}

	const FVector BaseLocation = OwnerActor->GetActorLocation() + FVector(0.0f, 0.0f, BlueprintDropHeightOffsetCm);
	int32 TotalSpawnedActors = 0;
	for (const int32 SelectedEntryIndex : SelectedEntryIndices)
	{
		if (!BlueprintDrops.IsValidIndex(SelectedEntryIndex))
		{
			continue;
		}

		const FObjectDepletionBlueprintDropEntry& DropEntry = BlueprintDrops[SelectedEntryIndex];
		const int32 AmountToSpawn = DropEntry.ResolveAmount(true);
		if (AmountToSpawn <= 0 || !DropEntry.DropActorClass.Get())
		{
			continue;
		}

		for (int32 SpawnIndex = 0; SpawnIndex < AmountToSpawn; ++SpawnIndex)
		{
			if (TotalSpawnedActors >= FMath::Max(1, MaxBlueprintDropActorSpawns))
			{
				return;
			}

			const FVector SpawnLocation = BaseLocation + BuildRandomHorizontalOffset(BlueprintDropScatterRadiusCm);
			const FTransform SpawnTransform(FRotator(0.0f, FMath::FRandRange(-180.0f, 180.0f), 0.0f), SpawnLocation);
			AActor* SpawnedActor = SpawnActorAtWorldTransform(DropEntry.DropActorClass, SpawnTransform);
			ApplyImpulseToActor(SpawnedActor, BlueprintDropImpulseMin, BlueprintDropImpulseMax);
			if (SpawnedActor)
			{
				++TotalSpawnedActors;
			}
		}
	}
}

void UObjectDepletionResponseComponent::ApplyOwnerPostDepletionState()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	if (bDisableOwnerCollisionAfterResponses)
	{
		TArray<UPrimitiveComponent*> PrimitiveComponents;
		OwnerActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (!PrimitiveComponent)
			{
				continue;
			}

			PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			PrimitiveComponent->SetSimulatePhysics(false);
		}
	}

	if (bHideOwnerAfterResponses)
	{
		OwnerActor->SetActorHiddenInGame(true);
	}

	if (bDestroyOwnerAfterResponses)
	{
		OwnerActor->Destroy();
	}
}

UObjectHealthComponent* UObjectDepletionResponseComponent::ResolveHealthComponent() const
{
	return UObjectHealthComponent::FindObjectHealthComponent(GetOwner());
}

UMaterialComponent* UObjectDepletionResponseComponent::ResolveMaterialComponent() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UMaterialComponent>() : nullptr;
}

UPrimitiveComponent* UObjectDepletionResponseComponent::ResolveOwnerPrimitive() const
{
	return AgentObjectBreak::ResolveBestPrimitiveOnActor(GetOwner());
}

bool UObjectDepletionResponseComponent::ResolveGroundHit(FHitResult& OutGroundHit) const
{
	OutGroundHit = FHitResult();

	UWorld* World = GetWorld();
	AActor* OwnerActor = GetOwner();
	if (!World || !OwnerActor)
	{
		return false;
	}

	const FVector OwnerLocation = OwnerActor->GetActorLocation();
	const FVector TraceStart = OwnerLocation + FVector(0.0f, 0.0f, FMath::Max(0.0f, GroundTraceUpDistanceCm));
	const FVector TraceEnd = OwnerLocation - FVector(0.0f, 0.0f, FMath::Max(1.0f, GroundTraceDownDistanceCm));

	FCollisionQueryParams QueryParams(TEXT("ObjectDepletionGroundTrace"), false, OwnerActor);
	return World->LineTraceSingleByChannel(OutGroundHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
}

TSubclassOf<AActor> UObjectDepletionResponseComponent::ResolveRawDropActorClass(FName ResourceId) const
{
	if (ResourceId.IsNone())
	{
		return RawDropFallbackPayloadActorClass.Get()
			? TSubclassOf<AActor>(RawDropFallbackPayloadActorClass.Get())
			: TSubclassOf<AActor>(AFactoryPayloadActor::StaticClass());
	}

	for (const FObjectDepletionRawDropClassOverride& DropOverride : RawDropClassOverrides)
	{
		if (!DropOverride.IsUsable())
		{
			continue;
		}

		if (DropOverride.MaterialId.IsEqual(ResourceId, ENameCase::IgnoreCase))
		{
			return DropOverride.DropActorClass;
		}
	}

	if (const UMaterialDefinitionAsset* Definition = FindMaterialDefinitionById(ResourceId))
	{
		if (const TSubclassOf<AActor> RawDropClass = Definition->ResolveOutputActorClass(EMaterialOutputForm::Raw))
		{
			return RawDropClass;
		}
	}

	if (RawDropFallbackPayloadActorClass.Get())
	{
		return TSubclassOf<AActor>(RawDropFallbackPayloadActorClass.Get());
	}

	return TSubclassOf<AActor>(AFactoryPayloadActor::StaticClass());
}

const UMaterialDefinitionAsset* UObjectDepletionResponseComponent::FindMaterialDefinitionById(FName ResourceId) const
{
	if (ResourceId.IsNone())
	{
		return nullptr;
	}

	for (TObjectIterator<UMaterialDefinitionAsset> It; It; ++It)
	{
		const UMaterialDefinitionAsset* Definition = *It;
		if (Definition && Definition->GetResolvedMaterialId().IsEqual(ResourceId, ENameCase::IgnoreCase))
		{
			return Definition;
		}
	}

	return nullptr;
}

AActor* UObjectDepletionResponseComponent::SpawnActorAtWorldTransform(TSubclassOf<AActor> ActorClass, const FTransform& WorldTransform) const
{
	if (!ActorClass.Get())
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwner();
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	return World->SpawnActor<AActor>(ActorClass.Get(), WorldTransform, SpawnParameters);
}

FTransform UObjectDepletionResponseComponent::BuildGroundAlignedTransform(
	const FHitResult& GroundHit,
	float OffsetAlongNormalCm,
	float RandomYawDegrees) const
{
	const FVector SurfaceNormal = GroundHit.ImpactNormal.IsNearlyZero()
		? FVector::UpVector
		: GroundHit.ImpactNormal.GetSafeNormal();
	const FVector SpawnLocation = GroundHit.ImpactPoint + (SurfaceNormal * OffsetAlongNormalCm);

	FRotator SpawnRotation = FRotationMatrix::MakeFromZ(SurfaceNormal).Rotator();
	if (RandomYawDegrees > KINDA_SMALL_NUMBER)
	{
		SpawnRotation.Yaw += FMath::FRandRange(-RandomYawDegrees, RandomYawDegrees);
	}

	return FTransform(SpawnRotation, SpawnLocation);
}

FVector UObjectDepletionResponseComponent::BuildRandomHorizontalOffset(float RadiusCm) const
{
	const float SafeRadius = FMath::Max(0.0f, RadiusCm);
	if (SafeRadius <= KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	FVector RandomDirection = FMath::VRand();
	RandomDirection.Z = 0.0f;
	RandomDirection = RandomDirection.GetSafeNormal();
	if (RandomDirection.IsNearlyZero())
	{
		RandomDirection = FVector::ForwardVector;
	}

	return RandomDirection * FMath::FRandRange(0.0f, SafeRadius);
}

void UObjectDepletionResponseComponent::ApplyImpulseToActor(AActor* SpawnedActor, float ImpulseMin, float ImpulseMax) const
{
	if (!SpawnedActor)
	{
		return;
	}

	UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(SpawnedActor->GetRootComponent());
	if (!RootPrimitive)
	{
		return;
	}

	const float SafeMinImpulse = FMath::Max(0.0f, ImpulseMin);
	const float SafeMaxImpulse = FMath::Max(SafeMinImpulse, ImpulseMax);
	const float ImpulseStrength = FMath::FRandRange(SafeMinImpulse, SafeMaxImpulse);
	if (ImpulseStrength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	FVector ImpulseDirection = FMath::VRand();
	ImpulseDirection.Z = FMath::Max(0.2f, FMath::Abs(ImpulseDirection.Z));
	ImpulseDirection = ImpulseDirection.GetSafeNormal();
	RootPrimitive->AddImpulse(ImpulseDirection * ImpulseStrength, NAME_None, true);
}
