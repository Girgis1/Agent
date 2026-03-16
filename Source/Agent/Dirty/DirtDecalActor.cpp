// Copyright Epic Games, Inc. All Rights Reserved.

#include "Dirty/DirtDecalActor.h"

#include "Components/ArrowComponent.h"
#include "Components/BillboardComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SceneComponent.h"
#include "Dirty/DirtDecalSubsystem.h"
#include "Dirty/DirtTextureUtilities.h"
#include "Engine/EngineTypes.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "TimerManager.h"

ADirtDecalActor::ADirtDecalActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	DirtDecalComponent = CreateDefaultSubobject<UDecalComponent>(TEXT("DirtDecalComponent"));
	DirtDecalComponent->SetupAttachment(SceneRoot);
	DirtDecalComponent->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	DirtDecalComponent->DecalSize = FVector(64.0f, 128.0f, 128.0f);
	DirtDecalComponent->SortOrder = 1;

#if WITH_EDITORONLY_DATA
	ArrowComponent = CreateEditorOnlyDefaultSubobject<UArrowComponent>(TEXT("ArrowComponent0"));
	SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));

	if (!IsRunningCommandlet())
	{
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> DecalTexture;
			FName DecalCategoryName;
			FText DecalCategoryText;

			FConstructorStatics()
				: DecalTexture(TEXT("/Engine/EditorResources/S_DecalActorIcon"))
				, DecalCategoryName(TEXT("Decals"))
				, DecalCategoryText(NSLOCTEXT("SpriteCategory", "Decals", "Decals"))
			{
			}
		};

		static FConstructorStatics ConstructorStatics;

		if (ArrowComponent)
		{
			ArrowComponent->bTreatAsASprite = true;
			ArrowComponent->ArrowSize = 1.0f;
			ArrowComponent->ArrowColor = FColor(80, 80, 200, 255);
			ArrowComponent->SpriteInfo.Category = ConstructorStatics.DecalCategoryName;
			ArrowComponent->SpriteInfo.DisplayName = ConstructorStatics.DecalCategoryText;
			ArrowComponent->SetupAttachment(DirtDecalComponent);
			ArrowComponent->SetUsingAbsoluteScale(true);
			ArrowComponent->bIsScreenSizeScaled = true;
		}

		if (SpriteComponent)
		{
			SpriteComponent->Sprite = ConstructorStatics.DecalTexture.Get();
			SpriteComponent->SetRelativeScale3D(FVector(0.5f));
			SpriteComponent->SpriteInfo.Category = ConstructorStatics.DecalCategoryName;
			SpriteComponent->SpriteInfo.DisplayName = ConstructorStatics.DecalCategoryText;
			SpriteComponent->SetupAttachment(DirtDecalComponent);
			SpriteComponent->SetUsingAbsoluteScale(true);
			SpriteComponent->bIsScreenSizeScaled = true;
			SpriteComponent->bReceivesDecals = false;
		}
	}
#endif
}

void ADirtDecalActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (DirtDecalComponent)
	{
		DirtDecalComponent->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
		if (BaseDecalMaterial)
		{
			DirtDecalComponent->SetDecalMaterial(BaseDecalMaterial);
		}
		DirtDecalComponent->SetHiddenInGame(false);
		DirtDecalComponent->SetVisibility(true, true);
	}
}

void ADirtDecalActor::BeginPlay()
{
	Super::BeginPlay();

	InitializeDirtDecal();

	if (UWorld* World = GetWorld())
	{
		if (UDirtDecalSubsystem* DirtSubsystem = World->GetSubsystem<UDirtDecalSubsystem>())
		{
			DirtSubsystem->RegisterDirtDecal(this);
		}
	}
}

void ADirtDecalActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SpotlessDestroyTimerHandle);
		if (UDirtDecalSubsystem* DirtSubsystem = World->GetSubsystem<UDirtDecalSubsystem>())
		{
			DirtSubsystem->UnregisterDirtDecal(this);
		}
	}

	ClearSimpleTileGrid();
	DynamicMaterial = nullptr;
	DirtMaskTexture = nullptr;
	DirtMaskPixels.Reset();

	Super::EndPlay(EndPlayReason);
}

void ADirtDecalActor::InitializeDirtDecal()
{
	if (!bDirtyDecalEnabled || !DirtDecalComponent)
	{
		return;
	}

	if (BaseDecalMaterial)
	{
		DirtDecalComponent->SetDecalMaterial(BaseDecalMaterial);
	}

	UMaterialInterface* EffectiveDecalMaterial = DirtDecalComponent->GetDecalMaterial();
	if (EffectiveDecalMaterial)
	{
		if (const UMaterial* BaseMaterial = EffectiveDecalMaterial->GetMaterial())
		{
			if (BaseMaterial->MaterialDomain != MD_DeferredDecal)
			{
				UE_LOG(
					LogTemp,
					Warning,
					TEXT("DirtDecalActor %s is using %s, but it is not a Deferred Decal material. Use a decal material or a decal material instance."),
					*GetNameSafe(this),
					*GetNameSafe(EffectiveDecalMaterial));
			}
		}
	}
	else
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("DirtDecalActor %s has no decal material assigned. Assign BaseDecalMaterial or a decal material on DirtDecalComponent to render dirt."),
			*GetNameSafe(this));
	}

	if (bUseSimpleTileGrid)
	{
		BuildSimpleTileGrid();
		if (TileDecalComponents.IsEmpty())
		{
			UE_LOG(
				LogTemp,
				Warning,
				TEXT("DirtDecalActor %s could not enter simple tile grid mode, so the preview decal will stay visible."),
				*GetNameSafe(this));
			return;
		}

		RecomputeDirtynessFromTileGrid();
		UpdateSpotlessDestroyState();
		return;
	}

	if (EffectiveDecalMaterial)
	{
		DynamicMaterial = DirtDecalComponent->CreateDynamicMaterialInstance();
	}

	CreateMaskTexture();
	FillMask(InitialDirtyness);
	RefreshVisualParameters();

	if (bDebugBrushLogging)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("DirtDecalActor %s initialized smooth mask mode with material %s at %dx%d."),
			*GetNameSafe(this),
			*GetNameSafe(DynamicMaterial ? DynamicMaterial->GetMaterial() : EffectiveDecalMaterial),
			DirtMaskTexture ? DirtMaskTexture->GetSizeX() : 0,
			DirtMaskTexture ? DirtMaskTexture->GetSizeY() : 0);
	}
}

bool ADirtDecalActor::ApplyBrushAtWorldHit(const FHitResult& Hit, const FDirtBrushStamp& Stamp, float DeltaTime)
{
	const FVector WorldPoint = Hit.ImpactPoint.IsNearlyZero() ? Hit.Location : Hit.ImpactPoint;
	return ApplyBrushAtWorldPoint(WorldPoint, Stamp, DeltaTime);
}

bool ADirtDecalActor::ApplyBrushAtWorldPoint(const FVector& WorldPoint, const FDirtBrushStamp& Stamp, float DeltaTime)
{
	if (!bDirtyDecalEnabled || DeltaTime <= 0.0f)
	{
		return false;
	}

	if (!bUseSimpleTileGrid && (!DirtMaskTexture || DirtMaskPixels.IsEmpty()))
	{
		return false;
	}

	FVector2D UV = FVector2D::ZeroVector;
	FVector LocalPoint = FVector::ZeroVector;
	if (!ResolveWorldPointToUV(WorldPoint, UV, LocalPoint))
	{
		if (bDebugBrushLogging)
		{
			UE_LOG(
				LogTemp,
				Verbose,
				TEXT("DirtDecalActor %s ignored brush because world point %s was outside the decal projection."),
				*GetNameSafe(this),
				*WorldPoint.ToCompactString());
		}
		return false;
	}

	if (!CanBrushAffectWorldPoint(WorldPoint, Stamp.BrushSizeCm * 0.5f))
	{
		return false;
	}

	if (bUseSimpleTileGrid)
	{
		return ApplyBrushToTileGrid(LocalPoint, Stamp);
	}

	float RadiusXPixels = 0.0f;
	float RadiusYPixels = 0.0f;
	ComputeBrushRadiiPixels(Stamp.BrushSizeCm, RadiusXPixels, RadiusYPixels);
	return ApplyBrushAtUV(UV, RadiusXPixels, RadiusYPixels, Stamp, DeltaTime);
}

void ADirtDecalActor::SetDirtyness(float NewDirtyness, bool bResetMask)
{
	const float ClampedDirtyness = FMath::Clamp(NewDirtyness, 0.0f, 1.0f);
	InitialDirtyness = ClampedDirtyness;

	if (bResetMask)
	{
		if (bUseSimpleTileGrid)
		{
			BuildSimpleTileGrid();
			const int32 TotalTiles = TileDecalComponents.Num();
			const int32 VisibleTiles = FMath::RoundToInt(FMath::Clamp(ClampedDirtyness, 0.0f, 1.0f) * static_cast<float>(TotalTiles));
			for (int32 TileIndex = 0; TileIndex < TotalTiles; ++TileIndex)
			{
				const bool bShouldBeActive = TileIndex < VisibleTiles;
				TileActiveStates[TileIndex] = bShouldBeActive;
				if (UDecalComponent* TileDecal = TileDecalComponents[TileIndex])
				{
					TileDecal->SetHiddenInGame(!bShouldBeActive);
					TileDecal->SetVisibility(bShouldBeActive, true);
				}
			}
			RecomputeDirtynessFromTileGrid();
			UpdateSpotlessDestroyState();
			return;
		}

		FillMask(ClampedDirtyness);
	}
	else
	{
		CurrentDirtyness = ClampedDirtyness;
		RefreshVisualParameters();
		UpdateSpotlessDestroyState();
	}
}

void ADirtDecalActor::RefreshVisualParameters()
{
	if (bUseSimpleTileGrid)
	{
		return;
	}

	if (!DynamicMaterial)
	{
		return;
	}

	if (DirtMaskTexture)
	{
		DynamicMaterial->SetTextureParameterValue(DirtMaskTextureParameterName, DirtMaskTexture);
	}

	if (DirtPatternTexture)
	{
		DynamicMaterial->SetTextureParameterValue(DirtPatternTextureParameterName, DirtPatternTexture);
	}

	DynamicMaterial->SetScalarParameterValue(DirtynessParameterName, CurrentDirtyness);
	DynamicMaterial->SetScalarParameterValue(DirtIntensityParameterName, DirtIntensity);
	DynamicMaterial->SetScalarParameterValue(SpotlessParameterName, IsSpotless() ? 1.0f : 0.0f);
	DynamicMaterial->SetVectorParameterValue(DirtyTintParameterName, DirtyTint);
	DynamicMaterial->SetScalarParameterValue(CleanRoughnessParameterName, CleanRoughness);
	DynamicMaterial->SetScalarParameterValue(DirtyRoughnessParameterName, DirtyRoughness);
	DynamicMaterial->SetScalarParameterValue(CleanSpecularParameterName, CleanSpecular);
	DynamicMaterial->SetScalarParameterValue(DirtySpecularParameterName, DirtySpecular);
	DynamicMaterial->SetScalarParameterValue(CleanMetallicParameterName, CleanMetallic);
	DynamicMaterial->SetScalarParameterValue(DirtyMetallicParameterName, DirtyMetallic);
}

void ADirtDecalActor::SetDecalHalfExtent(const FVector& NewHalfExtentCm)
{
	if (!DirtDecalComponent)
	{
		return;
	}

	DirtDecalComponent->DecalSize = NewHalfExtentCm.ComponentMax(FVector(1.0f));
	DirtDecalComponent->MarkRenderStateDirty();
}

bool ADirtDecalActor::CanBrushAffectWorldPoint(const FVector& WorldPoint, float BrushRadiusCm) const
{
	FVector2D UV = FVector2D::ZeroVector;
	FVector LocalPoint = FVector::ZeroVector;
	if (!ResolveWorldPointToUV(WorldPoint, UV, LocalPoint) || !DirtDecalComponent)
	{
		return false;
	}

	const FVector DecalHalfExtent = DirtDecalComponent->DecalSize.ComponentMax(FVector(1.0f));
	const FVector WorldScale = DirtDecalComponent->GetComponentTransform().GetScale3D().GetAbs().ComponentMax(FVector(UE_SMALL_NUMBER));
	const float LocalRadiusY = BrushRadiusCm / WorldScale.Y;
	const float LocalRadiusZ = BrushRadiusCm / WorldScale.Z;

	return FMath::Abs(LocalPoint.Y) <= (DecalHalfExtent.Y + LocalRadiusY)
		&& FMath::Abs(LocalPoint.Z) <= (DecalHalfExtent.Z + LocalRadiusZ);
}

void ADirtDecalActor::CreateMaskTexture()
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

void ADirtDecalActor::BuildSimpleTileGrid()
{
	ClearSimpleTileGrid();

	if (!DirtDecalComponent)
	{
		return;
	}

	UMaterialInterface* TileMaterial = BaseDecalMaterial ? BaseDecalMaterial.Get() : DirtDecalComponent->GetDecalMaterial();
	if (!TileMaterial)
	{
		UE_LOG(
			LogTemp,
			Warning,
			TEXT("DirtDecalActor %s could not build its simple tile grid because there is no decal material assigned."),
			*GetNameSafe(this));
		return;
	}

	const int32 SafeTileCountY = FMath::Clamp(TileCountY, 1, 32);
	const int32 SafeTileCountZ = FMath::Clamp(TileCountZ, 1, 32);
	const int32 TotalTileCount = SafeTileCountY * SafeTileCountZ;
	const int32 InitiallyVisibleTileCount = FMath::Clamp(
		FMath::RoundToInt(FMath::Clamp(InitialDirtyness, 0.0f, 1.0f) * static_cast<float>(TotalTileCount)),
		0,
		TotalTileCount);
	const FVector OverallHalfExtent = DirtDecalComponent->DecalSize.ComponentMax(FVector(1.0f));
	const float CellHalfExtentY = OverallHalfExtent.Y / static_cast<float>(SafeTileCountY);
	const float CellHalfExtentZ = OverallHalfExtent.Z / static_cast<float>(SafeTileCountZ);

	TileDecalComponents.Reserve(SafeTileCountY * SafeTileCountZ);
	TileCenterLocalPoints.Reserve(SafeTileCountY * SafeTileCountZ);
	TileActiveStates.Reserve(SafeTileCountY * SafeTileCountZ);

	for (int32 TileYIndex = 0; TileYIndex < SafeTileCountY; ++TileYIndex)
	{
		const float CenterY = -OverallHalfExtent.Y + CellHalfExtentY + (static_cast<float>(TileYIndex) * CellHalfExtentY * 2.0f);
		for (int32 TileZIndex = 0; TileZIndex < SafeTileCountZ; ++TileZIndex)
		{
			const int32 LinearTileIndex = (TileYIndex * SafeTileCountZ) + TileZIndex;
			const float CenterZ = -OverallHalfExtent.Z + CellHalfExtentZ + (static_cast<float>(TileZIndex) * CellHalfExtentZ * 2.0f);

			UDecalComponent* TileDecal = NewObject<UDecalComponent>(this);
			if (!TileDecal)
			{
				continue;
			}

			TileDecal->CreationMethod = EComponentCreationMethod::Instance;
			AddInstanceComponent(TileDecal);
			TileDecal->SetDecalMaterial(TileMaterial);
			TileDecal->DecalSize = FVector(OverallHalfExtent.X, CellHalfExtentY, CellHalfExtentZ);
			TileDecal->SortOrder = DirtDecalComponent->SortOrder;
			TileDecal->SetupAttachment(DirtDecalComponent);
			TileDecal->SetRelativeLocation(FVector(0.0f, CenterY, CenterZ));
			TileDecal->RegisterComponent();

			const bool bIsInitiallyActive = LinearTileIndex < InitiallyVisibleTileCount;
			TileDecal->SetHiddenInGame(!bIsInitiallyActive);
			TileDecal->SetVisibility(bIsInitiallyActive, true);

			TileDecalComponents.Add(TileDecal);
			TileCenterLocalPoints.Add(FVector2D(CenterY, CenterZ));
			TileActiveStates.Add(bIsInitiallyActive);
		}
	}

	if (bHidePreviewDecalAtRuntime)
	{
		DirtDecalComponent->SetHiddenInGame(true);
		DirtDecalComponent->SetVisibility(false, true);
	}
	else
	{
		DirtDecalComponent->SetHiddenInGame(false);
		DirtDecalComponent->SetVisibility(true, true);
	}

	if (bDebugBrushLogging)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("DirtDecalActor %s built a %dx%d simple tile grid using %s."),
			*GetNameSafe(this),
			SafeTileCountY,
			SafeTileCountZ,
			*GetNameSafe(TileMaterial));
	}
}

void ADirtDecalActor::ClearSimpleTileGrid()
{
	for (UDecalComponent* TileDecal : TileDecalComponents)
	{
		if (TileDecal)
		{
			TileDecal->DestroyComponent();
		}
	}

	TileDecalComponents.Reset();
	TileCenterLocalPoints.Reset();
	TileActiveStates.Reset();
}

void ADirtDecalActor::FillMask(float NormalizedValue)
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

void ADirtDecalActor::UploadMaskTexture()
{
	if (!AgentDirtTextureUtilities::UploadTexturePixels(DirtMaskTexture, DirtMaskPixels))
	{
		return;
	}

	RefreshVisualParameters();
	UpdateSpotlessDestroyState();
}

void ADirtDecalActor::RecomputeDirtyness()
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

void ADirtDecalActor::RecomputeDirtynessFromTileGrid()
{
	const int32 TotalTiles = TileActiveStates.Num();
	if (TotalTiles <= 0)
	{
		CurrentDirtyness = 0.0f;
		return;
	}

	int32 ActiveTileCount = 0;
	for (const bool bTileIsActive : TileActiveStates)
	{
		ActiveTileCount += bTileIsActive ? 1 : 0;
	}

	CurrentDirtyness = static_cast<float>(ActiveTileCount) / static_cast<float>(TotalTiles);
}

void ADirtDecalActor::ComputeBrushRadiiPixels(float BrushSizeCm, float& OutRadiusXPixels, float& OutRadiusYPixels) const
{
	OutRadiusXPixels = 1.0f;
	OutRadiusYPixels = 1.0f;

	if (!DirtDecalComponent || !DirtMaskTexture)
	{
		return;
	}

	const FVector WorldScale = DirtDecalComponent->GetComponentTransform().GetScale3D().GetAbs().ComponentMax(FVector(UE_SMALL_NUMBER));
	const float SurfaceWidthCm = FMath::Max(1.0f, DirtDecalComponent->DecalSize.Y * WorldScale.Y * 2.0f);
	const float SurfaceHeightCm = FMath::Max(1.0f, DirtDecalComponent->DecalSize.Z * WorldScale.Z * 2.0f);
	const float BrushDiameterCm = FMath::Max(1.0f, BrushSizeCm);
	const float ResolutionX = static_cast<float>(DirtMaskTexture->GetSizeX());
	const float ResolutionY = static_cast<float>(DirtMaskTexture->GetSizeY());

	OutRadiusXPixels = FMath::Max(1.0f, (BrushDiameterCm / SurfaceWidthCm) * ResolutionX * 0.5f);
	OutRadiusYPixels = FMath::Max(1.0f, (BrushDiameterCm / SurfaceHeightCm) * ResolutionY * 0.5f);
}

bool ADirtDecalActor::ResolveWorldPointToUV(const FVector& WorldPoint, FVector2D& OutUV, FVector& OutLocalPoint) const
{
	if (!DirtDecalComponent)
	{
		return false;
	}

	OutLocalPoint = DirtDecalComponent->GetComponentTransform().InverseTransformPosition(WorldPoint);
	const FVector DecalHalfExtent = DirtDecalComponent->DecalSize.ComponentMax(FVector(1.0f));

	if (FMath::Abs(OutLocalPoint.X) > DecalHalfExtent.X)
	{
		return false;
	}

	// Match the engine's deferred decal shader, which swizzles object-space decal
	// coordinates as zyx before assigning DecalUVs = SwizzlePos.xy.
	OutUV.X = (OutLocalPoint.Z / (DecalHalfExtent.Z * 2.0f)) + 0.5f;
	OutUV.Y = (OutLocalPoint.Y / (DecalHalfExtent.Y * 2.0f)) + 0.5f;
	return true;
}

bool ADirtDecalActor::ApplyBrushToTileGrid(const FVector& LocalPoint, const FDirtBrushStamp& Stamp)
{
	if (TileDecalComponents.IsEmpty() || TileCenterLocalPoints.Num() != TileDecalComponents.Num() || TileActiveStates.Num() != TileDecalComponents.Num() || !DirtDecalComponent)
	{
		return false;
	}

	const FVector WorldScale = DirtDecalComponent->GetComponentTransform().GetScale3D().GetAbs().ComponentMax(FVector(UE_SMALL_NUMBER));
	const FVector OverallHalfExtent = DirtDecalComponent->DecalSize.ComponentMax(FVector(1.0f));
	const int32 SafeTileCountY = FMath::Clamp(TileCountY, 1, 32);
	const int32 SafeTileCountZ = FMath::Clamp(TileCountZ, 1, 32);
	const float CellHalfExtentYWorld = (OverallHalfExtent.Y / static_cast<float>(SafeTileCountY)) * WorldScale.Y;
	const float CellHalfExtentZWorld = (OverallHalfExtent.Z / static_cast<float>(SafeTileCountZ)) * WorldScale.Z;
	const float CellInfluenceRadiusCm = FMath::Sqrt(FMath::Square(CellHalfExtentYWorld) + FMath::Square(CellHalfExtentZWorld));
	const float BrushRadiusCm = FMath::Max(1.0f, Stamp.BrushSizeCm * 0.5f);
	const float EffectiveRadiusCm = BrushRadiusCm + CellInfluenceRadiusCm;
	const bool bEnableTile = Stamp.Mode == EDirtBrushMode::Dirty;

	bool bAnyTileChanged = false;
	int32 ChangedTileCount = 0;
	for (int32 TileIndex = 0; TileIndex < TileDecalComponents.Num(); ++TileIndex)
	{
		const FVector2D& TileCenterLocal = TileCenterLocalPoints[TileIndex];
		const float DeltaYWorld = (LocalPoint.Y - TileCenterLocal.X) * WorldScale.Y;
		const float DeltaZWorld = (LocalPoint.Z - TileCenterLocal.Y) * WorldScale.Z;
		if (FMath::Sqrt(FMath::Square(DeltaYWorld) + FMath::Square(DeltaZWorld)) > EffectiveRadiusCm)
		{
			continue;
		}

		if (TileActiveStates[TileIndex] == bEnableTile)
		{
			continue;
		}

		TileActiveStates[TileIndex] = bEnableTile;
		if (UDecalComponent* TileDecal = TileDecalComponents[TileIndex])
		{
			TileDecal->SetHiddenInGame(!bEnableTile);
			TileDecal->SetVisibility(bEnableTile, true);
		}

		bAnyTileChanged = true;
		++ChangedTileCount;
	}

	if (!bAnyTileChanged)
	{
		return false;
	}

	RecomputeDirtynessFromTileGrid();
	UpdateSpotlessDestroyState();

	if (bDebugBrushLogging)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("DirtDecalActor %s %s %d tile(s); dirtyness now %.3f."),
			*GetNameSafe(this),
			bEnableTile ? TEXT("re-dirtied") : TEXT("cleaned"),
			ChangedTileCount,
			CurrentDirtyness);
	}

	return true;
}

bool ADirtDecalActor::ApplyBrushAtUV(const FVector2D& UV, float RadiusXPixels, float RadiusYPixels, const FDirtBrushStamp& Stamp, float DeltaTime)
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

			const float BrushU = (NormalizedU * 0.5f) + 0.5f;
			const float BrushV = (NormalizedV * 0.5f) + 0.5f;
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

	if (bDebugBrushLogging)
	{
		UE_LOG(
			LogTemp,
			Log,
			TEXT("DirtDecalActor %s %s brush at UV=(%.3f, %.3f); dirtyness now %.3f."),
			*GetNameSafe(this),
			Stamp.Mode == EDirtBrushMode::Dirty ? TEXT("applied dirty") : TEXT("applied clean"),
			UV.X,
			UV.Y,
			CurrentDirtyness);
	}
	return true;
}

void ADirtDecalActor::UpdateSpotlessDestroyState()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FTimerManager& TimerManager = World->GetTimerManager();
	if (bAutoDestroyWhenSpotless && IsSpotless())
	{
		if (DestroyDelayAfterSpotlessSeconds <= KINDA_SMALL_NUMBER)
		{
			HandleSpotlessDestroy();
			return;
		}

		if (!TimerManager.IsTimerActive(SpotlessDestroyTimerHandle))
		{
			TimerManager.SetTimer(
				SpotlessDestroyTimerHandle,
				this,
				&ADirtDecalActor::HandleSpotlessDestroy,
				DestroyDelayAfterSpotlessSeconds,
				false);
		}
		return;
	}

	TimerManager.ClearTimer(SpotlessDestroyTimerHandle);
}

void ADirtDecalActor::HandleSpotlessDestroy()
{
	if (!IsValid(this) || !bAutoDestroyWhenSpotless || !IsSpotless())
	{
		return;
	}

	Destroy();
}
