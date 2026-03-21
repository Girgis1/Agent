// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "WorldUISystemTypes.h"
#include "WorldUIHostComponent.generated.h"

class AWorldUIActor;

UCLASS(ClassGroup=(WorldUI), meta=(BlueprintSpawnableComponent))
class UWorldUIHostComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UWorldUIHostComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(BlueprintCallable, Category="World UI")
	AActor* GetResolvedProviderActor() const;

	UFUNCTION(BlueprintCallable, Category="World UI")
	bool BuildWorldUIModel(FWorldUIModel& OutModel) const;

	UFUNCTION(BlueprintCallable, Category="World UI")
	bool HandleWorldUIAction(const FWorldUIAction& Action) const;

	UFUNCTION(BlueprintPure, Category="World UI")
	FVector GetPanelWorldLocation() const;

	UFUNCTION(BlueprintPure, Category="World UI")
	FRotator GetPanelWorldRotation() const;

	UFUNCTION(BlueprintPure, Category="World UI")
	FVector GetLeaderAnchorWorldLocation() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI")
	TSubclassOf<AWorldUIActor> UIActorClass;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI")
	TObjectPtr<AActor> ProviderActorOverride = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Activation")
	EWorldUIActivationMode ActivationMode = EWorldUIActivationMode::InRangeOrAimed;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Activation")
	bool bRequireScannerMode = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Activation", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float ActivationRange = 350.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Activation", meta=(ClampMin="100.0", UIMin="100.0", Units="cm"))
	float AimTraceDistance = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Activation", meta=(ClampMin="-1.0", ClampMax="1.0", UIMin="-1.0", UIMax="1.0"))
	float AimDotThreshold = 0.72f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Activation")
	bool bRequireLineOfSightWhenAimed = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Activation")
	bool bRequireLineOfSightInRange = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Display")
	bool bAllowInteraction = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Display")
	bool bUseLeader = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Display")
	EWorldUIFacingMode FacingMode = EWorldUIFacingMode::LimitedFaceCamera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Display", meta=(ClampMin="0", UIMin="0"))
	int32 Priority = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Display", meta=(ClampMin="0.0", UIMin="0.0"))
	float MaxTiltDegrees = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Display", meta=(ClampMin="0.01", UIMin="0.01", ClampMax="1.0", UIMax="1.0"))
	float MinTiltInterpScaleAtLimit = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Display", meta=(ClampMin="0.2", UIMin="0.2"))
	float TiltEdgeSlowdownExponent = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Leader")
	bool bUseOwnerLocationForLeaderAnchor = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Leader")
	FVector LeaderAnchorLocalOffset = FVector::ZeroVector;
};
