// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ResourceSpawnerMachine.generated.h"

class AFactoryPayloadActor;
class UArrowComponent;
class UBoxComponent;
class UStaticMeshComponent;
class USceneComponent;

UCLASS()
class AResourceSpawnerMachine : public AActor
{
	GENERATED_BODY()

public:
	AResourceSpawnerMachine();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Spawner")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Spawner")
	TObjectPtr<UBoxComponent> SupportCollision = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Spawner")
	TObjectPtr<UStaticMeshComponent> MachineMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Spawner")
	TObjectPtr<UArrowComponent> OutputArrow = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Spawner")
	TSubclassOf<AFactoryPayloadActor> PayloadActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Spawner")
	float SpawnInterval = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Spawner")
	float SpawnImpulse = 250.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Spawner")
	float OutputClearanceRadius = 28.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Spawner")
	FName SpawnedPayloadType = TEXT("Metal");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Spawner", meta=(ClampMin="1"))
	int32 SpawnedPayloadUnits = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Spawner")
	bool bAutoSpawnEnabled = true;

protected:
	bool CanSpawnPayload() const;
	void SpawnPayload();

	float SpawnElapsed = 0.0f;
};
