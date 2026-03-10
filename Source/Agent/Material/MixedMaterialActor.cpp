// Copyright Epic Games, Inc. All Rights Reserved.

#include "Material/MixedMaterialActor.h"
#include "Components/StaticMeshComponent.h"
#include "Material/AgentResourceTypes.h"
#include "Material/MaterialComponent.h"
#include "Material/MaterialDefinitionAsset.h"
#include "Materials/MaterialParameters.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Texture.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/UObjectIterator.h"

namespace
{
struct FMixedMaterialVisualContribution
{
	FName ResourceId = NAME_None;
	FName ColorId = NAME_None;
	FLinearColor Color = FLinearColor::White;
	float Units = 0.0f;
	float ColorIdHash = 0.0f;
	UMaterialInterface* SourceMaterial = nullptr;
	UTexture* BaseColorTexture = nullptr;
	UTexture* NormalTexture = nullptr;
	UTexture* PackedTexture = nullptr;
};

const UMaterialDefinitionAsset* FindMaterialDefinition(FName ResourceId)
{
	if (ResourceId.IsNone())
	{
		return nullptr;
	}

	for (TObjectIterator<UMaterialDefinitionAsset> It; It; ++It)
	{
		const UMaterialDefinitionAsset* Definition = *It;
		if (Definition && Definition->GetResolvedMaterialId() == ResourceId)
		{
			return Definition;
		}
	}

	return nullptr;
}

float HashNameToUnitFloat(FName Name)
{
	if (Name.IsNone())
	{
		return 0.0f;
	}

	const uint32 HashValue = GetTypeHash(Name);
	return static_cast<float>(HashValue % 65536u) / 65535.0f;
}

UTexture* ResolveTextureParameterFromCandidates(UMaterialInterface* Material, const TArray<FName>& CandidateNames)
{
	if (!Material)
	{
		return nullptr;
	}

	for (const FName& ParameterName : CandidateNames)
	{
		if (ParameterName.IsNone())
		{
			continue;
		}

		UTexture* FoundTexture = nullptr;
		const FHashedMaterialParameterInfo ParameterInfo(ParameterName);
		if (Material->GetTextureParameterValue(ParameterInfo, FoundTexture, false) && FoundTexture)
		{
			return FoundTexture;
		}
	}

	return nullptr;
}

uint32 BuildCompositionHash(const TMap<FName, int32>& CompositionScaled)
{
	TArray<FName> Keys;
	CompositionScaled.GetKeys(Keys);
	Keys.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});

	uint32 HashValue = 2166136261u;
	for (const FName& ResourceId : Keys)
	{
		HashValue = HashCombine(HashValue, GetTypeHash(ResourceId));
		HashValue = HashCombine(HashValue, static_cast<uint32>(FMath::Max(0, CompositionScaled.FindRef(ResourceId))));
	}

	return HashValue;
}
}

AMixedMaterialActor::AMixedMaterialActor()
{
	PrimaryActorTick.bCanEverTick = false;

	ItemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ItemMesh"));
	SetRootComponent(ItemMesh);
	ItemMesh->SetSimulatePhysics(true);
	ItemMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ItemMesh->SetCollisionObjectType(ECC_PhysicsBody);
	ItemMesh->SetCollisionResponseToAllChannels(ECR_Block);
	ItemMesh->SetGenerateOverlapEvents(true);
	ItemMesh->SetCanEverAffectNavigation(false);
	ItemMesh->SetLinearDamping(0.3f);
	ItemMesh->SetAngularDamping(1.5f);

	MaterialData = CreateDefaultSubobject<UMaterialComponent>(TEXT("MaterialData"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (DefaultMesh.Succeeded())
	{
		ItemMesh->SetStaticMesh(DefaultMesh.Object);
	}
}

void AMixedMaterialActor::BeginPlay()
{
	Super::BeginPlay();
	ReapplyMixedMaterialPresentation();
}

bool AMixedMaterialActor::ConfigureMixedMaterialPayload(
	const TMap<FName, int32>& InCompositionScaled,
	float InMinUnitsForScale,
	float InMaxUnitsForScale,
	float InMinVisualScale,
	float InMaxVisualScale)
{
	TMap<FName, int32> SanitizedCompositionScaled;
	for (const TPair<FName, int32>& Pair : InCompositionScaled)
	{
		const FName ResourceId = Pair.Key;
		const int32 QuantityScaled = FMath::Max(0, Pair.Value);
		if (ResourceId.IsNone() || QuantityScaled <= 0)
		{
			continue;
		}

		SanitizedCompositionScaled.FindOrAdd(ResourceId) += QuantityScaled;
	}

	if (SanitizedCompositionScaled.Num() == 0 || !MaterialData)
	{
		return false;
	}

	if (!MaterialData->ConfigureResourcesById(SanitizedCompositionScaled))
	{
		return false;
	}

	CompositionScaled = MoveTemp(SanitizedCompositionScaled);
	MinUnitsForScale = FMath::Max(0.01f, InMinUnitsForScale);
	MaxUnitsForScale = FMath::Max(MinUnitsForScale, InMaxUnitsForScale);
	MinVisualScale = FMath::Max(0.05f, InMinVisualScale);
	MaxVisualScale = FMath::Max(MinVisualScale, InMaxVisualScale);

	TotalCompositionUnits = 0.0f;
	for (const TPair<FName, int32>& Pair : CompositionScaled)
	{
		TotalCompositionUnits += AgentResource::ScaledToUnits(FMath::Max(0, Pair.Value));
	}

	ReapplyMixedMaterialPresentation();
	return true;
}

float AMixedMaterialActor::GetTotalMaterialUnits() const
{
	return FMath::Max(0.0f, TotalCompositionUnits);
}

void AMixedMaterialActor::ReapplyMixedMaterialPresentation()
{
	ApplyScaleFromCurrentComposition();
	ApplyVisualFromCurrentComposition();
}

void AMixedMaterialActor::ApplyScaleFromCurrentComposition()
{
	if (!ItemMesh)
	{
		return;
	}

	const float SafeMinUnits = FMath::Max(0.01f, MinUnitsForScale);
	const float SafeMaxUnits = FMath::Max(SafeMinUnits, MaxUnitsForScale);
	const float SafeMinScale = FMath::Max(0.05f, MinVisualScale);
	const float SafeMaxScale = FMath::Max(SafeMinScale, MaxVisualScale);
	const float Alpha = FMath::Clamp((TotalCompositionUnits - SafeMinUnits) / FMath::Max(KINDA_SMALL_NUMBER, SafeMaxUnits - SafeMinUnits), 0.0f, 1.0f);
	const float FinalScale = FMath::Lerp(SafeMinScale, SafeMaxScale, Alpha);
	ItemMesh->SetWorldScale3D(FVector(FinalScale));
}

void AMixedMaterialActor::ApplyVisualFromCurrentComposition()
{
	if (!ItemMesh)
	{
		return;
	}

	static const TArray<FName> BaseColorTextureCandidates = {
		TEXT("BaseColorTexture"),
		TEXT("BaseColorTex"),
		TEXT("AlbedoTexture"),
		TEXT("AlbedoTex"),
		TEXT("Albedo"),
		TEXT("DiffuseTexture"),
		TEXT("DiffuseTex"),
		TEXT("ColorTexture"),
		TEXT("ColorMap")
	};

	static const TArray<FName> NormalTextureCandidates = {
		TEXT("NormalTexture"),
		TEXT("NormalTex"),
		TEXT("NormalMap")
	};

	static const TArray<FName> PackedTextureCandidates = {
		TEXT("PackedTexture"),
		TEXT("PackedTex"),
		TEXT("ORM"),
		TEXT("MRA"),
		TEXT("RMA"),
		TEXT("RoughnessMetallicAO")
	};

	TArray<FMixedMaterialVisualContribution> Contributions;
	Contributions.Reserve(CompositionScaled.Num());

	float TotalUnits = 0.0f;
	for (const TPair<FName, int32>& Pair : CompositionScaled)
	{
		const FName ResourceId = Pair.Key;
		const float Units = AgentResource::ScaledToUnits(FMath::Max(0, Pair.Value));
		if (ResourceId.IsNone() || Units <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		FMixedMaterialVisualContribution Contribution;
		Contribution.ResourceId = ResourceId;
		Contribution.ColorId = ResourceId;
		Contribution.Units = Units;
		if (const UMaterialDefinitionAsset* Definition = FindMaterialDefinition(ResourceId))
		{
			Contribution.Color = Definition->GetResolvedVisualColor();
			Contribution.ColorId = Definition->GetResolvedColorId();
			Contribution.SourceMaterial = Definition->ResolveVisualMaterial();
			Contribution.BaseColorTexture = ResolveTextureParameterFromCandidates(Contribution.SourceMaterial, BaseColorTextureCandidates);
			Contribution.NormalTexture = ResolveTextureParameterFromCandidates(Contribution.SourceMaterial, NormalTextureCandidates);
			Contribution.PackedTexture = ResolveTextureParameterFromCandidates(Contribution.SourceMaterial, PackedTextureCandidates);
		}
		Contribution.ColorIdHash = HashNameToUnitFloat(Contribution.ColorId);

		Contributions.Add(Contribution);
		TotalUnits += Units;
	}

	if (Contributions.Num() == 0 || TotalUnits <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	Contributions.Sort([](const FMixedMaterialVisualContribution& Left, const FMixedMaterialVisualContribution& Right)
	{
		if (!FMath::IsNearlyEqual(Left.Units, Right.Units))
		{
			return Left.Units > Right.Units;
		}

		if (Left.ColorId != Right.ColorId)
		{
			return Left.ColorId.LexicalLess(Right.ColorId);
		}

		return Left.ResourceId.LexicalLess(Right.ResourceId);
	});

	UMaterialInterface* ResolvedVisualMaterial = MixedVisualMaterialOverride.IsNull() ? nullptr : MixedVisualMaterialOverride.LoadSynchronous();
	if (!ResolvedVisualMaterial)
	{
		for (const FMixedMaterialVisualContribution& Contribution : Contributions)
		{
			if (Contribution.SourceMaterial)
			{
				ResolvedVisualMaterial = Contribution.SourceMaterial;
				break;
			}
		}
	}

	if (ResolvedVisualMaterial && ItemMesh->GetMaterial(0) != ResolvedVisualMaterial)
	{
		ItemMesh->SetMaterial(0, ResolvedVisualMaterial);
		DynamicVisualMaterial = nullptr;
	}

	UMaterialInterface* BaseMaterial = ItemMesh->GetMaterial(0);
	if (!BaseMaterial)
	{
		return;
	}

	if (!DynamicVisualMaterial || ItemMesh->GetMaterial(0) != DynamicVisualMaterial)
	{
		DynamicVisualMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		if (DynamicVisualMaterial)
		{
			ItemMesh->SetMaterial(0, DynamicVisualMaterial);
		}
	}

	if (!DynamicVisualMaterial)
	{
		return;
	}

	const int32 LayerCount = FMath::Min(4, Contributions.Num());
	const uint32 CompositionHash = BuildCompositionHash(CompositionScaled);
	const float MixMode = (VisualPattern == EMixedMaterialVisualPattern::WeightedColor)
		? 0.0f
		: (VisualPattern == EMixedMaterialVisualPattern::Voronoi ? 1.0f : 2.0f);
	const float SeedValue = FMath::Frac((static_cast<float>(CompositionHash % 65535u) / 65535.0f) + PatternSeedOffset);
	const float GradientAngleRadians = FMath::DegreesToRadians(GradientAngleDegrees);
	const FLinearColor GradientDirection(FMath::Cos(GradientAngleRadians), FMath::Sin(GradientAngleRadians), 0.0f, 0.0f);

	DynamicVisualMaterial->SetScalarParameterValue(MixModeParameterName, MixMode);
	DynamicVisualMaterial->SetScalarParameterValue(MixLayerCountParameterName, static_cast<float>(LayerCount));
	DynamicVisualMaterial->SetScalarParameterValue(MixSeedParameterName, SeedValue);
	DynamicVisualMaterial->SetScalarParameterValue(VoronoiScaleParameterName, FMath::Max(0.01f, VoronoiScale));
	DynamicVisualMaterial->SetScalarParameterValue(VoronoiEdgeWidthParameterName, FMath::Max(0.0f, VoronoiEdgeWidth));
	DynamicVisualMaterial->SetVectorParameterValue(GradientDirectionParameterName, GradientDirection);
	DynamicVisualMaterial->SetScalarParameterValue(GradientSharpnessParameterName, FMath::Max(0.01f, GradientSharpness));

	if (bApplyWeightedColorMixToMaterial)
	{
		FLinearColor WeightedColor = FLinearColor::Black;
		for (const FMixedMaterialVisualContribution& Contribution : Contributions)
		{
			const float Weight = Contribution.Units / TotalUnits;
			WeightedColor += Contribution.Color * Weight;
		}

		DynamicVisualMaterial->SetVectorParameterValue(TintColorParameterName, WeightedColor);
		DynamicVisualMaterial->SetVectorParameterValue(BaseColorParameterName, WeightedColor);
	}

	auto SetMixLayer = [this](int32 LayerIndex, const FMixedMaterialVisualContribution* Contribution, float TotalInUnits)
	{
		const FLinearColor LayerColor = Contribution ? Contribution->Color : FLinearColor::Black;
		const float LayerWeight = (Contribution && TotalInUnits > KINDA_SMALL_NUMBER) ? (Contribution->Units / TotalInUnits) : 0.0f;
		const float LayerColorId = Contribution ? Contribution->ColorIdHash : 0.0f;
		UTexture* LayerBaseTexture = Contribution ? Contribution->BaseColorTexture : nullptr;
		UTexture* LayerNormalTexture = Contribution ? Contribution->NormalTexture : nullptr;
		UTexture* LayerPackedTexture = Contribution ? Contribution->PackedTexture : nullptr;

		switch (LayerIndex)
		{
		case 0:
			DynamicVisualMaterial->SetVectorParameterValue(MixColorAParameterName, LayerColor);
			DynamicVisualMaterial->SetScalarParameterValue(MixWeightAParameterName, LayerWeight);
			DynamicVisualMaterial->SetScalarParameterValue(MixColorIdAParameterName, LayerColorId);
			if (bPushSourceMaterialTextures)
			{
				DynamicVisualMaterial->SetTextureParameterValue(MixBaseTextureAParameterName, LayerBaseTexture);
				DynamicVisualMaterial->SetTextureParameterValue(MixNormalTextureAParameterName, LayerNormalTexture);
				DynamicVisualMaterial->SetTextureParameterValue(MixPackedTextureAParameterName, LayerPackedTexture);
			}
			break;
		case 1:
			DynamicVisualMaterial->SetVectorParameterValue(MixColorBParameterName, LayerColor);
			DynamicVisualMaterial->SetScalarParameterValue(MixWeightBParameterName, LayerWeight);
			DynamicVisualMaterial->SetScalarParameterValue(MixColorIdBParameterName, LayerColorId);
			if (bPushSourceMaterialTextures)
			{
				DynamicVisualMaterial->SetTextureParameterValue(MixBaseTextureBParameterName, LayerBaseTexture);
				DynamicVisualMaterial->SetTextureParameterValue(MixNormalTextureBParameterName, LayerNormalTexture);
				DynamicVisualMaterial->SetTextureParameterValue(MixPackedTextureBParameterName, LayerPackedTexture);
			}
			break;
		case 2:
			DynamicVisualMaterial->SetVectorParameterValue(MixColorCParameterName, LayerColor);
			DynamicVisualMaterial->SetScalarParameterValue(MixWeightCParameterName, LayerWeight);
			DynamicVisualMaterial->SetScalarParameterValue(MixColorIdCParameterName, LayerColorId);
			if (bPushSourceMaterialTextures)
			{
				DynamicVisualMaterial->SetTextureParameterValue(MixBaseTextureCParameterName, LayerBaseTexture);
				DynamicVisualMaterial->SetTextureParameterValue(MixNormalTextureCParameterName, LayerNormalTexture);
				DynamicVisualMaterial->SetTextureParameterValue(MixPackedTextureCParameterName, LayerPackedTexture);
			}
			break;
		case 3:
		default:
			DynamicVisualMaterial->SetVectorParameterValue(MixColorDParameterName, LayerColor);
			DynamicVisualMaterial->SetScalarParameterValue(MixWeightDParameterName, LayerWeight);
			DynamicVisualMaterial->SetScalarParameterValue(MixColorIdDParameterName, LayerColorId);
			if (bPushSourceMaterialTextures)
			{
				DynamicVisualMaterial->SetTextureParameterValue(MixBaseTextureDParameterName, LayerBaseTexture);
				DynamicVisualMaterial->SetTextureParameterValue(MixNormalTextureDParameterName, LayerNormalTexture);
				DynamicVisualMaterial->SetTextureParameterValue(MixPackedTextureDParameterName, LayerPackedTexture);
			}
			break;
		}
	};

	for (int32 LayerIndex = 0; LayerIndex < 4; ++LayerIndex)
	{
		const FMixedMaterialVisualContribution* Contribution = Contributions.IsValidIndex(LayerIndex) ? &Contributions[LayerIndex] : nullptr;
		SetMixLayer(LayerIndex, Contribution, TotalUnits);
	}
}
