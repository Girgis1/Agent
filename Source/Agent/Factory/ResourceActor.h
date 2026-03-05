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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|ResourceActor")
	TObjectPtr<UStaticMeshComponent> ItemMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|ResourceActor")
	TObjectPtr<UResourceComponent> ResourceData = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|ResourceActor")
	bool bRandomizeMeshOnSpawn = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|ResourceActor")
	TArray<TObjectPtr<UStaticMesh>> MeshVariants;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|ResourceActor")
	bool bRandomizeScaleOnSpawn = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|ResourceActor", meta=(ClampMin="0.05"))
	float ItemMinScale = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|ResourceActor", meta=(ClampMin="0.05"))
	float ItemMaxScale = 1.25f;

protected:
	virtual void BeginPlay() override;

	void ApplyMeshVariant();
	void ApplyRandomizedScale();
};
