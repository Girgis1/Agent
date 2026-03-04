// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ResourceTypes.generated.h"

class UResourceDefinitionAsset;

namespace AgentResource
{
	static constexpr int32 UnitsScale = 1000;

	inline int32 WholeUnitsToScaled(int32 QuantityUnits)
	{
		return FMath::Max(0, QuantityUnits) * UnitsScale;
	}

	inline int32 UnitsToScaled(float Quantity)
	{
		return FMath::Max(0, FMath::RoundToInt(FMath::Max(0.0f, Quantity) * static_cast<float>(UnitsScale)));
	}

	inline float ScaledToUnits(int32 QuantityScaled)
	{
		return static_cast<float>(FMath::Max(0, QuantityScaled)) / static_cast<float>(UnitsScale);
	}
}

USTRUCT(BlueprintType)
struct FResourceAmount
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Resource")
	FName ResourceId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Resource", meta=(ClampMin="0"))
	int32 QuantityScaled = 0;

	bool HasQuantity() const
	{
		return !ResourceId.IsNone() && QuantityScaled > 0;
	}

	int32 GetScaledQuantity() const
	{
		return FMath::Max(0, QuantityScaled);
	}

	void SetScaledQuantity(int32 InQuantityScaled)
	{
		QuantityScaled = FMath::Max(0, InQuantityScaled);
	}

	void SetWholeUnits(int32 QuantityUnits)
	{
		QuantityScaled = AgentResource::WholeUnitsToScaled(QuantityUnits);
	}

	float GetUnits() const
	{
		return AgentResource::ScaledToUnits(QuantityScaled);
	}
};

USTRUCT(BlueprintType)
struct FResourceCompositionEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Resource")
	TObjectPtr<UResourceDefinitionAsset> Resource = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Resource", meta=(ClampMin="0.0", UIMin="0.0"))
	float MassFraction = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Resource", meta=(ClampMin="0.0", UIMin="0.0"))
	float YieldScalar = 1.0f;

	bool IsDefined() const
	{
		return Resource != nullptr && MassFraction > KINDA_SMALL_NUMBER && YieldScalar > 0.0f;
	}
};
