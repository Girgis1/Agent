// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Material/MaterialActor.h"
#include "ObjectFragmentActor.generated.h"

class UBoxComponent;
class UPrimitiveComponent;

UCLASS()
class AObjectFragmentActor : public AMaterialActor
{
	GENERATED_BODY()

public:
	AObjectFragmentActor();

	UFUNCTION(BlueprintCallable, Category="Objects|Fracture")
	void RefreshPhysicsProxyFromItemMesh();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Objects|Fracture")
	TObjectPtr<UBoxComponent> PhysicsProxy = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture", meta=(ClampMin="0.5", ClampMax="1.0", UIMin="0.5", UIMax="1.0"))
	float PhysicsProxyExtentScale = 0.9f;

protected:
	virtual UPrimitiveComponent* ResolveMaterialRuntimePrimitive() const override;
	virtual void HandlePostItemMeshConfiguration() override;
};
