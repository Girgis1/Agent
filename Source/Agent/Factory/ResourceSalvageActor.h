// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ResourceSalvageActor.generated.h"

class UResourceComponent;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS()
class AResourceSalvageActor : public AActor
{
	GENERATED_BODY()

public:
	AResourceSalvageActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Salvage")
	TObjectPtr<UStaticMeshComponent> ItemMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Salvage")
	TObjectPtr<UResourceComponent> ResourceData = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Salvage")
	bool bRandomizeMeshOnSpawn = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Salvage")
	TArray<TObjectPtr<UStaticMesh>> MeshVariants;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Salvage")
	bool bRandomizeScaleOnSpawn = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Salvage", meta=(ClampMin="0.05"))
	float ItemMinScale = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Salvage", meta=(ClampMin="0.05"))
	float ItemMaxScale = 1.25f;

protected:
	virtual void BeginPlay() override;

	void ApplyMeshVariant();
	void ApplyRandomizedScale();
};
