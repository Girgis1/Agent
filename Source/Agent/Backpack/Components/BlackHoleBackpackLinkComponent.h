// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BlackHoleBackpackLinkComponent.generated.h"

class ABlackHoleBackpackActor;
class UBlackHoleOutputSinkComponent;
class UMachineComponent;

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

protected:
	bool ResolveDependencies();
	UBlackHoleOutputSinkComponent* ResolveLinkedSink() const;
	bool ShouldTransferForCurrentDeploymentState() const;

protected:
	UPROPERTY(Transient)
	TObjectPtr<UMachineComponent> SourceMachineComponent = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<ABlackHoleBackpackActor> OwningBackItem = nullptr;

	float TimeUntilNextTransfer = 0.0f;

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
};
