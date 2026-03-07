// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MaterialActor.generated.h"

class UMaterialComponent;
class UStaticMesh;
class UStaticMeshComponent;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialActor")
	bool bRandomizeMeshOnSpawn = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialActor")
	TArray<TObjectPtr<UStaticMesh>> MeshVariants;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialActor")
	bool bRandomizeScaleOnSpawn = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialActor", meta=(ClampMin="0.05"))
	float ItemMinScale = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MaterialActor", meta=(ClampMin="0.05"))
	float ItemMaxScale = 1.25f;

protected:
	virtual void BeginPlay() override;

	void ApplyMeshVariant();
	void ApplyRandomizedScale();
};

