// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "WorldUISystemTypes.h"
#include "WorldUIButtonActor.generated.h"

class UBoxComponent;
class UPrimitiveComponent;
class USceneComponent;
class UStaticMeshComponent;
struct FHitResult;

UENUM(BlueprintType)
enum class EWorldUIButtonVisualState : uint8
{
	Idle,
	Hovered,
	Pressed
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWorldUIButtonClickedSignature, const FWorldUIAction&, Action);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWorldUIButtonStateChangedSignature, EWorldUIButtonVisualState, NewState);

UCLASS(Blueprintable)
class AWorldUIButtonActor : public AActor
{
	GENERATED_BODY()

public:
	AWorldUIButtonActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category="World UI|Button")
	void SetHovered(bool bInHovered);

	UFUNCTION(BlueprintCallable, Category="World UI|Button")
	void SetPressed(bool bInPressed);

	UFUNCTION(BlueprintCallable, Category="World UI|Button")
	void TriggerClick();

	UFUNCTION(BlueprintCallable, Category="World UI|Button")
	void SetRayTraceInteractionEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category="World UI|Button")
	bool IsHovered() const { return bHovered; }

	UFUNCTION(BlueprintPure, Category="World UI|Button")
	bool IsPressed() const { return bPressed; }

	UFUNCTION(BlueprintPure, Category="World UI|Button")
	EWorldUIButtonVisualState GetVisualState() const { return CurrentVisualState; }

protected:
	UFUNCTION()
	void HandleHoverCollisionBegin(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void HandleHoverCollisionEnd(
		UPrimitiveComponent* OverlappedComponent,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex);

	void RefreshVisualState();
	void UpdateVisualTransform(float DeltaSeconds);
	void RefreshInteractionCollision();

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World UI|Button")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World UI|Button")
	TObjectPtr<USceneComponent> ButtonVisualRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World UI|Button")
	TObjectPtr<UStaticMeshComponent> ButtonMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="World UI|Button")
	TObjectPtr<UBoxComponent> HoverCollision = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Button")
	FWorldUIAction Action;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Button")
	bool bAutoHoverFromOverlap = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Button")
	bool bOnlyHoverPawns = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Button|Raycast")
	bool bEnableRayTraceHit = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Button|Raycast")
	TEnumAsByte<ECollisionChannel> RayTraceChannel = ECC_Camera;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Button", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float HoverLiftDistance = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Button", meta=(ClampMin="0.01", UIMin="0.01"))
	float HoverScaleMultiplier = 1.06f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Button", meta=(ClampMin="0.0", UIMin="0.0", Units="cm"))
	float PressDepthDistance = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Button", meta=(ClampMin="0.01", UIMin="0.01"))
	float PressScaleMultiplier = 0.96f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Button", meta=(ClampMin="0.0", UIMin="0.0"))
	float HoverInterpSpeed = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Button", meta=(ClampMin="0.0", UIMin="0.0"))
	float PressInterpSpeed = 22.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Button", meta=(ClampMin="0.01", UIMin="0.01"))
	float ClickPressedDuration = 0.08f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="World UI|Button")
	FVector ButtonNormalLocal = FVector(1.0f, 0.0f, 0.0f);

	UPROPERTY(BlueprintAssignable, Category="World UI|Button")
	FWorldUIButtonClickedSignature OnButtonClicked;

	UPROPERTY(BlueprintAssignable, Category="World UI|Button")
	FWorldUIButtonStateChangedSignature OnVisualStateChanged;

protected:
	UPROPERTY(Transient)
	FVector BaseButtonRelativeLocation = FVector::ZeroVector;

	UPROPERTY(Transient)
	FVector BaseButtonRelativeScale = FVector(1.0f, 1.0f, 1.0f);

	UPROPERTY(Transient)
	TSet<TWeakObjectPtr<AActor>> HoveringActors;

	UPROPERTY(Transient)
	float HoverAlpha = 0.0f;

	UPROPERTY(Transient)
	float PressAlpha = 0.0f;

	UPROPERTY(Transient)
	float PressTimeRemaining = 0.0f;

	UPROPERTY(Transient)
	bool bHovered = false;

	UPROPERTY(Transient)
	bool bPressed = false;

	UPROPERTY(Transient)
	bool bRayTraceInteractionEnabled = true;

	UPROPERTY(Transient)
	EWorldUIButtonVisualState CurrentVisualState = EWorldUIButtonVisualState::Idle;
};
