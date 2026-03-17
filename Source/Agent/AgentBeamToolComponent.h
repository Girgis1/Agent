// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Dirty/DirtyTypes.h"
#include "Objects/Types/ObjectDamageTypes.h"
#include "AgentBeamToolComponent.generated.h"

class UObjectHealthComponent;
class UObjectSliceComponent;
class UPrimitiveComponent;

UENUM(BlueprintType)
enum class EAgentBeamMode : uint8
{
	Damage UMETA(DisplayName="Damage"),
	Heal UMETA(DisplayName="Heal")
};

UENUM(BlueprintType)
enum class EAgentBeamSliceState : uint8
{
	Inactive UMETA(DisplayName="Inactive"),
	Warmup UMETA(DisplayName="Warmup"),
	Tracking UMETA(DisplayName="Tracking"),
	Ready UMETA(DisplayName="Ready"),
	Rejected UMETA(DisplayName="Rejected")
};

USTRUCT(BlueprintType)
struct FAgentBeamTraceState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Beam")
	bool bHasHit = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Beam")
	bool bHasHealthTarget = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Beam")
	FVector ViewOrigin = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Beam")
	FVector VisualOrigin = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Beam")
	FVector EndPoint = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Beam")
	FVector ImpactPoint = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Beam")
	FVector ImpactNormal = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Beam")
	TObjectPtr<AActor> HitActor = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Beam")
	TObjectPtr<UPrimitiveComponent> HitComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Beam")
	float LastAppliedAmount = 0.0f;
};

USTRUCT(BlueprintType)
struct FAgentBeamSliceTraceState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Beam|Slice")
	bool bSliceModeActive = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Beam|Slice")
	float WarmupAlpha = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Beam|Slice")
	EAgentBeamSliceState SliceState = EAgentBeamSliceState::Inactive;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Beam|Slice")
	FVector BeamOrigin = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Beam|Slice")
	FVector FirstEntryPoint = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Beam|Slice")
	FVector CurrentEntryPoint = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Beam|Slice")
	FVector ExitPoint = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Beam|Slice")
	FVector PlaneNormal = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Beam|Slice")
	FVector PlaneAxisX = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Beam|Slice")
	FVector PlaneAxisY = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Beam|Slice")
	TObjectPtr<AActor> TargetActor = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Beam|Slice")
	TObjectPtr<UPrimitiveComponent> TargetComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Beam|Slice")
	FString StatusText;
};

UCLASS(ClassGroup=(Agent), meta=(BlueprintSpawnableComponent))
class UAgentBeamToolComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAgentBeamToolComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category="Beam")
	void StartBeam();

	UFUNCTION(BlueprintCallable, Category="Beam")
	void StopBeam();

	UFUNCTION(BlueprintCallable, Category="Beam")
	void SetBeamMode(EAgentBeamMode NewMode);

	UFUNCTION(BlueprintCallable, Category="Beam")
	void CycleBeamMode(int32 Direction = 1);

	UFUNCTION(BlueprintCallable, Category="Beam")
	void SetBeamScale(float NewBeamScale);

	UFUNCTION(BlueprintCallable, Category="Beam|Slice")
	void SetSliceIntentActive(bool bNewSliceIntentActive);

	UFUNCTION(BlueprintCallable, Category="Beam")
	void SetBeamPose(const FVector& InViewOrigin, const FVector& InViewDirection, const FVector& InVisualOrigin);

	UFUNCTION(BlueprintCallable, Category="Beam")
	void ClearBeamPose();

	UFUNCTION(BlueprintPure, Category="Beam")
	bool IsBeamEnabled() const { return bBeamEnabled; }

	UFUNCTION(BlueprintPure, Category="Beam")
	bool IsBeamActive() const { return bBeamActive; }

	UFUNCTION(BlueprintPure, Category="Beam")
	bool HasBeamPose() const { return bHasBeamPose; }

	UFUNCTION(BlueprintPure, Category="Beam")
	EAgentBeamMode GetBeamMode() const { return BeamMode; }

	UFUNCTION(BlueprintPure, Category="Beam")
	float GetBeamScale() const { return BeamScale; }

	UFUNCTION(BlueprintPure, Category="Beam|Slice")
	bool IsSliceIntentActive() const { return bSliceIntentActive; }

	UFUNCTION(BlueprintPure, Category="Beam")
	float GetEffectiveTraceRadius() const;

	UFUNCTION(BlueprintPure, Category="Beam")
	FLinearColor GetCurrentBeamColor() const;

	UFUNCTION(BlueprintPure, Category="Beam")
	const FAgentBeamTraceState& GetTraceState() const { return TraceState; }

	UFUNCTION(BlueprintPure, Category="Beam|Slice")
	const FAgentBeamSliceTraceState& GetSliceTraceState() const { return SliceTraceState; }

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam")
	bool bBeamEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam")
	EAgentBeamMode BeamMode = EAgentBeamMode::Damage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float BeamRange = 3000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam", meta=(ClampMin="0.01", UIMin="0.01"))
	float BeamScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float BaseTraceRadius = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam|Damage", meta=(ClampMin="0.0", UIMin="0.0"))
	float DamagePerSecond = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam|Heal", meta=(ClampMin="0.0", UIMin="0.0"))
	float HealingPerSecond = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam|Dirty")
	bool bAffectDirtySurfaces = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam|Dirty")
	EDirtBrushMode DirtyBrushMode = EDirtBrushMode::Clean;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam|Dirty")
	TObjectPtr<UTexture2D> DirtyBrushTexture = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam|Dirty", meta=(ClampMin="1.0", UIMin="1.0", Units="cm"))
	float DirtyBrushSizeCm = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam|Dirty", meta=(ClampMin="0.0", UIMin="0.0"))
	float DirtyBrushStrengthPerSecond = 0.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam|Dirty", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float DirtyBrushHardness = 0.7f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam|Damage")
	EObjectDamageSourceKind DamageSourceKind = EObjectDamageSourceKind::Tool;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam")
	FName BeamSourceName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam|Trace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam|Trace")
	bool bIgnoreOwner = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam|Slice")
	bool bSliceModeEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam|Slice", meta=(ClampMin="0.0", UIMin="0.0", Units="s"))
	float SliceWarmupDuration = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam|Slice", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float SliceTraceRadius = 1.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam|Slice", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float SliceMinimumDragDistanceCm = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam|Debug")
	bool bDrawDebugWhileFiring = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam|Debug")
	bool bDrawSliceDebug = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam|Debug", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bDrawDebugWhileFiring"))
	float DebugLineThickness = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam|Debug", meta=(ClampMin="1.0", UIMin="1.0", EditCondition="bDrawSliceDebug", Units="cm"))
	float SliceDebugPlaneExtent = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam|Visual")
	FLinearColor DamageBeamColor = FLinearColor(1.0f, 0.18f, 0.08f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam|Visual")
	FLinearColor HealBeamColor = FLinearColor(0.12f, 0.9f, 0.45f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Beam|Visual")
	FLinearColor NoTargetBeamColor = FLinearColor(0.35f, 0.65f, 1.0f, 1.0f);

protected:
	FCollisionQueryParams BuildTraceQueryParams() const;
	bool ShouldUseSliceMode() const;
	void UpdateBeamTrace(float DeltaTime);
	void UpdateSliceWarmup(float DeltaTime);
	bool HandleSliceMode(float DeltaTime, const FVector& TraceStart, const FVector& TraceEnd, const FHitResult& HitResult, bool bHasHit);
	bool TryCommitTrackedSlice(const FVector& BeamOrigin, FString& OutFailureReason);
	void DrawBeamDebug() const;
	void DrawSliceDebug() const;
	void ResetTraceState();
	void ResetSliceTraceState(bool bResetWarmupAlpha);
	FName ResolveEffectiveSourceName() const;
	FDirtBrushStamp BuildDirtyBrushStamp() const;

	UPROPERTY(Transient)
	bool bBeamActive = false;

	UPROPERTY(Transient)
	bool bHasBeamPose = false;

	UPROPERTY(Transient)
	FVector CurrentViewOrigin = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector CurrentViewDirection = FVector::ForwardVector;

	UPROPERTY(Transient)
	FVector CurrentVisualOrigin = FVector::ZeroVector;

	UPROPERTY(Transient)
	bool bSliceIntentActive = false;

	UPROPERTY(Transient)
	float SliceWarmupAlpha = 0.0f;

	UPROPERTY(Transient)
	FAgentBeamTraceState TraceState;

	UPROPERTY(Transient)
	FAgentBeamSliceTraceState SliceTraceState;

	UPROPERTY(Transient)
	TObjectPtr<UObjectSliceComponent> ActiveSliceTarget = nullptr;
};
