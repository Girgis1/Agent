// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/ResourceComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Factory/ResourceCompositionAsset.h"
#include "Factory/ResourceDefinitionAsset.h"
#include "Factory/ResourceTypes.h"

UResourceComponent::UResourceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UResourceComponent::HasDefinedResources() const
{
	return (Composition && Composition->HasDefinedResources()) || HasSimpleResourceDefinition();
}

bool UResourceComponent::HasSimpleResourceDefinition() const
{
	return PrimaryResource != nullptr
		&& !GetPrimaryResourceId().IsNone()
		&& PrimaryResourceUnitsPerKg > 0.0f;
}

FName UResourceComponent::GetPrimaryResourceId() const
{
	return PrimaryResource ? PrimaryResource->GetResolvedResourceId() : NAME_None;
}

int32 UResourceComponent::ResolveSimpleResourceQuantityScaled(const UPrimitiveComponent* SourcePrimitive) const
{
	if (!HasSimpleResourceDefinition())
	{
		return 0;
	}

	const float EffectiveMassKg = ResolveEffectiveMassKg(SourcePrimitive);
	if (EffectiveMassKg <= 0.0f)
	{
		return 0;
	}

	const float QuantityUnits = EffectiveMassKg * FMath::Max(0.0f, PrimaryResourceUnitsPerKg) * ResolveRecoveryScalar();
	return AgentResource::UnitsToScaled(QuantityUnits);
}

float UResourceComponent::ResolveEffectiveMassKg(const UPrimitiveComponent* SourcePrimitive) const
{
	float EffectiveMassKg = bUsePhysicsMass && SourcePrimitive
		? SourcePrimitive->GetMass()
		: FMath::Max(0.0f, MassOverrideKg);

	EffectiveMassKg *= FMath::Max(0.0f, MassMultiplier);
	return FMath::Max(0.0f, EffectiveMassKg);
}

float UResourceComponent::ResolveRecoveryScalar() const
{
	float ResolvedRecovery = FMath::Max(0.0f, RecoveryMultiplier);
	if (Composition && Composition->HasDefinedResources())
	{
		ResolvedRecovery *= FMath::Max(0.0f, Composition->DefaultRecoveryScalar);
	}

	return ResolvedRecovery;
}
