// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BlackHoleBackpackActor.generated.h"

class UBlackHoleBackpackLinkComponent;
class UAgentBatteryComponent;
class UBackpackMagnetComponent;
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

	UFUNCTION(BlueprintCallable, Category="Backpack")
	void SetMagnetHeldState(bool bInMagnetHeld);

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

	/** Called by charger pads/volumes to enable or disable external charging for this backpack. */
	UFUNCTION(BlueprintCallable, Category="BlackHole|Backpack|Battery")
	void SetExternalChargingFromSource(UObject* SourceContext, bool bEnable);

	UFUNCTION(BlueprintPure, Category="BlackHole|Backpack|Battery")
	bool IsExternalChargingActive() const;

	UFUNCTION(BlueprintPure, Category="Backpack")
	bool IsItemDeployed() const { return bIsDeployed; }

	UFUNCTION(BlueprintPure, Category="Backpack")
	bool IsMagnetHeld() const { return bIsMagnetHeld; }

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

	UFUNCTION(BlueprintPure, Category="Backpack|Magnet")
	UBackpackMagnetComponent* GetBackpackMagnet() const { return BackpackMagnet; }

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
	bool HasAnyValidExternalChargeSource() const;

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Backpack|Collision")
	FName MagnetHeldCollisionProfileName = TEXT("PhysicsActor");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Backpack|Deploy")
	bool bSimulatePhysicsWhenMagnetHeld = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Backpack|Deploy")
	bool bEnableGravityWhenMagnetHeld = true;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BlackHole|Backpack")
	TObjectPtr<UInputVolume> InputVolume = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BlackHole|Backpack")
	TObjectPtr<UBlackHoleBackpackLinkComponent> LinkComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BlackHole|Backpack|Battery")
	TObjectPtr<UAgentBatteryComponent> BackpackBattery = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Backpack|Magnet")
	TObjectPtr<UBackpackMagnetComponent> BackpackMagnet = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BlackHole|Backpack")
	TObjectPtr<UPointLightComponent> PortalStateLight = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Link")
	FName LinkId = TEXT("BlackHole_Default");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Backpack")
	bool bEnableIntakeWhenAttached = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Backpack")
	bool bEnableIntakeWhenDeployed = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Backpack|Portal")
	bool bStartPortalEnabled = false;

	/** When enabled, backpack will always initialize with portal disabled regardless of start flags. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Backpack|Portal")
	bool bForcePortalOffOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Backpack|Portal")
	bool bEnablePortalWhenDeployed = false;

	/** Manual-chaos mode: when deployed to world, backpack only turns on via explicit player toggle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Backpack|Portal")
	bool bRequireManualEnableWhenDeployed = true;

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

	/** Upgrade toggle: allows portal battery to self-charge while portal is off, without needing a charge pad. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Backpack|Battery|Charge")
	bool bEnableSelfRecharge = false;

	/** Allows charging via external volumes/pads that call SetExternalChargingFromSource. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BlackHole|Backpack|Battery|Charge")
	bool bEnableChargeFromExternalSources = true;

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Backpack")
	bool bIsDeployed = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Backpack")
	bool bIsMagnetHeld = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BlackHole|Backpack|Portal")
	bool bPortalEnabled = true;

	UPROPERTY(Transient)
	FVector BaseItemMeshRelativeScale = FVector::OneVector;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> AccessoryChargeMaterialInstances;

	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<UObject>> ExternalChargeSources;

	UPROPERTY(Transient)
	int32 ExternalChargeAnonymousSourceCount = 0;
};
