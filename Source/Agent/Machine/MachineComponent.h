// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "MachineComponent.generated.h"

class UInputVolume;
class UOutputVolume;
class UPrimitiveComponent;
class URecipeAsset;
class UResourceDefinitionAsset;

UENUM(BlueprintType)
enum class EMachineRuntimeState : uint8
{
	Idle,
	WaitingForInput,
	Crafting,
	WaitingForOutput,
	Paused,
	Error
};

UCLASS(ClassGroup=(Machine), meta=(BlueprintSpawnableComponent))
class UMachineComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UMachineComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category="Machine")
	void SetInputVolume(UInputVolume* InInputVolume);

	UFUNCTION(BlueprintCallable, Category="Machine")
	void SetOutputVolume(UOutputVolume* InOutputVolume);

	int32 AddInputResourceScaled(FName ResourceId, int32 QuantityScaled);

	bool AddInputResourcesScaledAtomic(const TMap<FName, int32>& ResourceQuantitiesScaled);

	UFUNCTION(BlueprintCallable, Category="Machine")
	int32 AddInputResourceUnits(FName ResourceId, int32 QuantityUnits);

	UFUNCTION(BlueprintPure, Category="Machine")
	float GetStoredUnits(FName ResourceId) const;

	UFUNCTION(BlueprintPure, Category="Machine")
	float GetTotalStoredUnits() const;

	UFUNCTION(BlueprintPure, Category="Machine")
	float GetStoredMassKg() const;

	UFUNCTION(BlueprintPure, Category="Machine")
	float GetCraftProgress01() const;

	UFUNCTION(BlueprintPure, Category="Machine")
	float GetCraftRemainingSeconds() const;

	UFUNCTION(BlueprintCallable, Category="Machine")
	void ClearStoredResources();

	UFUNCTION(BlueprintCallable, Category="Machine")
	void SetSpeed(float NewSpeed);

	UFUNCTION(BlueprintCallable, Category="Machine")
	void SetActiveRecipeIndex(int32 NewIndex);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine", meta=(ClampMin="0.01", UIMin="0.01"))
	float Speed = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine")
	bool bRecipeAny = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine", meta=(ClampMin="0", UIMin="0"))
	int32 ActiveRecipeIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine")
	TArray<TObjectPtr<URecipeAsset>> Recipes;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Input", meta=(ClampMin="0", UIMin="0"))
	int32 CapacityUnits = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Input")
	bool bUseWhitelist = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Input", meta=(EditCondition="bUseWhitelist"))
	TArray<TObjectPtr<UResourceDefinitionAsset>> WhitelistResources;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Input")
	bool bUseBlacklist = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Input", meta=(EditCondition="bUseBlacklist"))
	TArray<TObjectPtr<UResourceDefinitionAsset>> BlacklistResources;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine")
	FName MachineTag = TEXT("Machine");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Mass")
	bool bApplyStoredMassToOwner = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|Mass", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bApplyStoredMassToOwner"))
	float StoredMassMultiplier = 1.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Machine")
	EMachineRuntimeState RuntimeState = EMachineRuntimeState::Idle;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Machine")
	FText RuntimeStatus;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Machine")
	TMap<FName, int32> StoredResourcesScaled;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Machine")
	int32 TotalStoredScaled = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Machine")
	TMap<FName, int32> PendingOutputScaled;

protected:
	struct FRecipeView
	{
		const URecipeAsset* Asset = nullptr;
		FName AllowedMachineTag = NAME_None;
		float CraftTimeSeconds = 0.0f;
		TMap<FName, int32> InputScaled;
		TMap<FName, int32> InputGroupsScaled;
		TMap<FName, int32> OutputScaled;
		bool bConvertOtherInputsToScrap = false;
		float ScrapRecoveryScalar = 1.0f;
		bool bIsValid = false;
	};

	bool ResolveMachineVolumes();
	bool IsResourceAllowed(FName ResourceId) const;
	int32 GetRemainingCapacityScaled() const;
	bool ConsumeResourceScaled(FName ResourceId, int32 QuantityScaled);
	bool BuildRecipeView(const URecipeAsset* RecipeAsset, FRecipeView& OutRecipeView) const;
	const UResourceDefinitionAsset* ResolveResourceDefinition(FName ResourceId) const;
	float GetResourceEquivalentPerUnit(FName ResourceId) const;
	FName GetResourceRefiningGroup(FName ResourceId) const;
	double GetAvailableEquivalentUnitsForGroup(FName GroupId) const;
	bool CanSatisfyGroupRequirements(const TMap<FName, int32>& GroupRequirementsScaled) const;
	bool ConsumeGroupRequirements(const TMap<FName, int32>& GroupRequirementsScaled);
	void ExtractOtherResourcesAsScrap(const FRecipeView& RecipeView, TMap<FName, int32>& OutScrapByproductScaled);
	bool CanCraftRecipe(const FRecipeView& RecipeView) const;
	bool SelectCraftableRecipe(FRecipeView& OutRecipeView) const;
	bool TryStartCraft(const FRecipeView& RecipeView);
	void AdvanceCraft(float DeltaTime);
	void CompleteCraft();
	void FlushPendingOutput();
	void SetRuntimeState(EMachineRuntimeState NewState, const TCHAR* Message);
	void RebuildResourceMassLookup();
	void RegisterResourceMass(const UResourceDefinitionAsset* ResourceDefiniti