// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AgentScannerTypes.generated.h"

USTRUCT(BlueprintType)
struct FAgentScannerRowPresentation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner")
	FText Label;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner")
	FText Value;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner")
	FLinearColor Tint = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Scanner")
	bool bUseTint = false;
};

USTRUCT(BlueprintType)
struct FAgentScannerPresentation
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scanner")
	bool bHasTarget = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scanner")
	bool bScanComplete = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scanner")
	float ScanProgress01 = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scanner")
	FText Title;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scanner")
	FText Subtitle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scanner")
	TArray<FAgentScannerRowPresentation> Rows;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Scanner")
	FLinearColor AccentColor = FLinearColor(0.14f, 0.86f, 1.0f, 1.0f);
};
