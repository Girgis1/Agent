// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FactoryPayloadActor.generated.h"

class UStaticMeshComponent;

UCLASS()
class AFactoryPayloadActor : public AActor
{
	GENERATED_BODY()

public:
	AFactoryPayloadActor();

	UFUNCTION(BlueprintCallable, Category="Factory|Payload")
	void SetPayloadType(FName NewPayloadType);

	UFUNCTION(BlueprintPure, Category="Factory|Payload")
	FName GetPayloadType() const { return PayloadType; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Payload")
	TObjectPtr<UStaticMeshComponent> PayloadMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Payload")
	FName PayloadType = TEXT("RawOre");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Payload")
	bool bRandomizeScaleOnSpawn = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Payload", meta=(ClampMin="0.05"))
	float PayloadMinScale = 0.3f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Payload", meta=(ClampMin="0.05"))
	float PayloadMaxScale = 0.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Payload", meta=(ClampMin="0.05"))
	float PayloadReferenceScale = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Payload", meta=(ClampMin="0.01"))
	float PayloadReferenceMassKg = 12.0f;

protected:
	virtual void BeginPlay() override;
	void ApplyRandomizedScaleAndMass();
};
