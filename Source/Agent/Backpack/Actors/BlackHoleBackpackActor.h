// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BlackHoleBackpackActor.generated.h"

class UBlackHoleBackpackLinkComponent;
class UInputVolume;
class UMachineComponent;
class USceneComponent;
class UStaticMeshComponent;

UCLASS()
class ABlackHoleBackpackActor : public AActor
{
	GENERATED_BODY()

public:
	ABlackHoleBackpackActor();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category="Backpack")
	void AttachToCarrier(USceneComponent* CarrierComponent, FName SocketName, const FTransform& FineTuneOffset);

	UFUNCTION(BlueprintCallable, Category="Backpack")
	void DeployToWorld(const FVector& InitialImpulse = FVector::ZeroVector);

	UFUNCTION(BlueprintCallable, Category="Backpack")
	void SetDeployedState(bool bInDeployed, const FVector& InitialImpulse = FVector::ZeroVector);

	UFUNCTION(BlueprintPure, Category="Backpack")
	bool IsItemDeployed() const { return bIsDeployed; }

	UFUNCTION(BlueprintPure, Category="Backpack")
	FTransform GetSnapAnchorLocalTransform() const;

	UFUNCTION(BlueprintPure, Category="Backpack")
	FTransform GetSnapAnchorWorldTransform() const;

	UFUNCTION(BlueprintCallable, Category="Backpack|Tuning")
	void SetSnapAnchorLocalTransform(const FTransform& NewLocalTransform);

	UFUNCTION(BlueprintCallable, Category="Backpack|Tuning")
	void SetSnapAnchorLocalLocation(const FVector& NewLocalLocation);

	UFUNCTION(BlueprintCallable, Category="Backpack|Tuning")
	void SetSnapAnchorLocalRotation(const FRotator& NewLocalRotation);

	UFUNCTION(BlueprintPure, Category="Backpack")
	UStaticMeshComponent* GetItemMesh() const { return ItemMesh; }

protected:
	virtual void HandleDeploymentStateChanged(bool bNowDeployed);
	FTransform ResolveAttachParentWorldTransform(const USceneComponent* CarrierComponent, FName SocketName) const;
	FTransform BuildAttachedRootWorldTransform(USceneComponent* CarrierComponent, FName SocketName, const FTransform& FineTuneOffset) const;
	FVector ResolveAttachedRelativeScale(const FTransform& FineTuneOffset) const;
	void SetMeshSimulating(bool bShouldSimulate) const;
	void SetMeshCollisionForCurrentState() const;

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Backpack")
	TObjectPtr<UStaticMeshComponent> ItemMesh = nullptr;

	/** Move this scene component in Blueprint to retune what point on the backpack snaps to the character socket. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Backpack")
	TObjectPtr<USceneComponent> SnapAnchor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Backpack")
	bool bStartDeployed = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Backpack|Deploy")
	bool bSimulatePhysicsWhenDeployed = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Backpack|Deploy")
	bool bEnableGravityWhenDeployed = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Backpack|Collision")
	FName AttachedCollisionProfileName = TEXT("NoCollision");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Backpack|Collision")
	FName DeployedCollisionProfileName = TEXT("PhysicsActor");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BlackHole|Backpack")
	TObjectPtr<UInputVolume> InputVolume = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BlackHole|Backpack")
	TObjectPtr<UMachineComponent> MachineComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BlackHole|Backpack")
	TObjectPtr<UBlackHoleBackpackLinkComponent> LinkComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Link")
	FName LinkId = TEXT("BlackHole_Default");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Backpack")
	bool bEnableIntakeWhenAttached = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Backpack")
	bool bEnableIntakeWhenDeployed = true;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Backpack")
	bool bIsDeployed = false;

	UPROPERTY(Transient)
	FVector BaseItemMeshRelativeScale = FVector::OneVector;
};
