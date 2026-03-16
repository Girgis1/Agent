// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dirty/DirtTextureUtilities.h"

#include "Engine/Texture2D.h"

namespace
{
	struct FBrushTextureCache
	{
		int32 Width = 0;
		int32 Height = 0;
		TArray<FColor> Pixels;
	};

	TMap<TWeakObjectPtr<const UTexture2D>, FBrushTextureCache> GBrushTextureCache;
}

float AgentDirtTextureUtilities::SampleBrushAlpha(const UTexture2D* BrushTexture, float U, float V)
{
	if (!BrushTexture)
	{
		return 1.0f;
	}

	const TWeakObjectPtr<const UTexture2D> TextureKey(BrushTexture);
	FBrushTextureCache* Cache = GBrushTextureCache.Find(TextureKey);
	if (!Cache)
	{
		FBrushTextureCache NewCache;
		const FTexturePlatformData* PlatformData = BrushTexture->GetPlatformData();
		if (PlatformData && PlatformData->Mips.Num() > 0 && (PlatformData->PixelFormat == PF_B8G8R8A8 || PlatformData->PixelFormat == PF_R8G8B8A8))
		{
			const FTexture2DMipMap& Mip = PlatformData->Mips[0];
			const FColor* PixelData = static_cast<const FColor*>(Mip.BulkData.LockReadOnly());
			if (PixelData)
			{
				NewCache.Width = Mip.SizeX;
				NewCache.Height = Mip.SizeY;
				NewCache.Pixels.SetNumUninitialized(NewCache.Width * NewCache.Height);
				FMemory::Memcpy(NewCache.Pixels.GetData(), PixelData, NewCache.Pixels.Num() * sizeof(FColor));
			}
			Mip.BulkData.Unlock();
		}

		Cache = &GBrushTextureCache.Add(TextureKey, MoveTemp(NewCache));
	}

	if (!Cache || Cache->Width <= 0 || Cache->Height <= 0 || Cache->Pixels.IsEmpty())
	{
		return 1.0f;
	}

	const int32 X = FMath::Clamp(FMath::RoundToInt(U * static_cast<float>(Cache->Width - 1)), 0, Cache->Width - 1);
	const int32 Y = FMath::Clamp(FMath::RoundToInt(V * static_cast<float>(Cache->Height - 1)), 0, Cache->Height - 1);
	const FColor& Pixel = Cache->Pixels[(Y * Cache->Width) + X];
	const float Alpha = static_cast<float>(Pixel.A) / 255.0f;
	const float Luminance = (0.2126f * static_cast<float>(Pixel.R) + 0.7152f * static_cast<float>(Pixel.G) + 0.0722f * static_cast<float>(Pixel.B)) / 255.0f;
	return FMath::Clamp(FMath::Max(Alpha, Luminance), 0.0f, 1.0f);
}

bool AgentDirtTextureUtilities::UploadTexturePixels(UTexture2D* Texture, const TArray<FColor>& Pixels)
{
	if (!Texture || Pixels.IsEmpty())
	{
		return false;
	}

	const int32 Width = Texture->GetSizeX();
	const int32 Height = Texture->GetSizeY();
	if (Width <= 0 || Height <= 0 || Pixels.Num() != (Width * Height))
	{
		return false;
	}

	FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(0, 0, 0, 0, Width, Height);
	const SIZE_T DataSizeBytes = static_cast<SIZE_T>(Pixels.Num()) * sizeof(FColor);
	uint8* UploadData = static_cast<uint8*>(FMemory::Malloc(DataSizeBytes));
	FMemory::Memcpy(UploadData, Pixels.GetData(), DataSizeBytes);
	Texture->UpdateTextureRegions(
		0,
		1,
		Region,
		static_cast<uint32>(Width * sizeof(FColor)),
		sizeof(FColor),
		UploadData,
		[Region](uint8* SourceData, const FUpdateTextureRegion2D*)
		{
			FMemory::Free(SourceData);
			delete Region;
		});

	return true;
}
