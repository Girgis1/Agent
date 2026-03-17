// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UTexture2D;

namespace AgentDirtTextureUtilities
{
	float SampleTextureWeight(const UTexture2D* Texture, float U, float V);
	float SampleBrushAlpha(const UTexture2D* BrushTexture, float U, float V);
	bool UploadTexturePixels(UTexture2D* Texture, const TArray<FColor>& Pixels);
}
