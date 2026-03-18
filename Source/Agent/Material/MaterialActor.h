// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MaterialActor.generated.h"

class UMaterialComponent;
class UMaterialInterface;
class UObjectFractureComponent;
class UObjectHealthComponent;
class UStaticMesh;
class UStaticMeshComponent;
class UPrimitiveComponent;

USTRUCT(BlueprintType)
struct FMaterialActorMaterialSlot
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialActor|Material", meta=(ClampMin="0", UIMin="0"))
	int32 MaterialSlotIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialActor|Material", meta=(DisplayName="Material Instances"))
	TArray<TObjectPtr<UMaterialInterface>> MaterialInstances;
};

UCLASS()
class AMaterialActor : public AActor
{
	GENERATED_BODY()

public:
	AMaterialActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MaterialActor")
	TObjectPtr<UStaticMeshComponent> ItemMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MaterialActor")
	TObjectPtr<UMaterialComponent> MaterialData = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|MaterialActor")
	TObjectPtr<UObjectHealthComponent> ObjectHealth = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|MaterialActor")
	TObjectPtr<UObjectFractureComponent> ObjectFracture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialActor")
	bool bRandomizeMeshOnSpawn = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialActor")
	TArray<TObjectPtr<UStaticMesh>> MeshVariants;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialActor")
	bool bRandomizeScaleOnSpawn = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialActor|Material", meta=(DisplayName="Randomise Material"))
	bool bRandomizeMaterialOnSpawn = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialActor|Material", meta=(EditCondition="bRandomizeMaterialOnSpawn", EditConditionHides))
	TArray<FMaterialActorMaterialSlot> RandomizedMaterialSlots;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialActor", meta=(ClampMin="0.05"))
	float ItemMinScale = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialActor", meta=(ClampMin="0.05"))
	float ItemMaxScale = 1.25f;

protected:
	virtual void BeginPlay() override;
	virtual UPrimitiveComponent* ResolveMaterialRuntimePrimitive() const;
	virtual void HandlePostItemMeshConfiguration();

	void ApplyMeshVariant();
	void ApplyRandomizedScale();
	void ApplyRandomizedMaterials();
};

