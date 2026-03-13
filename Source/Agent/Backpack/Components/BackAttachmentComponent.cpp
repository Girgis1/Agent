// Copyright Epic Games, Inc. All Rights Reserved.

#include "Backpack/Components/BackAttachmentComponent.h"

#include "Backpack/Actors/BlackHoleBackpackActor.h"
#include "Backpack/Components/BackpackMagnetComponent.h"
#include "Backpack/Components/PlayerMagnetComponent.h"
#include "AgentCharacter.h"
#include "CollisionShape.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/ShapeComponent.h"
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Material/MaterialComponent.h"
#include "Material/MaterialDefinitionAsset.h"

namespace
{
	UPrimitiveComponent* ResolveBestPrimitive(const AActor* Actor)
	{
		if (!Actor)
		{
			return nullptr;
		}

		if (UPrimitiveComponent* RootPrimitive = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
		{
			if (RootPrimitive->GetCollisionEnabled() != ECollisionEnabled::NoCollision)
			{
				if (RootPrimitive->IsSimulatingPhysics())
				{
					return RootPrimitive;
				}
			}
		}

		TArray<UPrimitiveComponent*> PrimitiveComponents;
		Actor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

		UPrimitiveComponent* FirstValidPrimitive = nullptr;
		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (!PrimitiveComponent || PrimitiveComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
			{
				continue;
			}

			if (!FirstValidPrimitive)
			{
				FirstValidPrimitive = PrimitiveComponent;
			}

			if (PrimitiveComponent->IsSimulatingPhysics())
			{
				return PrimitiveComponent;
			}
		}

		return FirstValidPrimitive;
	}
}

UBackAttachmentComponent::UBackAttachmentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UBackAttachmentComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UBackAttachmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (TPair<FObjectKey, FTrackedMagnetItem>& Pair : TrackedItems)
	{
		UnweldLockedItemFromMagnet(Pair.Value);
		RestoreMagnetKinematicState(Pair.Value, false);
		RestoreMagnetCollisionOverrides(Pair.Value, -1.0f);
	}

	ReleaseAllLockedItems(false, false);
	ClearCapsuleBackItemCollisionIgnore();
	TrackedItems.Reset();
	LockedItemKeys.Reset();

	Super::EndPlay(EndPlayReason);
}

void UBackAttachmentComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateMagnetSimulation(DeltaTime);

	if (bShowAttachDebug)
	{
		DrawAttachDebug();
	}
}

bool UBackAttachmentComponent::IsBackItemDeployed() const
{
	return IsBackItemDeployedState(GetBackItemActorRaw());
}

ABlackHoleBackpackActor* UBackAttachmentComponent::GetBackItemActor() const
{
	return Cast<ABlackHoleBackpackActor>(GetBackItemActorRaw());
}

AActor* UBackAttachmentComponent::FindNearestBackItemInRange(float RangeOverrideCm) const
{
	const ACharacter* OwnerCharacter = ResolveOwnerCharacter();
	if (!OwnerCharacter)
	{
		return nullptr;
	}

	USceneComponent* AttachParent = nullptr;
	FName AttachSocket = NAME_None;
	FVector RangeOrigin = OwnerCharacter->GetActorLocation();
	if (ResolveAttachTarget(AttachParent, AttachSocket) && AttachParent)
	{
		RangeOrigin = AttachSocket.IsNone()
			? AttachParent->GetComponentLocation()
			: AttachParent->GetSocketLocation(AttachSocket);
	}

	return FindWorldBackItemCandidate(OwnerCharacter, RangeOrigin, RangeOverrideCm);
}

ABlackHoleBackpackActor* UBackAttachmentComponent::FindNearestBackpackInRange(float RangeOverrideCm) const
{
	return Cast<ABlackHoleBackpackActor>(FindNearestBackItemInRange(RangeOverrideCm));
}

bool UBackAttachmentComponent::EquipBackItemActor(AActor* InBackItemActor, bool bRequireManualHold)
{
	(void)bRequireManualHold;

	if (!InBackItemActor)
	{
		return false;
	}

	USceneComponent* AttachParent = nullptr;
	FName AttachSocket = NAME_None;
	if (!ResolveAttachTarget(AttachParent, AttachSocket) || !AttachParent)
	{
		return false;
	}

	const FVector RangeOrigin = AttachSocket.IsNone()
		? AttachParent->GetComponentLocation()
		: AttachParent->GetSocketLocation(AttachSocket);
	if (!CanEquipBackItem(InBackItemActor, RangeOrigin, RecallMaxDistance))
	{
		return false;
	}

	UPrimitiveComponent* BackItemPrimitive = ResolveBackItemPrimitive(InBackItemActor);
	if (!BackItemPrimitive)
	{
		return false;
	}

	RegisterTrackedItem(InBackItemActor, BackItemPrimitive, ResolveBackItemMagnet(InBackItemActor));
	BackItemActor = InBackItemActor;
	return true;
}

bool UBackAttachmentComponent::EquipBackpack(ABlackHoleBackpackActor* InBackpack, bool bUseMagnetLerp)
{
	return EquipBackItemActor(InBackpack, !bUseMagnetLerp);
}

bool UBackAttachmentComponent::EquipNearestBackpack(float RangeOverrideCm)
{
	ABlackHoleBackpackActor* CandidateBackpack = FindNearestBackpackInRange(RangeOverrideCm);
	return CandidateBackpack ? EquipBackpack(CandidateBackpack, true) : false;
}

void UBackAttachmentComponent::SetBackItemActor(ABlackHoleBackpackActor* InBackItemActor, bool bAttachImmediately)
{
	if (!InBackItemActor)
	{
		ClearBackItemActor(false);
		return;
	}

	BackItemActor = InBackItemActor;
	if (bAttachImmediately)
	{
		EquipBackpack(InBackItemActor, true);
	}
}

bool UBackAttachmentComponent::DeployBackItem()
{
	const bool bHadLockedItems = LockedItemKeys.Num() > 0;
	if (!bHadLockedItems)
	{
		return false;
	}

	ReleaseAllLockedItems(true, true);
	return true;
}

bool UBackAttachmentComponent::ToggleBackItemDeployment()
{
	if (LockedItemKeys.Num() > 0)
	{
		return DeployBackItem();
	}

	AActor* ExistingBackItem = GetBackItemActorRaw();
	return ExistingBackItem ? EquipBackItemActor(ExistingBackItem, false) : false;
}

bool UBackAttachmentComponent::RecallBackItem()
{
	AActor* ExistingBackItem = GetBackItemActorRaw();
	return ExistingBackItem ? EquipBackItemActor(ExistingBackItem, false) : false;
}

void UBackAttachmentComponent::ClearBackItemActor(bool bDestroyActor)
{
	AActor* ExistingBackItem = GetBackItemActorRaw();
	BackItemActor = nullptr;

	if (bDestroyActor && IsValid(ExistingBackItem))
	{
		ExistingBackItem->Destroy();
	}
}

void UBackAttachmentComponent::SetManualMagnetRequested(bool bInManualMagnetRequested)
{
	if (bManualMagnetRequested == bInManualMagnetRequested)
	{
		return;
	}

	bManualMagnetRequested = bInManualMagnetRequested;
	if (bManualMagnetRequested)
	{
		ManualMagnetHeldDuration = 0.0f;
		NextFrontMagnetImpactKnockdownTime = 0.0f;
		for (TPair<FObjectKey, FTrackedMagnetItem>& Pair : TrackedItems)
		{
			FTrackedMagnetItem& TrackedItem = Pair.Value;
			TrackedItem.bStageOneLiftInitialized = false;
			TrackedItem.bStageOneSnapDelayInitialized = false;
			TrackedItem.StageOneSnapDelaySeconds = 0.0f;
			TrackedItem.MagLockVolumeEnterTime = -1.0f;
		}
		return;
	}

	ManualMagnetChargeAlpha = 0.0f;
	ManualMagnetHeldDuration = 0.0f;
	const float CurrentWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const UPlayerMagnetComponent* PlayerMagnet = ResolvePlayerMagnetComponent();
	const float CaptureRange = PlayerMagnet ? FMath::Max(1.0f, PlayerMagnet->CaptureRange) : 100.0f;
	const float LockFallbackDistance = PlayerMagnet ? FMath::Max(0.0f, PlayerMagnet->LockDistanceFallback) : 0.0f;
	USceneComponent* MagnetAnchorComponent = nullptr;
	FName MagnetAnchorSocket = NAME_None;
	const bool bHasAnchor = ResolveMagLockAnchor(MagnetAnchorComponent, MagnetAnchorSocket) && MagnetAnchorComponent;
	const FTransform MagnetAnchorTransform = bHasAnchor
		? (MagnetAnchorSocket.IsNone()
			? MagnetAnchorComponent->GetComponentTransform()
			: MagnetAnchorComponent->GetSocketTransform(MagnetAnchorSocket, RTS_World))
		: FTransform::Identity;
	const FVector MagnetAnchorLocation = MagnetAnchorTransform.GetLocation();
	if (UShapeComponent* MagLockZone = ResolveMagLockZone())
	{
		TArray<FObjectKey> KeysToLock;
		for (const TPair<FObjectKey, FTrackedMagnetItem>& Pair : TrackedItems)
		{
			const FObjectKey ItemKey = Pair.Key;
			const FTrackedMagnetItem& TrackedItem = Pair.Value;
			if (TrackedItem.bLocked)
			{
				continue;
			}

			const AActor* CandidateActor = TrackedItem.Actor.Get();
			const UPrimitiveComponent* CandidatePrimitive = TrackedItem.Primitive.Get();
			if (!CandidateActor || !CandidatePrimitive)
			{
				continue;
			}

			const FVector AttachPointLocation = ResolveBackItemAttachPointWorldTransform(
				CandidateActor,
				TrackedItem.BackpackMagnet.Get()).GetLocation();
			const FVector PrimitiveLocation = CandidatePrimitive->GetComponentLocation();
			const bool bInsideZoneByAttachPoint = IsInsideMagLockZone(MagLockZone, AttachPointLocation);
			const bool bInsideZoneByPrimitive = IsInsideMagLockZone(MagLockZone, PrimitiveLocation);
			if (bInsideZoneByAttachPoint || bInsideZoneByPrimitive)
			{
				KeysToLock.Add(ItemKey);
			}
		}

		for (const FObjectKey& ItemKey : KeysToLock)
		{
			SetItemLocked(ItemKey, true, CurrentWorldTime, 0.0f);
		}
	}
	else if (bHasAnchor)
	{
		TArray<FObjectKey> KeysToLock;
		KeysToLock.Reserve(TrackedItems.Num());
		for (const TPair<FObjectKey, FTrackedMagnetItem>& Pair : TrackedItems)
		{
			const FObjectKey ItemKey = Pair.Key;
			const FTrackedMagnetItem& TrackedItem = Pair.Value;
			if (TrackedItem.bLocked)
			{
				continue;
			}

			const AActor* CandidateActor = TrackedItem.Actor.Get();
			const UPrimitiveComponent* CandidatePrimitive = TrackedItem.Primitive.Get();
			if (!CandidateActor || !CandidatePrimitive)
			{
				continue;
			}

			const bool bIsBackpackItem = TrackedItem.BackpackMagnet.IsValid();
			const float BackpackAutoLockDistance = (PlayerMagnet && bIsBackpackItem)
				? FMath::Max(0.0f, PlayerMagnet->BackpackItemAutoLockDistance)
				: 0.0f;
			const float FinalizeLockDistance = FMath::Max3(CaptureRange, LockFallbackDistance, BackpackAutoLockDistance);
			if (FinalizeLockDistance <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			const FVector AttachPointLocation = ResolveBackItemAttachPointWorldTransform(
				CandidateActor,
				TrackedItem.BackpackMagnet.Get()).GetLocation();
			const FVector PrimitiveLocation = CandidatePrimitive->GetComponentLocation();
			const bool bNearByAttachPoint = FVector::DistSquared(AttachPointLocation, MagnetAnchorLocation)
				<= FMath::Square(FinalizeLockDistance);
			const bool bNearByPrimitive = FVector::DistSquared(PrimitiveLocation, MagnetAnchorLocation)
				<= FMath::Square(FinalizeLockDistance);
			if (bNearByAttachPoint || bNearByPrimitive)
			{
				KeysToLock.Add(ItemKey);
			}
		}

		for (const FObjectKey& ItemKey : KeysToLock)
		{
			SetItemLocked(ItemKey, true, CurrentWorldTime, 0.0f);
		}
	}

	for (TPair<FObjectKey, FTrackedMagnetItem>& Pair : TrackedItems)
	{
		FTrackedMagnetItem& TrackedItem = Pair.Value;
		UPrimitiveComponent* BackItemPrimitive = TrackedItem.Primitive.Get();
		if (TrackedItem.bLocked)
		{
			continue;
		}

		if (BackItemPrimitive)
		{
			SetCapsuleBackItemCollisionIgnored(BackItemPrimitive, false);
		}

		UnweldLockedItemFromMagnet(TrackedItem);
		RestoreMagnetKinematicState(TrackedItem);
		RestoreMagnetCollisionOverrides(TrackedItem, CurrentWorldTime);
		TrackedItem.bStageOneLiftInitialized = false;
		TrackedItem.bStageOneSnapDelayInitialized = false;
		TrackedItem.StageOneSnapDelaySeconds = 0.0f;
		TrackedItem.MagLockVolumeEnterTime = -1.0f;
	}
}

void UBackAttachmentComponent::SetManualMagnetChargeAlpha(float InChargeAlpha)
{
	ManualMagnetChargeAlpha = bManualMagnetRequested
		? FMath::Clamp(InChargeAlpha, 0.0f, 1.0f)
		: 0.0f;
}

void UBackAttachmentComponent::SetOwnerRagdolling(bool bInOwnerRagdolling)
{
	bOwnerRagdolling = bInOwnerRagdolling;
}

ACharacter* UBackAttachmentComponent::ResolveOwnerCharacter() const
{
	return Cast<ACharacter>(GetOwner());
}

UPlayerMagnetComponent* UBackAttachmentComponent::ResolvePlayerMagnetComponent() const
{
	const AActor* OwnerActor = GetOwner();
	return OwnerActor ? OwnerActor->FindComponentByClass<UPlayerMagnetComponent>() : nullptr;
}

bool UBackAttachmentComponent::ResolveAttachTarget(USceneComponent*& OutAttachParent, FName& OutAttachSocketName) const
{
	OutAttachParent = nullptr;
	OutAttachSocketName = NAME_None;

	const UPlayerMagnetComponent* PlayerMagnetComponent = ResolvePlayerMagnetComponent();
	if (!PlayerMagnetComponent)
	{
		return false;
	}

	return PlayerMagnetComponent->ResolveAttachTarget(OutAttachParent, OutAttachSocketName);
}

bool UBackAttachmentComponent::ResolveMagLockAnchor(USceneComponent*& OutAnchorComponent, FName& OutAnchorSocketName) const
{
	OutAnchorComponent = nullptr;
	OutAnchorSocketName = NAME_None;

	const UPlayerMagnetComponent* PlayerMagnetComponent = ResolvePlayerMagnetComponent();
	if (!PlayerMagnetComponent)
	{
		return false;
	}

	return PlayerMagnetComponent->ResolveMagLockAnchor(OutAnchorComponent, OutAnchorSocketName);
}

UShapeComponent* UBackAttachmentComponent::ResolveMagLockZone() const
{
	const UPlayerMagnetComponent* PlayerMagnetComponent = ResolvePlayerMagnetComponent();
	return PlayerMagnetComponent ? PlayerMagnetComponent->ResolveMagLockZoneComponent() : nullptr;
}

AActor* UBackAttachmentComponent::FindWorldBackItemCandidate(
	const ACharacter* OwnerCharacter,
	const FVector& RangeOrigin,
	float MaxRangeCm) const
{
	if (!OwnerCharacter)
	{
		return nullptr;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const float EffectiveRangeCm = MaxRangeCm >= 0.0f ? MaxRangeCm : RecallMaxDistance;
	if (EffectiveRangeCm <= KINDA_SMALL_NUMBER)
	{
		return nullptr;
	}

	TArray<FOverlapResult> OverlapResults;
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BackpackFindNearest), false, OwnerCharacter);
	QueryParams.AddIgnoredActor(OwnerCharacter);

	World->OverlapMultiByObjectType(
		OverlapResults,
		RangeOrigin,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(EffectiveRangeCm),
		QueryParams);

	AActor* BestCandidate = nullptr;
	float BestDistanceSq = MAX_flt;

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		UPrimitiveComponent* CandidatePrimitive = OverlapResult.GetComponent();
		AActor* CandidateActor = CandidatePrimitive ? CandidatePrimitive->GetOwner() : nullptr;
		if (!CandidateActor || CandidateActor == OwnerCharacter)
		{
			continue;
		}

		if (!CanEquipBackItem(CandidateActor, RangeOrigin, EffectiveRangeCm))
		{
			continue;
		}

		const float CandidateDistanceSq = FVector::DistSquared(CandidateActor->GetActorLocation(), RangeOrigin);
		if (!BestCandidate || CandidateDistanceSq < BestDistanceSq)
		{
			BestCandidate = CandidateActor;
			BestDistanceSq = CandidateDistanceSq;
		}
	}

	return BestCandidate;
}

bool UBackAttachmentComponent::CanEquipBackItem(
	const AActor* CandidateItem,
	const FVector& RangeOrigin,
	float MaxRangeCm) const
{
	const ACharacter* OwnerCharacter = ResolveOwnerCharacter();
	const UPlayerMagnetComponent* PlayerMagnet = ResolvePlayerMagnetComponent();
	if (!OwnerCharacter || !PlayerMagnet || !CandidateItem || CandidateItem == OwnerCharacter)
	{
		return false;
	}

	if (bRestrictToBackItemClass && BackItemClass && !CandidateItem->IsA(BackItemClass))
	{
		return false;
	}

	const UPrimitiveComponent* CandidatePrimitive = ResolveBackItemPrimitive(CandidateItem);
	const UBackpackMagnetComponent* CandidateMagnet = ResolveBackItemMagnet(CandidateItem);
	if (!CandidatePrimitive || !CandidatePrimitive->IsSimulatingPhysics())
	{
		return false;
	}

	if (!IsActorMagneticEligible(CandidateItem, CandidatePrimitive, CandidateMagnet, PlayerMagnet))
	{
		return false;
	}

	const float EffectiveRangeCm = MaxRangeCm >= 0.0f ? MaxRangeCm : RecallMaxDistance;
	if (EffectiveRangeCm > KINDA_SMALL_NUMBER)
	{
		const float CandidateDistance = FVector::Dist(CandidatePrimitive->GetComponentLocation(), RangeOrigin);
		if (CandidateDistance > EffectiveRangeCm)
		{
			return false;
		}
	}

	if (!bRequireLineOfSightForRecall)
	{
		return true;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FHitResult HitResult;
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BackpackRecallTrace), false, OwnerCharacter);
	QueryParams.AddIgnoredActor(OwnerCharacter);
	QueryParams.AddIgnoredActor(CandidateItem);
	const bool bBlocked = World->LineTraceSingleByChannel(
		HitResult,
		OwnerCharacter->GetPawnViewLocation(),
		CandidatePrimitive->GetComponentLocation(),
		ECC_Visibility,
		QueryParams);
	return !bBlocked;
}

UPrimitiveComponent* UBackAttachmentComponent::ResolveBackItemPrimitive(AActor* BackItem) const
{
	return ResolveBackItemPrimitive(static_cast<const AActor*>(BackItem));
}

UPrimitiveComponent* UBackAttachmentComponent::ResolveBackItemPrimitive(const AActor* BackItem) const
{
	return ResolveBestPrimitive(BackItem);
}

UBackpackMagnetComponent* UBackAttachmentComponent::ResolveBackItemMagnet(AActor* BackItem) const
{
	return BackItem ? BackItem->FindComponentByClass<UBackpackMagnetComponent>() : nullptr;
}

const UBackpackMagnetComponent* UBackAttachmentComponent::ResolveBackItemMagnet(const AActor* BackItem) const
{
	return BackItem ? BackItem->FindComponentByClass<UBackpackMagnetComponent>() : nullptr;
}

bool UBackAttachmentComponent::IsBackItemDeployedState(const AActor* BackItem) const
{
	if (!BackItem)
	{
		return false;
	}

	return !IsActorTrackedAndLocked(BackItem);
}

bool UBackAttachmentComponent::IsActorTrackedAndLocked(const AActor* CandidateActor) const
{
	if (!CandidateActor)
	{
		return false;
	}

	for (const TPair<FObjectKey, FTrackedMagnetItem>& Pair : TrackedItems)
	{
		const FTrackedMagnetItem& Item = Pair.Value;
		if (Item.bLocked && Item.Actor.Get() == CandidateActor)
		{
			return true;
		}
	}

	return false;
}

bool UBackAttachmentComponent::HasAnyConfiguredTag(
	const AActor* Actor,
	const UActorComponent* Component,
	const TArray<FName>& RequiredTags) const
{
	for (const FName& RequiredTag : RequiredTags)
	{
		if (RequiredTag.IsNone())
		{
			continue;
		}

		if (Actor && Actor->ActorHasTag(RequiredTag))
		{
			return true;
		}

		const UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component);
		if (PrimitiveComponent && PrimitiveComponent->ComponentHasTag(RequiredTag))
		{
			return true;
		}
	}

	return false;
}

bool UBackAttachmentComponent::IsActorMagneticEligible(
	const AActor* CandidateItem,
	const UPrimitiveComponent* CandidatePrimitive,
	const UBackpackMagnetComponent* CandidateMagnet,
	const UPlayerMagnetComponent* PlayerMagnet) const
{
	if (!CandidateItem || !CandidatePrimitive || !PlayerMagnet)
	{
		return false;
	}

	if (CandidateMagnet
		&& PlayerMagnet->bAllowBackpackMagnetItems
		&& CandidateMagnet->bEligibleForPlayerMagnet)
	{
		return true;
	}

	if (PlayerMagnet->bAllowTaggedMagneticItems)
	{
		if (HasAnyConfiguredTag(CandidateItem, nullptr, PlayerMagnet->MagneticActorTags))
		{
			return true;
		}

		if (HasAnyConfiguredTag(nullptr, CandidatePrimitive, PlayerMagnet->MagneticComponentTags))
		{
			return true;
		}
	}

	if (PlayerMagnet->bAllowMaterialMagneticItems)
	{
		if (const UMaterialComponent* MaterialComponent = CandidateItem->FindComponentByClass<UMaterialComponent>())
		{
			for (const FMaterialEntry& MaterialEntry : MaterialComponent->Materials)
			{
				const UMaterialDefinitionAsset* MaterialAsset = MaterialEntry.Material;
				if (!MaterialAsset)
				{
					continue;
				}

				if (PlayerMagnet->IsMaterialIdMagnetic(MaterialAsset->GetResolvedMaterialId()))
				{
					return true;
				}
			}
		}
	}

	return false;
}

float UBackAttachmentComponent::ResolveItemMagnetStrengthMultiplier(const UBackpackMagnetComponent* BackItemMagnet) const
{
	return BackItemMagnet ? FMath::Max(0.0f, BackItemMagnet->MagnetStrengthMultiplier) : 1.0f;
}

float UBackAttachmentComponent::ResolveMagnetActivationAlpha() const
{
	if (!bManualMagnetRequested || bOwnerRagdolling)
	{
		return 0.0f;
	}

	return FMath::Clamp(ManualMagnetChargeAlpha, 0.0f, 1.0f);
}

bool UBackAttachmentComponent::DoesBackItemHaveMagLockAnchor(const AActor* BackItem) const
{
	if (!BackItem)
	{
		return false;
	}

	const UPlayerMagnetComponent* PlayerMagnet = ResolvePlayerMagnetComponent();
	const FName AnchorTag = (PlayerMagnet && !PlayerMagnet->ItemMagLockAnchorTag.IsNone())
		? PlayerMagnet->ItemMagLockAnchorTag
		: FName(TEXT("MagLockAnchor"));
	if (AnchorTag.IsNone())
	{
		return false;
	}

	TArray<USceneComponent*> SceneComponents;
	BackItem->GetComponents<USceneComponent>(SceneComponents);
	for (const USceneComponent* SceneComponent : SceneComponents)
	{
		if (SceneComponent && SceneComponent->ComponentHasTag(AnchorTag))
		{
			return true;
		}
	}

	return false;
}

FTransform UBackAttachmentComponent::ResolveBackItemAttachPointWorldTransform(
	const AActor* BackItem,
	const UBackpackMagnetComponent* BackItemMagnet) const
{
	if (!BackItem)
	{
		return FTransform::Identity;
	}

	if (BackItemMagnet)
	{
		return BackItemMagnet->GetAttachPointWorldTransform();
	}

	if (const UPrimitiveComponent* Primitive = ResolveBackItemPrimitive(BackItem))
	{
		return Primitive->GetComponentTransform();
	}

	return BackItem->GetActorTransform();
}

FTransform UBackAttachmentComponent::BuildTargetBackItemWorldTransform(
	const AActor* BackItem,
	const UBackpackMagnetComponent* BackItemMagnet,
	const FTransform& TargetAttachPointWorldTransform) const
{
	if (!BackItem)
	{
		return FTransform::Identity;
	}

	const FTransform CurrentActorWorldTransform = BackItem->GetActorTransform();
	const FTransform CurrentAttachPointWorldTransform = ResolveBackItemAttachPointWorldTransform(BackItem, BackItemMagnet);
	const FTransform AttachPointToActorTransform = CurrentAttachPointWorldTransform.GetRelativeTransform(CurrentActorWorldTransform);
	return AttachPointToActorTransform.Inverse() * TargetAttachPointWorldTransform;
}

FVector UBackAttachmentComponent::BuildDropImpulse() const
{
	const ACharacter* OwnerCharacter = ResolveOwnerCharacter();
	if (!OwnerCharacter)
	{
		return FVector::ZeroVector;
	}

	return OwnerCharacter->GetActorForwardVector() * DropForwardImpulse
		+ OwnerCharacter->GetActorRightVector() * DropRightImpulse
		+ FVector::UpVector * DropUpImpulse;
}

void UBackAttachmentComponent::UpdateMagnetSimulation(float DeltaTime)
{
	UWorld* World = GetWorld();
	UPlayerMagnetComponent* PlayerMagnet = ResolvePlayerMagnetComponent();
	if (!World || !PlayerMagnet)
	{
		if (LastMagnetAlpha > KINDA_SMALL_NUMBER)
		{
			ReleaseAllLockedItems(false, false);
			ClearCapsuleBackItemCollisionIgnore();

			for (TPair<FObjectKey, FTrackedMagnetItem>& Pair : TrackedItems)
			{
				UnweldLockedItemFromMagnet(Pair.Value);
				RestoreMagnetKinematicState(Pair.Value);
				RestoreMagnetCollisionOverrides(Pair.Value, -1.0f);
			}
		}

		ManualMagnetHeldDuration = 0.0f;
		LastMagnetAlpha = 0.0f;
		return;
	}

	if (bManualMagnetRequested && !bOwnerRagdolling)
	{
		ManualMagnetHeldDuration += FMath::Max(0.0f, DeltaTime);
	}
	else
	{
		ManualMagnetHeldDuration = 0.0f;
	}

	const bool bMagnetActive = bManualMagnetRequested && !bOwnerRagdolling;
	const float MagnetAlpha = bMagnetActive
		? FMath::Max(0.05f, ResolveMagnetActivationAlpha())
		: 0.0f;
	const float CurrentWorldTime = World->GetTimeSeconds();

	if (!bMagnetActive)
	{
		TSet<UPrimitiveComponent*> LockedProcessedPrimitives;
		USceneComponent* MagnetAnchorComponent = nullptr;
		FName MagnetAnchorSocket = NAME_None;
		const bool bCanMaintainLockedItems = LockedItemKeys.Num() > 0
			&& ResolveMagLockAnchor(MagnetAnchorComponent, MagnetAnchorSocket)
			&& MagnetAnchorComponent;

		if (bCanMaintainLockedItems)
		{
			const FTransform MagnetAnchorTransform = MagnetAnchorSocket.IsNone()
				? MagnetAnchorComponent->GetComponentTransform()
				: MagnetAnchorComponent->GetSocketTransform(MagnetAnchorSocket, RTS_World);

			for (const FObjectKey& LockedItemKey : LockedItemKeys)
			{
				FTrackedMagnetItem* TrackedItem = TrackedItems.Find(LockedItemKey);
				if (!TrackedItem)
				{
					continue;
				}

				AActor* CurrentBackItemActor = TrackedItem->Actor.Get();
				UPrimitiveComponent* BackItemPrimitive = TrackedItem->Primitive.Get();
				if (!CurrentBackItemActor || !BackItemPrimitive)
				{
					continue;
				}

				const UBackpackMagnetComponent* BackItemMagnet = TrackedItem->BackpackMagnet.Get();
				const bool bIsBackpackItem = BackItemMagnet != nullptr;
				const FVector BackItemAttachPointLocation = ResolveBackItemAttachPointWorldTransform(
					CurrentBackItemActor,
					BackItemMagnet).GetLocation();
				const float DistanceToAnchor = FVector::Distance(
					BackItemAttachPointLocation,
					MagnetAnchorTransform.GetLocation());
				const float ItemStrengthMultiplier = ResolveItemMagnetStrengthMultiplier(BackItemMagnet);

				EnsureMagnetKinematicState(*TrackedItem);
				ApplyMagnetCollisionOverrides(*TrackedItem, PlayerMagnet, CurrentWorldTime);

				const bool bShouldWeldLockedItem = PlayerMagnet->bWeldLockedItemsToMagnet
					&& MagnetAnchorComponent;
				if (bShouldWeldLockedItem)
				{
					WeldLockedItemToMagnet(*TrackedItem, MagnetAnchorComponent, MagnetAnchorSocket);
				}
				else
				{
					UnweldLockedItemFromMagnet(*TrackedItem);
					const FTransform TargetBackItemTransform = BuildTargetBackItemWorldTransform(
						CurrentBackItemActor,
						BackItemMagnet,
						MagnetAnchorTransform);
					MoveMagnetItemKinematic(
						DeltaTime,
						*TrackedItem,
						TargetBackItemTransform,
						DistanceToAnchor,
						1.0f,
						ItemStrengthMultiplier,
						PlayerMagnet,
						bIsBackpackItem || TrackedItem->bHasMagLockAnchor);
				}

				SetCapsuleBackItemCollisionIgnored(BackItemPrimitive, true);
				LockedProcessedPrimitives.Add(BackItemPrimitive);
				BackItemActor = CurrentBackItemActor;
			}
		}

		for (TPair<FObjectKey, FTrackedMagnetItem>& Pair : TrackedItems)
		{
			FTrackedMagnetItem& TrackedItem = Pair.Value;
			if (TrackedItem.bLocked)
			{
				continue;
			}

			if (UPrimitiveComponent* BackItemPrimitive = TrackedItem.Primitive.Get())
			{
				SetCapsuleBackItemCollisionIgnored(BackItemPrimitive, false);
			}

			UnweldLockedItemFromMagnet(TrackedItem);
			RestoreMagnetKinematicState(TrackedItem);
			RestoreMagnetCollisionOverrides(TrackedItem, CurrentWorldTime);
		}

		TArray<TWeakObjectPtr<UPrimitiveComponent>> IgnoredPrimitivesToClear;
		for (const TWeakObjectPtr<UPrimitiveComponent>& PrimitivePtr : CapsuleIgnoredBackItemPrimitives)
		{
			UPrimitiveComponent* IgnoredPrimitive = PrimitivePtr.Get();
			if (!IgnoredPrimitive || LockedProcessedPrimitives.Contains(IgnoredPrimitive))
			{
				continue;
			}

			IgnoredPrimitivesToClear.Add(PrimitivePtr);
		}

		for (const TWeakObjectPtr<UPrimitiveComponent>& PrimitivePtr : IgnoredPrimitivesToClear)
		{
			if (UPrimitiveComponent* IgnoredPrimitive = PrimitivePtr.Get())
			{
				SetCapsuleBackItemCollisionIgnored(IgnoredPrimitive, false);
			}
		}

		PruneTrackedItems(CurrentWorldTime, false);
		CandidateScanTimer = 0.0f;
		LastMagnetAlpha = 0.0f;
		return;
	}

	USceneComponent* MagnetAnchorComponent = nullptr;
	FName MagnetAnchorSocket = NAME_None;
	if (!ResolveMagLockAnchor(MagnetAnchorComponent, MagnetAnchorSocket) || !MagnetAnchorComponent)
	{
		LastMagnetAlpha = MagnetAlpha;
		return;
	}

	const FTransform MagnetAnchorTransform = MagnetAnchorSocket.IsNone()
		? MagnetAnchorComponent->GetComponentTransform()
		: MagnetAnchorComponent->GetSocketTransform(MagnetAnchorSocket, RTS_World);

	FVector MagnetAnchorVelocity = FVector::ZeroVector;
	if (UPrimitiveComponent* AnchorPrimitive = Cast<UPrimitiveComponent>(MagnetAnchorComponent))
	{
		if (AnchorPrimitive->IsSimulatingPhysics())
		{
			MagnetAnchorVelocity = AnchorPrimitive->GetPhysicsLinearVelocity(NAME_None);
		}
	}
	else if (const AActor* AnchorActor = MagnetAnchorComponent->GetOwner())
	{
		MagnetAnchorVelocity = AnchorActor->GetVelocity();
	}

	CandidateScanTimer -= FMath::Max(0.0f, DeltaTime);
	if (CandidateScanTimer <= KINDA_SMALL_NUMBER)
	{
		ScanMagnetCandidates(
			MagnetAnchorTransform.GetLocation(),
			FMath::Max(1.0f, PlayerMagnet->AttractRange));
		CandidateScanTimer = FMath::Max(0.01f, PlayerMagnet->CandidateScanInterval);
	}

	ApplyMagnetForces(
		DeltaTime,
		MagnetAlpha,
		MagnetAnchorComponent,
		MagnetAnchorSocket,
		MagnetAnchorTransform,
		MagnetAnchorVelocity,
		ResolveMagLockZone());

	PruneTrackedItems(CurrentWorldTime, true);
	LastMagnetAlpha = MagnetAlpha;
}

void UBackAttachmentComponent::ScanMagnetCandidates(const FVector& MagnetLocation, float FieldRangeCm)
{
	const ACharacter* OwnerCharacter = ResolveOwnerCharacter();
	if (!OwnerCharacter)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World || FieldRangeCm <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	TArray<FOverlapResult> OverlapResults;
	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_PhysicsBody);
	ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PlayerMagnetFieldScan), false, OwnerCharacter);
	QueryParams.AddIgnoredActor(OwnerCharacter);

	World->OverlapMultiByObjectType(
		OverlapResults,
		MagnetLocation,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(FieldRangeCm),
		QueryParams);

	AActor* NearestCandidate = nullptr;
	float NearestDistanceSq = MAX_flt;

	for (const FOverlapResult& OverlapResult : OverlapResults)
	{
		UPrimitiveComponent* OverlapPrimitive = OverlapResult.GetComponent();
		AActor* CandidateActor = OverlapPrimitive ? OverlapPrimitive->GetOwner() : nullptr;
		if (!CandidateActor || CandidateActor == OwnerCharacter)
		{
			continue;
		}

		if (!CanEquipBackItem(CandidateActor, MagnetLocation, FieldRangeCm))
		{
			continue;
		}

		UPrimitiveComponent* CandidatePrimitive = ResolveBackItemPrimitive(CandidateActor);
		if (!CandidatePrimitive || !CandidatePrimitive->IsSimulatingPhysics())
		{
			continue;
		}

		UBackpackMagnetComponent* CandidateMagnet = ResolveBackItemMagnet(CandidateActor);
		RegisterTrackedItem(CandidateActor, CandidatePrimitive, CandidateMagnet);

		const float DistanceSq = FVector::DistSquared(CandidatePrimitive->GetComponentLocation(), MagnetLocation);
		if (!NearestCandidate || DistanceSq < NearestDistanceSq)
		{
			NearestCandidate = CandidateActor;
			NearestDistanceSq = DistanceSq;
		}
	}

	if (NearestCandidate)
	{
		BackItemActor = NearestCandidate;
	}
}

void UBackAttachmentComponent::RegisterTrackedItem(
	AActor* InBackItemActor,
	UPrimitiveComponent* BackItemPrimitive,
	const UBackpackMagnetComponent* BackItemMagnet)
{
	if (!InBackItemActor || !BackItemPrimitive)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FObjectKey ItemKey(BackItemPrimitive);
	FTrackedMagnetItem& TrackedItem = TrackedItems.FindOrAdd(ItemKey);
	TrackedItem.Actor = InBackItemActor;
	TrackedItem.Primitive = BackItemPrimitive;
	TrackedItem.BackpackMagnet = const_cast<UBackpackMagnetComponent*>(BackItemMagnet);
	TrackedItem.bHasMagLockAnchor = DoesBackItemHaveMagLockAnchor(InBackItemActor);
	TrackedItem.LastSeenTime = World->GetTimeSeconds();
	if (!bManualMagnetRequested)
	{
		TrackedItem.bStageOneLiftInitialized = false;
		TrackedItem.bStageOneSnapDelayInitialized = false;
		TrackedItem.StageOneSnapDelaySeconds = 0.0f;
		TrackedItem.MagLockVolumeEnterTime = -1.0f;
	}
}

void UBackAttachmentComponent::PruneTrackedItems(float CurrentWorldTime, bool bMagnetActive)
{
	const UPlayerMagnetComponent* PlayerMagnet = ResolvePlayerMagnetComponent();
	const float ForgetDelay = PlayerMagnet
		? FMath::Max(0.01f, PlayerMagnet->CandidateForgetDelay)
		: 0.25f;

	for (auto It = TrackedItems.CreateIterator(); It; ++It)
	{
		const FObjectKey ItemKey = It.Key();
		FTrackedMagnetItem& TrackedItem = It.Value();
		UPrimitiveComponent* Primitive = TrackedItem.Primitive.Get();
		const bool bInvalid = !TrackedItem.Actor.IsValid() || !Primitive || !IsValid(Primitive);
		const bool bCameraDecayActive = TrackedItem.bCollisionResponsesCaptured
			&& CurrentWorldTime < TrackedItem.CameraIgnoreUntilTime;
		const bool bExpired = !TrackedItem.bLocked
			&& !bCameraDecayActive
			&& ((CurrentWorldTime - TrackedItem.LastSeenTime) > (bMagnetActive ? ForgetDelay : 0.05f));

		if (!bInvalid && !bExpired)
		{
			continue;
		}

		if (TrackedItem.bLocked)
		{
			LockedItemKeys.Remove(ItemKey);
		}

		if (Primitive)
		{
			UnweldLockedItemFromMagnet(TrackedItem);
			SetCapsuleBackItemCollisionIgnored(Primitive, false);
			RestoreMagnetKinematicState(TrackedItem);
			RestoreMagnetCollisionOverrides(TrackedItem, -1.0f);
		}

		It.RemoveCurrent();
	}
}

void UBackAttachmentComponent::ApplyMagnetForces(
	float DeltaTime,
	float MagnetAlpha,
	USceneComponent* MagnetAnchorComponent,
	FName MagnetAnchorSocketName,
	const FTransform& MagnetAnchorTransform,
	const FVector& MagnetAnchorVelocity,
	UShapeComponent* MagLockZone)
{
	(void)MagnetAnchorVelocity;

	const UPlayerMagnetComponent* PlayerMagnet = ResolvePlayerMagnetComponent();
	if (!PlayerMagnet || TrackedItems.Num() == 0)
	{
		return;
	}

	const FVector MagnetAnchorLocation = MagnetAnchorTransform.GetLocation();
	const bool bHasMagLockVolume = MagLockZone != nullptr;
	const FVector FieldTargetLocation = (bHasMagLockVolume && PlayerMagnet->bUseMagLockZoneCenterAsAttractTarget)
		? MagLockZone->GetComponentLocation()
		: MagnetAnchorLocation;
	const float FieldRange = FMath::Max(1.0f, PlayerMagnet->AttractRange);
	const float FieldRangeSq = FMath::Square(FieldRange);
	const float CaptureRange = FMath::Clamp(PlayerMagnet->CaptureRange, 1.0f, FieldRange);
	const float LockFallbackDistance = FMath::Max(0.0f, PlayerMagnet->LockDistanceFallback);
	const float WeldAttachDistance = FMath::Max(1.0f, PlayerMagnet->AttachDistance);
	const bool bAllowWeldWhileActive = !PlayerMagnet->bOnlyWeldWhenMagnetInactive;
	const bool bLooseRotationWhileActive = PlayerMagnet->bLooseRotationWhileMagnetActive;
	const float CurrentWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float StageOneDuration = FMath::Max(0.0f, PlayerMagnet->ChargeStageOneDuration);
	const float LiftStartDelay = FMath::Clamp(PlayerMagnet->ChargeLiftStartDelay, 0.0f, StageOneDuration);
	const float HeldDuration = FMath::Max(0.0f, ManualMagnetHeldDuration);
	const bool bStageOne = HeldDuration < StageOneDuration;
	const bool bLiftActiveInStageOne = bStageOne && HeldDuration >= LiftStartDelay;
	const float StageOneAlpha = StageOneDuration > KINDA_SMALL_NUMBER
		? FMath::Clamp(HeldDuration / StageOneDuration, 0.0f, 1.0f)
		: 1.0f;
	const float StageOneSnapDelayMin = FMath::Max(0.0f, PlayerMagnet->ChargeSnapDelayAfterLiftMin);
	const float StageOneSnapDelayMax = FMath::Max(StageOneSnapDelayMin, PlayerMagnet->ChargeSnapDelayAfterLiftMax);
	const float MagLockVolumeAnchorDelay = FMath::Max(0.0f, PlayerMagnet->MagLockVolumeAnchorDelay);
	const float SlamStrengthMultiplier = FMath::Max(1.0f, PlayerMagnet->ChargeSlamStrengthMultiplier);

	struct FProcessCandidate
	{
		FObjectKey ItemKey;
		float DistanceSq = MAX_flt;
	};

	TArray<FProcessCandidate> SortedCandidates;
	SortedCandidates.Reserve(TrackedItems.Num());

	for (const TPair<FObjectKey, FTrackedMagnetItem>& Pair : TrackedItems)
	{
		const FTrackedMagnetItem& TrackedItem = Pair.Value;
		const AActor* CandidateActor = TrackedItem.Actor.Get();
		const UPrimitiveComponent* BackItemPrimitive = TrackedItem.Primitive.Get();
		if (!CandidateActor || !BackItemPrimitive)
		{
			continue;
		}

		const FVector AttachPointLocation = ResolveBackItemAttachPointWorldTransform(
			CandidateActor,
			TrackedItem.BackpackMagnet.Get()).GetLocation();
		const float DistanceSq = FVector::DistSquared(AttachPointLocation, FieldTargetLocation);

		if (!TrackedItem.bLocked && DistanceSq > FieldRangeSq)
		{
			continue;
		}

		FProcessCandidate Candidate;
		Candidate.ItemKey = Pair.Key;
		Candidate.DistanceSq = DistanceSq;
		SortedCandidates.Add(Candidate);
	}

	SortedCandidates.Sort([](const FProcessCandidate& Left, const FProcessCandidate& Right)
	{
		return Left.DistanceSq < Right.DistanceSq;
	});

	const int32 MaxAffectedItems = FMath::Max(1, PlayerMagnet->MaxAffectedItems);
	TSet<UPrimitiveComponent*> ProcessedPrimitives;

	for (int32 CandidateIndex = 0; CandidateIndex < SortedCandidates.Num(); ++CandidateIndex)
	{
		const FObjectKey ItemKey = SortedCandidates[CandidateIndex].ItemKey;
		FTrackedMagnetItem* TrackedItem = TrackedItems.Find(ItemKey);
		if (!TrackedItem)
		{
			continue;
		}

		const bool bOverBudget = CandidateIndex >= MaxAffectedItems;

		AActor* CurrentBackItemActor = TrackedItem->Actor.Get();
		UPrimitiveComponent* BackItemPrimitive = TrackedItem->Primitive.Get();
		if (!CurrentBackItemActor || !BackItemPrimitive)
		{
			continue;
		}

		const UBackpackMagnetComponent* BackItemMagnet = TrackedItem->BackpackMagnet.Get();
		const bool bIsBackpackItem = BackItemMagnet != nullptr;
		const bool bIntentionalBackpackMotion = bIsBackpackItem && PlayerMagnet->bIntentionalBackpackItemSnapMotion;
		const float BackpackLocationSpeedScale = bIntentionalBackpackMotion
			? FMath::Max(0.05f, PlayerMagnet->BackpackItemApproachSpeedScale)
			: 1.0f;
		const float BackpackRotationSpeedScale = bIntentionalBackpackMotion
			? FMath::Max(0.05f, PlayerMagnet->BackpackItemRotationInterpScale)
			: 1.0f;
		const float BackpackFinalSnapDistance = bIntentionalBackpackMotion
			? FMath::Max(0.0f, PlayerMagnet->BackpackItemFinalSnapDistance)
			: 0.0f;
		const float BackpackAutoLockDistance = bIntentionalBackpackMotion
			? FMath::Max(0.0f, PlayerMagnet->BackpackItemAutoLockDistance)
			: 0.0f;
		const FTransform BackItemAttachPointTransform = ResolveBackItemAttachPointWorldTransform(
			CurrentBackItemActor,
			BackItemMagnet);
		const FVector BackItemAttachPointLocation = BackItemAttachPointTransform.GetLocation();
		const float DistanceToAnchor = FVector::Distance(BackItemAttachPointLocation, MagnetAnchorLocation);
		const float DistanceToFieldTarget = FVector::Distance(BackItemAttachPointLocation, FieldTargetLocation);
		if (!TrackedItem->bLocked && DistanceToFieldTarget > FieldRange)
		{
			continue;
		}

		const FVector PrimitiveLocation = BackItemPrimitive->GetComponentLocation();
		const bool bInsideZoneByAttachPoint = MagLockZone
			? IsInsideMagLockZone(MagLockZone, BackItemAttachPointLocation)
			: (DistanceToAnchor <= CaptureRange);
		const bool bInsideZoneByPrimitive = MagLockZone
			? IsInsideMagLockZone(MagLockZone, PrimitiveLocation)
			: (FVector::Distance(PrimitiveLocation, MagnetAnchorLocation) <= CaptureRange);
		const bool bInsideMagLockVolume = bInsideZoneByAttachPoint || bInsideZoneByPrimitive;
		if (bInsideMagLockVolume)
		{
			if (TrackedItem->MagLockVolumeEnterTime < 0.0f)
			{
				TrackedItem->MagLockVolumeEnterTime = CurrentWorldTime;
			}
		}
		else
		{
			TrackedItem->MagLockVolumeEnterTime = -1.0f;
		}
		const bool bVolumeAnchorDwellMet = bInsideMagLockVolume
			&& (CurrentWorldTime - TrackedItem->MagLockVolumeEnterTime) >= MagLockVolumeAnchorDelay;
		const float LowVelocityThreshold = FMath::Max(0.0f, PlayerMagnet->MagLockLowVelocityThreshold);
		const float CurrentItemSpeed = BackItemPrimitive->GetComponentVelocity().Size();
		const bool bLowVelocityLockReady = bInsideMagLockVolume
			&& PlayerMagnet->bAllowLowVelocityLockInsideMagLockVolume
			&& CurrentItemSpeed <= LowVelocityThreshold;

		bool bInsideFallbackLockRange = false;
		if (!bHasMagLockVolume && LockFallbackDistance > KINDA_SMALL_NUMBER && DistanceToAnchor <= LockFallbackDistance)
		{
			bInsideFallbackLockRange = true;
		}
		if (!bHasMagLockVolume && BackpackAutoLockDistance > KINDA_SMALL_NUMBER && DistanceToAnchor <= BackpackAutoLockDistance)
		{
			bInsideFallbackLockRange = true;
		}
		const bool bInsideLockRetentionZone = bHasMagLockVolume
			? bInsideMagLockVolume
			: (bInsideMagLockVolume || bInsideFallbackLockRange);

		if (bStageOne)
		{
			if (!TrackedItem->bStageOneSnapDelayInitialized)
			{
				TrackedItem->bStageOneSnapDelayInitialized = true;
				TrackedItem->StageOneSnapDelaySeconds = FMath::FRandRange(StageOneSnapDelayMin, StageOneSnapDelayMax);
			}
		}
		else
		{
			TrackedItem->bStageOneSnapDelayInitialized = false;
			TrackedItem->StageOneSnapDelaySeconds = 0.0f;
		}

		const float TimeSinceLiftStart = FMath::Max(0.0f, HeldDuration - LiftStartDelay);
		const bool bAllowSnapDuringStageOne = bStageOne
			&& bLiftActiveInStageOne
			&& TrackedItem->bStageOneSnapDelayInitialized
			&& TimeSinceLiftStart >= TrackedItem->StageOneSnapDelaySeconds;
		const bool bItemInStageOne = bStageOne && !bAllowSnapDuringStageOne;
		const bool bCanLockNow = CurrentWorldTime >= TrackedItem->LockReacquireTime;
		const bool bLockByZoneNow = bHasMagLockVolume
			? ((bVolumeAnchorDwellMet || bLowVelocityLockReady) && bCanLockNow)
			: (bInsideLockRetentionZone && bCanLockNow);

		const bool bBudgetBypassForZone = PlayerMagnet->bAllowUnlimitedItemsInsideMagLockZone && bInsideMagLockVolume;
		if (bOverBudget && !TrackedItem->bLocked && !bBudgetBypassForZone)
		{
			continue;
		}
		TrackedItem->LastSeenTime = CurrentWorldTime;

		if (!bItemInStageOne)
		{
			if (bLockByZoneNow)
			{
				SetItemLocked(ItemKey, true, CurrentWorldTime, 0.0f);
			}
			else if (TrackedItem->bLocked && !bInsideLockRetentionZone && !PlayerMagnet->bStickyLockInZone)
			{
				SetItemLocked(ItemKey, false, CurrentWorldTime, PlayerMagnet->LockReacquireDelay);
			}
		}

		const float ItemStrengthMultiplier = ResolveItemMagnetStrengthMultiplier(BackItemMagnet);
		const FTransform TargetBackItemTransform = BuildTargetBackItemWorldTransform(
			CurrentBackItemActor,
			BackItemMagnet,
			MagnetAnchorTransform);

		ApplyMagnetCollisionOverrides(*TrackedItem, PlayerMagnet, CurrentWorldTime);
		if (!bItemInStageOne)
		{
			TrackedItem->bStageOneLiftInitialized = false;
		}

		if (TrackedItem->bLocked)
		{
			EnsureMagnetKinematicState(*TrackedItem);
			const bool bShouldAlignLockedRotation = bIsBackpackItem
				|| (TrackedItem->bHasMagLockAnchor && !bLooseRotationWhileActive);
			const bool bShouldWeldLockedItem = bAllowWeldWhileActive
				&& PlayerMagnet->bWeldLockedItemsToMagnet
				&& MagnetAnchorComponent
				&& DistanceToAnchor <= WeldAttachDistance;
			if (bShouldWeldLockedItem)
			{
				WeldLockedItemToMagnet(*TrackedItem, MagnetAnchorComponent, MagnetAnchorSocketName);
			}
			else
			{
				UnweldLockedItemFromMagnet(*TrackedItem);
				if (PlayerMagnet->bDirectAnchorLockedItems)
				{
					const FQuat CurrentRotation = BackItemPrimitive->GetComponentQuat().GetNormalized();
					const FQuat TargetRotation = TargetBackItemTransform.GetRotation().GetNormalized();
					BackItemPrimitive->SetWorldLocationAndRotation(
						TargetBackItemTransform.GetLocation(),
						bShouldAlignLockedRotation ? TargetRotation : CurrentRotation,
						false,
						nullptr,
						ETeleportType::TeleportPhysics);
				}
				else
				{
					const float LockedPullAlpha = bItemInStageOne ? 1.0f : (MagnetAlpha * SlamStrengthMultiplier);
					MoveMagnetItemKinematic(
						DeltaTime,
						*TrackedItem,
						TargetBackItemTransform,
						DistanceToAnchor,
						LockedPullAlpha,
						ItemStrengthMultiplier,
						PlayerMagnet,
						bShouldAlignLockedRotation,
						BackpackLocationSpeedScale,
						BackpackRotationSpeedScale,
						BackpackFinalSnapDistance);
				}
			}
			BackItemActor = CurrentBackItemActor;
		}
		else if (bItemInStageOne)
		{
			UnweldLockedItemFromMagnet(*TrackedItem);
			RestoreMagnetKinematicState(*TrackedItem);
			if (BackItemPrimitive->IsSimulatingPhysics())
			{
				const float ItemMassKg = FMath::Max(0.1f, BackItemPrimitive->GetMass());
				ApplyStageOneWobble(
					BackItemPrimitive,
					ItemMassKg,
					CurrentWorldTime,
					StageOneAlpha,
					PlayerMagnet);

				if (bLiftActiveInStageOne)
				{
					const FVector AttachPointVelocity = BackItemPrimitive->GetPhysicsLinearVelocityAtPoint(
						BackItemAttachPointLocation,
						NAME_None);
					ApplyStageOneLiftForce(
						*TrackedItem,
						BackItemAttachPointLocation,
						AttachPointVelocity,
						PlayerMagnet);
				}
				else
				{
					TrackedItem->bStageOneLiftInitialized = false;
				}

				const float StageOnePullMinAlpha = FMath::Max(0.0f, PlayerMagnet->ChargeStageOnePullMinAlpha);
				const float StageOnePullMaxAlpha = FMath::Max(StageOnePullMinAlpha, PlayerMagnet->ChargeStageOnePullMaxAlpha);
				const float StageOnePullAlpha = FMath::Lerp(StageOnePullMinAlpha, StageOnePullMaxAlpha, StageOneAlpha);
				ApplyAttractionForce(
					BackItemPrimitive,
					ItemMassKg,
					BackItemAttachPointLocation,
					FieldTargetLocation,
					DistanceToFieldTarget,
					StageOnePullAlpha,
					ItemStrengthMultiplier,
					PlayerMagnet);
			}
		}
		else
		{
			UnweldLockedItemFromMagnet(*TrackedItem);
			if (DistanceToAnchor <= CaptureRange)
			{
				EnsureMagnetKinematicState(*TrackedItem);
				const bool bShouldAlignCaptureRotation = bIsBackpackItem
					|| (TrackedItem->bHasMagLockAnchor && !bLooseRotationWhileActive);
				MoveMagnetItemKinematic(
					DeltaTime,
					*TrackedItem,
					TargetBackItemTransform,
					DistanceToAnchor,
					MagnetAlpha * SlamStrengthMultiplier,
					ItemStrengthMultiplier,
					PlayerMagnet,
					bShouldAlignCaptureRotation,
					BackpackLocationSpeedScale,
					BackpackRotationSpeedScale,
					BackpackFinalSnapDistance);
			}
			else
			{
				RestoreMagnetKinematicState(*TrackedItem);
				if (BackItemPrimitive->IsSimulatingPhysics())
				{
					const float ItemMassKg = FMath::Max(0.1f, BackItemPrimitive->GetMass());
					const float OuterPullAlpha = FMath::Clamp(
						MagnetAlpha * FMath::Clamp(PlayerMagnet->InAirPullStrengthScale, 0.0f, 1.0f) * SlamStrengthMultiplier,
						0.0f,
						SlamStrengthMultiplier);
					ApplyAttractionForce(
						BackItemPrimitive,
						ItemMassKg,
						BackItemAttachPointLocation,
						FieldTargetLocation,
						DistanceToFieldTarget,
						OuterPullAlpha,
						ItemStrengthMultiplier,
						PlayerMagnet);
				}
			}
		}

		if (!TrackedItem->bLocked && !bItemInStageOne)
		{
			TryTriggerFrontMagnetImpactKnockdown(*TrackedItem, PlayerMagnet, CurrentWorldTime);
		}

		SetCapsuleBackItemCollisionIgnored(BackItemPrimitive, true);
		ProcessedPrimitives.Add(BackItemPrimitive);
	}

	for (TPair<FObjectKey, FTrackedMagnetItem>& Pair : TrackedItems)
	{
		FTrackedMagnetItem& TrackedItem = Pair.Value;
		UPrimitiveComponent* BackItemPrimitive = TrackedItem.Primitive.Get();
		if (!BackItemPrimitive || TrackedItem.bLocked || ProcessedPrimitives.Contains(BackItemPrimitive))
		{
			continue;
		}

		SetCapsuleBackItemCollisionIgnored(BackItemPrimitive, false);
		UnweldLockedItemFromMagnet(TrackedItem);
		RestoreMagnetKinematicState(TrackedItem);
		RestoreMagnetCollisionOverrides(TrackedItem, CurrentWorldTime);
		TrackedItem.bStageOneLiftInitialized = false;
		TrackedItem.bStageOneSnapDelayInitialized = false;
		TrackedItem.StageOneSnapDelaySeconds = 0.0f;
		TrackedItem.MagLockVolumeEnterTime = -1.0f;
	}

	TArray<TWeakObjectPtr<UPrimitiveComponent>> IgnoredPrimitivesToClear;
	for (const TWeakObjectPtr<UPrimitiveComponent>& PrimitivePtr : CapsuleIgnoredBackItemPrimitives)
	{
		UPrimitiveComponent* IgnoredPrimitive = PrimitivePtr.Get();
		if (!IgnoredPrimitive || ProcessedPrimitives.Contains(IgnoredPrimitive))
		{
			continue;
		}

		IgnoredPrimitivesToClear.Add(PrimitivePtr);
	}

	for (const TWeakObjectPtr<UPrimitiveComponent>& PrimitivePtr : IgnoredPrimitivesToClear)
	{
		if (UPrimitiveComponent* IgnoredPrimitive = PrimitivePtr.Get())
		{
			SetCapsuleBackItemCollisionIgnored(IgnoredPrimitive, false);
		}
	}
}

bool UBackAttachmentComponent::IsInsideMagLockZone(const UShapeComponent* MagLockZone, const FVector& WorldLocation) const
{
	if (!MagLockZone)
	{
		return false;
	}

	if (const USphereComponent* SphereComponent = Cast<USphereComponent>(MagLockZone))
	{
		const float Radius = SphereComponent->GetScaledSphereRadius();
		return FVector::DistSquared(WorldLocation, SphereComponent->GetComponentLocation()) <= FMath::Square(Radius);
	}

	if (const UBoxComponent* BoxComponent = Cast<UBoxComponent>(MagLockZone))
	{
		const FVector LocalLocation = BoxComponent->GetComponentTransform().InverseTransformPosition(WorldLocation);
		const FVector Extent = BoxComponent->GetScaledBoxExtent();
		return FMath::Abs(LocalLocation.X) <= Extent.X
			&& FMath::Abs(LocalLocation.Y) <= Extent.Y
			&& FMath::Abs(LocalLocation.Z) <= Extent.Z;
	}

	if (const UCapsuleComponent* CapsuleComponent = Cast<UCapsuleComponent>(MagLockZone))
	{
		const FVector LocalLocation = CapsuleComponent->GetComponentTransform().InverseTransformPosition(WorldLocation);
		const float Radius = CapsuleComponent->GetScaledCapsuleRadius();
		const float HalfHeight = CapsuleComponent->GetScaledCapsuleHalfHeight();
		const float CylinderHalfHeight = FMath::Max(0.0f, HalfHeight - Radius);
		const float ClampedZ = FMath::Clamp(LocalLocation.Z, -CylinderHalfHeight, CylinderHalfHeight);
		const FVector ClosestPoint(LocalLocation.X, LocalLocation.Y, ClampedZ);
		return FVector::DistSquared(LocalLocation, ClosestPoint) <= FMath::Square(Radius);
	}

	return MagLockZone->Bounds.GetBox().IsInsideOrOn(WorldLocation);
}

void UBackAttachmentComponent::SetItemLocked(
	FObjectKey ItemKey,
	bool bLocked,
	float CurrentWorldTime,
	float ReacquireDelay)
{
	FTrackedMagnetItem* TrackedItem = TrackedItems.Find(ItemKey);
	if (!TrackedItem)
	{
		return;
	}

	if (TrackedItem->bLocked == bLocked)
	{
		return;
	}

	TrackedItem->bLocked = bLocked;
	if (bLocked)
	{
		LockedItemKeys.Add(ItemKey);
		if (AActor* LockedActor = TrackedItem->Actor.Get())
		{
			BackItemActor = LockedActor;
		}
		return;
	}

	LockedItemKeys.Remove(ItemKey);
	UnweldLockedItemFromMagnet(*TrackedItem);
	TrackedItem->bStageOneLiftInitialized = false;
	TrackedItem->bStageOneSnapDelayInitialized = false;
	TrackedItem->StageOneSnapDelaySeconds = 0.0f;
	TrackedItem->MagLockVolumeEnterTime = -1.0f;
	TrackedItem->LockReacquireTime = CurrentWorldTime + FMath::Max(0.0f, ReacquireDelay);
}

void UBackAttachmentComponent::ReleaseAllLockedItems(bool bSetReacquireCooldown, bool bApplyDropImpulse)
{
	if (LockedItemKeys.Num() == 0)
	{
		return;
	}

	const UPlayerMagnetComponent* PlayerMagnet = ResolvePlayerMagnetComponent();
	const float ReacquireDelay = (bSetReacquireCooldown && PlayerMagnet)
		? FMath::Max(0.0f, PlayerMagnet->LockReacquireDelay)
		: 0.0f;
	const float CurrentWorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const FVector DropImpulse = bApplyDropImpulse ? BuildDropImpulse() : FVector::ZeroVector;

	for (const FObjectKey& ItemKey : LockedItemKeys)
	{
		FTrackedMagnetItem* TrackedItem = TrackedItems.Find(ItemKey);
		if (!TrackedItem)
		{
			continue;
		}

		TrackedItem->bLocked = false;
		UnweldLockedItemFromMagnet(*TrackedItem);
		TrackedItem->bStageOneLiftInitialized = false;
		TrackedItem->bStageOneSnapDelayInitialized = false;
		TrackedItem->StageOneSnapDelaySeconds = 0.0f;
		TrackedItem->MagLockVolumeEnterTime = -1.0f;
		TrackedItem->LockReacquireTime = CurrentWorldTime + ReacquireDelay;

		if (UPrimitiveComponent* BackItemPrimitive = TrackedItem->Primitive.Get())
		{
			RestoreMagnetKinematicState(*TrackedItem);
			if (!DropImpulse.IsNearlyZero())
			{
				BackItemPrimitive->AddImpulse(DropImpulse, NAME_None, true);
			}
		}
	}

	LockedItemKeys.Reset();
}

void UBackAttachmentComponent::ApplyAttractionForce(
	UPrimitiveComponent* BackItemPrimitive,
	float ItemMassKg,
	const FVector& CurrentAttachPointLocation,
	const FVector& MagnetAnchorLocation,
	float DistanceToAnchor,
	float MagnetAlpha,
	float ItemStrengthMultiplier,
	const UPlayerMagnetComponent* PlayerMagnet) const
{
	if (!BackItemPrimitive || !PlayerMagnet || ItemMassKg <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float FieldRange = FMath::Max(1.0f, PlayerMagnet->AttractRange);
	const float DistanceAlpha = 1.0f - FMath::Clamp(DistanceToAnchor / FieldRange, 0.0f, 1.0f);
	const float DistanceStrength = FMath::Pow(
		DistanceAlpha,
		FMath::Max(0.1f, PlayerMagnet->DistanceStrengthExponent));
	const float BaseStrength = FMath::Max(
		0.0f,
		MagnetAlpha * PlayerMagnet->MagnetStrengthMultiplier * ItemStrengthMultiplier);
	const float EffectiveStrength = FMath::Max(
		0.0f,
		BaseStrength * DistanceStrength);
	if (EffectiveStrength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector ToAnchor = MagnetAnchorLocation - CurrentAttachPointLocation;
	const FVector PullDirection = ToAnchor.GetSafeNormal();

	const float MaxPullSpeed = PlayerMagnet->MaxPullSpeed > KINDA_SMALL_NUMBER
		? PlayerMagnet->MaxPullSpeed
		: PlayerMagnet->PullMaxSpeed;
	const float DesiredSpeed = FMath::Lerp(
		FMath::Max(0.0f, PlayerMagnet->MinPullSpeed),
		FMath::Max(0.0f, MaxPullSpeed),
		DistanceStrength) * FMath::Max(0.05f, EffectiveStrength);
	FVector DesiredVelocity = PullDirection * DesiredSpeed;
	DesiredVelocity += FVector::UpVector
		* (FMath::Max(0.0f, PlayerMagnet->LiftSpeed) * EffectiveStrength);

	const FVector CurrentVelocity = BackItemPrimitive->GetPhysicsLinearVelocityAtPoint(CurrentAttachPointLocation, NAME_None);
	const FVector VelocityError = DesiredVelocity - CurrentVelocity;
	FVector Force = VelocityError * ItemMassKg * FMath::Max(0.0f, PlayerMagnet->VelocityGain);
	Force = Force.GetClampedToMaxSize(FMath::Max(0.0f, PlayerMagnet->MaxForcePerKg) * ItemMassKg);
	BackItemPrimitive->AddForceAtLocation(Force, CurrentAttachPointLocation, NAME_None);
}

void UBackAttachmentComponent::ApplyMagLockForce(
	UPrimitiveComponent* BackItemPrimitive,
	float ItemMassKg,
	const FVector& CurrentAttachPointLocation,
	const FVector& CurrentAttachPointVelocity,
	const FVector& MagnetAnchorLocation,
	const FVector& MagnetAnchorVelocity,
	float MagnetAlpha,
	float ItemStrengthMultiplier,
	const UPlayerMagnetComponent* PlayerMagnet) const
{
	if (!BackItemPrimitive || !PlayerMagnet || ItemMassKg <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float EffectiveStrength = FMath::Max(
		0.0f,
		MagnetAlpha * PlayerMagnet->MagnetStrengthMultiplier * ItemStrengthMultiplier);
	if (EffectiveStrength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector PositionError = MagnetAnchorLocation - CurrentAttachPointLocation;
	const FVector VelocityError = MagnetAnchorVelocity - CurrentAttachPointVelocity;
	FVector HoldForce = PositionError * FMath::Max(0.0f, PlayerMagnet->HoldSpringStrength)
		+ VelocityError * FMath::Max(0.0f, PlayerMagnet->HoldDamping);
	HoldForce *= EffectiveStrength;
	HoldForce = HoldForce.GetClampedToMaxSize(FMath::Max(0.0f, PlayerMagnet->MaxHoldForcePerKg) * ItemMassKg);

	BackItemPrimitive->AddForceAtLocation(HoldForce, CurrentAttachPointLocation, NAME_None);
}

void UBackAttachmentComponent::ApplyAlignmentTorque(
	UPrimitiveComponent* BackItemPrimitive,
	float ItemMassKg,
	const FQuat& TargetRotation,
	float MagnetAlpha,
	float ItemStrengthMultiplier,
	const UPlayerMagnetComponent* PlayerMagnet) const
{
	if (!BackItemPrimitive || !PlayerMagnet || ItemMassKg <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	FQuat CurrentRotation = BackItemPrimitive->GetComponentQuat();
	CurrentRotation.Normalize();

	FQuat DesiredRotation = TargetRotation;
	DesiredRotation.Normalize();

	FQuat DeltaRotation = DesiredRotation * CurrentRotation.Inverse();
	DeltaRotation.Normalize();

	FVector Axis = FVector::ZeroVector;
	float AngleRadians = 0.0f;
	DeltaRotation.ToAxisAndAngle(Axis, AngleRadians);
	if (AngleRadians > PI)
	{
		AngleRadians -= (2.0f * PI);
	}

	if (Axis.IsNearlyZero() || FMath::IsNearlyZero(AngleRadians, KINDA_SMALL_NUMBER))
	{
		return;
	}

	const float EffectiveStrength = FMath::Max(
		0.0f,
		MagnetAlpha * PlayerMagnet->MagnetStrengthMultiplier * ItemStrengthMultiplier);
	if (EffectiveStrength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector CurrentAngularVelocity = BackItemPrimitive->GetPhysicsAngularVelocityInRadians(NAME_None);
	FVector Torque = Axis.GetSafeNormal() * AngleRadians * FMath::Max(0.0f, PlayerMagnet->AlignTorqueGain)
		- CurrentAngularVelocity * FMath::Max(0.0f, PlayerMagnet->AlignAngularDamping);
	Torque *= (ItemMassKg * EffectiveStrength);
	Torque = Torque.GetClampedToMaxSize(FMath::Max(0.0f, PlayerMagnet->MaxTorquePerKg) * ItemMassKg);
	BackItemPrimitive->AddTorqueInRadians(Torque, NAME_None, false);
}

void UBackAttachmentComponent::EnsureMagnetKinematicState(FTrackedMagnetItem& TrackedItem) const
{
	UPrimitiveComponent* BackItemPrimitive = TrackedItem.Primitive.Get();
	if (!BackItemPrimitive)
	{
		return;
	}

	if (!TrackedItem.bPhysicsStateCaptured)
	{
		TrackedItem.bWasSimulatingPhysics = BackItemPrimitive->IsSimulatingPhysics();
		TrackedItem.CachedCollisionEnabled = BackItemPrimitive->GetCollisionEnabled();
		TrackedItem.bPhysicsStateCaptured = true;
	}

	if (TrackedItem.bWasSimulatingPhysics && BackItemPrimitive->IsSimulatingPhysics())
	{
		BackItemPrimitive->SetSimulatePhysics(false);
	}

	if (BackItemPrimitive->GetCollisionEnabled() != ECollisionEnabled::QueryAndPhysics)
	{
		BackItemPrimitive->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
}

void UBackAttachmentComponent::RestoreMagnetKinematicState(FTrackedMagnetItem& TrackedItem, bool bWakePhysics) const
{
	UPrimitiveComponent* BackItemPrimitive = TrackedItem.Primitive.Get();
	if (!BackItemPrimitive || !TrackedItem.bPhysicsStateCaptured)
	{
		TrackedItem.bPhysicsStateCaptured = false;
		return;
	}

	if (BackItemPrimitive->GetCollisionEnabled() != TrackedItem.CachedCollisionEnabled)
	{
		BackItemPrimitive->SetCollisionEnabled(TrackedItem.CachedCollisionEnabled);
	}

	if (TrackedItem.bWasSimulatingPhysics && !BackItemPrimitive->IsSimulatingPhysics())
	{
		BackItemPrimitive->SetSimulatePhysics(true);
		if (bWakePhysics)
		{
			BackItemPrimitive->WakeRigidBody(NAME_None);
		}
	}

	TrackedItem.bPhysicsStateCaptured = false;
}

void UBackAttachmentComponent::WeldLockedItemToMagnet(
	FTrackedMagnetItem& TrackedItem,
	USceneComponent* MagnetAnchorComponent,
	FName MagnetAnchorSocketName) const
{
	if (!MagnetAnchorComponent)
	{
		return;
	}

	AActor* BackItemOwnerActor = TrackedItem.Actor.Get();
	UPrimitiveComponent* BackItemPrimitive = TrackedItem.Primitive.Get();
	USceneComponent* BackItemRootComponent = BackItemOwnerActor ? BackItemOwnerActor->GetRootComponent() : nullptr;
	if (!BackItemOwnerActor || !BackItemPrimitive || !BackItemRootComponent)
	{
		TrackedItem.bWeldedToMagnet = false;
		return;
	}

	const USceneComponent* ExistingAttachParent = BackItemRootComponent->GetAttachParent();
	const FName ExistingAttachSocket = BackItemRootComponent->GetAttachSocketName();
	if (TrackedItem.bWeldedToMagnet
		&& ExistingAttachParent == MagnetAnchorComponent
		&& (MagnetAnchorSocketName.IsNone() || ExistingAttachSocket == MagnetAnchorSocketName))
	{
		return;
	}

	const FAttachmentTransformRules AttachRules(EAttachmentRule::KeepWorld, true);
	const bool bAttachedActor = BackItemOwnerActor->AttachToComponent(
		MagnetAnchorComponent,
		AttachRules,
		MagnetAnchorSocketName);
	bool bWeldAttached = bAttachedActor;
	if (!bWeldAttached)
	{
		bWeldAttached = BackItemRootComponent->AttachToComponent(MagnetAnchorComponent, AttachRules, MagnetAnchorSocketName);
	}

	TrackedItem.bWeldedToMagnet = bWeldAttached;
}

void UBackAttachmentComponent::UnweldLockedItemFromMagnet(FTrackedMagnetItem& TrackedItem) const
{
	if (!TrackedItem.bWeldedToMagnet)
	{
		return;
	}

	AActor* BackItemOwnerActor = TrackedItem.Actor.Get();
	USceneComponent* BackItemRootComponent = BackItemOwnerActor ? BackItemOwnerActor->GetRootComponent() : nullptr;
	if (BackItemOwnerActor && BackItemRootComponent && BackItemRootComponent->GetAttachParent())
	{
		BackItemOwnerActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}

	TrackedItem.bWeldedToMagnet = false;
}

void UBackAttachmentComponent::ApplyStageOneWobble(
	UPrimitiveComponent* BackItemPrimitive,
	float ItemMassKg,
	float CurrentWorldTime,
	float StageAlpha,
	const UPlayerMagnetComponent* PlayerMagnet) const
{
	if (!BackItemPrimitive || !PlayerMagnet || ItemMassKg <= KINDA_SMALL_NUMBER || !BackItemPrimitive->IsSimulatingPhysics())
	{
		return;
	}

	const float NoiseFrequency = FMath::Max(0.01f, PlayerMagnet->FieldNoiseFrequency);
	const float Time = CurrentWorldTime * NoiseFrequency;
	const float Seed = static_cast<float>(GetTypeHash(BackItemPrimitive) & 0xFFFF);
	const float SeedScale = 0.00017f;

	FVector Noise(
		FMath::PerlinNoise1D(Time + Seed * SeedScale + 1.31f),
		FMath::PerlinNoise1D(Time + Seed * SeedScale + 5.97f),
		FMath::PerlinNoise1D(Time + Seed * SeedScale + 9.43f));
	if (Noise.IsNearlyZero())
	{
		Noise = FVector::ForwardVector;
	}
	Noise = Noise.GetSafeNormal();

	const float StageAlphaClamped = FMath::Clamp(StageAlpha, 0.0f, 1.0f);
	const float LinearNoiseMin = FMath::Max(0.0f, PlayerMagnet->ChargeStageOneLinearNoiseMin);
	const float LinearNoiseMax = FMath::Max(LinearNoiseMin, PlayerMagnet->ChargeStageOneLinearNoiseMax);
	const float WobbleForceStrength = ItemMassKg
		* FMath::Lerp(LinearNoiseMin, LinearNoiseMax, StageAlphaClamped);
	BackItemPrimitive->AddForce(Noise * WobbleForceStrength, NAME_None, false);

	FVector TorqueNoise(
		FMath::PerlinNoise1D(Time + Seed * SeedScale + 2.17f),
		FMath::PerlinNoise1D(Time + Seed * SeedScale + 6.53f),
		FMath::PerlinNoise1D(Time + Seed * SeedScale + 10.11f));
	if (TorqueNoise.IsNearlyZero())
	{
		TorqueNoise = FVector::UpVector;
	}
	TorqueNoise = TorqueNoise.GetSafeNormal();

	const float AngularNoiseRad = FMath::DegreesToRadians(FMath::Max(0.0f, PlayerMagnet->FieldAngularNoiseDegPerSec));
	const float AngularNoiseMinScale = FMath::Max(0.0f, PlayerMagnet->ChargeStageOneAngularNoiseMinScale);
	const float AngularNoiseMaxScale = FMath::Max(
		AngularNoiseMinScale,
		PlayerMagnet->ChargeStageOneAngularNoiseMaxScale);
	const float TorqueStrength = ItemMassKg
		* AngularNoiseRad
		* FMath::Lerp(AngularNoiseMinScale, AngularNoiseMaxScale, StageAlphaClamped);
	BackItemPrimitive->AddTorqueInRadians(TorqueNoise * TorqueStrength, NAME_None, false);
}

void UBackAttachmentComponent::ApplyStageOneLiftForce(
	FTrackedMagnetItem& TrackedItem,
	const FVector& CurrentAttachPointLocation,
	const FVector& CurrentAttachPointVelocity,
	const UPlayerMagnetComponent* PlayerMagnet) const
{
	UPrimitiveComponent* BackItemPrimitive = TrackedItem.Primitive.Get();
	if (!BackItemPrimitive || !PlayerMagnet || !BackItemPrimitive->IsSimulatingPhysics())
	{
		return;
	}

	const float LiftMinCm = FMath::Max(0.0f, PlayerMagnet->ChargeLiftMinHeight);
	const float LiftMaxCm = FMath::Max(LiftMinCm, PlayerMagnet->ChargeLiftMaxHeight);
	if (!TrackedItem.bStageOneLiftInitialized)
	{
		TrackedItem.bStageOneLiftInitialized = true;
		TrackedItem.StageOneLiftBaseLocation = CurrentAttachPointLocation;

		const int32 Seed = static_cast<int32>(GetTypeHash(BackItemPrimitive) & 0x7FFFFFFF);
		FRandomStream RandomStream(Seed);
		TrackedItem.StageOneLiftTargetCm = FMath::Lerp(LiftMinCm, LiftMaxCm, RandomStream.GetFraction());
	}

	const float TargetZ = TrackedItem.StageOneLiftBaseLocation.Z + TrackedItem.StageOneLiftTargetCm;
	const float ZError = TargetZ - CurrentAttachPointLocation.Z;
	const float ItemMassKg = FMath::Max(0.1f, BackItemPrimitive->GetMass());
	const float SpringStrength = FMath::Max(0.0f, PlayerMagnet->ChargeLiftSpringStrength);
	const float DampingStrength = FMath::Max(0.0f, PlayerMagnet->ChargeLiftDamping);
	const float MaxLiftForce = ItemMassKg * FMath::Max(0.0f, PlayerMagnet->ChargeLiftMaxForcePerKg);
	const float DownwardClampRatio = FMath::Clamp(PlayerMagnet->ChargeLiftDownwardClampRatio, 0.0f, 1.0f);
	if (MaxLiftForce <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	float ForceZ = (ZError * SpringStrength - CurrentAttachPointVelocity.Z * DampingStrength) * ItemMassKg;
	ForceZ = FMath::Clamp(ForceZ, -MaxLiftForce * DownwardClampRatio, MaxLiftForce);
	BackItemPrimitive->AddForce(FVector(0.0f, 0.0f, ForceZ), NAME_None, false);
}

void UBackAttachmentComponent::TryTriggerFrontMagnetImpactKnockdown(
	const FTrackedMagnetItem& TrackedItem,
	const UPlayerMagnetComponent* PlayerMagnet,
	float CurrentWorldTime)
{
	if (!PlayerMagnet || !PlayerMagnet->bEnableFrontMagnetImpactKnockdown)
	{
		return;
	}

	if (CurrentWorldTime < NextFrontMagnetImpactKnockdownTime)
	{
		return;
	}

	AAgentCharacter* AgentCharacter = Cast<AAgentCharacter>(ResolveOwnerCharacter());
	UPrimitiveComponent* BackItemPrimitive = TrackedItem.Primitive.Get();
	if (!AgentCharacter || !BackItemPrimitive || AgentCharacter->IsRagdolling())
	{
		return;
	}

	const FVector CharacterLocation = AgentCharacter->GetActorLocation();
	const FVector ItemLocation = BackItemPrimitive->GetComponentLocation();
	const FVector CharacterToItem = ItemLocation - CharacterLocation;
	const float DistanceToCharacter = CharacterToItem.Size();
	const float MaxDistance = FMath::Max(0.0f, PlayerMagnet->FrontMagnetImpactMaxDistance);
	if (DistanceToCharacter <= KINDA_SMALL_NUMBER || DistanceToCharacter > MaxDistance)
	{
		return;
	}

	const FVector CharacterToItemDir = CharacterToItem / DistanceToCharacter;
	const float FrontDot = FVector::DotProduct(
		AgentCharacter->GetActorForwardVector().GetSafeNormal(),
		CharacterToItemDir);
	if (FrontDot < FMath::Clamp(PlayerMagnet->FrontMagnetImpactMinFrontDot, -1.0f, 1.0f))
	{
		return;
	}

	const FVector RelativeVelocity = BackItemPrimitive->GetComponentVelocity() - AgentCharacter->GetVelocity();
	const float ClosingSpeed = FVector::DotProduct(RelativeVelocity, -CharacterToItemDir);
	const float MinClosingSpeed = FMath::Max(0.0f, PlayerMagnet->FrontMagnetImpactMinClosingSpeed);
	const float MaxClosingSpeed = FMath::Max(
		MinClosingSpeed + KINDA_SMALL_NUMBER,
		PlayerMagnet->FrontMagnetImpactMaxClosingSpeed);
	if (ClosingSpeed < MinClosingSpeed)
	{
		return;
	}

	const float SpeedAlpha = FMath::Clamp(
		(ClosingSpeed - MinClosingSpeed) / (MaxClosingSpeed - MinClosingSpeed),
		0.0f,
		1.0f);
	const float MinChance = FMath::Clamp(PlayerMagnet->FrontMagnetImpactMinKnockdownChance, 0.0f, 1.0f);
	const float MaxChance = FMath::Clamp(PlayerMagnet->FrontMagnetImpactMaxKnockdownChance, MinChance, 1.0f);
	const float KnockdownChance = FMath::Lerp(MinChance, MaxChance, SpeedAlpha);

	NextFrontMagnetImpactKnockdownTime = CurrentWorldTime
		+ FMath::Max(0.0f, PlayerMagnet->FrontMagnetImpactAttemptCooldown);
	if (FMath::FRand() <= KnockdownChance)
	{
		AgentCharacter->RequestEnterRagdoll(EAgentRagdollReason::Impact);
	}
}

void UBackAttachmentComponent::MoveMagnetItemKinematic(
	float DeltaTime,
	FTrackedMagnetItem& TrackedItem,
	const FTransform& TargetBackItemTransform,
	float DistanceToAnchor,
	float MagnetAlpha,
	float ItemStrengthMultiplier,
	const UPlayerMagnetComponent* PlayerMagnet,
	bool bAlignRotation,
	float LocationSpeedScale,
	float RotationSpeedScale,
	float FinalSnapDistance) const
{
	UPrimitiveComponent* BackItemPrimitive = TrackedItem.Primitive.Get();
	if (!BackItemPrimitive || !PlayerMagnet)
	{
		return;
	}

	const float SafeDeltaTime = FMath::Max(0.0f, DeltaTime);
	float EffectiveStrength = FMath::Max(
		0.0f,
		MagnetAlpha * PlayerMagnet->MagnetStrengthMultiplier * ItemStrengthMultiplier);
	if (!TrackedItem.bLocked)
	{
		EffectiveStrength *= FMath::Clamp(PlayerMagnet->InAirPullStrengthScale, 0.0f, 1.0f);
	}
	if (EffectiveStrength <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const float FieldRange = FMath::Max(1.0f, PlayerMagnet->AttractRange);
	const float DistanceAlpha = 1.0f - FMath::Clamp(DistanceToAnchor / FieldRange, 0.0f, 1.0f);
	const float MaxPullSpeed = PlayerMagnet->MaxPullSpeed > KINDA_SMALL_NUMBER
		? PlayerMagnet->MaxPullSpeed
		: PlayerMagnet->PullMaxSpeed;
	float PullSpeed = FMath::Lerp(
		FMath::Max(0.0f, PlayerMagnet->MinPullSpeed),
		FMath::Max(0.0f, MaxPullSpeed),
		DistanceAlpha);
	if (TrackedItem.bLocked)
	{
		PullSpeed = FMath::Max(PullSpeed, MaxPullSpeed);
	}

	PullSpeed *= FMath::Max(0.05f, EffectiveStrength) * FMath::Max(0.05f, LocationSpeedScale);

	const FVector CurrentLocation = BackItemPrimitive->GetComponentLocation();
	const FVector TargetLocation = TargetBackItemTransform.GetLocation();
	const float SafeSnapDistance = FMath::Max(0.0f, FinalSnapDistance);
	const bool bWithinFinalSnapDistance = SafeSnapDistance > KINDA_SMALL_NUMBER
		&& FVector::DistSquared(CurrentLocation, TargetLocation) <= FMath::Square(SafeSnapDistance);
	const FVector NewLocation = FMath::VInterpConstantTo(
		CurrentLocation,
		TargetLocation,
		SafeDeltaTime,
		PullSpeed);

	const FQuat CurrentRotation = BackItemPrimitive->GetComponentQuat().GetNormalized();
	const FQuat TargetRotation = TargetBackItemTransform.GetRotation().GetNormalized();
	FQuat NewRotation = CurrentRotation;
	if (bAlignRotation)
	{
		const float RotationSpeed = FMath::Max(0.1f, PlayerMagnet->RotationInterpSpeed)
			* FMath::Max(0.15f, EffectiveStrength)
			* FMath::Max(0.05f, RotationSpeedScale);
		const float RotationAlpha = FMath::Clamp(SafeDeltaTime * RotationSpeed, 0.0f, 1.0f);
		NewRotation = FQuat::Slerp(CurrentRotation, TargetRotation, RotationAlpha).GetNormalized();
	}

	if (bWithinFinalSnapDistance)
	{
		BackItemPrimitive->SetWorldLocationAndRotation(
			TargetLocation,
			bAlignRotation ? TargetRotation : CurrentRotation,
			true,
			nullptr,
			ETeleportType::None);
		return;
	}

	BackItemPrimitive->SetWorldLocationAndRotation(
		NewLocation,
		NewRotation,
		true,
		nullptr,
		ETeleportType::None);
}

void UBackAttachmentComponent::ApplyMagnetCollisionOverrides(
	FTrackedMagnetItem& TrackedItem,
	const UPlayerMagnetComponent* PlayerMagnet,
	float CurrentWorldTime) const
{
	UPrimitiveComponent* BackItemPrimitive = TrackedItem.Primitive.Get();
	if (!BackItemPrimitive || !PlayerMagnet)
	{
		return;
	}

	if (!TrackedItem.bCollisionResponsesCaptured)
	{
		TrackedItem.CachedCameraResponse = BackItemPrimitive->GetCollisionResponseToChannel(ECC_Camera);
		TrackedItem.CachedPawnResponse = BackItemPrimitive->GetCollisionResponseToChannel(ECC_Pawn);
		TrackedItem.bCollisionResponsesCaptured = true;
	}

	const UBackpackMagnetComponent* BackItemMagnet = TrackedItem.BackpackMagnet.Get();
	const bool bWantsCameraIgnore = BackItemMagnet
		? BackItemMagnet->bIgnoreCameraCollisionWhenMagnetHeld
		: PlayerMagnet->bIgnoreCameraCollisionWhenMagnetHeld;
	const bool bIgnoreCamera = bWantsCameraIgnore || PlayerMagnet->CameraIgnoreDecayDuration > 0.0f;
	const bool bBlockPawn = BackItemMagnet
		? BackItemMagnet->bBlockPawnCollisionWhenMagnetHeld
		: PlayerMagnet->bBlockPawnCollisionWhenMagnetHeld;

	TrackedItem.CameraIgnoreUntilTime = FMath::Max(
		TrackedItem.CameraIgnoreUntilTime,
		CurrentWorldTime + FMath::Max(0.0f, PlayerMagnet->CameraIgnoreDecayDuration));

	BackItemPrimitive->SetCollisionResponseToChannel(ECC_Camera, bIgnoreCamera ? ECR_Ignore : ECR_Block);
	BackItemPrimitive->SetCollisionResponseToChannel(ECC_Pawn, bBlockPawn ? ECR_Block : ECR_Ignore);
}

void UBackAttachmentComponent::RestoreMagnetCollisionOverrides(FTrackedMagnetItem& TrackedItem, float CurrentWorldTime) const
{
	UPrimitiveComponent* BackItemPrimitive = TrackedItem.Primitive.Get();
	if (!BackItemPrimitive || !TrackedItem.bCollisionResponsesCaptured)
	{
		TrackedItem.bCollisionResponsesCaptured = false;
		TrackedItem.CameraIgnoreUntilTime = 0.0f;
		return;
	}

	const bool bKeepCameraIgnored = CurrentWorldTime >= 0.0f && CurrentWorldTime < TrackedItem.CameraIgnoreUntilTime;
	const ECollisionResponse CameraResponse = bKeepCameraIgnored
		? ECR_Ignore
		: static_cast<ECollisionResponse>(TrackedItem.CachedCameraResponse);
	const ECollisionResponse PawnResponse = static_cast<ECollisionResponse>(TrackedItem.CachedPawnResponse);
	BackItemPrimitive->SetCollisionResponseToChannel(
		ECC_Camera,
		CameraResponse);
	BackItemPrimitive->SetCollisionResponseToChannel(ECC_Pawn, PawnResponse);

	if (!bKeepCameraIgnored)
	{
		TrackedItem.bCollisionResponsesCaptured = false;
		TrackedItem.CameraIgnoreUntilTime = 0.0f;
	}
}

void UBackAttachmentComponent::SetCapsuleBackItemCollisionIgnored(UPrimitiveComponent* BackItemPrimitive, bool bIgnore)
{
	if (!bIgnoreOwnerCapsuleCollisionWhileRecalling || !BackItemPrimitive)
	{
		return;
	}

	ACharacter* OwnerCharacter = ResolveOwnerCharacter();
	if (!OwnerCharacter)
	{
		return;
	}

	UCapsuleComponent* CapsuleComponent = OwnerCharacter->GetCapsuleComponent();
	if (!CapsuleComponent)
	{
		return;
	}

	if (bIgnore)
	{
		if (CapsuleIgnoredBackItemPrimitives.Contains(BackItemPrimitive))
		{
			return;
		}

		CapsuleComponent->IgnoreComponentWhenMoving(BackItemPrimitive, true);
		BackItemPrimitive->IgnoreComponentWhenMoving(CapsuleComponent, true);
		CapsuleIgnoredBackItemPrimitives.Add(BackItemPrimitive);

		if (!bCapsulePhysicsBodyResponseOverridden)
		{
			CachedCapsulePhysicsBodyResponse = CapsuleComponent->GetCollisionResponseToChannel(ECC_PhysicsBody);
			bCapsulePhysicsBodyResponseOverridden = true;
		}

		CapsuleComponent->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
		return;
	}

	if (!CapsuleIgnoredBackItemPrimitives.Contains(BackItemPrimitive))
	{
		return;
	}

	CapsuleComponent->IgnoreComponentWhenMoving(BackItemPrimitive, false);
	BackItemPrimitive->IgnoreComponentWhenMoving(CapsuleComponent, false);
	CapsuleIgnoredBackItemPrimitives.Remove(BackItemPrimitive);

	if (bCapsulePhysicsBodyResponseOverridden && CapsuleIgnoredBackItemPrimitives.Num() == 0)
	{
		CapsuleComponent->SetCollisionResponseToChannel(ECC_PhysicsBody, CachedCapsulePhysicsBodyResponse);
		bCapsulePhysicsBodyResponseOverridden = false;
	}
}

void UBackAttachmentComponent::ClearCapsuleBackItemCollisionIgnore()
{
	ACharacter* OwnerCharacter = ResolveOwnerCharacter();
	UCapsuleComponent* CapsuleComponent = OwnerCharacter ? OwnerCharacter->GetCapsuleComponent() : nullptr;

	for (const TWeakObjectPtr<UPrimitiveComponent>& PrimitivePtr : CapsuleIgnoredBackItemPrimitives)
	{
		UPrimitiveComponent* BackItemPrimitive = PrimitivePtr.Get();
		if (!BackItemPrimitive || !CapsuleComponent)
		{
			continue;
		}

		CapsuleComponent->IgnoreComponentWhenMoving(BackItemPrimitive, false);
		BackItemPrimitive->IgnoreComponentWhenMoving(CapsuleComponent, false);
	}

	CapsuleIgnoredBackItemPrimitives.Reset();

	if (CapsuleComponent && bCapsulePhysicsBodyResponseOverridden)
	{
		CapsuleComponent->SetCollisionResponseToChannel(ECC_PhysicsBody, CachedCapsulePhysicsBodyResponse);
	}

	bCapsulePhysicsBodyResponseOverridden = false;
}

void UBackAttachmentComponent::DrawAttachDebug() const
{
	const UPlayerMagnetComponent* PlayerMagnet = ResolvePlayerMagnetComponent();
	USceneComponent* MagnetAnchorComponent = nullptr;
	FName MagnetAnchorSocket = NAME_None;
	if (!PlayerMagnet
		|| !const_cast<UBackAttachmentComponent*>(this)->ResolveMagLockAnchor(MagnetAnchorComponent, MagnetAnchorSocket)
		|| !MagnetAnchorComponent)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector MagnetLocation = MagnetAnchorSocket.IsNone()
		? MagnetAnchorComponent->GetComponentLocation()
		: MagnetAnchorComponent->GetSocketLocation(MagnetAnchorSocket);
	DrawDebugSphere(
		World,
		MagnetLocation,
		FMath::Max(1.0f, PlayerMagnet->AttractRange),
		24,
		FColor::Cyan,
		false,
		0.0f,
		0,
		1.0f);
	DrawDebugCoordinateSystem(
		World,
		MagnetLocation,
		MagnetAnchorComponent->GetComponentRotation(),
		FMath::Max(1.0f, AttachDebugAxisLength),
		false,
		0.0f,
		0,
		1.0f);

	if (UShapeComponent* MagLockZone = const_cast<UBackAttachmentComponent*>(this)->ResolveMagLockZone())
	{
		if (const USphereComponent* SphereComponent = Cast<USphereComponent>(MagLockZone))
		{
			DrawDebugSphere(
				World,
				SphereComponent->GetComponentLocation(),
				SphereComponent->GetScaledSphereRadius(),
				16,
				FColor::Green,
				false,
				0.0f,
				0,
				1.0f);
		}
		else if (const UBoxComponent* BoxComponent = Cast<UBoxComponent>(MagLockZone))
		{
			DrawDebugBox(
				World,
				BoxComponent->GetComponentLocation(),
				BoxComponent->GetScaledBoxExtent(),
				BoxComponent->GetComponentQuat(),
				FColor::Green,
				false,
				0.0f,
				0,
				1.0f);
		}
	}

	for (const TPair<FObjectKey, FTrackedMagnetItem>& Pair : TrackedItems)
	{
		const FTrackedMagnetItem& Item = Pair.Value;
		const AActor* ItemActor = Item.Actor.Get();
		if (!ItemActor)
		{
			continue;
		}

		const FVector ItemLocation = ItemActor->GetActorLocation();
		DrawDebugLine(
			World,
			ItemLocation,
			MagnetLocation,
			Item.bLocked ? FColor::Yellow : FColor::Blue,
			false,
			0.0f,
			0,
			0.8f);
	}
}
