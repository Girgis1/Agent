// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BlackHoleBackpackActor.generated.h"

class UBlackHoleBackpackLinkComponent;
class UAgentBatteryComponent;
class UInputVolume;
class UMaterialInstanceDynamic;
class UPointLightComponent;
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

	UFUNCTION(BlueprintCallable, Category="BlackHole|Backpack|Portal")
	void SetPortalEnabled(bool bInEnabled);

	UFUNCTION(BlueprintCallable, Category="BlackHole|Backpack|Portal")
	void TogglePortalEnabled();

	UFUNCTION(BlueprintPure, Category="BlackHole|Backpack|Portal")
	bool IsPortalEnabled() const { return bPortalEnabled; }

	UFUNCTION(BlueprintPure, Category="BlackHole|Backpack|Battery")
	float GetPortalBatteryPercent() const;

	UFUNCTION(BlueprintPure, Category="BlackHole|Backpack|Battery")
	bool IsPortalBatteryDepleted() const;

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
	void RefreshIntakeVolumeState();
	void RefreshPortalStateVisuals();
	void UpdatePortalBatteryRuntimeState();
	void InitializeAccessoryChargeMaterialInstances();
	void RefreshAccessoryChargeMaterialState();
	bool IsBackpackCharging() const;

	UFUNCTION()
	void HandlePortalBatteryDepleted();

	UFUNCTION()
	void HandlePortalBatteryChargeChanged(float NewCharge);

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
	TObjectPtr<UBlackHoleBackpackLinkComponent> LinkComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BlackHole|Backpack|Battery")
	TObjectPtr<UAgentBatteryComponent> BackpackBattery = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BlackHole|Backpack")
	TObjectPtr<UPointLightComponent> PortalStateLight = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Link")
	FName LinkId = TEXT("BlackHole_Default");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Backpack")
	bool bEnableIntakeWhenAttached = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Backpack")
	bool bEnableIntakeWhenDeployed = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Backpack|Portal")
	bool bStartPortalEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Backpack|Portal")
	bool bEnablePortalWhenDeployed = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Backpack|Portal")
	bool bDisablePortalWhenAttached = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Backpack|Portal|Light")
	bool bEnablePortalStateLight = true;

	/** When enabled, runtime portal state only shows/hides the light and preserves all light component settings from Blueprint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Backpack|Portal|Light")
	bool bUseManualPortalLightSettings = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Backpack|Portal|Light", meta=(ClampMin="0.0", UIMin="0.0"))
	float PortalLightOnIntensity = 4000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Backpack|Portal|Light", meta=(ClampMin="0.0", UIMin="0.0"))
	float PortalLightOffIntensity = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Backpack|Portal|Light")
	FLinearColor PortalLightOnColor = FLinearColor(0.1f, 0.8f, 1.0f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Backpack|Portal|Light")
	FLinearColor PortalLightOffColor = FLinearColor::Black;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Backpack|Charge|Material")
	bool bUseAccessoryChargeMaterial = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Backpack|Charge|Material")
	FName AccessoryChargeMaterialSlotName = TEXT("AccessoryMaterial");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Backpack|Charge|Material")
	FName AccessoryEmissiveColorParameterName = TEXT("EmissiveColor");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Backpack|Charge|Material")
	FName AccessoryEmissiveIntensityParameterName = TEXT("EmissiveIntensity");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Backpack|Charge|Material")
	FLinearColor AccessoryChargingEmissiveColor = FLinearColor(0.0f, 1.0f, 0.2f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Backpack|Charge|Material", meta=(ClampMin="0.0", UIMin="0.0"))
	float AccessoryChargingEmissiveIntensity = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Backpack|Charge|Material")
	FLinearColor AccessoryIdleEmissiveColor = FLinearColor::Black;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Backpack|Charge|Material", meta=(ClampMin="0.0", UIMin="0.0"))
	float AccessoryIdleEmissiveIntensity = 0.0f;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Backpack")
	bool bIsDeployed = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BlackHole|Backpack|Portal")
	bool bPortalEnabled = true;

	UPROPERTY(Transient)
	FVector BaseItemMeshRelativeScale = FVector::OneVector;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> AccessoryChargeMaterialInstances;
};
