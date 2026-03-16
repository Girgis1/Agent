// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Dirty/DirtyTypes.h"
#include "DirtBrushComponent.generated.h"

class UTexture2D;

UCLASS(ClassGroup=(Dirty), meta=(BlueprintSpawnableComponent))
class UDirtBrushComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDirtBrushComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category="Dirty")
	void SetBrushActive(bool bNewActive);

	UFUNCTION(BlueprintPure, Category="Dirty")
	bool IsBrushActive() const { return bBrushActive; }

	UFUNCTION(BlueprintPure, Category="Dirty")
	FDirtBrushStamp BuildBrushStamp() const;

	UFUNCTION(BlueprintCallable, Category="Dirty")
	bool ApplyHitBrush(const FHitResult& Hit, float DeltaTime);

	UFUNCTION(BlueprintCallable, Category="Dirty")
	void ApplyTrailBrush(const FVector& Start, const FVector& End, float DeltaTime);

	UFUNCTION(BlueprintCallable, Category="Dirty")
	void ApplyAreaBrush(const FVector& Origin, float DeltaTime);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty")
	bool bBrushEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty")
	bool bStartActive = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty")
	EDirtBrushMode BrushMode = EDirtBrushMode::Clean;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty")
	EDirtBrushApplicationType ApplicationType = EDirtBrushApplicationType::Hit;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty")
	TObjectPtr<UTexture2D> BrushTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty", meta=(ClampMin="1.0", UIMin="1.0", Units="cm"))
	float BrushSizeCm = 36.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty", meta=(ClampMin="0.0", UIMin="0.0"))
	float BrushStrengthPerSecond = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float BrushHardness = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty", meta=(ClampMin="0.01", UIMin="0.01"))
	float ApplyIntervalSeconds = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Trail", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float TrailTraceStartOffsetCm = 24.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Trail", meta=(ClampMin="1.0", UIMin="1.0", Units="cm"))
	float TrailTraceLengthCm = 120.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Trail", meta=(ClampMin="1.0", UIMin="1.0", Units="cm"))
	float TrailMinSegmentLengthCm = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Trail")
	bool bTrailUseOwnerDownVector = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Area", meta=(ClampMin="1.0", UIMin="1.0", Units="cm"))
	float AreaRadiusCm = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Area", meta=(ClampMin="1", UIMin="1"))
	int32 AreaTraceCount = 8;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Area", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float AreaTraceHeightCm = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Area", meta=(ClampMin="1.0", UIMin="1.0", Units="cm"))
	float AreaTraceDepthCm = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Dirty|Debug")
	bool bDrawDebug = false;

protected:
	bool TryApplyBrushToHit(const FHitResult& Hit, float DeltaTime);
	void TickTrailBrush(float DeltaTime);
	void TickAreaBrush(float DeltaTime);
	FVector ResolveTraceDownVector() const;
	FCollisionQueryParams BuildTraceParams() const;

	UPROPERTY(Transient)
	bool bBrushActive = false;

	UPROPERTY(Transient)
	bool bHasLastTrailLocation = false;

	UPROPERTY(Transient)
	FVector LastTrailLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	float TimeUntilNextApplication = 0.0f;
};
