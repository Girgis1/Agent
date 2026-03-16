// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DirtyTypes.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class EDirtBrushMode : uint8
{
	Clean UMETA(DisplayName="Clean"),
	Dirty UMETA(DisplayName="Dirty")
};

UENUM(BlueprintType)
enum class EDirtBrushApplicationType : uint8
{
	Hit UMETA(DisplayName="Hit"),
	Trail UMETA(DisplayName="Trail"),
	Area UMETA(DisplayName="Area")
};

USTRUCT(BlueprintType)
struct FDirtBrushStamp
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty")
	EDirtBrushMode Mode = EDirtBrushMode::Clean;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty")
	TObjectPtr<UTexture2D> BrushTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty", meta=(ClampMin="1.0", UIMin="1.0", Units="cm"))
	float BrushSizeCm = 36.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty", meta=(ClampMin="0.0", UIMin="0.0"))
	float BrushStrengthPerSecond = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float BrushHardness = 0.7f;

	float GetSignedStrength(float DeltaTime) const
	{
		const float Strength = FMath::Max(0.0f, BrushStrengthPerSecond) * FMath::Max(0.0f, DeltaTime);
		return Mode == EDirtBrushMode::Dirty ? Strength : -Strength;
	}
};
