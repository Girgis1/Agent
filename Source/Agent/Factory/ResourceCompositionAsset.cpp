// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/ResourceCompositionAsset.h"

bool UResourceCompositionAsset::HasDefinedResources() const
{
	for (const FResourceCompositionEntry& Entry : Entries)
	{
		if (Entry.IsDefined())
		{
			return true;
		}
	}

	return false;
}

float UResourceCompositionAsset::GetTotalMassFraction() const
{
	float TotalMassFraction = 0.0f;
	for (const FResourceCompositionEntry& Entry : Entries)
	{
		if (Entry.IsDefined())
		{
			TotalMassFraction += FMath::Max(0.0f, Entry.MassFraction);
		}
	}

	return TotalMassFraction;
}
