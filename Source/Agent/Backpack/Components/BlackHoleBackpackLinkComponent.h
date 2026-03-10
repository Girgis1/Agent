// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BlackHoleBackpackLinkComponent.generated.h"

class ABlackHoleBackpackActor;
class AActor;
class UBlackHoleOutputSinkComponent;
class UInputVolume;
class UMachineComponent;
class UPrimitiveComponent;

USTRUCT(BlueprintType)
struct FBlackHoleTeleportQueuedItem
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Teleport")
	TSubclassOf<AActor> ActorClass = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Teleport")
	TMap<FName, int32> ResourceQuantitiesScaled;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Teleport")
	float TotalMassKg = 0.0f;
};

UCLASS(ClassGroup=(BlackHole), meta=(BlueprintSpawnableComponent))
class UBlackHoleBackpackLinkComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UBlackHoleBackpackLinkComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category="BlackHole|Link")
	void SetSourceMachineComponent(UMachineComponent* InMachineComponent);

	UFUNCTION(BlueprintCallable, Category="BlackHole|Link")
	void SetLinkId(FName InLinkId);

	UFUNCTION(BlueprintCallable, Category="BlackHole|Link")
	bool TransferBufferedResourcesNow();

	UFUNCTION(BlueprintCallable, Category="BlackHole|Teleport")
	bool TransferTeleportItemsNow();

	UFUNCTION(BlueprintCallable, Category="BlackHole|Teleport")
	void SetBlackHoleEnabled(bool bInEnabled);

	UFUNCTION(BlueprintPure, Category="BlackHole|Teleport")
	bool IsBlackHoleEnabled() const { return bBlackHoleEnabled; }

	UFUNCTION(BlueprintPure, Category="BlackHole|Teleport")
	float GetStoredTeleportMassKg() const { return StoredTeleportMassKg; }

	UFUNCTION(BlueprintPure, Category="BlackHole|Teleport")
	int32 GetStoredTeleportItemCount() const { return QueuedTeleportItems.Num(); }

protected:
	bool ResolveDependencies();
	UBlackHoleOutputSinkComponent* ResolveLinkedSink() const;
	bool ShouldTransferForCurrentDeploymentState() const;
	bool TryQueueTeleportActorFromOverlap(AActor* OverlappingActor);
	void ProcessTeleportIntake();
	bool BuildTeleportItemFromActor(AActor* SourceActor, FBlackHoleTeleportQueuedItem& OutQueuedItem);
	float ResolveResourceMassPerUnitKg(FName ResourceId) const;
	float ComputeResourceMassKg(const TMap<FName, int32>& ResourceQuantitiesScaled) const;
	void UpdateBlackHoleIntakeState(bool bSinkActive);
	UInputVolume* ResolveBackpackInputVolume() const;
	UPrimitiveComponent* ResolveResourceSourcePrimitive(AActor* Actor) const;

protected:
	UPROPERTY(Transient)
	TObjectPtr<UMachineComponent> SourceMachineComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ABlackHoleBackpackActor> OwningBackItem = nullptr;

	float TimeUntilNextTransfer = 0.0f;

	UPROPERTY(Transient)
	mutable TMap<FName, float> ResourceMassPerUnitKgById;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BlackHole|Teleport", meta=(AllowPrivateAccess="true"))
	TArray<FBlackHoleTeleportQueuedItem> QueuedTeleportItems;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BlackHole|Teleport", meta=(AllowPrivateAccess="true"))
	float StoredTeleportMassKg = 0.0f;

	bool bBlackHoleEnabled = true;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Link")
	FName LinkId = TEXT("BlackHole_Default");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Link", meta=(ClampMin="0.0", UIMin="0.0"))
	float TransferInterval = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Link", meta=(ClampMin="0", UIMin="0"))
	int32 MaxTransferUnitsPerTick = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Link")
	bool bTransferWhenAttached = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Link")
	bool bTransferWhenDeployed = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Teleport")
	bool bUseTeleportQueueFlow = true;

	/** When true, the backpack stops accepting new teleport input while the machine is activated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Teleport")
	bool bDeactivateBackpackWhileSinkActivated = true;

	/** Zero means unlimited backpack teleport storage mass. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Teleport", meta=(ClampMin="0.0", UIMin="0.0"))
	float MaxStoredTeleportMassKg = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Teleport")
	bool bAllowActorsWithoutMaterialData = true;
};
