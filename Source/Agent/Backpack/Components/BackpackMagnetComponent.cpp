// Copyright Epic Games, Inc. All Rights Reserved.

#include "Backpack/Components/BackpackMagnetComponent.h"

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

