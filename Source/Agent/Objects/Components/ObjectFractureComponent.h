// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Objects/Assets/ObjectFractureDefinitionAsset.h"
#include "ObjectFractureComponent.generated.h"

class AActor;
class UGeometryCollection;
class UGeometryCollectionComponent;
class UObjectHealthComponent;
class UMaterialComponent;
class UPrimitiveComponent;
class UStaticMesh;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnObjectFracturedSignature, int32, SpawnedFragmentCount);

UENUM(BlueprintType)
enum class EObjectFractureOptionType : uint8
{
	FragmentDefinition UMETA(DisplayName="Fragment Definition"),
	InlineFragments UMETA(DisplayName="Static Mesh Fragments"),
	SpawnActor UMETA(DisplayName="Spawn Actor"),
	GeometryCollection UMETA(DisplayName="Geometry Collection")
};

USTRUCT(BlueprintType)
struct FObjectFractureOption
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture")
	FName OptionId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture")
	EObjectFractureOptionType OptionType = EObjectFractureOptionType::InlineFragments;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture", meta=(ClampMin="0.0", UIMin="0.0"))
	float SelectionWeight = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture", meta=(EditCondition="OptionType == EObjectFractureOptionType::FragmentDefinition", EditConditionHides))
	TObjectPtr<UObjectFractureDefinitionAsset> FractureDefinition = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture", meta=(EditCondition="OptionType == EObjectFractureOptionType::InlineFragments", EditConditionHides, ToolTip="Default actor/BP to spawn for each baked shard. If a fragment entry sets its own actor class, that override wins."))
	TSubclassOf<AActor> InlineDefaultFragmentActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture", meta=(EditCondition="OptionType == EObjectFractureOptionType::InlineFragments", EditConditionHides, ToolTip="Add every baked shard mesh here. This option spawns all usable entries, not one at random."))
	TArray<FObjectFractureFragmentEntry> InlineFragments;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture", meta=(EditCondition="OptionType == EObjectFractureOptionType::SpawnActor", EditConditionHides))
	TSubclassOf<AActor> ReplacementActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture", meta=(EditCondition="OptionType == EObjectFractureOptionType::GeometryCollection", EditConditionHides))
	TObjectPtr<UGeometryCollection> GeometryCollectionAsset = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture", meta=(EditCondition="OptionType == EObjectFractureOptionType::GeometryCollection", EditConditionHides, ToolTip="Optional actor or BP to spawn for this collection. If unset, ObjectGeometryCollectionActor is used."))
	TSubclassOf<AActor> GeometryCollectionActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture")
	FTransform RelativeTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture")
	FVector AdditionalImpulse = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture", meta=(ClampMin="0.0", UIMin="0.0"))
	float RandomImpulseMagnitude = 0.0f;

	bool IsUsable() const;
};

UCLASS(ClassGroup=(Objects), meta=(BlueprintSpawnableComponent))
class UObjectFractureComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UObjectFractureComponent();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category="Objects|Fracture")
	bool HasFractured() const { return bHasFractured; }

	UFUNCTION(BlueprintPure, Category="Objects|Fracture")
	bool CanFracture() const;

	UFUNCTION(BlueprintCallable, Category="Objects|Fracture")
	bool TriggerFracture();

	UFUNCTION(BlueprintPure, Category="Objects|Fracture")
	bool HasFractureOptions() const;

	UFUNCTION(BlueprintPure, Category="Objects|Fracture")
	bool HasFractureDefinitions() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture")
	bool bFractureEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture|Simple", meta=(ToolTip="Default actor or BP used for each baked shard mesh."))
	TSubclassOf<AActor> StaticMeshFragmentActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture|Simple", meta=(ToolTip="Drag one or many baked shard meshes here. If set, fracture spawns all listed meshes. Multi-drag from the Content Browser should populate this array."))
	TArray<TObjectPtr<UStaticMesh>> StaticMeshFragments;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture|Simple", meta=(ToolTip="Optional geometry collection fallback. This is used when no baked shard meshes are assigned."))
	TObjectPtr<UGeometryCollection> GeometryCollection = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture|Simple", meta=(EditCondition="GeometryCollection != nullptr", EditConditionHides, ToolTip="Optional actor or BP to spawn for the geometry collection path. If unset, ObjectGeometryCollectionActor is used."))
	TSubclassOf<AActor> GeometryCollectionActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture|Legacy", AdvancedDisplay)
	TArray<FObjectFractureOption> FractureOptions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture|Legacy", AdvancedDisplay, meta=(EditCondition="FractureOptions.Num > 1"))
	bool bSelectRandomFractureOption = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture|Behavior")
	bool bInitializeSpawnedActorHealthFromMass = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture|Behavior")
	bool bPropagateDamagedPenaltyToSpawnedActor = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture|Behavior")
	bool bTransferVelocityToSpawnedActor = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture|Behavior")
	bool bTransferAngularVelocityToSpawnedActor = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture|Behavior", meta=(ClampMin="0.0", UIMin="0.0"))
	float SpawnedActorOutwardImpulseStrength = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture|Behavior", meta=(ClampMin="0.0", UIMin="0.0"))
	float SpawnedActorRandomImpulseStrength = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture|Behavior")
	bool bHideSourceActorOnSpawnedReplacement = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture|Behavior")
	bool bDisableSourceCollisionOnSpawnedReplacement = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture|Behavior")
	bool bDestroySourceActorAfterSpawnedReplacement = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture|Legacy", AdvancedDisplay)
	TObjectPtr<UObjectFractureDefinitionAsset> FractureDefinition = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture|Legacy", AdvancedDisplay)
	TArray<TObjectPtr<UObjectFractureDefinitionAsset>> FractureDefinitions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture|Legacy", AdvancedDisplay, meta=(EditCondition="FractureDefinitions.Num > 1"))
	bool bSelectRandomFractureDefinition = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture|Behavior")
	bool bAutoFractureOnDepleted = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Objects|Fracture|Behavior")
	bool bOnlyFractureOnce = true;

	UPROPERTY(BlueprintAssignable, Category="Objects|Fracture")
	FOnObjectFracturedSignature OnFractured;

protected:
	UFUNCTION()
	void HandleOwnerHealthDepleted();

	bool HasSimpleStaticMeshFragments() const;
	UObjectFractureDefinitionAsset* BuildSimpleStaticMeshDefinition() const;
	bool TriggerSimpleGeometryCollection();
	const FObjectFractureOption* ResolveFractureOptionForThisBreak() const;
	UObjectFractureDefinitionAsset* BuildInlineFragmentDefinition(const FObjectFractureOption& SelectedOption) const;
	UObjectFractureDefinitionAsset* ResolveFractureDefinitionForThisBreak() const;
	bool TriggerFractureDefinition(UObjectFractureDefinitionAsset* SelectedDefinition);
	bool TriggerSpawnedReplacement(const FObjectFractureOption& SelectedOption);
	UObjectHealthComponent* ResolveHealthComponent() const;
	UPrimitiveComponent* ResolveSourcePrimitive() const;
	UMaterialComponent* ResolveMaterialComponent() const;
	UGeometryCollectionComponent* ResolveGeometryCollectionComponent(AActor* Actor) const;
	void ApplyStaticMeshOverride(AActor* SpawnedActor, UStaticMesh* StaticMeshOverride) const;
	void ApplyGeometryCollectionOverride(AActor* SpawnedActor, UGeometryCollection* GeometryCollectionAsset) const;
	void ApplySpawnedActorState(
		AActor* SpawnedActor,
		const TMap<FName, int32>& SourceResourceQuantitiesScaled,
		float SourceCurrentMassKg,
		float SourceBaseMassBeforeMultiplierKg,
		float InheritedDamagedPenaltyPercent) const;
	void ApplySpawnedActorVelocityAndImpulses(
		AActor* SpawnedActor,
		const FVector& SourceLinearVelocity,
		const FVector& SourceAngularVelocityDeg,
		const FVector& SourceLocation,
		const FObjectFractureOption& SelectedOption) const;
	void DisableSourceActorCollisionForFracture() const;
	void ApplySourceActorPostFracture(bool bDisableCollision, bool bHideActor, bool bDestroyActor) const;

	UPROPERTY(Transient)
	bool bHasFractured = false;
};
