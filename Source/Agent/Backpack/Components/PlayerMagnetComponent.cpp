// Copyright Epic Games, Inc. All Rights Reserved.

#include "Backpack/Components/PlayerMagnetComponent.h"
#include "Components/SceneComponent.h"
#include "Components/ShapeComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

UPlayerMagnetComponent::UPlayerMagnetComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	if (MagneticActorTags.Num() == 0)
	{
		MagneticActorTags.Add(TEXT("Magnetic"));
		MagneticActorTags.Add(TEXT("BackpackItem"));
	}

	if (MagneticComponentTags.Num() == 0)
	{
		MagneticComponentTags.Add(TEXT("Magnetic"));
	}

	if (MagneticMaterialIds.Num() == 0)
	{
		MagneticMaterialIds.Add(TEXT("Iron"));
	}
}

USkeletalMeshComponent* UPlayerMagnetComponent::ResolveOwnerMesh() const
{
	if (const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner()))
	{
		return OwnerCharacter->GetMesh();
	}

	return nullptr;
}

float UPlayerMagnetComponent::ComputeDistanceStrength(float DistanceToTarget, float MaxDistance) const
{
	const float SafeMaxDistance = FMath::Max(1.0f, MaxDistance);
	const float ProximityAlpha = 1.0f - FMath::Clamp(DistanceToTarget / SafeMaxDistance, 0.0f, 1.0f);

	float CurveAlpha = ProximityAlpha;
	if (const FRichCurve* Curve = StrengthByDistanceCurve.GetRichCurveConst())
	{
		if (Curve->GetNumKeys() > 0)
		{
			CurveAlpha = Curve->Eval(ProximityAlpha);
		}
	}

	CurveAlpha = FMath::Clamp(CurveAlpha, 0.0f, 1.0f);
	const float MinStrength = FMath::Clamp(StrengthAtMaxDistance, 0.0f, 1.0f);
	return FMath::Lerp(MinStrength, 1.0f, CurveAlpha);
}

void UPlayerMagnetComponent::GatherOwnerAndAttachedActors(TArray<const AActor*>& OutActors) const
{
	OutActors.Reset();

	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	OutActors.Add(OwnerActor);

	TArray<AActor*> AttachedActors;
	OwnerActor->GetAttachedActors(AttachedActors, true, true);
	for (const AActor* AttachedActor : AttachedActors)
	{
		if (AttachedActor)
		{
			OutActors.AddUnique(AttachedActor);
		}
	}
}

bool UPlayerMagnetComponent::ResolveTaggedSceneComponent(FName ComponentTag, USceneComponent*& OutSceneComponent) const
{
	OutSceneComponent = nullptr;

	if (ComponentTag.IsNone())
	{
		return false;
	}

	TArray<const AActor*> SearchActors;
	GatherOwnerAndAttachedActors(SearchActors);

	for (const AActor* CandidateActor : SearchActors)
	{
		if (!CandidateActor)
		{
			continue;
		}

		TArray<USceneComponent*> SceneComponents;
		CandidateActor->GetComponents<USceneComponent>(SceneComponents);
		for (USceneComponent* SceneComponent : SceneComponents)
		{
			if (!SceneComponent || !SceneComponent->ComponentHasTag(ComponentTag))
			{
				continue;
			}

			OutSceneComponent = SceneComponent;
			return true;
		}
	}

	return false;
}

bool UPlayerMagnetComponent::ResolveTaggedShapeComponent(FName ComponentTag, UShapeComponent*& OutShapeComponent) const
{
	OutShapeComponent = nullptr;

	if (ComponentTag.IsNone())
	{
		return false;
	}

	TArray<const AActor*> SearchActors;
	GatherOwnerAndAttachedActors(SearchActors);
	const FString TagString = ComponentTag.ToString();
	const bool bTagLooksMagnetRelated = TagString.Contains(TEXT("MagLock"), ESearchCase::IgnoreCase)
		|| TagString.Contains(TEXT("Magnet"), ESearchCase::IgnoreCase);

	for (const AActor* CandidateActor : SearchActors)
	{
		if (!CandidateActor)
		{
			continue;
		}

		TArray<UShapeComponent*> ShapeComponents;
		CandidateActor->GetComponents<UShapeComponent>(ShapeComponents);
		for (UShapeComponent* ShapeComponent : ShapeComponents)
		{
			if (!ShapeComponent || !ShapeComponent->ComponentHasTag(ComponentTag))
			{
				continue;
			}

			OutShapeComponent = ShapeComponent;
			return true;
		}

		if (CandidateActor->ActorHasTag(ComponentTag))
		{
			for (UShapeComponent* ShapeComponent : ShapeComponents)
			{
				if (!ShapeComponent || ShapeComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
				{
					continue;
				}

				OutShapeComponent = ShapeComponent;
				return true;
			}
		}

		for (UShapeComponent* ShapeComponent : ShapeComponents)
		{
			if (!ShapeComponent)
			{
				continue;
			}

			const FString ShapeName = ShapeComponent->GetName();
			const bool bNameMatchesTag = !TagString.IsEmpty() && ShapeName.Contains(TagString, ESearchCase::IgnoreCase);
			const bool bNameLooksMagnetRelated = bTagLooksMagnetRelated
				&& (ShapeName.Contains(TEXT("MagLock"), ESearchCase::IgnoreCase)
					|| ShapeName.Contains(TEXT("Magnet"), ESearchCase::IgnoreCase));
			if (bNameMatchesTag || bNameLooksMagnetRelated)
			{
				OutShapeComponent = ShapeComponent;
				return true;
			}
		}

		if (bTagLooksMagnetRelated)
		{
			const FString ActorName = CandidateActor->GetName();
			const bool bActorLooksMagnetRelated = ActorName.Contains(TEXT("MagLock"), ESearchCase::IgnoreCase)
				|| ActorName.Contains(TEXT("Magnet"), ESearchCase::IgnoreCase);
			if (bActorLooksMagnetRelated)
			{
				for (UShapeComponent* ShapeComponent : ShapeComponents)
				{
					if (!ShapeComponent || ShapeComponent->GetCollisionEnabled() == ECollisionEnabled::NoCollision)
					{
						continue;
					}

					OutShapeComponent = ShapeComponent;
					return true;
				}
			}
		}
	}

	return false;
}

bool UPlayerMagnetComponent::ResolveAttachTarget(USceneComponent*& OutAttachParent, FName& OutAttachSocketName) const
{
	OutAttachParent = nullptr;
	OutAttachSocketName = NAME_None;

	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return false;
	}

	USceneComponent* TaggedAttachComponent = nullptr;
	if (ResolveTaggedSceneComponent(AttachComponentTag, TaggedAttachComponent) && TaggedAttachComponent)
	{
		OutAttachParent = TaggedAttachComponent;
		OutAttachSocketName = NAME_None;
		return true;
	}

	if (!AttachActorTag.IsNone())
	{
		TArray<const AActor*> SearchActors;
		GatherOwnerAndAttachedActors(SearchActors);
		for (const AActor* CandidateActor : SearchActors)
		{
			if (!CandidateActor || CandidateActor == OwnerActor || !CandidateActor->ActorHasTag(AttachActorTag))
			{
				continue;
			}

			if (USceneComponent* RootScene = CandidateActor->GetRootComponent())
			{
				OutAttachParent = RootScene;
				OutAttachSocketName = NAME_None;
				return true;
			}
		}
	}

	if (!AttachActorNameHint.IsEmpty())
	{
		TArray<const AActor*> SearchActors;
		GatherOwnerAndAttachedActors(SearchActors);
		for (const AActor* CandidateActor : SearchActors)
		{
			if (!CandidateActor)
			{
				continue;
			}

			if (CandidateActor != OwnerActor)
			{
				const bool bActorNameMatch = CandidateActor->GetName().Contains(
					AttachActorNameHint,
					ESearchCase::IgnoreCase);
				const UClass* CandidateClass = CandidateActor->GetClass();
				const bool bClassNameMatch = CandidateClass
					&& CandidateClass->GetName().Contains(AttachActorNameHint, ESearchCase::IgnoreCase);
				if (bActorNameMatch || bClassNameMatch)
				{
					if (USceneComponent* RootScene = CandidateActor->GetRootComponent())
					{
						OutAttachParent = RootScene;
						OutAttachSocketName = NAME_None;
						return true;
					}
				}
			}

			TArray<USceneComponent*> SceneComponents;
			CandidateActor->GetComponents<USceneComponent>(SceneComponents);
			for (USceneComponent* SceneComponent : SceneComponents)
			{
				if (!SceneComponent)
				{
					continue;
				}

				if (!SceneComponent->GetName().Contains(AttachActorNameHint, ESearchCase::IgnoreCase))
				{
					continue;
				}

				OutAttachParent = SceneComponent;
				OutAttachSocketName = NAME_None;
				return true;
			}
		}
	}

	if (!bUseBackpackSocket)
	{
		if (USceneComponent* OwnerRoot = OwnerActor->GetRootComponent())
		{
			OutAttachParent = OwnerRoot;
			OutAttachSocketName = NAME_None;
			return true;
		}

		return false;
	}

	USkeletalMeshComponent* OwnerMesh = ResolveOwnerMesh();
	if (!OwnerMesh)
	{
		if (USceneComponent* OwnerRoot = OwnerActor->GetRootComponent())
		{
			OutAttachParent = OwnerRoot;
			OutAttachSocketName = NAME_None;
			return true;
		}

		return false;
	}

	if (BackpackSocketName != NAME_None && OwnerMesh->DoesSocketExist(BackpackSocketName))
	{
		OutAttachParent = OwnerMesh;
		OutAttachSocketName = BackpackSocketName;
		return true;
	}

	OutAttachParent = OwnerMesh;
	OutAttachSocketName = NAME_None;
	return true;
}

bool UPlayerMagnetComponent::ResolveMagLockAnchor(USceneComponent*& OutAnchorComponent, FName& OutAnchorSocketName) const
{
	OutAnchorComponent = nullptr;
	OutAnchorSocketName = NAME_None;

	USceneComponent* TaggedAnchorComponent = nullptr;
	if (ResolveTaggedSceneComponent(MagLockAnchorComponentTag, TaggedAnchorComponent) && TaggedAnchorComponent)
	{
		OutAnchorComponent = TaggedAnchorComponent;
		return true;
	}

	UShapeComponent* ZoneComponent = nullptr;
	if (ResolveTaggedShapeComponent(MagLockZoneComponentTag, ZoneComponent) && ZoneComponent)
	{
		OutAnchorComponent = ZoneComponent;
		OutAnchorSocketName = NAME_None;
		return true;
	}

	if (bUseAttachTargetAsMagLockAnchor && ResolveAttachTarget(OutAnchorComponent, OutAnchorSocketName))
	{
		return true;
	}

	if (const AActor* OwnerActor = GetOwner())
	{
		if (USceneComponent* OwnerRoot = OwnerActor->GetRootComponent())
		{
			OutAnchorComponent = OwnerRoot;
			OutAnchorSocketName = NAME_None;
			return true;
		}
	}

	return false;
}

UShapeComponent* UPlayerMagnetComponent::ResolveMagLockZoneComponent() const
{
	UShapeComponent* ZoneComponent = nullptr;
	if (ResolveTaggedShapeComponent(MagLockZoneComponentTag, ZoneComponent))
	{
		return ZoneComponent;
	}

	return nullptr;
}

bool UPlayerMagnetComponent::IsMaterialIdMagnetic(FName MaterialId) const
{
	if (MaterialId.IsNone())
	{
		return false;
	}

	const FString MaterialIdString = MaterialId.ToString();

	for (const FName& MagneticId : MagneticMaterialIds)
	{
		if (MagneticId.IsNone())
		{
			continue;
		}

		if (MagneticId.IsEqual(MaterialId, ENameCase::IgnoreCase))
		{
			return true;
		}

		const FString MagneticIdString = MagneticId.ToString();
		if (MaterialIdString.Contains(MagneticIdString, ESearchCase::IgnoreCase))
		{
			return true;
		}
	}

	return false;
}
