// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Material/MaterialActor.h"
#include "ObjectFragmentActor.generated.h"

class UBoxComponent;
class UDynamicMesh;
class UDynamicMeshComponent;
class UMaterialInterface;
class UObjectSliceComponent;
class UProceduralMeshComponent;
class UPrimitiveComponent;
class UStaticMeshComponent;

USTRUCT(BlueprintType)
struct FObjectFragmentCollisionGenerationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture")
	bool bUseLongObjectProfile = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture", meta=(ClampMin="1", UIMin="1"))
	int32 LongObjectMaxConvexHulls = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture", meta=(ClampMin="1", UIMin="1"))
	int32 LongObjectMaxShapeCount = 6;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture", meta=(ClampMin="0.01", UIMin="0.01", Units="cm"))
	float LongObjectMinThicknessCm = 0.15f;
};

UCLASS()
class AObjectFragmentActor : public AMaterialActor
{
	GENERATED_BODY()

public:
	AObjectFragmentActor();

	UFUNCTION(BlueprintCallable, Category="Objects|Fracture")
	void RefreshPhysicsProxyFromItemMesh();

	UFUNCTION(BlueprintCallable, Category="Objects|Fracture")
	bool InitializeFromDynamicMesh(
		const UDynamicMesh* SourceMesh,
		const TArray<UMaterialInterface*>& MaterialSet);

	bool InitializeFromDynamicMesh(
		const UDynamicMesh* SourceMesh,
		const TArray<UMaterialInterface*>& MaterialSet,
		const FObjectFragmentCollisionGenerationSettings& CollisionSettings);

	bool InitializeFromStaticMeshSlice(
		UStaticMeshComponent* SourceStaticMeshComponent,
		const FVector& PlanePositionWorld,
		const FVector& PlaneNormalWorld,
		bool bKeepPositiveHalf,
		UMaterialInterface* CapMaterial);

	UFUNCTION(BlueprintPure, Category="Objects|Fracture")
	bool IsUsingDynamicMeshPiece() const { return bUsingDynamicMeshPiece; }

	UFUNCTION(BlueprintPure, Category="Objects|Fracture")
	bool IsUsingProceduralMeshPiece() const { return bUsingProceduralMeshPiece; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|Fracture")
	TObjectPtr<UBoxComponent> PhysicsProxy = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|Fracture")
	TObjectPtr<UDynamicMeshComponent> DynamicMeshPiece = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|Fracture")
	TObjectPtr<UProceduralMeshComponent> ProceduralMeshPiece = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|Slice")
	TObjectPtr<UObjectSliceComponent> ObjectSlice = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture", meta=(ClampMin="0.5", ClampMax="1.0", UIMin="0.5", UIMax="1.0"))
	float PhysicsProxyExtentScale = 0.9f;

protected:
	virtual UPrimitiveComponent* ResolveMaterialRuntimePrimitive() const override;
	virtual void HandlePostItemMeshConfiguration() override;
	void DisableInactiveVisualComponents();

	UPROPERTY(Transient)
	bool bUsingDynamicMeshPiece = false;

	UPROPERTY(Transient)
	bool bUsingProceduralMeshPiece = false;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> ActiveProceduralMeshPiece = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> GeneratedProceduralOtherHalf = nullptr;
};
