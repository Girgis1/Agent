// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Material/MaterialTypes.h"
#include "GameFramework/Actor.h"
#include "FactoryPayloadActor.generated.h"

class UStaticMeshComponent;
class UMaterialComponent;

UCLASS()
class AFactoryPayloadActor : public AActor
{
	GENERATED_BODY()

public:
	AFactoryPayloadActor();

	UFUNCTION(BlueprintCallable, Category="Factory|Payload")
	void SetPayloadType(FName NewPayloadType);

	UFUNCTION(BlueprintCallable, Category="Factory|Payload")
	void SetPayloadTypeAndWholeUnits(FName NewPayloadType, int32 QuantityUnits);

	UFUNCTION(BlueprintCallable, Category="Factory|Payload")
	void SetPayloadResource(const FResourceAmount& NewPayloadResource);

	UFUNCTION(BlueprintPure, Category="Factory|Payload")
	FName GetPayloadType() const { return ResourcePayload.ResourceId.IsNone() ? PayloadType : ResourcePayload.ResourceId; }

	UFUNCTION(BlueprintPure, Category="Factory|Payload")
	float GetPayloadUnits() const { return ResourcePayload.GetUnits(); }

	const FResourceAmount& GetPayloadResource() const { return ResourcePayload; }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Payload")
	TObjectPtr<UStaticMeshComponent> PayloadMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|Payload")
	TObjectPtr<UMaterialComponent> ResourceData = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Payload")
	FName PayloadType = TEXT("Metal");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Payload")
	FResourceAmount ResourcePayload;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|Payload", meta=(ClampMin="0"))
	int32 DefaultPayloadUnits = 1;

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

