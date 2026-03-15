// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "MiningBotActor.generated.h"

class AMaterialNodeActor;
class AMiningSwarmMachine;
class UBoxComponent;
class UCapsuleComponent;
class UPrimitiveComponent;
class USceneComponent;
class USphereComponent;
class UStaticMeshComponent;
struct FHitResult;

UENUM(BlueprintType)
enum class EMiningBotRuntimeState : uint8
{
	Disabled UMETA(DisplayName="Disabled"),
	MissingSwarm UMETA(DisplayName="Missing Swarm"),
	Idle UMETA(DisplayName="Idle"),
	NoNodeAvailable UMETA(DisplayName="No Node Available"),
	TravelingToNode UMETA(DisplayName="Traveling To Node"),
	Mining UMETA(DisplayName="Mining"),
	PhysicsRagdoll UMETA(DisplayName="Physics Ragdoll"),
	ReturningToSwarm UMETA(DisplayName="Returning To Swarm"),
	WaitingForOutput UMETA(DisplayName="Waiting For Output")
};

UENUM(BlueprintType)
enum class EMiningBotCollisionBodyType : uint8
{
	Sphere UMETA(DisplayName="Sphere"),
	Box UMETA(DisplayName="Box"),
	Capsule UMETA(DisplayName="Capsule"),
	CustomMesh UMETA(DisplayName="Custom Mesh")
};

UCLASS()
class AMiningBotActor : public AActor
{
	GENERATED_BODY()

public:
	AMiningBotActor();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category="Factory|MiningBot")
	void SetOwningSwarmMachine(AMiningSwarmMachine* NewOwningSwarmMachine);

	UFUNCTION(BlueprintPure, Category="Factory|MiningBot")
	AMiningSwarmMachine* GetOwningSwarmMachine() const { return OwningSwarmMachine; }

	UFUNCTION(BlueprintCallable, Category="Factory|MiningBot")
	void SetMiningTuning(int32 InCapacityUnits, float InTravelSpeedCmPerSecond, float InMiningSpeedMultiplier);

	UFUNCTION(BlueprintPure, Category="Factory|MiningBot")
	UPrimitiveComponent* GetPickupPhysicsComponent() const;

	UFUNCTION(BlueprintCallable, Category="Factory|MiningBot")
	void NotifyPickedUpByPhysicsHandle(bool bIsPickedUp);

	UFUNCTION(BlueprintPure, Category="Factory|MiningBot")
	EMiningBotRuntimeState GetRuntimeState() const { return RuntimeState; }

	UFUNCTION(BlueprintPure, Category="Factory|MiningBot")
	FText GetRuntimeStatus() const { return RuntimeStatus; }

	UFUNCTION(BlueprintPure, Category="Factory|MiningBot")
	int32 GetCarriedTotalQuantityScaled() const;

	UFUNCTION(BlueprintImplementableEvent, Category="Factory|MiningBot")
	void OnMiningBotStateChanged(EMiningBotRuntimeState NewState, const FText& NewStatus);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningBot")
	TObjectPtr<USceneComponent> SceneRoot = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningBot")
	TObjectPtr<USphereComponent> BotCollision = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningBot")
	TObjectPtr<UBoxComponent> BotBoxCollision = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningBot")
	TObjectPtr<UCapsuleComponent> BotCapsuleCollision = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningBot")
	TObjectPtr<UStaticMeshComponent> BotCustomCollision = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningBot")
	TObjectPtr<UPrimitiveComponent> ActiveCollisionBody = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningBot")
	TObjectPtr<UStaticMeshComponent> BotMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Collision")
	EMiningBotCollisionBodyType CollisionBodyType = EMiningBotCollisionBodyType::Sphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningBot")
	TObjectPtr<AMiningSwarmMachine> OwningSwarmMachine = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Mining", meta=(ClampMin="1", UIMin="1"))
	int32 MiningCapacityUnits = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Movement", meta=(ClampMin="1.0", ClampMax="100.0", UIMin="1.0", UIMax="100.0"))
	float TravelSpeedCmPerSecond = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Mining", meta=(ClampMin="0.05", UIMin="0.05"))
	float MiningDurationSeconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Mining", meta=(ClampMin="0.1", UIMin="0.1"))
	float MiningSpeedMultiplier = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Movement", meta=(ClampMin="0.05", UIMin="0.05"))
	float SurfaceAlignmentInterpSpeed = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Movement", meta=(ClampMin="1.0", UIMin="1.0"))
	float ArrivalToleranceCm = 25.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Movement", meta=(ClampMin="0.0", UIMin="0.0"))
	float SurfaceHoverOffsetCm = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Movement", meta=(ClampMin="0.0", UIMin="0.0"))
	float SurfaceProbeStartOffsetCm = 55.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Movement", meta=(ClampMin="25.0", UIMin="25.0"))
	float SurfaceProbeDepthCm = 240.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Movement", meta=(ClampMin="0.0", UIMin="0.0"))
	float SurfaceProbeForwardLookCm = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Movement", meta=(ClampMin="0.0", UIMin="0.0"))
	float MaxStepUpHeightCm = 30.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Movement", meta=(ClampMin="0.05", UIMin="0.05"))
	float GroundSnapInterpSpeed = 18.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Movement", meta=(ClampMin="0.05", UIMin="0.05"))
	float SupportNormalSmoothingSpeed = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Movement", meta=(ClampMin="1.0", UIMin="1.0"))
	float MaxTraversalStepPerTickCm = 40.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Movement", meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float ObstacleSlideScale = 0.65f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Movement", meta=(ClampMin="0.0", UIMin="0.0"))
	float ObstacleContactPadding = 0.03f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Visual")
	bool bEnableVisualLag = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Visual", meta=(ClampMin="0.05", UIMin="0.05", EditCondition="bEnableVisualLag"))
	float VisualLagLocationInterpSpeed = 14.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Visual", meta=(ClampMin="0.05", UIMin="0.05", EditCondition="bEnableVisualLag"))
	float VisualLagRotationInterpSpeed = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Visual", meta=(ClampMin="0.0", UIMin="0.0", EditCondition="bEnableVisualLag"))
	float VisualLagMaxDistanceCm = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Output", meta=(ClampMin="0.05", UIMin="0.05"))
	float DepositRetryIntervalSeconds = 0.25f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Recovery", meta=(ClampMin="1", UIMin="1"))
	int32 MaxMoveFailuresBeforeRecovery = 3;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Recovery", meta=(ClampMin="0.1", UIMin="0.1"))
	float MinimumMoveProgressBeforeFailureCm = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Recovery", meta=(ClampMin="1", UIMin="1"))
	int32 MaxNoSurfaceFailuresBeforeRecovery = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Recovery", meta=(ClampMin="0.0", UIMin="0.0"))
	float NoSurfaceGraceSecondsBeforeRecovery = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Recovery", meta=(ClampMin="0.0", UIMin="0.0"))
	float PhysicsRagdollLinearDamping = 1.8f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Recovery", meta=(ClampMin="0.0", UIMin="0.0"))
	float PhysicsRagdollAngularDamping = 6.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Recovery", meta=(ClampMin="0.0", UIMin="0.0"))
	float PhysicsRagdollMaxAngularVelocityDegPerSec = 220.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Recovery", meta=(ClampMin="0.0", UIMin="0.0"))
	float PhysicsRagdollSurfaceDragPerSecond = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Recovery", meta=(ClampMin="0.05", UIMin="0.05"))
	float PhysicsRagdollSupportedRecoveryDelaySeconds = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Recovery", meta=(ClampMin="0.0", UIMin="0.0"))
	float PhysicsRagdollForceRecoverLinearSpeedThresholdCmPerSec = 45.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Recovery", meta=(ClampMin="1.0", UIMin="1.0"))
	float PhysicsRagdollSupportSnapDistanceCm = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Recovery", meta=(ClampMin="0.0", UIMin="0.0"))
	float PhysicsRecoveryRestLinearSpeedThresholdCmPerSec = 8.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Recovery", meta=(ClampMin="0.0", UIMin="0.0"))
	float PhysicsRecoveryRestAngularSpeedThresholdDegPerSec = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Factory|MiningBot|Recovery", meta=(ClampMin="0.05", UIMin="0.05"))
	float PhysicsRecoveryRestDurationSeconds = 0.5f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningBot")
	EMiningBotRuntimeState RuntimeState = EMiningBotRuntimeState::MissingSwarm;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningBot")
	FText RuntimeStatus;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningBot")
	TObjectPtr<AMaterialNodeActor> CurrentTargetNode = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningBot")
	FVector CurrentTargetLocation = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningBot")
	FVector CurrentTargetSurfaceNormal = FVector::UpVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Factory|MiningBot")
	TMap<FName, int32> CarriedResourcesScaled;

protected:
	void SetRuntimeState(EMiningBotRuntimeState NewState, const FString& NewStatusMessage);
	void ApplyCollisionBodySelection();
	UPrimitiveComponent* ResolveCollisionBodyForType(EMiningBotCollisionBodyType BodyType) const;
	UPrimitiveComponent* GetActiveCollisionBody() const;
	float GetSurfaceSupportDistance(const FVector& SurfaceNormal) const;
	float GetTraversalTraceRadius() const;
	void UpdateVisualLag(float DeltaSeconds);
	bool RequestAssignmentFromSwarm();
	void StartMiningCycle();
	void CompleteMiningCycle();
	void HandleTravelToNode(float DeltaSeconds);
	void HandleMining(float DeltaSeconds);
	void HandleReturnToSwarm(float DeltaSeconds);
	bool MoveTowardsPoint(const FVector& TargetLocation, const FVector& SurfaceNormal, float DeltaSeconds);
	void UpdateSurfaceAlignment(const FVector& DesiredDirection, const FVector& SurfaceNormal, float DeltaSeconds);
	bool ResolveSupportSurface(const FVector& ProbeOrigin, const FVector& PreferredSurfaceNormal, FHitResult& OutSurfaceHit) const;
	bool TraceSurface(const FVector& ProbeOrigin, const FVector& DownDirection, FHitResult& OutSurfaceHit) const;
	float GetEffectiveMiningDurationSeconds() const;
	void EnterPhysicsRagdollMode(const FString& Reason, bool bReturnHomeAfterRecover);
	void HandlePhysicsRagdoll(float DeltaSeconds);
	void ExitPhysicsRagdollMode();
	void RegisterMovementFailure(const FString& Reason, bool bReturnHomeAfterRecover);
	void ResetMovementFailures();

	float MiningTimeRemaining = 0.0f;
	float TimeUntilNextDepositAttempt = 0.0f;
	FVector LastMovementDirection = FVector::ForwardVector;
	FVector SmoothedSupportNormal = FVector::UpVector;
	bool bHasSmoothedSupportNormal = false;
	bool bVisualLagInitialized = false;
	EMiningBotRuntimeState PrePhysicsRuntimeState = EMiningBotRuntimeState::Idle;
	int32 ConsecutiveMoveFailureCount = 0;
	int32 ConsecutiveNoSurfaceFailureCount = 0;
	float NoSurfaceAirborneTimeSeconds = 0.0f;
	bool bHeldByPhysicsHandle = false;
	bool bReturnHomeAfterPhysicsRecovery = false;
	float PhysicsRecoveryRestTime = 0.0f;
	float PhysicsSupportedRecoveryTime = 0.0f;
	float CachedPrePhysicsLinearDamping = 0.0f;
	float CachedPrePhysicsAngularDamping = 0.0f;
	bool bHasCachedPrePhysicsDamping = false;
};
