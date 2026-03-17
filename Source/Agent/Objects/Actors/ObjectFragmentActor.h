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

UCLASS()
class AObjectFragmentActor : public AMaterialActor
{
	GENERATED_BODY()

public:
	AObjectFragmentActor();

	UFUNCTION(BlueprintCallable, Category="Objects|Fracture")
	void RefreshPhysicsProxyFromItemMesh();

	UFUNCTION(BlueprintCallable, Category="Objects|Fracture")
	bool InitializeFromDynamicMesh(const UDynamicMesh* SourceMesh, const TArray<UMaterialInterface*>& MaterialSet);

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
