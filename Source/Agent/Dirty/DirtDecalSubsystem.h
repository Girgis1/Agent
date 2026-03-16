// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dirty/DirtyTypes.h"
#include "Subsystems/WorldSubsystem.h"
#include "DirtDecalSubsystem.generated.h"

class ADirtDecalActor;

UCLASS()
class UDirtDecalSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Deinitialize() override;

	void RegisterDirtDecal(ADirtDecalActor* DirtDecal);
	void UnregisterDirtDecal(ADirtDecalActor* DirtDecal);

	UFUNCTION(BlueprintCallable, Category="Dirty|Decal")
	bool ApplyBrushAtHit(const FHitResult& Hit, const FDirtBrushStamp& Stamp, float DeltaTime);

	UFUNCTION(BlueprintCallable, Category="Dirty|Decal")
	bool ApplyBrushAtWorldPoint(const FVector& WorldPoint, const FDirtBrushStamp& Stamp, float DeltaTime);

	UFUNCTION(BlueprintPure, Category="Dirty|Decal")
	int32 GetRegisteredDirtDecalCount() const { return RegisteredDirtDecals.Num(); }

	UFUNCTION(BlueprintCallable, Category="Dirty|Decal")
	void FindDecalsNearPoint(const FVector& WorldPoint, float BrushRadiusCm, TArray<ADirtDecalActor*>& OutDecals);

protected:
	void CompactRegisteredDecals();

	UPROPERTY(Transient)
	TArray<TWeakObjectPtr<ADirtDecalActor>> RegisteredDirtDecals;
};
