// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/BoxComponent.h"
#include "AtomiserOutputVolume.generated.h"

class AActor;
class AFactoryPayloadActor;
class UArrowComponent;
class UFactoryResourceBankSubsystem;

UCLASS(ClassGroup=(Machine), meta=(BlueprintSpawnableComponent))
class UAtomiserOutputVolume : public UBoxComponent
{
	GENERATED_BODY()

public:
	UAtomiserOutputVolume();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category="Machine|AtomiserOutput")
	void SetOutputArrow(UArrowComponent* InOutputArrow);

	UFUNCTION(BlueprintCallable, Category="Machine|AtomiserOutput")
	void SetOutputPureMaterials(bool bInOutputPureMaterials);

	UFUNCTION(BlueprintCallable, Category="Machine|AtomiserOutput")
	bool EmitOnce();

	UFUNCTION(BlueprintPure, Category="Machine|AtomiserOutput")
	float GetAvailableUnits(FName ResourceId) const;

protected:
	UFactoryResourceBankSubsystem* ResolveSharedResourceBank() const;
	bool ResolveOutputArrow();
	bool CanSpawnPayload() const;
	bool TryEmitOnePayload();
	bool TrySelectResourceToEmit(const UFactoryResourceBankSubsystem* ResourceBank, FName& OutResourceId, int32& OutEmitQuantityScaled) const;
	TSubclassOf<AActor> ResolveSpawnClassForResource(FName ResourceId);
	void RebuildResourceOutputClassLookup();
	FVector GetSpawnLocation() const;
	FRotator GetSpawnRotation() const;

protected:
	float TimeUntilNextSpawn = 0.0f;

	UPROPERTY(Transient)
	TObjectPtr<UArrowComponent> OutputArrow = nullptr;

	UPROPERTY(Transient)
	TMap<FName, TSubclassOf<AActor>> ResourceOutputActorClassById;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|AtomiserOutput")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|AtomiserOutput")
	bool bAutoOutput = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|AtomiserOutput", meta=(ClampMin="0.0", UIMin="0.0"))
	float SpawnInterval = 0.1f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|AtomiserOutput", meta=(ClampMin="1", UIMin="1"))
	int32 OutputChunkUnits = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|AtomiserOutput")
	TSubclassOf<AFactoryPayloadActor> PayloadActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|AtomiserOutput")
	bool bPreferMaterialOutputActorClass = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|AtomiserOutput")
	bool bOutputPureMaterials = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|AtomiserOutput")
	bool bTreatMaterialOutputClassAsSelfContained = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|AtomiserOutput")
	FVector LocalSpawnOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|AtomiserOutput")
	float SpawnImpulse = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|AtomiserOutput", meta=(ClampMin="1.0", UIMin="1.0"))
	float OutputClearanceRadius = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|AtomiserOutput")
	FName OutputArrowComponentName = TEXT("OutputArrow");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|AtomiserOutput|Filter")
	bool bUseResourceWhitelist = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|AtomiserOutput|Filter", meta=(EditCondition="bUseResourceWhitelist"))
	TArray<FName> AllowedResourceIds;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|AtomiserOutput|Filter")
	bool bPreferSingleResource = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|AtomiserOutput|Filter", meta=(EditCondition="bPreferSingleResource"))
	FName PreferredResourceId = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|AtomiserOutput|Debug")
	bool bShowDebugMessages = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Machine|AtomiserOutput|Debug")
	FName DebugName = TEXT("AtomiserOutput");
};
