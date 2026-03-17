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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice", meta=(ClampMin="0", UIMin="0"))
	int32 MaxGeneratedPiecesPerSlice = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|Validation", meta=(ClampMin="0.0", UIMin="0.0"))
	float MinSourceMassKg = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|Validation", meta=(ClampMin="0.0", UIMin="0.0"))
	float MinSourceVolumeCm3 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|Validation", meta=(ClampMin="0.0", UIMin="0.0"))
	float MinPieceMassKg = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|Validation", meta=(ClampMin="0.0", UIMin="0.0"))
	float MinPieceVolumeCm3 = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|Validation", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float MinPieceExtentCm = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|Validation", meta=(ClampMin="0.0", ClampMax="0.5", UIMin="0.0", UIMax="0.5"))
	float MinPieceVolumeRatio = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|Validation", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float MinimumPenetrationThicknessCm = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|Validation", meta=(ClampMin="0", UIMin="0", ToolTip="0 means unlimited."))
	int32 MaxSourceTriangleCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|Validation")
	bool bRequireClosedMesh = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|Validation")
	bool bRequireSingleConnectedSourceIsland = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|Behavior", meta=(ClampMin="0.01", UIMin="0.01", Units="cm"))
	float SliceGapWidthCm = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|Behavior", meta=(ClampMin="0.0", UIMin="0.0"))
	float SliceSeparationImpulse = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|Behavior")
	bool bDisableSourceCollisionOnSlice = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|Behavior")
	bool bHideSourceActorOnSlice = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|Behavior")
	bool bDestroySourceActorAfterSlice = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Slice|Behavior")
	bool bDisableSourceFractureAfterSlice = true;

protected:
	USceneComponent* ResolveSliceSourceSceneComponent() const;
	UMeshComponent* ResolveSliceSourceMeshComponent() const;
	bool BuildSliceSourceCache(FString& OutFailureReason);
	bool ValidateCachedSourceMesh(FString& OutFailureReason) const;
	bool CollectSourceMaterialSet(TArray<UMaterialInterface*>& OutMaterialSet) const;
	void ApplySourceActorPostSlice() const;
	void DisableFractureOnActor(AActor* Actor) const;

	UPROPERTY(Transient)
	int32 CompletedSliceCount = 0;

	UPROPERTY(Transient)
	TObjectPtr<UDynamicMesh> CachedSourceMesh = nullptr;

	UPROPERTY(Transient)
	FGeometryScriptDynamicMeshBVH CachedSourceMeshBVH;

	UPROPERTY(Transient)
	bool bSourceCacheValid = false;

	UPROPERTY(Transient)
	FBox CachedSourceLocalBounds = FBox(EForceInit::ForceInit);

	UPROPERTY(Transient)
	float CachedSourceVolumeCm3 = 0.0f;

	UPROPERTY(Transient)
	bool bCachedSourceClosedMesh = false;

	UPROPERTY(Transient)
	int32 CachedSourceConnectedComponents = 0;

	UPROPERTY(Transient)
	int32 CachedSourceTriangleCount = 0;
};
