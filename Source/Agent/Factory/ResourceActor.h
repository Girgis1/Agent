// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ResourceActor.generated.h"

class UResourceComponent;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS()
class AResourceActor : public AActor
{
	GENERATED_BODY()

public:
	AResourceActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Resource")
	TObjectPtr<UStaticMeshComponent> ItemMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Resource")
	TObjectPtr<UResourceComponent> ResourceData = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Resource")
	bool bRandomizeMeshOnSpawn = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Resource")
	TArray<TObjectPtr<UStaticMesh>> MeshVariants;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Resource")
	bool bRandomizeScaleOnSpawn = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Resource", meta=(ClampMin="0.05"))
	float ItemMinScale = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Resource", meta=(ClampMin="0.05"))
	float ItemMaxScale = 1.25f;

protected:
	virtual void BeginPlay() override;

	void ApplyMeshVariant();
	void ApplyRandomizedScale();
};
