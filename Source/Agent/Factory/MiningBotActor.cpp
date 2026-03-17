// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/MiningBotActor.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "CollisionShape.h"
#include "Dirty/DirtBrushComponent.h"
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

	BotCollision = CreateDefaultSubobject<USphereComponent>(TEXT("BotCollision"));
	BotCollision->InitSphereRadius(18.0f);
	BotCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BotCollision->SetCollisionObjectType(ECC_WorldDynamic);
	BotCollision->SetCollisionResponseToAllChannels(ECR_Block);
	BotCollision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	BotCollision->SetGenerateOverlapEvents(false);
	BotCollision->SetCanEverAffectNavigation(false);
	BotCollision->SetSimulatePhysics(false);
	SetRootComponent(BotCollision);

	BotBoxCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("BotBoxCollision"));
	BotBoxCollision->SetupAttachment(BotCollision);
	BotBoxCollision->SetBoxExtent(FVector(18.0f, 18.0f, 18.0f));
	BotBoxCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BotBoxCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	BotBoxCollision->SetGenerateOverlapEvents(false);
	BotBoxCollision->SetCanEverAffectNavigation(false);
	BotBoxCollision->SetSimulatePhysics(false);

	BotCapsuleCollision = CreateDefaultSubobject<UCapsuleComponent>(TEXT("BotCapsuleCollision"));
	BotCapsuleCollision->SetupAttachment(BotCollision);
	BotCapsuleCollision->SetCapsuleSize(16.0f, 22.0f);
	BotCapsuleCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BotCapsuleCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	BotCapsuleCollision->SetGenerateOverlapEvents(false);
	BotCapsuleCollision->SetCanEverAffectNavigation(false);
	BotCapsuleCollision->SetSimulatePhysics(false);

	BotCustomCollision = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BotCustomCollision"));
	BotCustomCollision->SetupAttachment(BotCollision);
	BotCustomCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BotCustomCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	BotCustomCollision->SetGenerateOverlapEvents(false);
	BotCustomCollision->SetCanEverAffectNavigation(false);
	BotCustomCollision->SetSimulatePhysics(false);
	BotCustomCollision->SetHiddenInGame(true);
	BotCustomCollision->SetVisibility(false, true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SceneRoot->SetupAttachment(BotCollision);
	SceneRoot->SetUsingAbsoluteLocation(true);
	SceneRoot->SetUsingAbsoluteRotation(true);

	BotMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BotMesh"));
	BotMesh->SetupAttachment(SceneRoot);
	BotMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
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

	DirtTrailBrushComponent = CreateDefaultSubobject<UDirtBrushComponent>(TEXT("DirtTrailBrushComponent"));
	DirtTrailBrushComponent->BrushMode = EDirtBrushMode::Clean;
	DirtTrailBrushComponent->ApplicationType = EDirtBrushApplicationType::Trail;
	DirtTrailBrushComponent->BrushSizeCm = 50.0f;
	DirtTrailBrushComponent->BrushStrengthPerSecond = 3.0f;
	DirtTrailBrushComponent->BrushHardness = 0.5f;
	DirtTrailBrushComponent->ApplyIntervalSeconds = 0.05f;
	DirtTrailBrushComponent->TrailTraceStartOffsetCm = 10.0f;
	DirtTrailBrushComponent->TrailTraceLengthCm = 60.0f;
	DirtTrailBrushComponent->TrailMinSegmentLengthCm = 8.0f;
	DirtTrailBrushComponent->bTrailUseOwnerDownVector = true;

	RuntimeStatus = FText::FromString(TEXT("Awaiting swarm"));
	ActiveCollisionBody = BotCollision;
	SmoothedSupportNormal = FVector::UpVector;
	bHasSmoothedSupportNormal = false;
	bVisualLagInitialized = false;
	TravelSpeedCmPerSecond = 100.0f;
	MaxTraversalStepPerTickCm = 35.0f;
	ObstacleSlideScale = 0.65f;
	ObstacleContactPadding = 0.03f;
}

void AMiningBotActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyCollisionBodySelection();
}

void AMiningBotActor::BeginPlay()
{
	Super::BeginPlay();
	ApplyCollisionBodySelection();
	SmoothedSupportNormal = GetActorUpVector().GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	bHasSmoothedSupportNormal = true;
	bVisualLagInitialized = false;

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

	if (DirtTrailBrushComponent)
	{
		const bool bShouldClean = bEnableCleaningTrail
			&& RuntimeState != EMiningBotRuntimeState::PhysicsRagdoll
			&& !bHeldByPhysicsHandle;
		DirtTrailBrushComponent->SetBrushActive(bShouldClean);
	}

	if (!OwningSwarmMachine)
	{
		SetRuntimeState(EMiningBotRuntimeState::MissingSwarm, TEXT("No swarm machine"));
		return;
	}

	if (RuntimeState == EMiningBotRuntimeState::PhysicsRagdoll)
	{
		HandlePhysicsRagdoll(DeltaSeconds);
		return;
	}

	if (!OwningSwarmMachine->IsSwarmEnabled())
	{
		CurrentTargetNode = nullptr;
		MiningTimeRemaining = 0.0f;
		SetRuntimeState(EMiningBotRuntimeState::Idle, TEXT("Swarm offline"));
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

	case EMiningBotRuntimeState::PhysicsRagdoll:
		HandlePhysicsRagdoll(DeltaSeconds);
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
	TravelSpeedCmPerSecond = FMath::Clamp(InTravelSpeedCmPerSecond, 1.0f, 100.0f);
	MiningSpeedMultiplier = FMath::Max(0.1f, InMiningSpeedMultiplier);
}

UPrimitiveComponent* AMiningBotActor::GetPickupPhysicsComponent() const
{
	return GetActiveCollisionBody();
}

void AMiningBotActor::NotifyPickedUpByPhysicsHandle(bool bIsPickedUp)
{
	bHeldByPhysicsHandle = bIsPickedUp;
	if (bHeldByPhysicsHandle)
	{
		EnterPhysicsRagdollMode(TEXT("Grabbed"), false);
	}
	else
	{
		PhysicsRecoveryRestTime = 0.0f;
		if (RuntimeState == EMiningBotRuntimeState::PhysicsRagdoll)
		{
			SetRuntimeState(EMiningBotRuntimeState::PhysicsRagdoll, TEXT("Thrown"));
		}
	}
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

void AMiningBotActor::ApplyCollisionBodySelection()
{
	TArray<UPrimitiveComponent*> CollisionBodies;
	if (BotCollision)
	{
		CollisionBodies.Add(BotCollision);
	}
	if (BotBoxCollision)
	{
		CollisionBodies.Add(BotBoxCollision);
	}
	if (BotCapsuleCollision)
	{
		CollisionBodies.Add(BotCapsuleCollision);
	}
	if (BotCustomCollision)
	{
		CollisionBodies.Add(BotCustomCollision);
	}

	UPrimitiveComponent* SelectedBody = ResolveCollisionBodyForType(CollisionBodyType);
	if (!SelectedBody)
	{
		SelectedBody = BotCollision;
	}

	if (!SelectedBody)
	{
		return;
	}

	const FName SelectedBodyName = SelectedBody->GetFName();
	const FTransform OldSelectedWorldTransform = SelectedBody->GetComponentTransform();

	for (UPrimitiveComponent* Body : CollisionBodies)
	{
		if (!Body)
		{
			continue;
		}

		Body->SetCollisionObjectType(ECC_WorldDynamic);
		Body->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		Body->SetGenerateOverlapEvents(false);
		Body->SetCanEverAffectNavigation(false);
		Body->SetSimulatePhysics(false);

		const bool bIsSelected = Body == SelectedBody;
		Body->SetCollisionEnabled(bIsSelected ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		Body->SetCollisionResponseToAllChannels(bIsSelected ? ECR_Block : ECR_Ignore);
	}

	if (GetRootComponent() != SelectedBody)
	{
		SetRootComponent(SelectedBody);
	}

	SelectedBody->SetWorldTransform(OldSelectedWorldTransform, false, nullptr, ETeleportType::TeleportPhysics);

	for (UPrimitiveComponent* Body : CollisionBodies)
	{
		if (!Body || Body == SelectedBody)
		{
			continue;
		}

		Body->AttachToComponent(SelectedBody, FAttachmentTransformRules::KeepWorldTransform);
	}

	if (SceneRoot && SceneRoot->GetAttachParent() != SelectedBody)
	{
		SceneRoot->AttachToComponent(SelectedBody, FAttachmentTransformRules::KeepWorldTransform);
	}

	// Custom collision mesh is optional and usually only used as the selected collision body.
	if (BotCustomCollision)
	{
		const bool bCustomIsSelected = SelectedBodyName == BotCustomCollision->GetFName();
		BotCustomCollision->SetHiddenInGame(!bCustomIsSelected);
		BotCustomCollision->SetVisibility(bCustomIsSelected, true);
	}

	ActiveCollisionBody = SelectedBody;
}

UPrimitiveComponent* AMiningBotActor::ResolveCollisionBodyForType(EMiningBotCollisionBodyType BodyType) const
{
	switch (BodyType)
	{
	case EMiningBotCollisionBodyType::Box:
		return BotBoxCollision.Get();
	case EMiningBotCollisionBodyType::Capsule:
		return BotCapsuleCollision.Get();
	case EMiningBotCollisionBodyType::CustomMesh:
		return BotCustomCollision.Get();
	case EMiningBotCollisionBodyType::Sphere:
	default:
		return BotCollision.Get();
	}
}

UPrimitiveComponent* AMiningBotActor::GetActiveCollisionBody() const
{
	if (ActiveCollisionBody)
	{
		return ActiveCollisionBody;
	}

	if (UPrimitiveComponent* SelectedBody = ResolveCollisionBodyForType(CollisionBodyType))
	{
		return SelectedBody;
	}

	return BotCollision.Get();
}

float AMiningBotActor::GetSurfaceSupportDistance(const FVector& SurfaceNormal) const
{
	UPrimitiveComponent* CollisionBody = GetActiveCollisionBody();
	if (!CollisionBody)
	{
		return 0.0f;
	}

	const FVector SafeNormal = SurfaceNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	if (const USphereComponent* SphereBody = Cast<USphereComponent>(CollisionBody))
	{
		return FMath::Max(0.0f, SphereBody->GetScaledSphereRadius());
	}

	if (const UBoxComponent* BoxBody = Cast<UBoxComponent>(CollisionBody))
	{
		const FVector Extent = BoxBody->GetScaledBoxExtent();
		const FVector LocalNormal = BoxBody->GetComponentTransform().InverseTransformVectorNoScale(SafeNormal).GetAbs();
		return FMath::Max(0.0f, FVector::DotProduct(Extent, LocalNormal));
	}

	if (const UCapsuleComponent* CapsuleBody = Cast<UCapsuleComponent>(CollisionBody))
	{
		const float Radius = FMath::Max(0.0f, CapsuleBody->GetScaledCapsuleRadius());
		const float HalfHeight = FMath::Max(Radius, CapsuleBody->GetScaledCapsuleHalfHeight());
		const FVector LocalNormal = CapsuleBody->GetComponentTransform().InverseTransformVectorNoScale(SafeNormal).GetAbs();
		const float Axial = FMath::Clamp(LocalNormal.Z, 0.0f, 1.0f);
		return FMath::Lerp(Radius, HalfHeight, Axial);
	}

	const FVector Extent = CollisionBody->Bounds.BoxExtent;
	const FVector AbsNormal = SafeNormal.GetAbs();
	return FMath::Max(0.0f, FVector::DotProduct(Extent, AbsNormal));
}

float AMiningBotActor::GetTraversalTraceRadius() const
{
	UPrimitiveComponent* CollisionBody = GetActiveCollisionBody();
	if (!CollisionBody)
	{
		return 8.0f;
	}

	if (const USphereComponent* SphereBody = Cast<USphereComponent>(CollisionBody))
	{
		const float Radius = FMath::Max(4.0f, SphereBody->GetScaledSphereRadius());
		return FMath::Clamp(Radius * 0.7f, 4.0f, Radius);
	}

	if (const UBoxComponent* BoxBody = Cast<UBoxComponent>(CollisionBody))
	{
		const FVector Extent = BoxBody->GetScaledBoxExtent();
		const float Radius = FMath::Max(4.0f, FMath::Min(Extent.X, Extent.Y));
		return FMath::Clamp(Radius * 0.7f, 4.0f, Radius);
	}

	if (const UCapsuleComponent* CapsuleBody = Cast<UCapsuleComponent>(CollisionBody))
	{
		const float Radius = FMath::Max(4.0f, CapsuleBody->GetScaledCapsuleRadius());
		return FMath::Clamp(Radius * 0.7f, 4.0f, Radius);
	}

	const FVector Extent = CollisionBody->Bounds.BoxExtent;
	const float Radius = FMath::Max(4.0f, FMath::Min3(Extent.X, Extent.Y, Extent.Z));
	return FMath::Clamp(Radius * 0.7f, 4.0f, Radius);
}

void AMiningBotActor::UpdateVisualLag(float DeltaSeconds)
{
	if (!SceneRoot || !GetRootComponent())
	{
		return;
	}

	const FTransform TargetTransform = GetRootComponent()->GetComponentTransform();
	if (!bEnableVisualLag)
	{
		SceneRoot->SetWorldTransform(TargetTransform, false, nullptr, ETeleportType::None);
		bVisualLagInitialized = false;
		return;
	}

	if (!bVisualLagInitialized)
	{
		SceneRoot->SetWorldTransform(TargetTransform, false, nullptr, ETeleportType::None);
		bVisualLagInitialized = true;
		return;
	}

	const FVector CurrentLocation = SceneRoot->GetComponentLocation();
	const FVector TargetLocation = TargetTransform.GetLocation();
	const float MaxLagDistance = FMath::Max(0.0f, VisualLagMaxDistanceCm);
	if (MaxLagDistance > 0.0f
		&& FVector::DistSquared(CurrentLocation, TargetLocation) > FMath::Square(MaxLagDistance))
	{
		SceneRoot->SetWorldTransform(TargetTransform, false, nullptr, ETeleportType::None);
		return;
	}

	const FVector NewLocation = FMath::VInterpTo(
		CurrentLocation,
		TargetLocation,
		FMath::Max(0.0f, DeltaSeconds),
		FMath::Max(0.05f, VisualLagLocationInterpSpeed));
	const FQuat NewRotation = FMath::QInterpTo(
		SceneRoot->GetComponentQuat(),
		TargetTransform.GetRotation(),
		FMath::Max(0.0f, DeltaSeconds),
		FMath::Max(0.05f, VisualLagRotationInterpSpeed));

	SceneRoot->SetWorldLocationAndRotation(NewLocation, NewRotation, false, nullptr, ETeleportType::None);
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
	if (RuntimeState == EMiningBotRuntimeState::PhysicsRagdoll)
	{
		return;
	}

	if (bReachedTarget)
	{
		ResetMovementFailures();
		StartMiningCycle();
	}
}

void AMiningBotActor::HandleMining(float DeltaSeconds)
{
	MoveTowardsPoint(CurrentTargetLocation, CurrentTargetSurfaceNormal, DeltaSeconds);
	if (RuntimeState == EMiningBotRuntimeState::PhysicsRagdoll)
	{
		return;
	}

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

	const float DockPlanarTolerance = FMath::Max(ArrivalToleranceCm, 80.0f);
	const float DockVerticalTolerance = FMath::Max(
		ArrivalToleranceCm,
		OwningSwarmMachine->BotDockVerticalAcceptanceCm);
	const bool bInsideDockVolume = OwningSwarmMachine->IsBotInsideDockVolume(
		this,
		DockPlanarTolerance,
		DockVerticalTolerance);
	bool bAtHome = bInsideDockVolume;
	if (!bAtHome)
	{
		const FVector HomeLocation = OwningSwarmMachine->GetBotHomeLocation(this);
		const FVector HomeNormal = OwningSwarmMachine->GetBotHomeNormal();
		bAtHome = MoveTowardsPoint(HomeLocation, HomeNormal, DeltaSeconds);
	}

	if (RuntimeState == EMiningBotRuntimeState::PhysicsRagdoll)
	{
		return;
	}

	if (!bAtHome)
	{
		SetRuntimeState(EMiningBotRuntimeState::ReturningToSwarm, TEXT("Returning"));
		return;
	}

	int32 QueuedScaled = 0;
	int32 BufferedScaled = 0;
	const bool bDockedAndDespawned = OwningSwarmMachine->HandleBotDocking(
		this,
		CarriedResourcesScaled,
		QueuedScaled,
		BufferedScaled);

	CarriedResourcesScaled.Reset();
	TimeUntilNextDepositAttempt = 0.0f;

	if (bDockedAndDespawned)
	{
		return;
	}

	if (BufferedScaled > 0)
	{
		SetRuntimeState(EMiningBotRuntimeState::Idle, TEXT("Docked (buffered)"));
		return;
	}

	if (QueuedScaled > 0)
	{
		SetRuntimeState(EMiningBotRuntimeState::Idle, TEXT("Docked (delivered)"));
		return;
	}

	SetRuntimeState(EMiningBotRuntimeState::Idle, TEXT("Docked"));
}

bool AMiningBotActor::MoveTowardsPoint(const FVector& TargetLocation, const FVector& SurfaceNormal, float DeltaSeconds)
{
	const FVector RequestedSurfaceNormal = SurfaceNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	const float SafeHoverOffset = FMath::Max(0.0f, SurfaceHoverOffsetCm);
	const float ArrivalTolerance = FMath::Max(1.0f, ArrivalToleranceCm);
	const float EffectiveTravelSpeed = FMath::Clamp(TravelSpeedCmPerSecond, 1.0f, 100.0f);
	const float RequestedStepDistance = EffectiveTravelSpeed * FMath::Max(0.0f, DeltaSeconds);
	const float MaxStepDistance = FMath::Min(RequestedStepDistance, FMath::Max(1.0f, MaxTraversalStepPerTickCm));

	const FVector CurrentLocation = GetActorLocation();
	const FVector StartMoveLocation = CurrentLocation;
	FVector ActiveSurfaceNormal = RequestedSurfaceNormal;
	FHitResult CurrentSurfaceHit;
	if (ResolveSupportSurface(CurrentLocation, RequestedSurfaceNormal, CurrentSurfaceHit))
	{
		ActiveSurfaceNormal = CurrentSurfaceHit.ImpactNormal.GetSafeNormal(UE_SMALL_NUMBER, ActiveSurfaceNormal);
	}

	if (!bHasSmoothedSupportNormal)
	{
		SmoothedSupportNormal = ActiveSurfaceNormal;
		bHasSmoothedSupportNormal = true;
	}

	const float NormalSmoothingAlpha = FMath::Clamp(
		FMath::Max(0.05f, SupportNormalSmoothingSpeed) * FMath::Max(0.0f, DeltaSeconds),
		0.0f,
		1.0f);
	SmoothedSupportNormal = FMath::Lerp(SmoothedSupportNormal, ActiveSurfaceNormal, NormalSmoothingAlpha)
		.GetSafeNormal(UE_SMALL_NUMBER, ActiveSurfaceNormal);
	ActiveSurfaceNormal = SmoothedSupportNormal;

	const float SurfaceClearance = FMath::Max(0.0f, GetSurfaceSupportDistance(ActiveSurfaceNormal) + SafeHoverOffset);
	const FVector DesiredTargetLocation = TargetLocation + ActiveSurfaceNormal * SurfaceClearance;
	const FVector ToTarget = DesiredTargetLocation - CurrentLocation;
	const float DistanceToTarget = ToTarget.Size();

	FVector MoveDirection = FVector::VectorPlaneProject(ToTarget, ActiveSurfaceNormal).GetSafeNormal();
	if (MoveDirection.IsNearlyZero())
	{
		MoveDirection = FVector::VectorPlaneProject(LastMovementDirection, ActiveSurfaceNormal).GetSafeNormal();
	}

	if (MoveDirection.IsNearlyZero())
	{
		MoveDirection = FVector::CrossProduct(ActiveSurfaceNormal, FVector::RightVector).GetSafeNormal(
			UE_SMALL_NUMBER,
			FVector::ForwardVector);
	}

	LastMovementDirection = MoveDirection;
	FVector NewSurfaceNormal = ActiveSurfaceNormal;
	FVector MoveStartLocation = GetActorLocation();

	if (DistanceToTarget > ArrivalTolerance && MaxStepDistance > 0.0f)
	{
		const float StepDistance = FMath::Min(DistanceToTarget, MaxStepDistance);
		const FVector RequestedMoveDelta = MoveDirection * StepDistance;
		FVector AppliedMoveDelta = RequestedMoveDelta;

		if (UWorld* World = GetWorld())
		{
			FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MiningBotMoveSweep), false, this);
			QueryParams.AddIgnoredActor(this);

			const FCollisionShape SweepShape = FCollisionShape::MakeSphere(FMath::Max(4.0f, GetTraversalTraceRadius()));
			FHitResult MoveHit;
			bool bMoveHit = World->SweepSingleByChannel(
				MoveHit,
				MoveStartLocation,
				MoveStartLocation + RequestedMoveDelta,
				FQuat::Identity,
				AgentFactory::BuildPlacementTraceChannel,
				SweepShape,
				QueryParams);
			if (!bMoveHit)
			{
				bMoveHit = World->SweepSingleByChannel(
					MoveHit,
					MoveStartLocation,
					MoveStartLocation + RequestedMoveDelta,
					FQuat::Identity,
					ECC_Visibility,
					SweepShape,
					QueryParams);
			}

			if (bMoveHit && MoveHit.bBlockingHit)
			{
				const float ContactPadding = FMath::Clamp(ObstacleContactPadding, 0.0f, 0.45f);
				const float SafeHitTime = FMath::Clamp(MoveHit.Time - ContactPadding, 0.0f, 1.0f);
				const FVector SafeMoveDelta = RequestedMoveDelta * SafeHitTime;
				const FVector RemainingDelta = RequestedMoveDelta - SafeMoveDelta;
				const FVector SlideMoveDelta = FVector::VectorPlaneProject(RemainingDelta, MoveHit.Normal)
					* FMath::Clamp(ObstacleSlideScale, 0.0f, 1.0f);
				AppliedMoveDelta = SafeMoveDelta + SlideMoveDelta;
			}
		}

		MoveStartLocation += AppliedMoveDelta;
		SetActorLocation(MoveStartLocation, false, nullptr, ETeleportType::None);
	}

	FVector PostMoveLocation = GetActorLocation();
	const FVector SurfaceProbeForward = MoveDirection * FMath::Max(0.0f, SurfaceProbeForwardLookCm);
	FHitResult FinalSurfaceHit;
	bool bHasFinalSurface = ResolveSupportSurface(PostMoveLocation + SurfaceProbeForward, NewSurfaceNormal, FinalSurfaceHit);
	if (!bHasFinalSurface && MaxStepUpHeightCm > 0.0f)
	{
		const FVector StepUpProbeOrigin = PostMoveLocation + SurfaceProbeForward + NewSurfaceNormal * FMath::Max(0.0f, MaxStepUpHeightCm);
		bHasFinalSurface = ResolveSupportSurface(StepUpProbeOrigin, NewSurfaceNormal, FinalSurfaceHit);
	}

	if (bHasFinalSurface)
	{
		ConsecutiveNoSurfaceFailureCount = 0;
		NoSurfaceAirborneTimeSeconds = 0.0f;
		NewSurfaceNormal = FinalSurfaceHit.ImpactNormal.GetSafeNormal(UE_SMALL_NUMBER, NewSurfaceNormal);
		SmoothedSupportNormal = FMath::Lerp(SmoothedSupportNormal, NewSurfaceNormal, NormalSmoothingAlpha)
			.GetSafeNormal(UE_SMALL_NUMBER, NewSurfaceNormal);
		NewSurfaceNormal = SmoothedSupportNormal;

		const float NewSurfaceClearance = FMath::Max(0.0f, GetSurfaceSupportDistance(NewSurfaceNormal) + SafeHoverOffset);
		const FVector SnappedLocation = FinalSurfaceHit.ImpactPoint + NewSurfaceNormal * NewSurfaceClearance;
		const FVector SmoothedSnapLocation = FMath::VInterpTo(
			GetActorLocation(),
			SnappedLocation,
			FMath::Max(0.0f, DeltaSeconds),
			FMath::Max(0.05f, GroundSnapInterpSpeed));
		SetActorLocation(SmoothedSnapLocation, false, nullptr, ETeleportType::None);
	}
	else
	{
		NoSurfaceAirborneTimeSeconds += FMath::Max(0.0f, DeltaSeconds);
	}

	UpdateSurfaceAlignment(MoveDirection, NewSurfaceNormal, DeltaSeconds);
	UpdateVisualLag(DeltaSeconds);

	const bool bReachedTarget = FVector::DistSquared(GetActorLocation(), DesiredTargetLocation) <= FMath::Square(ArrivalTolerance);
	if (bReachedTarget)
	{
		ResetMovementFailures();
		return true;
	}

	const float MoveProgress = FVector::Dist(StartMoveLocation, GetActorLocation());
	ConsecutiveMoveFailureCount = (MoveProgress < FMath::Max(0.1f, MinimumMoveProgressBeforeFailureCm))
		? (ConsecutiveMoveFailureCount + 1)
		: 0;

	return false;
}

bool AMiningBotActor::ResolveSupportSurface(
	const FVector& ProbeOrigin,
	const FVector& PreferredSurfaceNormal,
	FHitResult& OutSurfaceHit) const
{
	const FVector SafeSurfaceNormal = PreferredSurfaceNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	const FVector PreferredDownDirection = -SafeSurfaceNormal;
	if (TraceSurface(ProbeOrigin, PreferredDownDirection, OutSurfaceHit))
	{
		return true;
	}

	const FVector ActorDownDirection = -GetActorUpVector().GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	if (TraceSurface(ProbeOrigin, ActorDownDirection, OutSurfaceHit))
	{
		return true;
	}

	return TraceSurface(ProbeOrigin, FVector(0.0f, 0.0f, -1.0f), OutSurfaceHit);
}

bool AMiningBotActor::TraceSurface(
	const FVector& ProbeOrigin,
	const FVector& DownDirection,
	FHitResult& OutSurfaceHit) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const FVector SafeDownDirection = DownDirection.GetSafeNormal(UE_SMALL_NUMBER, FVector(0.0f, 0.0f, -1.0f));
	const float ProbeStartOffset = FMath::Max(0.0f, SurfaceProbeStartOffsetCm);
	const float ProbeDepth = FMath::Max(25.0f, SurfaceProbeDepthCm);

	const FVector TraceStart = ProbeOrigin - SafeDownDirection * ProbeStartOffset;
	const FVector TraceEnd = ProbeOrigin + SafeDownDirection * ProbeDepth;

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MiningBotSurfaceTrace), false, this);
	QueryParams.AddIgnoredActor(this);

	const float TraceRadius = FMath::Max(4.0f, GetTraversalTraceRadius());
	const FCollisionShape SweepShape = FCollisionShape::MakeSphere(TraceRadius);

	bool bHit = World->SweepSingleByChannel(
		OutSurfaceHit,
		TraceStart,
		TraceEnd,
		FQuat::Identity,
		AgentFactory::BuildPlacementTraceChannel,
		SweepShape,
		QueryParams);
	if (!bHit)
	{
		bHit = World->SweepSingleByChannel(
			OutSurfaceHit,
			TraceStart,
			TraceEnd,
			FQuat::Identity,
			ECC_Visibility,
			SweepShape,
			QueryParams);
	}

	return bHit && OutSurfaceHit.bBlockingHit;
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

void AMiningBotActor::EnterPhysicsRagdollMode(const FString& Reason, bool bReturnHomeAfterRecover)
{
	UPrimitiveComponent* CollisionBody = GetActiveCollisionBody();
	if (!CollisionBody)
	{
		return;
	}

	if (RuntimeState != EMiningBotRuntimeState::PhysicsRagdoll)
	{
		PrePhysicsRuntimeState = RuntimeState;
	}

	bReturnHomeAfterPhysicsRecovery = bReturnHomeAfterPhysicsRecovery || bReturnHomeAfterRecover;
	if (bReturnHomeAfterRecover)
	{
		CurrentTargetNode = nullptr;
		MiningTimeRemaining = 0.0f;
	}

	if (!bHasCachedPrePhysicsDamping)
	{
		CachedPrePhysicsLinearDamping = CollisionBody->GetLinearDamping();
		CachedPrePhysicsAngularDamping = CollisionBody->GetAngularDamping();
		bHasCachedPrePhysicsDamping = true;
	}

	CollisionBody->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	CollisionBody->SetCollisionResponseToAllChannels(ECR_Block);
	CollisionBody->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	CollisionBody->SetLinearDamping(FMath::Max(0.0f, PhysicsRagdollLinearDamping));
	CollisionBody->SetAngularDamping(FMath::Max(0.0f, PhysicsRagdollAngularDamping));
	CollisionBody->SetPhysicsMaxAngularVelocityInDegrees(
		FMath::Max(0.0f, PhysicsRagdollMaxAngularVelocityDegPerSec),
		false);
	CollisionBody->SetSimulatePhysics(true);
	CollisionBody->SetEnableGravity(true);
	CollisionBody->WakeAllRigidBodies();

	if (BotMesh)
	{
		BotMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		BotMesh->SetCollisionResponseToAllChannels(ECR_Block);
		BotMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}

	PhysicsRecoveryRestTime = 0.0f;
	PhysicsSupportedRecoveryTime = 0.0f;
	ConsecutiveMoveFailureCount = 0;
	ConsecutiveNoSurfaceFailureCount = 0;
	NoSurfaceAirborneTimeSeconds = 0.0f;

	const FString StatusMessage = Reason.IsEmpty()
		? TEXT("Physics ragdoll")
		: FString::Printf(TEXT("Physics: %s"), *Reason);
	SetRuntimeState(EMiningBotRuntimeState::PhysicsRagdoll, StatusMessage);
}

void AMiningBotActor::HandlePhysicsRagdoll(float DeltaSeconds)
{
	UPrimitiveComponent* CollisionBody = GetActiveCollisionBody();
	if (!CollisionBody)
	{
		SetRuntimeState(EMiningBotRuntimeState::Idle, TEXT("Recovery failed"));
		return;
	}

	if (!CollisionBody->IsSimulatingPhysics())
	{
		CollisionBody->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		CollisionBody->SetCollisionResponseToAllChannels(ECR_Block);
		CollisionBody->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
		CollisionBody->SetSimulatePhysics(true);
		CollisionBody->SetEnableGravity(true);
		CollisionBody->WakeAllRigidBodies();
	}

	UpdateVisualLag(DeltaSeconds);

	if (bHeldByPhysicsHandle)
	{
		PhysicsRecoveryRestTime = 0.0f;
		PhysicsSupportedRecoveryTime = 0.0f;
		SetRuntimeState(EMiningBotRuntimeState::PhysicsRagdoll, TEXT("Grabbed"));
		return;
	}

	const float SafeDeltaSeconds = FMath::Max(0.0f, DeltaSeconds);
	FHitResult SupportHit;
	const FVector ProbeNormal = GetActorUpVector().GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	const bool bHasSupport = ResolveSupportSurface(GetActorLocation(), ProbeNormal, SupportHit);
	FVector SupportNormal = ProbeNormal;
	if (bHasSupport)
	{
		SupportNormal = SupportHit.ImpactNormal.GetSafeNormal(UE_SMALL_NUMBER, ProbeNormal);
		const FVector CurrentLinearVelocity = CollisionBody->GetPhysicsLinearVelocity();
		const FVector SurfaceLinearVelocity = FVector::VectorPlaneProject(CurrentLinearVelocity, SupportNormal);
		const FVector NormalLinearVelocity = CurrentLinearVelocity - SurfaceLinearVelocity;
		const float DragAlpha = FMath::Clamp(
			FMath::Max(0.0f, PhysicsRagdollSurfaceDragPerSecond) * SafeDeltaSeconds,
			0.0f,
			1.0f);
		const FVector DampedSurfaceVelocity = FMath::Lerp(SurfaceLinearVelocity, FVector::ZeroVector, DragAlpha);
		CollisionBody->SetPhysicsLinearVelocity(NormalLinearVelocity + DampedSurfaceVelocity, false);
	}

	const float LinearSpeed = CollisionBody->GetPhysicsLinearVelocity().Size();
	const float AngularSpeed = CollisionBody->GetPhysicsAngularVelocityInDegrees().Size();
	const bool bAtRest = LinearSpeed <= FMath::Max(0.0f, PhysicsRecoveryRestLinearSpeedThresholdCmPerSec)
		&& AngularSpeed <= FMath::Max(0.0f, PhysicsRecoveryRestAngularSpeedThresholdDegPerSec);

	if (bAtRest)
	{
		PhysicsRecoveryRestTime += SafeDeltaSeconds;
	}
	else
	{
		PhysicsRecoveryRestTime = 0.0f;
	}

	if (bHasSupport)
	{
		const float SurfaceClearance = FMath::Max(0.0f, GetSurfaceSupportDistance(SupportNormal) + FMath::Max(0.0f, SurfaceHoverOffsetCm));
		const FVector SupportedLocation = SupportHit.ImpactPoint + SupportNormal * SurfaceClearance;
		const float SupportDistance = FVector::Dist(GetActorLocation(), SupportedLocation);
		const bool bSupportedCloseEnough = SupportDistance <= FMath::Max(1.0f, PhysicsRagdollSupportSnapDistanceCm);
		const bool bRecoverableSlideSpeed = LinearSpeed <= FMath::Max(
			0.0f,
			PhysicsRagdollForceRecoverLinearSpeedThresholdCmPerSec);
		if (bSupportedCloseEnough && bRecoverableSlideSpeed)
		{
			PhysicsSupportedRecoveryTime += SafeDeltaSeconds;
		}
		else
		{
			PhysicsSupportedRecoveryTime = 0.0f;
		}
	}
	else
	{
		PhysicsSupportedRecoveryTime = 0.0f;
	}

	const bool bReachedSupportedRecovery = PhysicsSupportedRecoveryTime >= FMath::Max(
		0.05f,
		PhysicsRagdollSupportedRecoveryDelaySeconds);
	const bool bReachedRestRecovery = PhysicsRecoveryRestTime >= FMath::Max(0.05f, PhysicsRecoveryRestDurationSeconds);
	if (!bReachedSupportedRecovery && !bReachedRestRecovery)
	{
		return;
	}

	CollisionBody->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
	CollisionBody->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
	ExitPhysicsRagdollMode();
}

void AMiningBotActor::ExitPhysicsRagdollMode()
{
	if (UPrimitiveComponent* CollisionBody = GetActiveCollisionBody())
	{
		CollisionBody->SetSimulatePhysics(false);
		CollisionBody->SetEnableGravity(false);
		CollisionBody->SetAllPhysicsLinearVelocity(FVector::ZeroVector);
		CollisionBody->SetAllPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		if (bHasCachedPrePhysicsDamping)
		{
			CollisionBody->SetLinearDamping(CachedPrePhysicsLinearDamping);
			CollisionBody->SetAngularDamping(CachedPrePhysicsAngularDamping);
		}
		CollisionBody->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		CollisionBody->SetCollisionResponseToAllChannels(ECR_Block);
		CollisionBody->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}

	if (BotMesh)
	{
		BotMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		BotMesh->SetCollisionResponseToAllChannels(ECR_Block);
		BotMesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	}

	bHasCachedPrePhysicsDamping = false;
	PhysicsRecoveryRestTime = 0.0f;
	PhysicsSupportedRecoveryTime = 0.0f;
	bHeldByPhysicsHandle = false;

	const FVector ProbeNormal = GetActorUpVector().GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
	FHitResult RecoverySurfaceHit;
	if (ResolveSupportSurface(GetActorLocation(), ProbeNormal, RecoverySurfaceHit))
	{
		const FVector SurfaceNormal = RecoverySurfaceHit.ImpactNormal.GetSafeNormal(UE_SMALL_NUMBER, ProbeNormal);
		const float SurfaceClearance = FMath::Max(0.0f, GetSurfaceSupportDistance(SurfaceNormal) + FMath::Max(0.0f, SurfaceHoverOffsetCm));
		const FVector RecoveredLocation = RecoverySurfaceHit.ImpactPoint + SurfaceNormal * SurfaceClearance;
		SetActorLocation(RecoveredLocation, false, nullptr, ETeleportType::None);
	}

	ResetMovementFailures();

	if (bReturnHomeAfterPhysicsRecovery || GetCarriedTotalQuantityScaled() > 0)
	{
		bReturnHomeAfterPhysicsRecovery = false;
		SetRuntimeState(EMiningBotRuntimeState::ReturningToSwarm, TEXT("Recovered, returning"));
		return;
	}

	if (CurrentTargetNode && !CurrentTargetNode->IsDepleted())
	{
		if (PrePhysicsRuntimeState == EMiningBotRuntimeState::Mining)
		{
			SetRuntimeState(EMiningBotRuntimeState::Mining, TEXT("Recovered, mining"));
			return;
		}

		SetRuntimeState(EMiningBotRuntimeState::TravelingToNode, TEXT("Recovered, resuming"));
		return;
	}

	SetRuntimeState(EMiningBotRuntimeState::Idle, TEXT("Recovered"));
}

void AMiningBotActor::RegisterMovementFailure(const FString& Reason, bool bReturnHomeAfterRecover)
{
	++ConsecutiveMoveFailureCount;
	if (ConsecutiveMoveFailureCount < FMath::Max(1, MaxMoveFailuresBeforeRecovery))
	{
		return;
	}

	EnterPhysicsRagdollMode(Reason, bReturnHomeAfterRecover);
}

void AMiningBotActor::ResetMovementFailures()
{
	ConsecutiveMoveFailureCount = 0;
	ConsecutiveNoSurfaceFailureCount = 0;
	NoSurfaceAirborneTimeSeconds = 0.0f;
	PhysicsSupportedRecoveryTime = 0.0f;
}

float AMiningBotActor::GetEffectiveMiningDurationSeconds() const
{
	const float SafeDuration = FMath::Max(0.05f, MiningDurationSeconds);
	const float SafeMultiplier = FMath::Max(0.1f, MiningSpeedMultiplier);
	return SafeDuration / SafeMultiplier;
}
