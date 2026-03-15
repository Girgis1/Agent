// Copyright Epic Games, Inc. All Rights Reserved.

#include "Objects/Assets/ObjectFractureDefinitionAsset.h"

bool FObjectFractureFragmentEntry::IsUsable() const
{
	return FragmentActorClass.Get() != nullptr || StaticMeshOverride != nullptr;
}

bool UObjectFractureDefinitionAsset::HasUsableFragments() const
{
	for (const FObjectFractureFragmentEntry& FragmentEntry : Fragments)
	{
		const bool bHasResolvedFragmentActor = FragmentEntry.FragmentActorClass.Get() != nullptr
			|| DefaultFragmentActorClass.Get() != nullptr;
		if (FragmentEntry.StaticMeshOverride != nullptr || bHasResolvedFragmentActor)
		{
			return true;
		}
	}

	return false;
}
