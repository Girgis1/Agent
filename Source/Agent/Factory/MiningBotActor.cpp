// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/MiningBotActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Factory/FactoryPlacementHelpers.h"
#include "Factory/MaterialNodeActor.h"
#include "Factory/MiningSwarmMachine.h"
#include "Material/AgentResourceTypes.h"
#include "Math/RotationMatrix.h"
#include "UObject/ConstructorHelpers.h"

AMiningBotActor::AMiningBotActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	BotMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BotMesh"));
	BotMesh->SetupAttachment(SceneRoot);
	BotMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	BotMesh->SetCollisionResponseToAllChannels(ECR_Block);
	BotMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	BotMesh->SetGenerateOverlapEvents(false);
	BotMesh->SetCanEverAffectNavigation(false);
	BotMesh->SetSimulatePhysics(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> BotMeshAsset(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (BotMeshAsset.Succeeded())
	{
		BotMesh->SetStaticMesh(BotMeshAsset.Object);
		BotMesh->SetRelativeScale3D(FVector(0.35f));
	}

	RuntimeStatus = FText::FromString(TEXT("Awaiting swarm"));
}

void AMiningBotActor::BeginPlay()
{
	Super::BeginPlay();

	if (!OwningSwarmMachine)
	{
		if (AMiningSwarmMachine* OwnerMachine = Cast<AMiningSwarmMachine>(GetOwner()))
		{
			OwningSwarmMachine = OwnerMachine;
		}
		else if (AMiningSwarmMachine* AttachedMachine = Cast<AMiningSwarmMachine>(GetAttachParentActor()))
		{
			OwningSwarmMachine = AttachedMachine;
		}
	}

	if (!bEnabled)
	{
		SetRuntimeState(EMiningBotRuntimeState::Disabled, TEXT("Disabled"));
	}
	else if (!OwningSwarmMachine)
	{
		SetRuntimeState(EMiningBotRuntimeState::MissingSwarm, TEXT("No swarm machine"));
	}
	else
	{
		SetRuntimeState(EMiningBotRuntimeState::Idle, TEXT("Ready"));
	}
}

void AMiningBotActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bEnabled)
	{
		SetRuntimeState(EMiningBotRuntimeState::Disabled, TEXT("Disabled"));
		return;
	}

	if (!OwningSwarmMachine)
	{
		SetRuntimeState(EMiningBotRuntimeState::MissingSwarm, TEXT("No swarm machine"));
		return;
	}

	if (!OwningSwarmMachine->IsSwarmEnabled())
	{
		const FVector HomeLocation = OwningSwarmMachine->GetBotHomeLocation(this);
		MoveTowardsPoint(HomeLocation, OwningSwarmMachine->GetBotHomeNormal(), DeltaSeconds);
		SetRuntimeState(EMiningBotRuntimeState::Idle, TEXT("Swarm paused"));
		return;
	}

	switch (RuntimeState)
	{
	case EMiningBotRuntimeState::Disabled:
	case EMiningBotRuntimeState::MissingSwarm:
	case EMiningBotRuntimeState::Idle:
	case EMiningBotRuntimeState::NoNodeAvailable:
		if (GetCarriedTotalQuantityScaled() > 0)
		{
			SetRuntimeState(EMiningBotRuntimeState::ReturningToSwarm, TEXT("Returning cargo"));
			HandleReturnToSwarm(DeltaSeconds);
			return;
		}

		if (RequestAssignmentFromSwarm())
		{
			SetRuntimeState(EMiningBotRuntimeState::TravelingToNode, TEXT("Traveling"));
			HandleTravelToNode(DeltaSeconds);
		}
		else
		{
			const FVector HomeLocation = OwningSwarmMachine->GetBotHomeLocation(this);
			MoveTowardsPoint(HomeLocation, OwningSwarmMachine->GetBotHomeNormal(), DeltaSeconds);
			SetRuntimeState(EMiningBotRuntimeState::NoNodeAvailable, TEXT("No nodes in range"));
		}
		break;

	case EMiningBotRuntimeState::TravelingToNode:
		HandleTravelToNode(DeltaSeconds);
		break;

	case EMiningBotRuntimeState::Mining:
		HandleMining(DeltaSeconds);
		break;

	case EMiningBotRuntimeState::ReturningToSwarm:
	case EMiningBotRuntimeState::WaitingForOutput:
		HandleReturnToSwarm(DeltaSeconds);
		break;

	default:
		SetRuntimeState(EMiningBotRuntimeState::Idle, TEXT("Ready"));
		break;
	}
}

void AMiningBotActor::SetOwningSwarmMachine(AMiningSwarmMachine* NewOwningSwarmMachine)
{
	OwningSwarmMachine = NewOwningSwarmMachine;

	if (!OwningSwarmMachine)
	{
		SetRuntimeState(EMiningBotRuntimeState::MissingSwarm, TEXT("No swarm machine"));
		return;
	}

	if (RuntimeState == EMiningBotRuntimeState::MissingSwarm)
	{
		SetRuntimeState(EMiningBotRuntimeState::Idle, TEXT("Ready"));
	}
}

void AMiningBotActor::SetMiningTuning(int32 InCapacityUnits, float InTravelSpeedCmPerSecond, float InMiningSpeedMultiplier)
{
	MiningCapacityUnits = FMath::Max(1, InCapacityUnits);
	TravelSpeedCmPerSecond = FMath::Max(1.0f, InTravelSpeedCmPerSecond);
	MiningSpeedMultiplier = FMath::Max(0.1f, InMiningSpeedMultiplier);
}

int32 AMiningBotActor::GetCarriedTotalQuantityScaled() const
{
	int32 TotalQuantityScaled = 0;
	for (const TPair<FName, int32>& Pair : CarriedResourcesScaled)
	{
		TotalQuantityScaled += FMath::Max(0, Pair.Value);
	}

	return TotalQuantityScaled;
}

void AMiningBotActor::SetRuntimeState(EMiningBotRuntimeState NewState, const FString& NewStatusMessage)
{
	const FText NewStatusText = FText::FromString(NewStatusMessage);
	if (RuntimeState == NewState && RuntimeStatus.EqualTo(NewStatusText))
	{
		return;
	}

	RuntimeState = NewState;
	RuntimeStatus = NewStatusText;
	OnMiningBotStateChanged(RuntimeState, RuntimeStatus);
}

bool AMiningBotActor::RequestAssignmentFromSwarm()
{
	if (!OwningSwarmMachine)
	{
		return false;
	}

	AMaterialNodeActor* TargetNode = nullptr;
	FVector TargetLocation = FVector::ZeroVector;
	FVector TargetNormal = FVector::UpVector;
	if (!OwningSwarmMachine->RequestMiningAssignment(this, TargetNode, TargetLocation, TargetNormal))
	{
		CurrentTargetNode = nullptr;
		return false;
	}

	CurrentTargetNode = TargetNode;
	CurrentTargetLocation = TargetLocation;
	CurrentTargetSurfaceNormal = TargetNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	LastMovementDirection = (CurrentTargetLocation - GetActorLocation()).GetSafeNormal(UE_SMALL_NUMBER, LastMovementDirection);
	return CurrentTargetNode != nullptr;
}

void AMiningBotActor::StartMiningCycle()
{
	MiningTimeRemaining = GetEffectiveMiningDurationSeconds();
	SetRuntimeState(
		EMiningBotRuntimeState::Mining,
		FString::Printf(TEXT("Mining %.2fs"), MiningTimeRemaining));
}

void AMiningBotActor::CompleteMiningCycle()
{
	MiningTimeRemaining = 0.0f;

	if (!CurrentTargetNode)
	{
		return;
	}

	const int32 RequestScaled = AgentResource::WholeUnitsToScaled(FMath::Max(1, MiningCapacityUnits));
	if (RequestScaled <= 0)
	{
		CurrentTargetNode = nullptr;
		return;
	}

	TMap<FName, int32> ExtractedResourcesScaled;
	if (CurrentTargetNode->ExtractResourcesScaled(RequestScaled, ExtractedResourcesScaled))
	{
		for (const TPair<FName, int32>& ExtractedPair : ExtractedResourcesScaled)
		{
			const FName ResourceId = ExtractedPair.Key;
			const int32 QuantityScaled = FMath::Max(0, ExtractedPair.Value);
			if (ResourceId.IsNone() || QuantityScaled <= 0)
			{
				continue;
			}

			CarriedResourcesScaled.FindOrAdd(ResourceId) += QuantityScaled;
		}
	}

	CurrentTargetNode = nullptr;
}

void AMiningBotActor::HandleTravelToNode(float DeltaSeconds)
{
	if (!CurrentTargetNode || CurrentTargetNode->IsDepleted())
	{
		CurrentTargetNode = nullptr;
		SetRuntimeState(EMiningBotRuntimeState::Idle, TEXT("Retargeting"));
		return;
	}

	const bool bReachedTarget = MoveTowardsPoint(CurrentTargetLocation, CurrentTargetSurfaceNormal, DeltaSeconds);
	if (bReachedTarget)
	{
		StartMiningCycle();
	}
}

void AMiningBotActor::HandleMining(float DeltaSeconds)
{
	MoveTowardsPoint(CurrentTargetLocation, CurrentTargetSurfaceNormal, DeltaSeconds);

	MiningTimeRemaining = FMath::Max(0.0f, MiningTimeRemaining - FMath::Max(0.0f, DeltaSeconds));
	if (MiningTimeRemaining > 0.0f)
	{
		return;
	}

	CompleteMiningCycle();
	SetRuntimeState(EMiningBotRuntimeState::ReturningToSwarm, TEXT("Returning cargo"));
}

void AMiningBotActor::HandleReturnToSwarm(float DeltaSeconds)
{
	if (!OwningSwarmMachine)
	{
		SetRuntimeState(EMiningBotRuntimeState::MissingSwarm, TEXT("No swarm machine"));
		return;
	}

	const FVector HomeLocation = OwningSwarmMachine->GetBotHomeLocation(this);
	const FVector HomeNormal = OwningSwarmMachine->GetBotHomeNormal();
	const bool bAtHome = MoveTowardsPoint(HomeLocation, HomeNormal, DeltaSeconds);
	if (!bAtHome)
	{
		SetRuntimeState(EMiningBotRuntimeState::ReturningToSwarm, TEXT("Returning"));
		return;
	}

	if (GetCarriedTotalQuantityScaled() <= 0)
	{
		CarriedResourcesScaled.Reset();
		SetRuntimeState(EMiningBotRuntimeState::Idle, TEXT("Ready"));
		return;
	}

	TimeUntilNextDepositAttempt = FMath::Max(0.0f, TimeUntilNextDepositAttempt - FMath::Max(0.0f, DeltaSeconds));
	if (TimeUntilNextDepositAttempt > 0.0f)
	{
		SetRuntimeState(EMiningBotRuntimeState::WaitingForOutput, TEXT("Waiting for output"));
		return;
	}

	TMap<FName, int32> RejectedResourcesScaled;
	int32 QueuedScaled = 0;
	OwningSwarmMachine->QueueBotResources(CarriedResourcesScaled, RejectedResourcesScaled, QueuedScaled);

	if (QueuedScaled <= 0)
	{
		TimeUntilNextDepositAttempt = FMath::Max(0.05f, DepositRetryIntervalSeconds);
		SetRuntimeState(EMiningBotRuntimeState::WaitingForOutput, TEXT("Output blocked"));
		return;
	}

	CarriedResourcesScaled = MoveTemp(RejectedResourcesScaled);
	if (GetCarriedTotalQuantityScaled() > 0)
	{
		TimeUntilNextDepositAttempt = FMath::Max(0.05f, DepositRetryIntervalSeconds);
		SetRuntimeState(EMiningBotRuntimeState::WaitingForOutput, TEXT("Deposited partial cargo"));
		return;
	}

	SetRuntimeState(EMiningBotRuntimeState::Idle, TEXT("Cargo delivered"));
}

bool AMiningBotActor::MoveTowardsPoint(const FVector& TargetLocation, const FVector& SurfaceNormal, float DeltaSeconds)
{
	const FVector SafeSurfaceNormal = SurfaceNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	const FVector DesiredTargetLocation = TargetLocation + SafeSurfaceNormal * FMath::Max(0.0f, SurfaceHoverOffsetCm);

	const FVector CurrentLocation = GetActorLocation();
	const FVector ToTarget = DesiredTargetLocation - CurrentLocation;
	const float DistanceToTarget = ToTarget.Size();
	const float ArrivalTolerance = FMath::Max(1.0f, ArrivalToleranceCm);
	const float MaxStepDistance = FMath::Max(1.0f, TravelSpeedCmPerSecond) * FMath::Max(0.0f, DeltaSeconds);

	FVector MoveDirection = ToTarget.GetSafeNormal();
	if (!MoveDirection.IsNearlyZero())
	{
		LastMovementDirection = MoveDirection;
	}
	else
	{
		MoveDirection = LastMovementDirection.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
	}

	FVector NewLocation = CurrentLocation;
	if (DistanceToTarget > ArrivalTolerance && MaxStepDistance > 0.0f)
	{
		NewLocation += MoveDirection * FMath::Min(DistanceToTarget, MaxStepDistance);
		SetActorLocation(NewLocation, false, nullptr, ETeleportType::None);
	}
	else
	{
		NewLocation = DesiredTargetLocation;
		SetActorLocation(NewLocation, false, nullptr, ETeleportType::None);
	}

	FVector AlignmentNormal = SafeSurfaceNormal;
	if (UWorld* World = GetWorld())
	{
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MiningBotSurfaceAlign), false, this);
		QueryParams.AddIgnoredActor(this);
		if (OwningSwarmMachine)
		{
			QueryParams.AddIgnoredActor(OwningSwarmMachine);
		}

		FHitResult SurfaceHit;
		const FVector TraceStart = NewLocation + SafeSurfaceNormal * 120.0f;
		const FVector TraceEnd = NewLocation - SafeSurfaceNormal * 220.0f;
		bool bHit = World->LineTraceSingleByChannel(
			SurfaceHit,
			TraceStart,
			TraceEnd,
			AgentFactory::BuildPlacementTraceChannel,
			QueryParams);
		if (!bHit)
		{
			bHit = World->LineTraceSingleByChannel(SurfaceHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams);
		}

		if (bHit)
		{
			AlignmentNormal = SurfaceHit.ImpactNormal.GetSafeNormal(UE_SMALL_NUMBER, AlignmentNormal);
		}
	}

	UpdateSurfaceAlignment(MoveDirection, AlignmentNormal, DeltaSeconds);
	return FVector::DistSquared(NewLocation, DesiredTargetLocation) <= FMath::Square(ArrivalTolerance);
}

void AMiningBotActor::UpdateSurfaceAlignment(const FVector& DesiredDirection, const FVector& SurfaceNormal, float DeltaSeconds)
{
	const FVector SafeSurfaceNormal = SurfaceNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);

	FVector ForwardDirection = FVector::VectorPlaneProject(DesiredDirection, SafeSurfaceNormal).GetSafeNormal();
	if (ForwardDirection.IsNearlyZero())
	{
		ForwardDirection = FVector::VectorPlaneProject(GetActorForwardVector(), SafeSurfaceNormal).GetSafeNormal();
	}

	if (ForwardDirection.IsNearlyZero())
	{
		ForwardDirection = FVector::CrossProduct(SafeSurfaceNormal, FVector::RightVector).GetSafeNormal(
			UE_SMALL_NUMBER,
			FVector::ForwardVector);
	}

	const FQuat DesiredRotation = FRotationMatrix::MakeFromXZ(ForwardDirection, SafeSurfaceNormal).ToQuat();
	const float InterpSpeed = FMath::Max(0.05f, SurfaceAlignmentInterpSpeed);
	const FQuat SmoothedRotation = FMath::QInterpTo(GetActorQuat(), DesiredRotation, FMath::Max(0.0f, DeltaSeconds), InterpSpeed);
	SetActorRotation(SmoothedRotation);
}

float AMiningBotActor::GetEffectiveMiningDurationSeconds() const
{
	const float SafeDuration = FMath::Max(0.05f, MiningDurationSeconds);
	const float SafeMultiplier = FMath::Max(0.1f, MiningSpeedMultiplier);
	return SafeDuration / SafeMultiplier;
}
