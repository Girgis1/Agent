// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/MiningSwarmMachine.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Factory/ConveyorPlacementPreview.h"
#include "Factory/FactoryPlacementHelpers.h"
#include "Factory/MachineOutputVolumeComponent.h"
#include "Factory/MaterialNodeActor.h"
#include "Factory/MiningBotActor.h"
#include "UObject/ConstructorHelpers.h"

AMiningSwarmMachine::AMiningSwarmMachine()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SupportCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("SupportCollision"));
	SupportCollision->SetupAttachment(SceneRoot);
	SupportCollision->SetBoxExtent(FVector(85.0f, 85.0f, 70.0f));
	SupportCollision->SetRelativeLocation(FVector(0.0f, 0.0f, 70.0f));
	SupportCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SupportCollision->SetCollisionObjectType(AgentFactory::FactoryBuildableChannel);
	SupportCollision->SetCollisionResponseToAllChannels(ECR_Block);
	SupportCollision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	SupportCollision->SetGenerateOverlapEvents(false);
	SupportCollision->SetCanEverAffectNavigation(false);

	MachineMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MachineMesh"));
	MachineMesh->SetupAttachment(SupportCollision);
	MachineMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	MachineMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	MachineMesh->SetGenerateOverlapEvents(false);
	MachineMesh->SetCanEverAffectNavigation(false);

	BotSpawnRoot = CreateDefaultSubobject<USceneComponent>(TEXT("BotSpawnRoot"));
	BotSpawnRoot->SetupAttachment(SceneRoot);
	BotSpawnRoot->SetRelativeLocation(FVector(0.0f, 0.0f, 85.0f));

	BotDockVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("BotDockVolume"));
	BotDockVolume->SetupAttachment(SceneRoot);
	BotDockVolume->SetRelativeLocation(FVector(0.0f, 0.0f, 100.0f));
	BotDockVolume->SetBoxExtent(FVector(140.0f, 140.0f, 70.0f));
	BotDockVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BotDockVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	BotDockVolume->SetGenerateOverlapEvents(false);
	BotDockVolume->SetCanEverAffectNavigation(false);

	OutputVolume = CreateDefaultSubobject<UMachineOutputVolumeComponent>(TEXT("OutputVolume"));
	OutputVolume->SetupAttachment(SceneRoot);
	OutputVolume->SetBoxExtent(FVector(28.0f, 28.0f, 28.0f));
	OutputVolume->SetRelativeLocation(FVector(125.0f, 0.0f, 60.0f));
	OutputVolume->ProcessingInterval = 0.1f;
	OutputVolume->OutputChunkSizeUnits = 1;
	OutputVolume->bAutoEject = true;

	OutputArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("OutputArrow"));
	OutputArrow->SetupAttachment(SceneRoot);
	OutputArrow->SetRelativeLocation(FVector(125.0f, 0.0f, 60.0f));
	OutputArrow->ArrowSize = 1.4f;
	OutputArrow->SetCanEverAffectNavigation(false);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> MachineMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (MachineMeshAsset.Succeeded())
	{
		MachineMesh->SetStaticMesh(MachineMeshAsset.Object);
		MachineMesh->SetRelativeScale3D(FVector(1.6f, 1.6f, 1.15f));
	}

	static ConstructorHelpers::FClassFinder<AMiningBotActor> MiningBotBlueprintClass(
		TEXT("/Game/Factory/Mining/BP_MiningBot"));
	if (MiningBotBlueprintClass.Succeeded())
	{
		MiningBotClass = MiningBotBlueprintClass.Class;
	}
}

void AMiningSwarmMachine::BeginPlay()
{
	Super::BeginPlay();

	if (IsPlacementPreviewInstance())
	{
		SetActorTickEnabled(false);
		return;
	}

	TimeUntilNodeRefresh = 0.0f;
	TimeUntilBotMaintenance = 0.0f;
	TimeUntilNextBotSpawn = 0.0f;
	bLastEnabledState = bEnabled;
	if (bEnabled)
	{
		StartActivationWarmup();
	}
	else
	{
		WarmupTimeRemainingSeconds = 0.0f;
		SetSwarmOperational(false);
	}

	RefreshNearbyResourceNodes();
	EnsureBotPopulation();
}

void AMiningSwarmMachine::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IsPlacementPreviewInstance())
	{
		return;
	}

	UpdateActivationState(DeltaSeconds);
	if (!IsSwarmEnabled())
	{
		return;
	}

	FlushBufferedBotResourcesToOutput();

	TimeUntilNodeRefresh = FMath::Max(0.0f, TimeUntilNodeRefresh - FMath::Max(0.0f, DeltaSeconds));
	if (TimeUntilNodeRefresh <= 0.0f)
	{
		RefreshNearbyResourceNodes();
		TimeUntilNodeRefresh = FMath::Max(0.05f, NodeRefreshIntervalSeconds);
	}

	TimeUntilBotMaintenance = FMath::Max(0.0f, TimeUntilBotMaintenance - FMath::Max(0.0f, DeltaSeconds));
	if (TimeUntilBotMaintenance <= 0.0f)
	{
		EnsureBotPopulation();
		TimeUntilBotMaintenance = FMath::Max(0.05f, BotMaintenanceIntervalSeconds);
	}

	if (bAutoSpawnBots)
	{
		TimeUntilNextBotSpawn = FMath::Max(0.0f, TimeUntilNextBotSpawn - FMath::Max(0.0f, DeltaSeconds));
		if (TimeUntilNextBotSpawn <= 0.0f)
		{
			EnsureBotPopulation();
		}
	}
}

void AMiningSwarmMachine::RefreshNearbyResourceNodes()
{
	NearbyResourceNodes.Reset();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (TActorIterator<AMaterialNodeActor> NodeIt(World); NodeIt; ++NodeIt)
	{
		AMaterialNodeActor* CandidateNode = *NodeIt;
		if (!IsNodeWithinSearchRange(CandidateNode))
		{
			continue;
		}

		NearbyResourceNodes.Add(CandidateNode);
	}
}

int32 AMiningSwarmMachine::GetNearbyNodeCount() const
{
	int32 Count = 0;
	for (const TWeakObjectPtr<AMaterialNodeActor>& NodePtr : NearbyResourceNodes)
	{
		if (IsNodeWithinSearchRange(NodePtr.Get()))
		{
			++Count;
		}
	}

	return Count;
}

int32 AMiningSwarmMachine::GetActiveBotCount() const
{
	int32 Count = 0;
	for (AMiningBotActor* Bot : SpawnedBots)
	{
		if (IsValid(Bot))
		{
			++Count;
		}
	}

	return Count;
}

bool AMiningSwarmMachine::IsSwarmEnabled() const
{
	return bEnabled && bSwarmOperational;
}

bool AMiningSwarmMachine::RequestMiningAssignment(
	AMiningBotActor* RequestingBot,
	AMaterialNodeActor*& OutTargetNode,
	FVector& OutTargetLocation,
	FVector& OutTargetSurfaceNormal)
{
	(void)RequestingBot;

	OutTargetNode = nullptr;
	OutTargetLocation = FVector::ZeroVector;
	OutTargetSurfaceNormal = FVector::UpVector;

	if (IsPlacementPreviewInstance() || !IsSwarmEnabled())
	{
		return false;
	}

	if (NearbyResourceNodes.Num() == 0)
	{
		RefreshNearbyResourceNodes();
	}

	TArray<AMaterialNodeActor*> CandidateNodes;
	CandidateNodes.Reserve(NearbyResourceNodes.Num());
	for (const TWeakObjectPtr<AMaterialNodeActor>& NodePtr : NearbyResourceNodes)
	{
		if (AMaterialNodeActor* CandidateNode = NodePtr.Get())
		{
			if (IsNodeViableForMining(CandidateNode))
			{
				CandidateNodes.Add(CandidateNode);
			}
		}
	}

	while (CandidateNodes.Num() > 0)
	{
		const int32 PickIndex = FMath::RandRange(0, CandidateNodes.Num() - 1);
		AMaterialNodeActor* PickedNode = CandidateNodes[PickIndex];
		CandidateNodes.RemoveAtSwap(PickIndex);

		if (!SampleRandomSurfacePointOnNode(PickedNode, OutTargetLocation, OutTargetSurfaceNormal))
		{
			continue;
		}

		OutTargetNode = PickedNode;
		return true;
	}

	return false;
}

bool AMiningSwarmMachine::QueueBotResources(
	const TMap<FName, int32>& ResourcesScaled,
	TMap<FName, int32>& OutRejectedResourcesScaled,
	int32& OutQueuedScaled)
{
	OutRejectedResourcesScaled.Reset();
	OutQueuedScaled = 0;

	if (IsPlacementPreviewInstance() || !IsSwarmEnabled() || !OutputVolume || ResourcesScaled.Num() == 0)
	{
		return false;
	}

	for (const TPair<FName, int32>& ResourcePair : ResourcesScaled)
	{
		const FName ResourceId = ResourcePair.Key;
		const int32 QuantityScaled = FMath::Max(0, ResourcePair.Value);
		if (ResourceId.IsNone() || QuantityScaled <= 0)
		{
			continue;
		}

		const int32 AcceptedScaled = FMath::Clamp(OutputVolume->QueueResourceScaled(ResourceId, QuantityScaled), 0, QuantityScaled);
		const int32 RejectedScaled = FMath::Max(0, QuantityScaled - AcceptedScaled);
		OutQueuedScaled += AcceptedScaled;

		if (RejectedScaled > 0)
		{
			OutRejectedResourcesScaled.FindOrAdd(ResourceId) += RejectedScaled;
		}
	}

	return OutQueuedScaled > 0;
}

bool AMiningSwarmMachine::HandleBotDocking(
	AMiningBotActor* Bot,
	const TMap<FName, int32>& ResourcesScaled,
	int32& OutQueuedScaled,
	int32& OutBufferedScaled)
{
	OutQueuedScaled = 0;
	OutBufferedScaled = 0;

	if (!IsValid(Bot))
	{
		return false;
	}

	if (ResourcesScaled.Num() > 0)
	{
		TMap<FName, int32> RejectedResourcesScaled;
		QueueBotResources(ResourcesScaled, RejectedResourcesScaled, OutQueuedScaled);

		for (const TPair<FName, int32>& RejectedPair : RejectedResourcesScaled)
		{
			const FName ResourceId = RejectedPair.Key;
			const int32 QuantityScaled = FMath::Max(0, RejectedPair.Value);
			if (ResourceId.IsNone() || QuantityScaled <= 0)
			{
				continue;
			}

			BufferedBotResourcesScaled.FindOrAdd(ResourceId) += QuantityScaled;
			OutBufferedScaled += QuantityScaled;
		}
	}

	if (!bDespawnBotsOnDock)
	{
		return false;
	}

	SpawnedBots.RemoveSingleSwap(Bot);
	TimeUntilNextBotSpawn = FMath::Max(
		TimeUntilNextBotSpawn,
		FMath::Max(0.01f, BotSpawnIntervalSeconds));
	Bot->Destroy();
	return true;
}

FVector AMiningSwarmMachine::GetBotHomeLocation(const AMiningBotActor* Bot) const
{
	return ResolveBotDockPoint(ResolveBotIndex(Bot));
}

FVector AMiningSwarmMachine::GetBotHomeNormal() const
{
	const FVector PreferredNormal = BotDockVolume
		? BotDockVolume->GetUpVector()
		: (BotSpawnRoot ? BotSpawnRoot->GetUpVector() : GetActorUpVector());
	return PreferredNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
}

void AMiningSwarmMachine::StartActivationWarmup()
{
	const float WarmupDuration = bUseActivationWarmup
		? FMath::Max(0.0f, ActivationWarmupDurationSeconds)
		: 0.0f;

	WarmupTimeRemainingSeconds = WarmupDuration;
	SetSwarmOperational(WarmupDuration <= KINDA_SMALL_NUMBER);
	if (IsSwarmEnabled())
	{
		TimeUntilBotMaintenance = 0.0f;
		TimeUntilNextBotSpawn = FMath::Max(0.0f, InitialBotSpawnDelaySeconds);
	}
}

void AMiningSwarmMachine::SetSwarmOperational(bool bNewOperational)
{
	bSwarmOperational = bNewOperational;

	if (OutputVolume)
	{
		OutputVolume->bEnabled = bSwarmOperational;
		OutputVolume->SetComponentTickEnabled(bSwarmOperational);
		OutputVolume->SetCollisionEnabled(bSwarmOperational ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
		OutputVolume->SetGenerateOverlapEvents(bSwarmOperational);
		OutputVolume->SetHiddenInGame(!bSwarmOperational);
		OutputVolume->SetVisibility(bSwarmOperational, true);
	}

	if (OutputArrow)
	{
		OutputArrow->SetHiddenInGame(!bSwarmOperational);
		OutputArrow->SetVisibility(bSwarmOperational, true);
	}
}

void AMiningSwarmMachine::UpdateActivationState(float DeltaSeconds)
{
	const bool bCurrentEnabledState = bEnabled;
	if (bCurrentEnabledState != bLastEnabledState)
	{
		if (bCurrentEnabledState)
		{
			StartActivationWarmup();
		}
		else
		{
			WarmupTimeRemainingSeconds = 0.0f;
			SetSwarmOperational(false);
		}
		bLastEnabledState = bCurrentEnabledState;
	}

	if (!bEnabled || bSwarmOperational)
	{
		return;
	}

	WarmupTimeRemainingSeconds = FMath::Max(0.0f, WarmupTimeRemainingSeconds - FMath::Max(0.0f, DeltaSeconds));
	if (WarmupTimeRemainingSeconds > 0.0f)
	{
		return;
	}

	SetSwarmOperational(true);
	TimeUntilBotMaintenance = 0.0f;
	TimeUntilNextBotSpawn = FMath::Max(0.0f, InitialBotSpawnDelaySeconds);
}

void AMiningSwarmMachine::FlushBufferedBotResourcesToOutput()
{
	if (!IsSwarmEnabled() || !OutputVolume || BufferedBotResourcesScaled.Num() == 0)
	{
		return;
	}

	TArray<FName> ResourceIdsToClear;
	for (TPair<FName, int32>& BufferedPair : BufferedBotResourcesScaled)
	{
		const FName ResourceId = BufferedPair.Key;
		const int32 PendingQuantityScaled = FMath::Max(0, BufferedPair.Value);
		if (ResourceId.IsNone() || PendingQuantityScaled <= 0)
		{
			ResourceIdsToClear.Add(ResourceId);
			continue;
		}

		const int32 AcceptedScaled = FMath::Clamp(
			OutputVolume->QueueResourceScaled(ResourceId, PendingQuantityScaled),
			0,
			PendingQuantityScaled);
		BufferedPair.Value = PendingQuantityScaled - AcceptedScaled;
		if (BufferedPair.Value <= 0)
		{
			ResourceIdsToClear.Add(ResourceId);
		}
	}

	for (const FName& ResourceId : ResourceIdsToClear)
	{
		BufferedBotResourcesScaled.Remove(ResourceId);
	}
}

bool AMiningSwarmMachine::IsNodeWithinSearchRange(const AMaterialNodeActor* Node) const
{
	if (!IsValid(Node))
	{
		return false;
	}

	const float SearchRadius = FMath::Max(0.0f, ResourceSearchRadiusCm);
	if (SearchRadius <= 0.0f)
	{
		return true;
	}

	return FVector::DistSquared(Node->GetActorLocation(), GetActorLocation()) <= FMath::Square(SearchRadius);
}

bool AMiningSwarmMachine::IsNodeViableForMining(const AMaterialNodeActor* Node) const
{
	if (!IsNodeWithinSearchRange(Node))
	{
		return false;
	}

	return Node->IsInfiniteNode() || !Node->IsDepleted();
}

bool AMiningSwarmMachine::IsPlacementPreviewInstance() const
{
	if (const AActor* OwnerActor = GetOwner())
	{
		if (OwnerActor->IsA<AConveyorPlacementPreview>())
		{
			return true;
		}
	}

	for (const AActor* ParentActor = GetAttachParentActor(); ParentActor; ParentActor = ParentActor->GetAttachParentActor())
	{
		if (ParentActor->IsA<AConveyorPlacementPreview>())
		{
			return true;
		}
	}

	return false;
}

int32 AMiningSwarmMachine::ResolveBotIndex(const AMiningBotActor* Bot) const
{
	if (!Bot)
	{
		return 0;
	}

	int32 BotIndex = SpawnedBots.IndexOfByPredicate(
		[Bot](const AMiningBotActor* SpawnedBot)
		{
			return SpawnedBot == Bot;
		});

	if (BotIndex == INDEX_NONE)
	{
		BotIndex = static_cast<int32>(Bot->GetUniqueID() & 0x7FFFFFFF);
	}

	return FMath::Max(0, BotIndex);
}

FVector AMiningSwarmMachine::ResolveBotDockPoint(int32 BotIndex) const
{
	const int32 SafeIndex = FMath::Max(0, BotIndex);

	if (BotDockVolume)
	{
		const FTransform DockTransform = BotDockVolume->GetComponentTransform();
		const FVector DockExtent = BotDockVolume->GetScaledBoxExtent()
			* FMath::Clamp(BotDockAreaFill, 0.1f, 1.0f);

		const float U = FMath::Frac(static_cast<float>(SafeIndex + 1) * 0.754877666f);
		const float V = FMath::Frac(static_cast<float>(SafeIndex + 1) * 0.569840296f);
		const FVector LocalPoint(
			(U * 2.0f - 1.0f) * DockExtent.X,
			(V * 2.0f - 1.0f) * DockExtent.Y,
			0.0f);
		return DockTransform.TransformPositionNoScale(LocalPoint);
	}

	const FTransform SpawnTransform = BotSpawnRoot ? BotSpawnRoot->GetComponentTransform() : GetActorTransform();
	if (BotHomeRingRadiusCm <= KINDA_SMALL_NUMBER || MaxBotCount <= 1)
	{
		return SpawnTransform.GetLocation();
	}

	const int32 SafeCount = FMath::Max(1, MaxBotCount);
	const float AngleDegrees = (360.0f / static_cast<float>(SafeCount)) * static_cast<float>(SafeIndex % SafeCount);
	const float AngleRadians = FMath::DegreesToRadians(AngleDegrees);
	const FVector RingOffset(
		FMath::Cos(AngleRadians) * BotHomeRingRadiusCm,
		FMath::Sin(AngleRadians) * BotHomeRingRadiusCm,
		0.0f);
	return SpawnTransform.TransformPositionNoScale(RingOffset);
}

void AMiningSwarmMachine::RemoveInvalidBotReferences()
{
	SpawnedBots.RemoveAll(
		[](const AMiningBotActor* Bot)
		{
			return !IsValid(Bot);
		});
}

void AMiningSwarmMachine::EnsureBotPopulation()
{
	RemoveInvalidBotReferences();

	if (IsPlacementPreviewInstance() || !IsSwarmEnabled())
	{
		return;
	}

	const int32 DesiredCount = FMath::Max(0, MaxBotCount);
	if (DesiredCount < SpawnedBots.Num())
	{
		const int32 RemoveCount = SpawnedBots.Num() - DesiredCount;
		for (int32 RemoveIndex = 0; RemoveIndex < RemoveCount; ++RemoveIndex)
		{
			const int32 BotArrayIndex = SpawnedBots.Num() - 1;
			if (BotArrayIndex < 0)
			{
				break;
			}

			AMiningBotActor* BotToRemove = SpawnedBots[BotArrayIndex];
			SpawnedBots.RemoveAt(BotArrayIndex);
			if (IsValid(BotToRemove))
			{
				BotToRemove->Destroy();
			}
		}
	}

	if (!bAutoSpawnBots || DesiredCount <= 0 || SpawnedBots.Num() >= DesiredCount)
	{
		return;
	}

	if (TimeUntilNextBotSpawn > 0.0f)
	{
		return;
	}

	const int32 MaxBatchCount = bDespawnBotsOnDock
		? 1
		: FMath::Max(1, BotsPerSpawnBatch);
	int32 SpawnedCount = 0;
	while (SpawnedCount < MaxBatchCount && SpawnedBots.Num() < DesiredCount)
	{
		if (!TrySpawnSingleBot())
		{
			break;
		}
		++SpawnedCount;
	}

	if (SpawnedCount > 0)
	{
		TimeUntilNextBotSpawn = FMath::Max(0.01f, BotSpawnIntervalSeconds);
	}
	else
	{
		TimeUntilNextBotSpawn = FMath::Max(0.1f, BotSpawnIntervalSeconds);
	}
}

bool AMiningSwarmMachine::TrySpawnSingleBot()
{
	if (IsPlacementPreviewInstance() || !IsSwarmEnabled() || !bAutoSpawnBots)
	{
		return false;
	}

	const int32 DesiredCount = FMath::Max(0, MaxBotCount);
	if (DesiredCount <= 0 || SpawnedBots.Num() >= DesiredCount)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	UClass* SpawnClass = MiningBotClass.Get() ? MiningBotClass.Get() : AMiningBotActor::StaticClass();
	const int32 BotIndex = SpawnedBots.Num();
	const FVector SpawnLocation = ResolveBotDockPoint(BotIndex);
	const FRotator SpawnRotation = (BotDockVolume
		? BotDockVolume->GetComponentRotation()
		: (BotSpawnRoot ? BotSpawnRoot->GetComponentRotation() : GetActorRotation()));

	FActorSpawnParameters SpawnParams{};
	SpawnParams.Owner = this;
	SpawnParams.Instigator = nullptr;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	AMiningBotActor* SpawnedBot = World->SpawnActor<AMiningBotActor>(
		SpawnClass,
		SpawnLocation,
		SpawnRotation,
		SpawnParams);
	if (!SpawnedBot)
	{
		return false;
	}

	SpawnedBot->SetOwningSwarmMachine(this);
	SpawnedBot->SetMiningTuning(
		DefaultBotMiningCapacityUnits,
		DefaultBotTravelSpeedCmPerSecond,
		DefaultBotMiningSpeedMultiplier);
	SpawnedBot->MiningDurationSeconds = FMath::Max(0.05f, DefaultBotMiningDurationSeconds);
	SpawnedBot->SurfaceAlignmentInterpSpeed = FMath::Max(0.05f, DefaultBotSurfaceAlignmentInterpSpeed);
	SpawnedBot->GroundSnapInterpSpeed = FMath::Max(0.05f, DefaultBotGroundSnapInterpSpeed);
	SpawnedBot->SupportNormalSmoothingSpeed = FMath::Max(0.05f, DefaultBotSupportNormalSmoothingSpeed);
	SpawnedBot->MaxTraversalStepPerTickCm = FMath::Max(1.0f, DefaultBotMaxTraversalStepPerTickCm);
	SpawnedBot->ObstacleSlideScale = FMath::Clamp(DefaultBotObstacleSlideScale, 0.0f, 1.0f);
	SpawnedBot->ObstacleContactPadding = FMath::Max(0.0f, DefaultBotObstacleContactPadding);
	SpawnedBot->ArrivalToleranceCm = FMath::Max(1.0f, DefaultBotArrivalToleranceCm);
	SpawnedBot->SurfaceHoverOffsetCm = FMath::Max(0.0f, DefaultBotSurfaceHoverOffsetCm);
	SpawnedBot->bEnableVisualLag = bDefaultBotVisualLagEnabled;
	SpawnedBot->VisualLagLocationInterpSpeed = FMath::Max(0.05f, DefaultBotVisualLagLocationInterpSpeed);
	SpawnedBot->VisualLagRotationInterpSpeed = FMath::Max(0.05f, DefaultBotVisualLagRotationInterpSpeed);
	SpawnedBot->VisualLagMaxDistanceCm = FMath::Max(0.0f, DefaultBotVisualLagMaxDistanceCm);
	SpawnedBot->DepositRetryIntervalSeconds = FMath::Max(0.05f, DefaultBotDepositRetryIntervalSeconds);
	SpawnedBot->SurfaceProbeStartOffsetCm = FMath::Max(0.0f, DefaultBotSurfaceProbeStartOffsetCm);
	SpawnedBot->SurfaceProbeDepthCm = FMath::Max(25.0f, DefaultBotSurfaceProbeDepthCm);
	SpawnedBot->SurfaceProbeForwardLookCm = FMath::Max(0.0f, DefaultBotSurfaceProbeForwardLookCm);
	SpawnedBot->MaxStepUpHeightCm = FMath::Max(0.0f, DefaultBotMaxStepUpHeightCm);

	SpawnedBots.Add(SpawnedBot);
	OnMiningBotSpawned(SpawnedBot);
	return true;
}

bool AMiningSwarmMachine::SampleRandomSurfacePointOnNode(
	AMaterialNodeActor* Node,
	FVector& OutTargetLocation,
	FVector& OutTargetSurfaceNormal) const
{
	OutTargetLocation = FVector::ZeroVector;
	OutTargetSurfaceNormal = FVector::UpVector;

	if (!IsNodeViableForMining(Node))
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FVector NodeOrigin = FVector::ZeroVector;
	FVector NodeExtent = FVector::ZeroVector;
	Node->GetActorBounds(true, NodeOrigin, NodeExtent);

	const float NodeRadius = FMath::Max3(NodeExtent.X, NodeExtent.Y, NodeExtent.Z);
	const float TraceDistance = FMath::Max(60.0f, NodeRadius + FMath::Max(0.0f, SurfaceSampleTracePaddingCm));
	const int32 MaxAttempts = FMath::Max(1, SurfaceSampleAttemptsPerNode);

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(MiningSwarmSurfaceSample), false, this);
	QueryParams.AddIgnoredActor(this);
	for (AMiningBotActor* Bot : SpawnedBots)
	{
		if (IsValid(Bot))
		{
			QueryParams.AddIgnoredActor(Bot);
		}
	}

	for (int32 AttemptIndex = 0; AttemptIndex < MaxAttempts; ++AttemptIndex)
	{
		const FVector RandomDirection = FMath::VRand();
		if (RandomDirection.IsNearlyZero())
		{
			continue;
		}

		const FVector TraceStart = NodeOrigin + RandomDirection * TraceDistance;
		const FVector TraceEnd = NodeOrigin - RandomDirection * TraceDistance;

		FHitResult SurfaceHit;
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

		if (!bHit)
		{
			continue;
		}

		AMaterialNodeActor* HitNode = ResolveMaterialNodeFromActor(SurfaceHit.GetActor());
		if (HitNode != Node)
		{
			continue;
		}

		const FVector SurfaceNormal = SurfaceHit.ImpactNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
		OutTargetSurfaceNormal = SurfaceNormal;
		OutTargetLocation = SurfaceHit.ImpactPoint + SurfaceNormal * FMath::Max(0.0f, SurfaceSampleOffsetCm);
		return true;
	}

	return false;
}

AMaterialNodeActor* AMiningSwarmMachine::ResolveMaterialNodeFromActor(AActor* CandidateActor) const
{
	if (!CandidateActor)
	{
		return nullptr;
	}

	if (AMaterialNodeActor* HitNode = Cast<AMaterialNodeActor>(CandidateActor))
	{
		return HitNode;
	}

	if (AMaterialNodeActor* OwnerNode = Cast<AMaterialNodeActor>(CandidateActor->GetOwner()))
	{
		return OwnerNode;
	}

	for (AActor* ParentActor = CandidateActor->GetAttachParentActor(); ParentActor; ParentActor = ParentActor->GetAttachParentActor())
	{
		if (AMaterialNodeActor* ParentNode = Cast<AMaterialNodeActor>(ParentActor))
		{
			return ParentNode;
		}
	}

	return nullptr;
}
