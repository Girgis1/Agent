// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ResourceComponent.generated.h"

class UPrimitiveComponent;
class UResourceCompositionAsset;
class UResourceDefinitionAsset;

UCLASS(ClassGroup=(Factory), meta=(BlueprintSpawnableComponent))
class UResourceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UResourceComponent();

	UFUNCTION(BlueprintPure, Category="Factory|Resource")
	bool HasDefinedResources() const;

	UFUNCTION(BlueprintPure, Category="Factory|Resource")
	bool HasSimpleResourceDefinition() const;

	UFUNCTION(BlueprintPure, Category="Factory|Resource")
	FName GetPrimaryResourceId() const;

	UFUNCTION(BlueprintPure, Category="Factory|Resource")
	int32 ResolveSimpleResourceQuantityScaled(const UPrimitiveComponent* SourcePrimitive) const;

	UFUNCTION(BlueprintPure, Category="Factory|Resource")
	float ResolveEffectiveMassKg(const UPrimitiveComponent* SourcePrimitive) const;

	UFUNCTION(BlueprintPure, Category="Factory|Resource")
	float ResolveRecoveryScalar() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Resource")
	TObjectPtr<UResourceCompositionAsset> Composition = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Resource|Simple")
	TObjectPtr<UResourceDefinitionAsset> PrimaryResource = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Resource|Simple", meta=(ClampMin="0.0", UIMin="0.0"))
	float PrimaryResourceUnitsPerKg = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Resource")
	bool bUsePhysicsMass = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Resource", meta=(ClampMin="0.0", UIMin="0.0"))
	float MassMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Resource", meta=(ClampMin="0.0", UIMin="0.0"))
	float MassOverrideKg = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Factory|Resource", meta=(ClampMin="0.0", UIMin="0.0"))
	float RecoveryMultiplier = 1.0f;
};
