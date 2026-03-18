// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "ObjectSliceComponent.generated.h"

class AActor;
class AObjectFragmentActor;
class UDynamicMesh;
class UMaterialInterface;
class UMeshComponent;
class UPrimitiveComponent;
class USceneComponent;
class UBoxComponent;

USTRUCT(BlueprintType)
struct FObjectSliceBeamIntersection
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|Slice")
	bool bHasPenetration = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|Slice")
	FVector EntryPoint = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|Slice")
	FVector ExitPoint = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|Slice")
	float ThicknessCm = 0.0f;
};

UENUM(BlueprintType)
enum class EObjectSliceAnchorMode : uint8
{
	None UMETA(DisplayName="None"),
	OriginBand UMETA(DisplayName="Origin Band"),
	SupportVolumes UMETA(DisplayName="Support Collision Boxes")
};

UCLASS(ClassGroup=(Objects), meta=(BlueprintSpawnableComponent))
class AGENT_API UObjectSliceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UObjectSliceComponent();

	static UObjectSliceComponent* FindObjectSliceComponent(AActor* Actor, const UPrimitiveComponent* PreferredPrimitive = nullptr);

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category="Objects|Slice")
	bool IsSlicingEnabled() const { return bSlicingEnabled; }

	UFUNCTION(BlueprintPure, Category="Objects|Slice")
	bool HasRemainingSlices() const;

	UFUNCTION(BlueprintPure, Category="Objects|Slice")
	int32 GetCompletedSliceCount() const { return CompletedSliceCount; }

	UFUNCTION(BlueprintPure, Category="Objects|Slice")
	UPrimitiveComponent* GetSliceSourcePrimitive() const;

	bool CanSliceNow(FString& OutFailureReason) const;
	bool QueryBeamPenetration(
		const FVector& BeamOriginWorld,
		const FVector& BeamDirectionWorld,
		float MaxDistanceCm,
		FObjectSliceBeamIntersection& OutIntersection,
		FString& OutFailureReason);
	bool TryExecuteSliceFromBeamGesture(
		const FVector& BeamStartWorld,
		const FVector& FirstEntryPointWorld,
		const FVector& CurrentEntryPointWorld,
		const FVector& CurrentExitPointWorld,
		AActor* SliceInstigator,
		FString& OutFailureReason);
	bool TryExecuteSliceFromStrokeCutter(
		const TArray<FVector>& StrokePointsWorld,
		const FVector& ExtrudeDirectionTowardRayStartWorld,
		float CutDepthCm,
		float StrokeWidthCm,
		AActor* SliceInstigator,
		FString& OutFailureReason);
	bool TryExecuteSliceFromRibbonCutter(
		const TArray<FVector>& BeamStartPointsWorld,
		const TArray<FVector>& StrokePointsWorld,
		const TArray<FVector>& FarPointsWorld,
		float RibbonThicknessCm,
		AActor* SliceInstigator,
		FString& OutFailureReason);
	void InvalidateSourceMeshCache();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice")
	bool bSlicingEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice")
	FComponentReference SliceSourceComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice")
	TSubclassOf<AObjectFragmentActor> SliceResultActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice")
	TObjectPtr<UMaterialInterface> SliceCapMaterial = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice", meta=(ClampMin="0", UIMin="0"))
	int32 MaxRuntimeSlices = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|Validation", meta=(ClampMin="0.0", UIMin="0.0"))
	float MinPieceVolumeCm3 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|Validation", meta=(DisplayName="Nanite Sliceable (Experimental)", ToolTip="Allows runtime slicing attempts on Nanite static meshes for testing. Heavy assets can hitch or freeze when sliced."))
	bool bAllowNaniteStaticMeshes = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|Validation", meta=(ClampMin="0", UIMin="0", ToolTip="0 means no triangle safety cap."))
	int32 MaxSourceLod0Triangles = 50000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|Validation", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float MinPieceExtentCm = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|Validation", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float MinimumPenetrationThicknessCm = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|Behavior", meta=(ClampMin="0.01", UIMin="0.01", Units="cm"))
	float SliceGapWidthCm = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|Behavior", meta=(ClampMin="0.0", UIMin="0.0"))
	float SliceSeparationImpulse = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|Behavior", meta=(ClampMin="0.0", UIMin="0.0", Units="s", ToolTip="If a generated piece is below any MinPiece threshold, assign this lifespan (0 = keep forever)."))
	float SmallPieceLifespanSeconds = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|LongObject", meta=(DisplayName="Long Object Profile", ToolTip="Uses tree-friendly collision generation and spawn stabilization for long meshes."))
	bool bUseLongObjectProfile = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|LongObject", meta=(ClampMin="1", UIMin="1", EditCondition="bUseLongObjectProfile", EditConditionHides))
	int32 LongObjectMaxConvexHulls = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|LongObject", meta=(ClampMin="1", UIMin="1", EditCondition="bUseLongObjectProfile", EditConditionHides))
	int32 LongObjectMaxShapeCount = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|LongObject", meta=(ClampMin="0.01", UIMin="0.01", Units="cm", EditCondition="bUseLongObjectProfile", EditConditionHides))
	float LongObjectMinCollisionThicknessCm = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|LongObject", meta=(ClampMin="0.0", UIMin="0.0", Units="s", EditCondition="bUseLongObjectProfile", EditConditionHides, ToolTip="Temporarily ignores PhysicsBody collisions between fresh slice pieces to prevent first-frame pop."))
	float LongObjectSiblingCollisionIgnoreSeconds = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|Anchoring")
	EObjectSliceAnchorMode AnchorMode = EObjectSliceAnchorMode::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|Anchoring", meta=(ClampMin="0.0", UIMin="0.0", Units="cm", EditCondition="AnchorMode==EObjectSliceAnchorMode::OriginBand", EditConditionHides))
	float StaticAnchorMaxLocalZCm = 35.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|Anchoring", meta=(DisplayName="Support Box Colliders", UseComponentPicker, AllowedClasses="/Script/Engine.BoxComponent", ToolTip="Box collision components used to anchor slice pieces that overlap them.", EditCondition="AnchorMode==EObjectSliceAnchorMode::SupportVolumes", EditConditionHides))
	TArray<FComponentReference> SupportBoxColliders;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|Anchoring", meta=(ClampMin="0.0", UIMin="0.0", Units="cm", EditCondition="AnchorMode==EObjectSliceAnchorMode::SupportVolumes", EditConditionHides))
	float MinSupportOverlapCm = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|Anchoring")
	bool bDisableSlicingOnAnchoredPieces = false;

protected:
	USceneComponent* ResolveSliceSourceSceneComponent() const;
	UMeshComponent* ResolveSliceSourceMeshComponent() const;
	bool BuildSliceSourceCache(FString& OutFailureReason);
	bool ValidateCachedSourceMesh(FString& OutFailureReason) const;
	bool CollectSourceMaterialSet(TArray<UMaterialInterface*>& OutMaterialSet) const;
	bool FinalizeSliceFromWorkingMesh(
		UDynamicMesh* WorkingMesh,
		const FVector& PlaneNormalWorld,
		const FVector& SliceMidpointWorld,
		AActor* SliceInstigator,
		FString& OutFailureReason);
	void ApplySourceActorPostSlice() const;
	void DisableFractureOnActor(AActor* Actor) const;
	bool IsPieceAnchored(const FBox& PieceLocalBounds, const FTransform& SourceTransform) const;
	bool IsAnchoredByOriginBand(const FBox& PieceLocalBounds, const FTransform& SourceTransform) const;
	bool IsAnchoredBySupportBoxes(const FBox& PieceLocalBounds, const FTransform& SourceTransform) const;

	UPROPERTY(Transient)
	int32 CompletedSliceCount = 0;

	UPROPERTY(Transient)
	TObjectPtr<UDynamicMesh> CachedSourceMesh = nullptr;

	UPROPERTY(Transient)
	FGeometryScriptDynamicMeshBVH CachedSourceMeshBVH;

	UPROPERTY(Transient)
	bool bSourceCacheValid = false;

	UPROPERTY(Transient)
	float CachedSourceVolumeCm3 = 0.0f;
};
