// Copyright Epic Games, Inc. All Rights Reserved.

#include "Machine/CompactorComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Factory/FactoryWorldConfig.h"
#include "GameFramework/Actor.h"
#include "Machine/MachineComponent.h"
#include "Machine/OutputVolume.h"
#include "Material/AgentResourceTypes.h"
#include "Material/MixedMaterialVoronoiActor.h"

namespace
{
struct FCompactionAllocationCandidate
{
	FName ResourceId = NAME_None;
	int32 AvailableScaled = 0;
	int32 ConsumedScaled = 0;
	double RemainderScore = 0.0;
};
}

UCompactorComponent::UCompactorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	MixedMaterialActorClass = AMixedMaterialVoronoiActor::StaticClass();
	RuntimeStatus = FText::FromString(TEXT("Idle"));
}

void UCompactorComponent::BeginPlay()
{
	Super::BeginPlay();
	ResolveDependencies();
}

void UCompactorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!ResolveDependencies())
	{
		SetRuntimeStatus(TEXT("Missing machine/output links"));
		return;
	}

	if (!bEnabled)
	{
		SetRuntimeStatus(TEXT("Paused"));
		return;
	}

	const float EffectiveCycleSpeed = ResolveEffectiveCycleSpeed();
	if (EffectiveCycleSpeed <= KINDA_SMALL_NUMBER)
	{
		SetRuntimeStatus(TEXT("Paused (global factory speed)"));
		return;
	}

	const float SafeCycleSeconds = FMath::Max(KINDA_SMALL_NUMBER, CycleTimeSeconds);
	CurrentCycleProgressSeconds += FMath::Max(0.0f, DeltaTime) * EffectiveCycleSpeed;

	bool bCompactedThisTick = false;
	int32 Iterations = 0;
	while (CurrentCycleProgressSeconds >= SafeCycleSeconds && Iterations < 8)
	{
		CurrentCycleProgressSeconds -= SafeCycleSeconds;
		bCompactedThisTick |= TryRunCompactionCycle();
		++Iterations;
	}

	if (bCompactedThisTick)
	{
		SetRuntimeStatus(TEXT("Compacting"));
		return;
	}

	SetRuntimeStatus(TEXT("Waiting for input"));
}

void UCompactorComponent::SetMachineComponent(UMachineComponent* InMachineComponent)
{
	MachineComponent = InMachineComponent;
}

void UCompactorComponent::SetOutputVolume(UOutputVolume* InOutputVolume)
{
	OutputVolume = InOutputVolume;
}

float UCompactorComponent::GetCycleProgress01() const
{
	const float SafeCycleSeconds = FMath::Max(KINDA_SMALL_NUMBER, CycleTimeSeconds);
	return FMath::Clamp(CurrentCycleProgressSeconds / SafeCycleSeconds, 0.0f, 1.0f);
}

bool UCompactorComponent::ResolveDependencies()
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return false;
	}

	if (!MachineComponent)
	{
		MachineComponent = OwnerActor->FindComponentByClass<UMachineComponent>();
	}

	if (!OutputVolume)
	{
		OutputVolume = OwnerActor->FindComponentByClass<UOutputVolume>();
	}

	return MachineComponent != nullptr && OutputVolume != nullptr;
}

float UCompactorComponent::ResolveGlobalFactorySpeed() const
{
	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<AFactoryWorldConfig> It(World); It; ++It)
		{
			if (const AFactoryWorldConfig* WorldConfig = *It)
			{
				return FMath::Max(0.0f, WorldConfig->FactoryCraftSpeedMultiplier);
			}
		}
	}

	if (const AFactoryWorldConfig* DefaultConfig = GetDefault<AFactoryWorldConfig>())
	{
		return FMath::Max(0.0f, DefaultConfig->FactoryCraftSpeedMultiplier);
	}

	return 1.0f;
}

float UCompactorComponent::ResolveEffectiveCycleSpeed() const
{
	const float MachineSpeed = MachineComponent ? FMath::Max(0.0f, MachineComponent->Speed) : 1.0f;
	return FMath::Max(0.0f, CompactionSpeedMultiplier) * MachineSpeed * ResolveGlobalFactorySpeed();
}

bool UCompactorComponent::TryRunCompactionCycle()
{
	if (!MachineComponent || !OutputVolume)
	{
		return false;
	}

	TMap<FName, int32> ConsumptionScaled;
	int32 ConsumedTotalScaled = 0;
	if (!BuildCompactionConsumptionMap(MachineComponent->StoredResourcesScaled, ConsumptionScaled, ConsumedTotalScaled))
	{
		return false;
	}

	if (!MachineComponent->ConsumeStoredResourcesScaledAtomic(ConsumptionScaled))
	{
		return false;
	}

	const bool bQueuedMixedOutput = OutputVolume->QueueMixedMaterialScaled(
		ConsumptionScaled,
		MixedMaterialActorClass,
		MinOutputUnits,
		MaxOutputUnits,
		MinOutputVisualScale,
		MaxOutputVisualScale);

	if (!bQueuedMixedOutput)
	{
		MachineComponent->AddInputResourcesScaledAtomic(ConsumptionScaled);
		return false;
	}

	return ConsumedTotalScaled > 0;
}

bool UCompactorComponent::BuildCompactionConsumptionMap(
	const TMap<FName, int32>& StoredResourcesScaled,
	TMap<FName, int32>& OutConsumptionScaled,
	int32& OutConsumedTotalScaled) const
{
	OutConsumptionScaled.Reset();
	OutConsumedTotalScaled = 0;

	int64 TotalStoredScaled = 0;
	TArray<FCompactionAllocationCandidate> Candidates;
	Candidates.Reserve(StoredResourcesScaled.Num());

	for (const TPair<FName, int32>& Pair : StoredResourcesScaled)
	{
		const FName ResourceId = Pair.Key;
		const int32 AvailableScaled = FMath::Max(0, Pair.Value);
		if (ResourceId.IsNone() || AvailableScaled <= 0)
		{
			continue;
		}

		FCompactionAllocationCandidate Candidate;
		Candidate.ResourceId = ResourceId;
		Candidate.AvailableScaled = AvailableScaled;
		Candidates.Add(Candidate);
		TotalStoredScaled += static_cast<int64>(AvailableScaled);
	}

	if (Candidates.Num() == 0 || TotalStoredScaled <= 0)
	{
		return false;
	}

	const int32 MinScaled = FMath::Max(1, AgentResource::UnitsToScaled(FMath::Max(0.01f, MinOutputUnits)));
	const int32 MaxScaled = FMath::Max(MinScaled, AgentResource::UnitsToScaled(FMath::Max(MinOutputUnits, MaxOutputUnits)));
	if (TotalStoredScaled < static_cast<int64>(MinScaled))
	{
		return false;
	}

	const int32 TargetConsumeScaled = static_cast<int32>(FMath::Min<int64>(TotalStoredScaled, static_cast<int64>(MaxScaled)));
	if (TargetConsumeScaled <= 0)
	{
		return false;
	}

	int64 ConsumedBaseTotalScaled = 0;
	for (FCompactionAllocationCandidate& Candidate : Candidates)
	{
		const double ExactScaled = (static_cast<double>(Candidate.AvailableScaled) * static_cast<double>(TargetConsumeScaled)) / static_cast<double>(TotalStoredScaled);
		Candidate.ConsumedScaled = FMath::Clamp(FMath::FloorToInt(ExactScaled), 0, Candidate.AvailableScaled);
		Candidate.RemainderScore = ExactScaled - static_cast<double>(Candidate.ConsumedScaled);
		ConsumedBaseTotalScaled += static_cast<int64>(Candidate.ConsumedScaled);
	}

	int32 RemainingScaled = FMath::Max(0, TargetConsumeScaled - static_cast<int32>(ConsumedBaseTotalScaled));
	Candidates.Sort([](const FCompactionAllocationCandidate& Left, const FCompactionAllocationCandidate& Right)
	{
		if (!FMath::IsNearlyEqual(static_cast<float>(Left.RemainderScore), static_cast<float>(Right.RemainderScore)))
		{
			return Left.RemainderScore > Right.RemainderScore;
		}

		return Left.ResourceId.LexicalLess(Right.ResourceId);
	});

	while (RemainingScaled > 0)
	{
		bool bAllocatedAny = false;
		for (FCompactionAllocationCandidate& Candidate : Candidates)
		{
			if (Candidate.ConsumedScaled >= Candidate.AvailableScaled)
			{
				continue;
			}

			++Candidate.ConsumedScaled;
			--RemainingScaled;
			bAllocatedAny = true;

			if (RemainingScaled <= 0)
			{
				break;
			}
		}

		if (!bAllocatedAny)
		{
			break;
		}
	}

	for (const FCompactionAllocationCandidate& Candidate : Candidates)
	{
		const int32 QuantityScaled = FMath::Max(0, Candidate.ConsumedScaled);
		if (Candidate.ResourceId.IsNone() || QuantityScaled <= 0)
		{
			continue;
		}

		OutConsumptionScaled.Add(Candidate.ResourceId, QuantityScaled);
		OutConsumedTotalScaled += QuantityScaled;
	}

	return OutConsumptionScaled.Num() > 0 && OutConsumedTotalScaled >= MinScaled;
}

void UCompactorComponent::SetRuntimeStatus(const TCHAR* Message)
{
	RuntimeStatus = FText::FromString(Message ? Message : TEXT(""));
}
