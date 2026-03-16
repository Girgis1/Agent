// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dirty/DirtySurfaceComponent.h"

#include "Dirty/DirtTextureUtilities.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "PhysicsEngine/PhysicsSettings.h"

namespace
{
	float GetComponentSurfaceExtentForAxis(const FVector& BoxExtent, const FVector& LocalNormal, bool bResolvePrimaryAxis)
	{
		const FVector AbsNormal = LocalNormal.GetAbs();
		if (AbsNormal.Z >= AbsNormal.X && AbsNormal.Z >= AbsNormal.Y)
		{
			return bResolvePrimaryAxis ? (BoxExtent.X * 2.0f) : (BoxExtent.Y * 2.0f);
		}

		if (AbsNormal.X >= AbsNormal.Y)
		{
			return bResolvePrimaryAxis ? (BoxExtent.Y * 2.0f) : (BoxExtent.Z * 2.0f);
		}

		return bResolvePrimaryAxis ? (BoxExtent.X * 2.0f) : (BoxExtent.Z * 2.0f);
	}
}

UDirtySurfaceComponent::UDirtySurfaceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDirtySurfaceComponent::BeginPlay()
{
	Super::BeginPlay();
	InitializeDirtySurface();
}

void UDirtySurfaceComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	DynamicMaterials.Reset();
	TargetPrimitive.Reset();
	DirtMaskTexture = nullptr;
	DirtMaskPixels.Reset();
}

void UDirtySurfaceComponent::InitializeDirtySurface()
{
	ResolveTargetPrimitive();
	CreateMaskTexture();
	BuildDynamicMaterials();
	FillMask(InitialDirtyness);
	RefreshVisualParameters();
}

bool UDirtySurfaceComponent::ApplyBrushHit(const FHitResult& Hit, const FDirtBrushStamp& Stamp, float DeltaTime)
{
	if (!bDirtySurfaceEnabled || !ResolveTargetPrimitive() || !DirtMaskTexture || DeltaTime <= 0.0f)
	{
		return false;
	}

	if (Hit.GetComponent() && Hit.GetComponent() != TargetPrimitive.Get())
	{
		return false;
	}

	FVector2D UV = FVector2D::ZeroVector;
	if (!ResolveBrushUV(Hit, UV))
	{
		return false;
	}

	float RadiusXPixels = 0.0f;
	float RadiusYPixels = 0.0f;
	ComputeBrushRadiiPixels(Hit, Stamp.BrushSizeCm, RadiusXPixels, RadiusYPixels);
	return ApplyBrushAtUV(UV, RadiusXPixels, RadiusYPixels, Stamp, DeltaTime);
}

void UDirtySurfaceComponent::SetDirtyness(float NewDirtyness, bool bResetMask)
{
	const float ClampedDirtyness = FMath::Clamp(NewDirtyness, 0.0f, 1.0f);
	InitialDirtyness = ClampedDirtyness;

	if (bResetMask)
	{
		FillMask(ClampedDirtyness);
	}
	else
	{
		CurrentDirtyness = ClampedDirtyness;
		RefreshVisualParameters();
	}
}

void UDirtySurfaceComponent::RefreshVisualParameters()
{
	if (DynamicMaterials.IsEmpty())
	{
		return;
	}

	const float SpotlessFlag = IsSpotless() ? 1.0f : 0.0f;
	for (UMaterialInstanceDynamic* DynamicMaterial : DynamicMaterials)
	{
		if (!DynamicMaterial)
		{
			continue;
		}

		if (DirtMaskTexture)
		{
			DynamicMaterial->SetTextureParameterValue(DirtMaskTextureParameterName, DirtMaskTexture);
		}

		if (DirtTexture)
		{
			DynamicMaterial->SetTextureParameterValue(DirtPatternTextureParameterName, DirtTexture);
		}

		DynamicMaterial->SetScalarParameterValue(DirtynessParameterName, DirtMaskTexture ? 1.0f : CurrentDirtyness);
		DynamicMaterial->SetScalarParameterValue(DirtIntensityParameterName, DirtIntensity);
		DynamicMaterial->SetScalarParameterValue(DirtWorldScaleParameterName, DirtWorldScale);
		DynamicMaterial->SetScalarParameterValue(UseWorldAlignedParameterName, bUseWorldAlignedDirt ? 1.0f : 0.0f);
		DynamicMaterial->SetScalarParameterValue(SpotlessParameterName, SpotlessFlag);
		DynamicMaterial->SetVectorParameterValue(DirtyTintParameterName, DirtyTint);
		DynamicMaterial->SetScalarParameterValue(CleanRoughnessParameterName, CleanRoughness);
		DynamicMaterial->SetScalarParameterValue(DirtyRoughnessParameterName, DirtyRoughness);
		DynamicMaterial->SetScalarParameterValue(CleanSpecularParameterName, CleanSpecular);
		DynamicMaterial->SetScalarParameterValue(DirtySpecularParameterName, DirtySpecular);
		DynamicMaterial->SetScalarParameterValue(CleanMetallicParameterName, CleanMetallic);
		DynamicMaterial->SetScalarParameterValue(DirtyMetallicParameterName, DirtyMetallic);
	}
}

UDirtySurfaceComponent* UDirtySurfaceComponent::FindDirtySurfaceComponent(const AActor* Actor, const UPrimitiveComponent* PreferredPrimitive)
{
	if (!Actor)
	{
		return nullptr;
	}

	TArray<UDirtySurfaceComponent*> Components;
	Actor->GetComponents<UDirtySurfaceComponent>(Components);
	for (UDirtySurfaceComponent* Component : Components)
	{
		if (!Component || !Component->bDirtySurfaceEnabled)
		{
			continue;
		}

		if (!PreferredPrimitive || !Component->GetTargetPrimitive() || Component->GetTargetPrimitive() == PreferredPrimitive)
		{
			return Component;
		}
	}

	return Components.IsEmpty() ? nullptr : Components[0];
}

UPrimitiveComponent* UDirtySurfaceComponent::ResolveTargetPrimitive()
{
	if (TargetPrimitive.IsValid())
	{
		return TargetPrimitive.Get();
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	OwnerActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	if (PrimitiveComponents.IsEmpty())
	{
		return nullptr;
	}

	if (TargetPrimitiveComponentName != NAME_None)
	{
		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (PrimitiveComponent && PrimitiveComponent->GetFName() == TargetPrimitiveComponentName)
			{
				TargetPrimitive = PrimitiveComponent;
				return PrimitiveComponent;
			}
		}
	}

	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (PrimitiveComponent && PrimitiveComponent->GetNumMaterials() > 0)
		{
			TargetPrimitive = PrimitiveComponent;
			return PrimitiveComponent;
		}
	}

	TargetPrimitive = PrimitiveComponents[0];
	return TargetPrimitive.Get();
}

void UDirtySurfaceComponent::BuildDynamicMaterials()
{
	DynamicMaterials.Reset();

	UPrimitiveComponent* PrimitiveComponent = ResolveTargetPrimitive();
	if (!PrimitiveComponent)
	{
		return;
	}

	TArray<int32> ResolvedMaterialIndices = MaterialIndices;
	if (ResolvedMaterialIndices.IsEmpty())
	{
		for (int32 MaterialIndex = 0; MaterialIndex < PrimitiveComponent->GetNumMaterials(); ++MaterialIndex)
		{
			ResolvedMaterialIndices.Add(MaterialIndex);
		}
	}

	for (int32 MaterialIndex : ResolvedMaterialIndices)
	{
		if (MaterialIndex < 0 || MaterialIndex >= PrimitiveComponent->GetNumMaterials())
		{
			continue;
		}

		if (UMaterialInstanceDynamic* DynamicMaterial = PrimitiveComponent->CreateAndSetMaterialInstanceDynamic(MaterialIndex))
		{
			DynamicMaterials.Add(DynamicMaterial);
		}
	}
}

void UDirtySurfaceComponent::CreateMaskTexture()
{
	if (DirtMaskTexture)
	{
		return;
	}

	const int32 SafeResolution = FMath::Clamp(MaskResolution, 32, 1024);
	DirtMaskTexture = UTexture2D::CreateTransient(SafeResolution, SafeResolution, PF_B8G8R8A8);
	if (!DirtMaskTexture)
	{
		return;
	}

	DirtMaskTexture->AddressX = TA_Clamp;
	DirtMaskTexture->AddressY = TA_Clamp;
	DirtMaskTexture->Filter = TF_Bilinear;
	DirtMaskTexture->SRGB = false;
	DirtMaskTexture->NeverStream = true;
	DirtMaskTexture->UpdateResource();

	DirtMaskPixels.SetNumZeroed(SafeResolution * SafeResolution);
}

void UDirtySurfaceComponent::FillMask(float NormalizedValue)
{
	if (!DirtMaskTexture || DirtMaskPixels.IsEmpty())
	{
		return;
	}

	const uint8 QuantizedValue = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(NormalizedValue * 255.0f), 0, 255));
	for (FColor& Pixel : DirtMaskPixels)
	{
		Pixel = FColor(QuantizedValue, QuantizedValue, QuantizedValue, 255);
	}

	RecomputeDirtyness();
	UploadMaskTexture();
}

void UDirtySurfaceComponent::UploadMaskTexture()
{
	if (!DirtMaskTexture || DirtMaskPixels.IsEmpty())
	{
		return;
	}

	if (!AgentDirtTextureUtilities::UploadTexturePixels(DirtMaskTexture, DirtMaskPixels))
	{
		return;
	}

	RefreshVisualParameters();
}

void UDirtySurfaceComponent::RecomputeDirtyness()
{
	if (DirtMaskPixels.IsEmpty())
	{
		CurrentDirtyness = 0.0f;
		return;
	}

	uint64 TotalValue = 0;
	for (const FColor& Pixel : DirtMaskPixels)
	{
		TotalValue += static_cast<uint64>(Pixel.R);
	}

	const double Denominator = static_cast<double>(DirtMaskPixels.Num()) * 255.0;
	CurrentDirtyness = Denominator > 0.0 ? static_cast<float>(TotalValue / Denominator) : 0.0f;
}

bool UDirtySurfaceComponent::ResolveBrushUV(const FHitResult& Hit, FVector2D& OutUV) const
{
	if (UPhysicsSettings::Get()->bSupportUVFromHitResults && UGameplayStatics::FindCollisionUV(Hit, 0, OutUV))
	{
		return true;
	}

	if (!bWarnedAboutMissingUVSupport)
	{
		UE_LOG(LogTemp, Warning, TEXT("DirtySurfaceComponent on %s could not resolve hit UVs; falling back to local projection. Enable Support UV From Hit Results and restart the editor for mesh-authored UV painting."),
			*GetNameSafe(GetOwner()));
		const_cast<UDirtySurfaceComponent*>(this)->bWarnedAboutMissingUVSupport = true;
	}

	return ResolveFallbackProjectionUV(Hit, OutUV);
}

bool UDirtySurfaceComponent::ResolveFallbackProjectionUV(const FHitResult& Hit, FVector2D& OutUV) const
{
	const UPrimitiveComponent* PrimitiveComponent = TargetPrimitive.Get();
	if (!PrimitiveComponent)
	{
		return false;
	}

	const FBoxSphereBounds LocalBounds = PrimitiveComponent->CalcBounds(FTransform::Identity);
	const FVector LocalPosition = PrimitiveComponent->GetComponentTransform().InverseTransformPosition(Hit.ImpactPoint);
	const FVector LocalNormal = PrimitiveComponent->GetComponentTransform().InverseTransformVectorNoScale(Hit.ImpactNormal).GetSafeNormal();
	const FVector AbsNormal = LocalNormal.GetAbs();
	const FVector BoxExtent = LocalBounds.BoxExtent.ComponentMax(FVector(1.0f));

	if (AbsNormal.Z >= AbsNormal.X && AbsNormal.Z >= AbsNormal.Y)
	{
		OutUV.X = (LocalPosition.X / (BoxExtent.X * 2.0f)) + 0.5f;
		OutUV.Y = (LocalPosition.Y / (BoxExtent.Y * 2.0f)) + 0.5f;
	}
	else if (AbsNormal.X >= AbsNormal.Y)
	{
		OutUV.X = (LocalPosition.Y / (BoxExtent.Y * 2.0f)) + 0.5f;
		OutUV.Y = (LocalPosition.Z / (BoxExtent.Z * 2.0f)) + 0.5f;
	}
	else
	{
		OutUV.X = (LocalPosition.X / (BoxExtent.X * 2.0f)) + 0.5f;
		OutUV.Y = (LocalPosition.Z / (BoxExtent.Z * 2.0f)) + 0.5f;
	}

	OutUV.X = FMath::Clamp(OutUV.X, 0.0f, 1.0f);
	OutUV.Y = FMath::Clamp(OutUV.Y, 0.0f, 1.0f);
	return true;
}

void UDirtySurfaceComponent::ComputeBrushRadiiPixels(const FHitResult& Hit, float BrushSizeCm, float& OutRadiusXPixels, float& OutRadiusYPixels) const
{
	OutRadiusXPixels = 1.0f;
	OutRadiusYPixels = 1.0f;

	const UPrimitiveComponent* PrimitiveComponent = TargetPrimitive.Get();
	if (!PrimitiveComponent || !DirtMaskTexture)
	{
		return;
	}

	const FBoxSphereBounds LocalBounds = PrimitiveComponent->CalcBounds(FTransform::Identity);
	const FVector LocalNormal = PrimitiveComponent->GetComponentTransform().InverseTransformVectorNoScale(Hit.ImpactNormal).GetSafeNormal();
	const FVector BoxExtent = LocalBounds.BoxExtent.ComponentMax(FVector(1.0f));
	const float SurfaceWidthCm = FMath::Max(1.0f, GetComponentSurfaceExtentForAxis(BoxExtent, LocalNormal, true));
	const float SurfaceHeightCm = FMath::Max(1.0f, GetComponentSurfaceExtentForAxis(BoxExtent, LocalNormal, false));
	const float BrushDiameterCm = FMath::Max(1.0f, BrushSizeCm);
	const float Resolution = static_cast<float>(DirtMaskTexture->GetSizeX());

	OutRadiusXPixels = FMath::Max(1.0f, (BrushDiameterCm / SurfaceWidthCm) * Resolution * 0.5f);
	OutRadiusYPixels = FMath::Max(1.0f, (BrushDiameterCm / SurfaceHeightCm) * Resolution * 0.5f);
}

bool UDirtySurfaceComponent::ApplyBrushAtUV(const FVector2D& UV, float RadiusXPixels, float RadiusYPixels, const FDirtBrushStamp& Stamp, float DeltaTime)
{
	if (!DirtMaskTexture || DirtMaskPixels.IsEmpty())
	{
		return false;
	}

	const int32 Width = DirtMaskTexture->GetSizeX();
	const int32 Height = DirtMaskTexture->GetSizeY();
	const float CenterX = UV.X * static_cast<float>(Width - 1);
	const float CenterY = UV.Y * static_cast<float>(Height - 1);
	const float SignedStrength = Stamp.GetSignedStrength(DeltaTime);
	if (FMath::IsNearlyZero(SignedStrength))
	{
		return false;
	}

	const int32 MinX = FMath::Clamp(FMath::FloorToInt(CenterX - RadiusXPixels), 0, Width - 1);
	const int32 MaxX = FMath::Clamp(FMath::CeilToInt(CenterX + RadiusXPixels), 0, Width - 1);
	const int32 MinY = FMath::Clamp(FMath::FloorToInt(CenterY - RadiusYPixels), 0, Height - 1);
	const int32 MaxY = FMath::Clamp(FMath::CeilToInt(CenterY + RadiusYPixels), 0, Height - 1);
	const float HardnessExponent = FMath::Lerp(4.0f, 1.0f, FMath::Clamp(Stamp.BrushHardness, 0.0f, 1.0f));

	bool bAnyPixelChanged = false;
	for (int32 PixelY = MinY; PixelY <= MaxY; ++PixelY)
	{
		const float NormalizedV = (static_cast<float>(PixelY) - CenterY) / FMath::Max(1.0f, RadiusYPixels);
		for (int32 PixelX = MinX; PixelX <= MaxX; ++PixelX)
		{
			const float NormalizedU = (static_cast<float>(PixelX) - CenterX) / FMath::Max(1.0f, RadiusXPixels);
			const float DistanceAlpha = 1.0f - FMath::Sqrt(FMath::Square(NormalizedU) + FMath::Square(NormalizedV));
			if (DistanceAlpha <= 0.0f)
			{
				continue;
			}

			const float BrushU = ((NormalizedU * 0.5f) + 0.5f);
			const float BrushV = ((NormalizedV * 0.5f) + 0.5f);
			const float TextureAlpha = AgentDirtTextureUtilities::SampleBrushAlpha(Stamp.BrushTexture, BrushU, BrushV);
			const float PixelAlpha = FMath::Pow(FMath::Clamp(DistanceAlpha, 0.0f, 1.0f), HardnessExponent) * TextureAlpha;
			if (PixelAlpha <= KINDA_SMALL_NUMBER)
			{
				continue;
			}

			FColor& Pixel = DirtMaskPixels[(PixelY * Width) + PixelX];
			const float NewValue = FMath::Clamp((static_cast<float>(Pixel.R) / 255.0f) + (SignedStrength * PixelAlpha), 0.0f, 1.0f);
			const uint8 QuantizedValue = static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(NewValue * 255.0f), 0, 255));
			if (QuantizedValue != Pixel.R)
			{
				Pixel = FColor(QuantizedValue, QuantizedValue, QuantizedValue, 255);
				bAnyPixelChanged = true;
			}
		}
	}

	if (!bAnyPixelChanged)
	{
		return false;
	}

	RecomputeDirtyness();
	UploadMaskTexture();
	return true;
}
