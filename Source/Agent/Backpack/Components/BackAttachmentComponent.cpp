// Copyright Epic Games, Inc. All Rights Reserved.

#include "Backpack/Components/BackAttachmentComponent.h"
#include "Backpack/Actors/BlackHoleBackpackActor.h"
#include "Components/ChildActorComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"

UBackAttachmentComponent::UBackAttachmentComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	BackItemClass = ABlackHoleBackpackActor::StaticClass();
}

void UBackAttachmentComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UBackAttachmentComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateMagnetRecall(DeltaTime);

	if (bShowAttachDebug)
	{
		DrawAttachDebug();
	}
}

ABlackHoleBackpackActor* UBackAttachmentComponent::EnsureBackItem()
{
	if (!IsValid(BackItemActor.Get()))
	{
		BackItemActor = nullptr;
	}

	return BackItemActor.Get();
}

ABlackHoleBackpackActor* UBackAttachmentComponent::FindNearestBackpackInRange(float RangeOverrideCm) const
{
	const ACharacter* OwnerCharacter = ResolveOwnerCharacter();
	if (!OwnerCharacter)
	{
		return nullptr;
	}

	return FindWorldBackItemCandidate(OwnerCharacter, RangeOverrideCm);
}

bool UBackAttachmentComponent::EquipNearestBackpack(float RangeOverrideCm)
{
	ABlackHoleBackpackActor* CandidateBackpack = FindNearestBackpackInRange(RangeOverrideCm);
	return EquipBackpack(CandidateBackpack, bUseMagnetRecall);
}

bool UBackAttachmentComponent::EquipBackpack(ABlackHoleBackpackActor* InBackpack, bool bUseMagnetLerp)
{
	if (!CanEquipBackItem(InBackpack))
	{
		return false;
	}

	ABlackHoleBackpackActor* CurrentBackpack = BackItemActor.Get();
	if (CurrentBackpack && CurrentBackpack != InBackpack && !CurrentBackpack->IsItemDeployed())
	{
		CurrentBackpack->DeployToWorld(BuildDropImpulse());
	}

	StopMagnetRecall(false);
	BackItemActor = InBackpack;

	USceneComponent* AttachParent = nullptr;
	FName AttachSocket = NAME_None;
	if (!ResolveAttachTarget(AttachParent, AttachSocket))
	{
		return false;
	}

	if (bUseMagnetLerp)
	{
		if (BackItemActor->IsItemDeployed())
		{
			BackItemActor->SetDeployedState(false);
		}

		BackItemActor->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		bMagnetRecallActive = true;
		return true;
	}

	BackItemActor->AttachToCarrier(AttachParent, AttachSocket, BuildAttachFineTuneTransform());
	return true;
}

ABlackHoleBackpackActor* UBackAttachmentComponent::FindWorldBackItemCandidate(
	const ACharacter* OwnerCharacter,
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
	const float MaxClaimDistanceSq = EffectiveRangeCm > 0.0f
		? FMath::Square(EffectiveRangeCm)
		: MAX_flt;

	ABlackHoleBackpackActor* BestCandidate = nullptr;
	float BestDistanceSq = MAX_flt;
	for (TActorIterator<ABlackHoleBackpackActor> It(World); It; ++It)
	{
		ABlackHoleBackpackActor* CandidateBackItem = *It;
		if (!CandidateBackItem || CandidateBackItem == BackItemActor)
		{
			continue;
		}

		if (BackItemClass && !CandidateBackItem->IsA(BackItemClass))
		{
			continue;
		}

		const AActor* CandidateAttachParent = CandidateBackItem->GetAttachParentActor();
		if (CandidateAttachParent && CandidateAttachParent != OwnerCharacter)
		{
			continue;
		}

		if (!CanEquipBackItem(CandidateBackItem, EffectiveRangeCm))
		{
			continue;
		}

		const float CandidateDistanceSq = FVector::DistSquared(
			CandidateBackItem->GetActorLocation(),
			OwnerCharacter->GetActorLocation());
		if (CandidateDistanceSq > MaxClaimDistanceSq)
		{
			continue;
		}

		if (!BestCandidate || CandidateDistanceSq < BestDistanceSq)
		{
			BestCandidate = CandidateBackItem;
			BestDistanceSq = CandidateDistanceSq;
		}
	}

	return BestCandidate;
}

void UBackAttachmentComponent::SetBackItemActor(ABlackHoleBackpackActor* InBackItemActor, bool bAttachImmediately)
{
	if (!InBackItemActor)
	{
		return;
	}

	if (bAttachImmediately)
	{
		EquipBackpack(InBackItemActor, bUseMagnetRecall);
		return;
	}

	StopMagnetRecall(false);
	BackItemActor = InBackItemActor;
}

void UBackAttachmentComponent::ClearBackItemActor(bool bDestroyActor)
{
	StopMagnetRecall(false);

	ABlackHoleBackpackActor* ExistingBackItem = BackItemActor.Get();
	BackItemActor = nullptr;

	if (bDestroyActor && IsValid(ExistingBackItem))
	{
		ExistingBackItem->Destroy();
	}
}

bool UBackAttachmentComponent::ToggleBackItemDeployment()
{
	ABlackHoleBackpackActor* BackItem = EnsureBackItem();
	if (BackItem)
	{
		if (!BackItem->IsItemDeployed())
		{
			return DeployBackItem();
		}

		if (EquipBackpack(BackItem, bUseMagnetRecall))
		{
			return true;
		}
	}

	return EquipNearestBackpack();
}

bool UBackAttachmentComponent::DeployBackItem()
{
	ABlackHoleBackpackActor* BackItem = EnsureBackItem();
	if (!BackItem)
	{
		return false;
	}

	StopMagnetRecall(false);

	if (BackItem->IsItemDeployed())
	{
		BackItemActor = nullptr;
		return false;
	}

	BackItem->DeployToWorld(BuildDropImpulse());
	BackItemActor = nullptr;
	return true;
}

bool UBackAttachmentComponent::RecallBackItem()
{
	ABlackHoleBackpackActor* BackItem = EnsureBackItem();
	if (!BackItem)
	{
		return EquipNearestBackpack();
	}

	return EquipBackpack(BackItem, bUseMagnetRecall);
}

void UBackAttachmentComponent::RefreshBackItemAttachment()
{
	StopMagnetRecall(false);

	ABlackHoleBackpackActor* BackItem = BackItemActor.Get();
	USceneComponent* AttachParent = nullptr;
	FName AttachSocket = NAME_None;
	if (!BackItem || !ResolveAttachTarget(AttachParent, AttachSocket) || BackItem->IsItemDeployed())
	{
		return;
	}

	BackItem->AttachToCarrier(AttachParent, AttachSocket, BuildAttachFineTuneTransform());
}

bool UBackAttachmentComponent::IsBackItemDeployed() const
{
	return BackItemActor && BackItemActor->IsItemDeployed();
}

void UBackAttachmentComponent::SetAttachFineTune(const FVector& NewLocationOffset, const FRotator& NewRotationOffset)
{
	AttachLocationOffset = NewLocationOffset;
	AttachRotationOffset = NewRotationOffset;
	RefreshBackItemAttachment();
}

ACharacter* UBackAttachmentComponent::ResolveOwnerCharacter() const
{
	return Cast<ACharacter>(GetOwner());
}

USkeletalMeshComponent* UBackAttachmentComponent::ResolveCarrierMesh() const
{
	if (const ACharacter* OwnerCharacter = ResolveOwnerCharacter())
	{
		return OwnerCharacter->GetMesh();
	}

	return nullptr;
}

bool UBackAttachmentComponent::ResolveAttachTarget(USceneComponent*& OutAttachParent, FName& OutAttachSocketName) const
{
	OutAttachParent = nullptr;
	OutAttachSocketName = NAME_None;

	if (USceneComponent* MagnetComponent = ResolveMagnetAttachComponent())
	{
		OutAttachParent = MagnetComponent;
		return true;
	}

	if (bPreferAttachMagnet && bRequireAttachMagnet)
	{
		return false;
	}

	if (USkeletalMeshComponent* CarrierMesh = ResolveCarrierMesh())
	{
		OutAttachParent = CarrierMesh;
		OutAttachSocketName = AttachSocketName;
		return true;
	}

	if (ACharacter* OwnerCharacter = ResolveOwnerCharacter())
	{
		if (USceneComponent* RootComponent = OwnerCharacter->GetRootComponent())
		{
			OutAttachParent = RootComponent;
			return true;
		}
	}

	return false;
}

USceneComponent* UBackAttachmentComponent::ResolveMagnetAttachComponent() const
{
	if (!bPreferAttachMagnet)
	{
		return nullptr;
	}

	if (USceneComponent* CachedMagnetComponent = CachedAttachMagnetComponent.Get())
	{
		if (CachedMagnetComponent->GetOwner() == GetOwner())
		{
			return CachedMagnetComponent;
		}

		CachedAttachMagnetComponent = nullptr;
	}

	ACharacter* OwnerCharacter = ResolveOwnerCharacter();
	if (!OwnerCharacter)
	{
		return nullptr;
	}

	TArray<USceneComponent*> SceneComponents;
	OwnerCharacter->GetComponents(SceneComponents);
	for (USceneComponent* SceneComponent : SceneComponents)
	{
		if (!SceneComponent || SceneComponent == OwnerCharacter->GetRootComponent())
		{
			continue;
		}

		const bool bTagMatch = !AttachMagnetTag.IsNone() && SceneComponent->ComponentHasTag(AttachMagnetTag);
		const bool bNameMatch = !AttachMagnetNameHint.IsEmpty()
			&& SceneComponent->GetName().Contains(AttachMagnetNameHint, ESearchCase::IgnoreCase);
		if (!bTagMatch && !bNameMatch)
		{
			continue;
		}

		CachedAttachMagnetComponent = SceneComponent;
		return SceneComponent;
	}

	TArray<UChildActorComponent*> ChildActorComponents;
	OwnerCharacter->GetComponents(ChildActorComponents);
	for (UChildActorComponent* ChildActorComponent : ChildActorComponents)
	{
		if (!ChildActorComponent)
		{
			continue;
		}

		AActor* ChildActor = ChildActorComponent->GetChildActor();
		if (!ChildActor)
		{
			continue;
		}

		const bool bComponentTagMatch = !AttachMagnetTag.IsNone() && ChildActorComponent->ComponentHasTag(AttachMagnetTag);
		const bool bComponentNameMatch = !AttachMagnetNameHint.IsEmpty()
			&& ChildActorComponent->GetName().Contains(AttachMagnetNameHint, ESearchCase::IgnoreCase);
		if (!bComponentTagMatch && !bComponentNameMatch && !DoesActorMatchMagnetHint(ChildActor))
		{
			continue;
		}

		if (USceneComponent* ChildRootComponent = ChildActor->GetRootComponent())
		{
			CachedAttachMagnetComponent = ChildRootComponent;
			return ChildRootComponent;
		}
	}

	TArray<AActor*> AttachedActors;
	OwnerCharacter->GetAttachedActors(AttachedActors, true);
	for (AActor* AttachedActor : AttachedActors)
	{
		if (!DoesActorMatchMagnetHint(AttachedActor))
		{
			continue;
		}

		if (USceneComponent* AttachedRootComponent = AttachedActor->GetRootComponent())
		{
			CachedAttachMagnetComponent = AttachedRootComponent;
			return AttachedRootComponent;
		}
	}

	return nullptr;
}

bool UBackAttachmentComponent::DoesActorMatchMagnetHint(const AActor* CandidateActor) const
{
	if (!CandidateActor)
	{
		return false;
	}

	if (!AttachMagnetTag.IsNone() && CandidateActor->ActorHasTag(AttachMagnetTag))
	{
		return true;
	}

	if (AttachMagnetNameHint.IsEmpty())
	{
		return false;
	}

	return CandidateActor->GetName().Contains(AttachMagnetNameHint, ESearchCase::IgnoreCase)
		|| CandidateActor->GetClass()->GetName().Contains(AttachMagnetNameHint, ESearchCase::IgnoreCase);
}

FTransform UBackAttachmentComponent::BuildAttachFineTuneTransform() const
{
	return FTransform(AttachRotationOffset, AttachLocationOffset, AttachScale.GetAbs());
}

FTransform UBackAttachmentComponent::BuildTargetBackItemWorldTransform(
	const ABlackHoleBackpackActor* BackItem,
	const USceneComponent* AttachParent,
	FName AttachSocketNameValue) const
{
	if (!BackItem || !AttachParent)
	{
		return FTransform::Identity;
	}

	const FTransform SocketWorldTransform = AttachSocketNameValue.IsNone()
		? AttachParent->GetComponentTransform()
		: AttachParent->GetSocketTransform(AttachSocketNameValue, RTS_World);
	FTransform FineTuneTransform = BuildAttachFineTuneTransform();
	FineTuneTransform.SetScale3D(FVector::OneVector);
	const FTransform TargetSnapAnchorWorld = FineTuneTransform * SocketWorldTransform;
	const FTransform CurrentRootWorldTransform = BackItem->GetActorTransform();
	const FTransform CurrentSnapAnchorWorldTransform = BackItem->GetSnapAnchorWorldTransform();
	const FTransform SnapAnchorToRootTransform = CurrentSnapAnchorWorldTransform.GetRelativeTransform(CurrentRootWorldTransform);
	return SnapAnchorToRootTransform.Inverse() * TargetSnapAnchorWorld;
}

FVector UBackAttachmentComponent::BuildDropImpulse() const
{
	const ACharacter* OwnerCharacter = ResolveOwnerCharacter();
	if (!OwnerCharacter)
	{
		return FVector::ZeroVector;
	}

	const FVector Forward = OwnerCharacter->GetActorForwardVector() * DropForwardImpulse;
	const FVector Right = OwnerCharacter->GetActorRightVector() * DropRightImpulse;
	const FVector Up = FVector::UpVector * DropUpImpulse;
	return Forward + Right + Up;
}

bool UBackAttachmentComponent::CanEquipBackItem(const ABlackHoleBackpackActor* CandidateItem, float MaxRangeCm) const
{
	const ACharacter* OwnerCharacter = ResolveOwnerCharacter();
	if (!OwnerCharacter || !CandidateItem)
	{
		return false;
	}

	const AActor* CandidateAttachParent = CandidateItem->GetAttachParentActor();
	if (CandidateAttachParent && CandidateAttachParent != OwnerCharacter)
	{
		return false;
	}

	const float EffectiveRangeCm = MaxRangeCm >= 0.0f ? MaxRangeCm : RecallMaxDistance;
	if (EffectiveRangeCm > 0.0f)
	{
		const float DistanceToBackItem = FVector::Dist(OwnerCharacter->GetActorLocation(), CandidateItem->GetActorLocation());
		if (DistanceToBackItem > EffectiveRangeCm)
		{
			return false;
		}
	}

	if (CandidateAttachParent == OwnerCharacter)
	{
		return true;
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
	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BackItemRecallTrace), false, OwnerCharacter);
	QueryParams.AddIgnoredActor(OwnerCharacter);
	QueryParams.AddIgnoredActor(CandidateItem);
	const bool bBlocked = World->LineTraceSingleByChannel(
		HitResult,
		OwnerCharacter->GetPawnViewLocation(),
		CandidateItem->GetActorLocation(),
		ECC_Visibility,
		QueryParams);
	return !bBlocked;
}

void UBackAttachmentComponent::UpdateMagnetRecall(float DeltaTime)
{
	if (!bMagnetRecallActive)
	{
		return;
	}

	ABlackHoleBackpackActor* BackItem = BackItemActor.Get();
	USceneComponent* AttachParent = nullptr;
	FName AttachSocket = NAME_None;
	if (!BackItem || !ResolveAttachTarget(AttachParent, AttachSocket) || DeltaTime <= 0.0f)
	{
		StopMagnetRecall(false);
		return;
	}

	const FTransform CurrentWorldTransform = BackItem->GetActorTransform();
	const FTransform TargetWorldTransform = BuildTargetBackItemWorldTransform(BackItem, AttachParent, AttachSocket);

	const FVector NewLocation = FMath::VInterpTo(
		CurrentWorldTransform.GetLocation(),
		TargetWorldTransform.GetLocation(),
		DeltaTime,
		FMath::Max(0.01f, MagnetMoveInterpSpeed));
	const FRotator NewRotation = FMath::RInterpTo(
		CurrentWorldTransform.Rotator(),
		TargetWorldTransform.Rotator(),
		DeltaTime,
		FMath::Max(0.01f, MagnetRotationInterpSpeed));

	BackItem->SetActorLocationAndRotation(NewLocation, NewRotation, false, nullptr, ETeleportType::TeleportPhysics);

	const float DistanceToTarget = FVector::Distance(NewLocation, TargetWorldTransform.GetLocation());
	const float AngleToTargetDeg = FMath::RadiansToDegrees(NewRotation.Quaternion().AngularDistance(TargetWorldTransform.GetRotation()));
	if (DistanceToTarget <= FMath::Max(0.1f, MagnetAttachDistance)
		&& AngleToTargetDeg <= FMath::Max(0.1f, MagnetAttachAngleDegrees))
	{
		StopMagnetRecall(true);
	}
}

void UBackAttachmentComponent::StopMagnetRecall(bool bSnapAttach)
{
	if (!bMagnetRecallActive)
	{
		return;
	}

	bMagnetRecallActive = false;

	if (!bSnapAttach)
	{
		return;
	}

	ABlackHoleBackpackActor* BackItem = BackItemActor.Get();
	USceneComponent* AttachParent = nullptr;
	FName AttachSocket = NAME_None;
	if (!BackItem || !ResolveAttachTarget(AttachParent, AttachSocket))
	{
		return;
	}

	BackItem->AttachToCarrier(AttachParent, AttachSocket, BuildAttachFineTuneTransform());
}

void UBackAttachmentComponent::DrawAttachDebug() const
{
	const ACharacter* OwnerCharacter = ResolveOwnerCharacter();
	const ABlackHoleBackpackActor* BackItem = BackItemActor.Get();
	USceneComponent* AttachParent = nullptr;
	FName AttachSocket = NAME_None;
	if (!OwnerCharacter || !BackItem || !ResolveAttachTarget(AttachParent, AttachSocket))
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FTransform AttachTargetTransform = AttachSocket.IsNone()
		? AttachParent->GetComponentTransform()
		: AttachParent->GetSocketTransform(AttachSocket, RTS_World);
	const FVector SocketLocation = AttachTargetTransform.GetLocation();
	const FRotator SocketRotation = AttachTargetTransform.Rotator();
	const FTransform SnapAnchorTransform = BackItem->GetSnapAnchorWorldTransform();
	const FVector SnapAnchorLocation = BackItem->GetSnapAnchorWorldTransform().GetLocation();
	const FColor SocketColor = FColor::Green;
	const FColor AnchorColor = FColor::Yellow;
	const float AxisLength = FMath::Max(1.0f, AttachDebugAxisLength);

	DrawDebugSphere(World, SocketLocation, 6.0f, 12, SocketColor, false, 0.0f, 0, 1.5f);
	DrawDebugSphere(World, SnapAnchorLocation, 6.0f, 12, AnchorColor, false, 0.0f, 0, 1.5f);
	DrawDebugLine(World, SocketLocation, SnapAnchorLocation, FColor::Cyan, false, 0.0f, 0, 1.0f);
	DrawDebugCoordinateSystem(World, SocketLocation, SocketRotation, AxisLength, false, 0.0f, 0, 1.0f);
	DrawDebugCoordinateSystem(World, SnapAnchorTransform.GetLocation(), SnapAnchorTransform.Rotator(), AxisLength, false, 0.0f, 0, 1.0f);

	if (bMagnetRecallActive)
	{
		const FTransform TargetWorldTransform = BuildTargetBackItemWorldTransform(BackItem, AttachParent, AttachSocket);
		DrawDebugSphere(World, TargetWorldTransform.GetLocation(), 5.0f, 10, FColor::Magenta, false, 0.0f, 0, 1.5f);
		DrawDebugCoordinateSystem(World, TargetWorldTransform.GetLocation(), TargetWorldTransform.Rotator(), AxisLength, false, 0.0f, 0, 1.0f);
		DrawDebugLine(World, SnapAnchorLocation, TargetWorldTransform.GetLocation(), FColor::Magenta, false, 0.0f, 0, 1.0f);
	}
}
