// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BlackHoleMachineActor.generated.h"

class UArrowComponent;
class UBlackHoleOutputSinkComponent;
class UBoxComponent;
class UOutputVolume;
class UPrimitiveComponent;
class USceneComponent;
class UStorageVolumeComponent;
class UStaticMeshComponent;
class APawn;
struct FHitResult;

UCLASS()
class ABlackHoleMachineActor : public AActor
{
	GENERATED_BODY()

public:
	ABlackHoleMachineActor();

protected:
	virtual void BeginPlay() override;
	UFUNCTION()
	void OnActivationRangeBeginOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnActivationRangeEndOverlap(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	bool TryAddActivationPawn(AActor* OtherActor);
	void RemoveActivationPawn(AActor* OtherActor);
	void RefreshProximityActivationState();

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BlackHole|Machine")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BlackHole|Machine")
	TObjectPtr<UBoxComponent> SupportCollision = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BlackHole|Machine")
	TObjectPtr<UStaticMeshComponent> MachineMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BlackHole|Machine")
	TObjectPtr<UOutputVolume> OutputVolume = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BlackHole|Machine")
	TObjectPtr<UArrowComponent> OutputArrow = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BlackHole|Machine")
	TObjectPtr<UBlackHoleOutputSinkComponent> OutputSink = nullptr;

	/** Player proximity volume that toggles teleport machine activation on/off. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BlackHole|Machine")
	TObjectPtr<UBoxComponent> ActivationRangeVolume = nullptr;

	/** Built-in storage used by the output volume. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BlackHole|Machine")
	TObjectPtr<UStorageVolumeComponent> StorageVolume = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Link")
	FName LinkId = TEXT("BlackHole_Default");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Machine|Activation")
	bool bUsePawnProximityActivation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Machine|Activation")
	bool bRequirePlayerControlledPawn = true;

protected:
	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<APawn>> ActivationPawnsInRange;
};
