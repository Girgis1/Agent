// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ObjectFractureDefinitionAsset.generated.h"

class AActor;
class UGeometryCollection;
class UStaticMesh;
class UObjectFractureDefinitionAsset;

USTRUCT(BlueprintType)
struct FObjectFractureFragmentEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture")
	FName FragmentId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture")
	TSubclassOf<AActor> FragmentActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture")
	TObjectPtr<UStaticMesh> StaticMeshOverride = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture")
	FTransform RelativeTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture", meta=(ClampMin="0.0", UIMin="0.0"))
	float MassRatio = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture")
	FVector AdditionalImpulse = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture", meta=(ClampMin="0.0", UIMin="0.0"))
	float RandomImpulseMagnitude = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture")
	TObjectPtr<UObjectFractureDefinitionAsset> NextFractureDefinition = nullptr;

	bool IsUsable() const;
};

UCLASS(BlueprintType)
class UObjectFractureDefinitionAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category="Objects|Fracture")
	bool HasUsableFragments() const;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objects|Fracture", meta=(ToolTip="Optional authoring reference. Gameplay fracture still spawns normal fragment actors so pieces remain easy to pick up and damage individually."))
	TObjectPtr<UGeometryCollection> GeometryCollectionReference = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objects|Fracture")
	TSubclassOf<AActor> DefaultFragmentActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Objects|Fracture")
	TArray<FObjectFractureFragmentEntry> Fragments;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture")
	bool bTransferSourceVelocity = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture")
	bool bTransferSourceAngularVelocity = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture")
	bool bInitializeFragmentHealthFromMass = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture")
	bool bPropagateDamagedPenaltyToFragments = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture", meta=(ClampMin="0.0", UIMin="0.0"))
	float OutwardImpulseStrength = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture", meta=(ClampMin="0.0", UIMin="0.0"))
	float RandomImpulseStrength = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture")
	bool bHideSourceActorOnFracture = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture")
	bool bDisableSourceCollisionOnFracture = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture")
	bool bDestroySourceActorAfterFracture = true;
};
