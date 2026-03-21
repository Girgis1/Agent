// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WorldUISystemTypes.h"
#include "WorldUIInteractorComponent.generated.h"

class AWorldUIActor;
class UWorldUIHostComponent;

UCLASS(ClassGroup=(WorldUI), meta=(BlueprintSpawnableComponent))
class UWorldUIInteractorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWorldUIInteractorComponent();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category="World UI")
	void ClearWorldUI(bool bDestroyActor = false);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI")
	bool bWorldUIEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Interaction")
	TEnumAsByte<ECollisionChannel> InteractionTraceChannel = ECC_Camera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Interaction", meta=(ClampMin="100.0", UIMin="100.0", Units="cm"))
	float InteractionTraceDistance = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Refresh", meta=(ClampMin="0.01", UIMin="0.01"))
	float ModelRefreshInterval = 0.1f;

protected:
	bool ResolveViewerPose(FVector& OutViewLocation, FVector& OutViewDirection) const;
	bool ResolveScannerModeActive() const;
	UWorldUIHostComponent* ResolveBestHost(const FVector& ViewLocation, const FVector& ViewDirection, bool bScannerModeActive) const;
	bool DoesHostPassVisibility(
		UWorldUIHostComponent* HostComponent,
		const FVector& ViewLocation,
		const FVector& ViewDirection,
		bool bScannerModeActive,
		float& OutDistanceToHost,
		bool& bOutIsAimed) const;
	bool HasLineOfSightToHost(UWorldUIHostComponent* HostComponent, const FVector& ViewLocation, const FVector& TargetLocation) const;
	bool IsActorPartOfHost(AActor* CandidateActor, const UWorldUIHostComponent* HostComponent) const;
	AWorldUIActor* EnsureUIActorForHost(UWorldUIHostComponent* HostComponent);
	void UpdateActiveWorldUI(float DeltaTime, const FVector& ViewLocation, const FVector& ViewDirection, bool bScannerModeActive);

	UPROPERTY(Transient)
	TWeakObjectPtr<UWorldUIHostComponent> ActiveHostComponent;

	UPROPERTY(Transient)
	TWeakObjectPtr<AWorldUIActor> ActiveUIActor;

	UPROPERTY(Transient)
	FWorldUIModel ActiveModel;

	UPROPERTY(Transient)
	float TimeUntilModelRefresh = 0.0f;
};
