// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/MinerActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Factory/MachineOutputVolumeComponent.h"
#include "Factory/MaterialNodeActor.h"
#include "Material/AgentResourceTypes.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
FString FormatScaledUnits(int32 QuantityScaled)
{
	const float Units = AgentResource::ScaledToUnits(FMath::Max(0, QuantityScaled));
	if (FMath::IsNearlyEqual(Units, FMath::RoundToFloat(Units), 0.001f))
	{
		return FString::FromInt(FMath::RoundToInt(Units));
	}

	return FString::Printf(TEXT("%.2f"), Units);
}
}

AMinerActor::AMinerActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	MinerMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MinerMesh"));
	MinerMesh->SetupAttachment(SceneRoot);
	MinerMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MinerMesh->SetCollisionResponseToAllChannels(ECR_Block);
	MinerMesh->SetGenerateOverlapEvents(false);
	MinerMesh->SetCanEverAffectNavigation(false);

	OutputVolume = CreateDefaultSubobject<UMachineOutputVolumeComponent>(TEXT("OutputVolume"));
	OutputVolume->SetupAttachment(SceneRoot);
	OutputVolume->SetBoxExtent(FVector(24.0f, 24.0f, 24.0f));
	OutputVolume->SetRelativeLocation(FVector(95.0f, 0.0f, 45.0f));
	OutputVolume->ProcessingInterval = 0.1f;
	OutputVolume->OutputChunkSizeUnits = 1;
	OutputVolume->bAutoEject = true;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> MinerMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (MinerMeshAsset.Succeeded())
	{
		MinerMesh->SetStaticMesh(MinerMeshAsset.Object);
		MinerMesh->SetRelativeScale3D(FVector(1.2f, 1.2f, 0.9f));
	}

	RuntimeStatus = FText::FromString(TEXT("Missing node"));
}

void AMinerActor::BeginPlay()
{
	Super::BeginPlay();

	if (!TargetNode)
	{
		if (AMaterialNodeActor* AttachedNode = Cast<AMaterialNodeActor>(GetAttachParentActor()))
		{
			TargetNode = AttachedNode;
		}
	}

	TimeUntilNextMine = 0.0f;
	if (!bEnabled)
	{
		SetRuntimeState(EMaterialMinerRuntimeState::Paused, TEXT("Paused"));
	}
	else if (!TargetNode)
	{
		SetRuntimeState(EMaterialMinerRuntimeState::MissingNode, TEXT("No target node"));
	}
	else
	{
		SetRuntimeState(EMaterialMinerRuntimeState::Mining, TEXT("Ready"));
	}
}

void AMinerActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bEnabled)
	{
		SetRuntimeState(EMaterialMinerRuntimeState::Paused, TEXT("Paused"));
		return;
	}

	if (!TargetNode)
	{
		SetRuntimeState(EMaterialMinerRuntimeState::MissingNode, TEXT("No target node"));
		return;
	}

	if (TargetNode->IsDepleted())
	{
		SetRuntimeState(EMaterialMinerRuntimeState::NodeDepleted, TEXT("Node depleted"));
		return;
	}

	TimeUntilNextMine = FMath::Max(0.0f, TimeUntilNextMine - FMath::Max(0.0f, DeltaSeconds));
	if (TimeUntilNextMine > 0.0f)
	{
		return;
	}

	const bool bDidMine = TryMineNow();
	TimeUntilNextMine = FMath::Max(0.01f, MiningIntervalSeconds);

	if (!bDidMine)
	{
		if (!TargetNode)
		{
			SetRuntimeState(EMaterialMinerRuntimeState::MissingNode, TEXT("No target node"));
		}
		else if (TargetNode->IsDepleted())
		{
			SetRuntimeState(EMaterialMinerRuntimeState::NodeDepleted, TEXT("Node depleted"));
		}
		else
		{
			SetRuntimeState(EMaterialMinerRuntimeState::OutputBlocked, TEXT("Output blocked"));
		}
	}
}

void AMinerActor::SetTargetNode(AMaterialNodeActor* NewTargetNode)
{
	if (TargetNode == NewTargetNode)
	{
		return;
	}

	if (TargetNode && GetAttachParentActor() == TargetNode)
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}

	TargetNode = NewTargetNode;

	if (bAttachToTargetNode && TargetNode)
	{
		AttachToActor(TargetNode, FAttachmentTransformRules::KeepWorldTransform);
	}
	else if (GetAttachParentActor())
	{
		DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
}

bool AMinerActor::TryMineNow()
{
	if (!bEnabled)
	{
		SetRuntimeState(EMaterialMinerRuntimeState::Paused, TEXT("Paused"));
		return false;
	}

	if (!TargetNode)
	{
		SetRuntimeState(EMaterialMinerRuntimeState::MissingNode, TEXT("No target node"));
		return false;
	}

	if (!OutputVolume)
	{
		SetRuntimeState(EMaterialMinerRuntimeState::OutputBlocked, TEXT("Missing output volume"));
		return false;
	}

	const int32 DesiredScaled = AgentResource::WholeUnitsToScaled(ExtractionUnitsPerCycle);
	const int32 RequestBudgetScaled = ResolveOutputRequestBudgetScaled(DesiredScaled);
	if (RequestBudgetScaled <= 0)
	{
		SetRuntimeState(EMaterialMinerRuntimeState::OutputBlocked, TEXT("Output buffer full"));
		return false;
	}

	TMap<FName, int32> ExtractedResourcesScaled;
	if (!TargetNode->ExtractResourcesScaled(RequestBudgetScaled, ExtractedResourcesScaled) || ExtractedResourcesScaled.Num() == 0)
	{
		if (TargetNode->IsDepleted())
		{
			SetRuntimeState(EMaterialMinerRuntimeState::NodeDepleted, TEXT("Node depleted"));
		}
		else
		{
			SetRuntimeState(EMaterialMinerRuntimeState::OutputBlocked, TEXT("No extractable resources"));
		}
		return false;
	}

	TMap<FName, int32> RejectedResourcesScaled;
	int32 QueuedScaled = 0;
	QueueExtractedResources(ExtractedResourcesScaled, RejectedResourcesScaled, QueuedScaled);

	if (QueuedScaled <= 0)
	{
		// Nothing was accepted by output, so fully rollback this extraction.
		TargetNode->AddResourcesBack(ExtractedResourcesScaled);
		SetRuntimeState(EMaterialMinerRuntimeState::OutputBlocked, TEXT("Output blocked"));
		return false;
	}

	if (RejectedResourcesScaled.Num() > 0)
	{
		TargetNode->AddResourcesBack(RejectedResourcesScaled);
	}

	if (RejectedResourcesScaled.Num() > 0)
	{
		SetRuntimeStateFormatted(
			EMaterialMinerRuntimeState::Mining,
			FString::Printf(TEXT("Mined %s units (partial)"), *FormatScaledUnits(QueuedScaled)));
	}
	else
	{
		SetRuntimeStateFormatted(
			EMaterialMinerRuntimeState::Mining,
			FString::Printf(TEXT("Mined %s units"), *FormatScaledUnits(QueuedScaled)));
	}

	return true;
}

int32 AMinerActor::ResolveOutputRequestBudgetScaled(int32 DesiredScaled) const
{
	if (!OutputVolume)
	{
		return 0;
	}

	const int32 SafeDesiredScaled = FMath::Max(0, DesiredScaled);
	if (SafeDesiredScaled <= 0)
	{
		return 0;
	}

	const int32 MaxOutputScaled = OutputVolume->GetMaxQuantityScaled();
	if (MaxOutputScaled <= 0)
	{
		return SafeDesiredScaled;
	}

	const int32 CurrentOutputScaled = FMath::Max(0, OutputVolume->GetCurrentQuantityScaled());
	const int32 RemainingOutputScaled = FMath::Max(0, MaxOutputScaled - CurrentOutputScaled);
	return FMath::Clamp(SafeDesiredScaled, 0, RemainingOutputScaled);
}

void AMinerActor::SetRuntimeState(EMaterialMinerRuntimeState NewState, const TCHAR* NewStatusMessage)
{
	SetRuntimeStateFormatted(NewState, FString(NewStatusMessage ? NewStatusMessage : TEXT("")));
}

void AMinerActor::SetRuntimeStateFormatted(EMaterialMinerRuntimeState NewState, const FString& NewStatusMessage)
{
	const FText NewStatusText = FText::FromString(NewStatusMessage);
	if (RuntimeState == NewState && RuntimeStatus.EqualTo(NewStatusText))
	{
		return;
	}

	RuntimeState = NewState;
	RuntimeStatus = NewStatusText;
	OnMinerStateChanged(RuntimeState, RuntimeStatus);
}

bool AMinerActor::QueueExtractedResources(
	const TMap<FName, int32>& ExtractedResourcesScaled,
	TMap<FName, int32>& OutRejectedResourcesScaled,
	int32& OutQueuedScaled) const
{
	OutRejectedResourcesScaled.Reset();
	OutQueuedScaled = 0;

	if (!OutputVolume || ExtractedResourcesScaled.Num() == 0)
	{
		return false;
	}

	for (const TPair<FName, int32>& ExtractedPair : ExtractedResourcesScaled)
	{
		const FName ResourceId = ExtractedPair.Key;
		const int32 QuantityScaled = FMath::Max(0, ExtractedPair.Value);
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
