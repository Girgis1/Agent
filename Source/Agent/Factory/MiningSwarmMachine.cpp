// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/MiningSwarmMachine.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "EngineUtils.h"
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

	static ConstructorHelpers::FClassFinder<AMiningBotActor> MiningBotBlueprintClass(TEXT("/Game/Factory/BP_MiningBot"));
	if (MiningBotBlueprintClass.Succeeded())
	{
		MiningBotClass = MiningBotBlueprintClass.Class;
	}
}

void AMiningSwarmMachine::BeginPlay()
{
	Super::BeginPlay();

	TimeUntilNodeRefresh = 0.0f;
	TimeUntilBotMaintenance = 0.0f;

	RefreshNearbyResourceNodes();
	EnsureBotPopulation();
}

void AMiningSwarmMachine::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bEnabled)
	{
		return;
	}

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

	if (!bEnabled)
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

	if (!OutputVolume || ResourcesScaled.Num() == 0)
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

FVector AMiningSwarmMachine::GetBotHomeLocation(const AMiningBotActor* Bot) const
{
	const FTransform SpawnTransform = BotSpawnRoot ? BotSpawnRoot->GetComponentTransform() : GetActorTransform();
	if (!Bot || BotHomeRingRadiusCm <= KINDA_SMALL_NUMBER || MaxBotCount <= 1)
	{
		return SpawnTransform.GetLocation();
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

	const int32 SafeCount = FMath::Max(1, MaxBotCount);
	const float AngleDegrees = (360.0f / static_cast<float>(SafeCount)) * static_cast<float>(BotIndex % SafeCount);
	const float AngleRadians = FMath::DegreesToRadians(AngleDegrees);
	const FVector RingOffset(
		FMath::Cos(AngleRadians) * BotHomeRingRadiusCm,
		FMath::Sin(AngleRadians) * BotHomeRingRadiusCm,
		0.0f);

	return SpawnTransform.TransformPositionNoScale(RingOffset);
}

FVector AMiningSwarmMachine::GetBotHomeNormal() const
{
	const FVector PreferredNormal = BotSpawnRoot ? BotSpawnRoot->GetUpVector() : GetActorUpVector();
	return PreferredNormal.GetSafeNormal(UE_SMALL_NUMBER, FVector::UpVector);
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

	if (!bEnabled)
	{
		return;
	}

	const int32 DesiredCount = FMath::Max(0, MaxBotCount);
	if (DesiredCount <= 0)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UClass* SpawnClass = MiningBotClass.Get() ? MiningBotClass.Get() : AMiningBotActor::StaticClass();
	while (SpawnedBots.Num() < DesiredCount)
	{
		const int32 BotIndex = SpawnedBots.Num();
		const float AngleDegrees = static_cast<float>(BotIndex) * (360.0f / static_cast<float>(DesiredCount));
		const float AngleRadians = FMath::DegreesToRadians(AngleDegrees);
		const FVector SpawnOffset(
			FMath::Cos(AngleRadians) * FMath::Max(0.0f, BotHomeRingRadiusCm),
			FMath::Sin(AngleRadians) * FMath::Max(0.0f, BotHomeRingRadiusCm),
			0.0f);

		const FTransform SpawnRootTransform = BotSpawnRoot ? BotSpawnRoot->GetComponentTransform() : GetActorTransform();
		const FVector SpawnLocation = SpawnRootTransform.TransformPositionNoScale(SpawnOffset);
		const FRotator SpawnRotation = SpawnRootTransform.GetRotation().Rotator();

		FActorSpawnParameters SpawnParams{};
		SpawnParams.Owner = this;
		SpawnParams.Instigator = nullptr;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AMiningBotActor* SpawnedBot = World->SpawnActor<AMiningBotActor>(
			SpawnClass,
			SpawnLocation,
			SpawnRotation,
			SpawnParams);
		if (!SpawnedBot)
		{
			break;
		}

		SpawnedBot->SetOwningSwarmMachine(this);
		SpawnedBot->SetMiningTuning(
			DefaultBotMiningCapacityUnits,
			DefaultBotTravelSpeedCmPerSecond,
			DefaultBotMiningSpeedMultiplier);

		SpawnedBots.Add(SpawnedBot);
		OnMiningBotSpawned(SpawnedBot);
	}
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
