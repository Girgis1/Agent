// Copyright Epic Games, Inc. All Rights Reserved.

#include "Backpack/Components/BackpackMagnetComponent.h"
#include "Components/SceneComponent.h"

UBackpackMagnetComponent::UBackpackMagnetComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

float UBackpackMagnetComponent::ComputeDistanceStrength(float DistanceToTarget, float MaxDistance) const
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

bool UBackpackMagnetComponent::ResolveAttachPoint(USceneComponent*& OutAttachPointComponent, FName& OutAttachSocketName) const
{
	OutAttachPointComponent = nullptr;
	OutAttachSocketName = NAME_None;

	const AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return false;
	}

	TArray<USceneComponent*> SceneComponents;
	OwnerActor->GetComponents<USceneComponent>(SceneComponents);
	for (USceneComponent* SceneComponent : SceneComponents)
	{
		if (!SceneComponent)
		{
			continue;
		}

		const bool bTagMatch = !AttachPointComponentTag.IsNone()
			&& SceneComponent->ComponentHasTag(AttachPointComponentTag);
		const bool bNameMatch = !AttachPointNameHint.IsEmpty()
			&& SceneComponent->GetName().Contains(AttachPointNameHint, ESearchCase::IgnoreCase);
		if (!bTagMatch && !bNameMatch)
		{
			continue;
		}

		OutAttachPointComponent = SceneComponent;
		OutAttachSocketName = AttachPointSocketName;
		return true;
	}

	if (USceneComponent* RootComponent = OwnerActor->GetRootComponent())
	{
		OutAttachPointComponent = RootComponent;
		OutAttachSocketName = NAME_None;
		return true;
	}

	return false;
}

FTransform UBackpackMagnetComponent::GetAttachPointWorldTransform() const
{
	USceneComponent* AttachPointComponent = nullptr;
	FName AttachSocketName = NAME_None;
	if (!ResolveAttachPoint(AttachPointComponent, AttachSocketName) || !AttachPointComponent)
	{
		if (const AActor* OwnerActor = GetOwner())
		{
			return OwnerActor->GetActorTransform();
		}

		return FTransform::Identity;
	}

	return AttachSocketName.IsNone()
		? AttachPointComponent->GetComponentTransform()
		: AttachPointComponent->GetSocketTransform(AttachSocketName, RTS_World);
}
