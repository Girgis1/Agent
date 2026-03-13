// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/MaterialNodeActor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Material/MaterialComponent.h"
#include "Material/MaterialDefinitionAsset.h"
#include "Material/AgentResourceTypes.h"

float FMaterialNodeEntry::ResolveChance() const
{
	return FMath::Max(0.0f, Chance);
}

AMaterialNodeActor::AMaterialNodeActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void AMaterialNodeActor::BeginPlay()
{
	Super::BeginPlay();
	InitializeNodeState();
}

void AMaterialNodeActor::InitializeNodeState()
{
	ResolvedEntries.Reset();
	InitialTotalQuantityScaled = 0;
	RemainingTotalQuantityScaled = 0;
	QuantityScaleMultiplier = 1.0f;
	InitialActorScale = GetActorScale3D();

	const int32 RolledCapacityScaled = ResolveRolledCapacityScaled();
	const int32 AllocationChunkScaled = FMath::Max(1, AgentResource::WholeUnitsToScaled(1));

	for (const FMaterialNodeEntry& Entry : MaterialEntries)
	{
		FResolvedNodeEntry ResolvedEntry;
		ResolvedEntry.ResourceId = ResolveResourceIdFromEntry(Entry);
		ResolvedEntry.SelectionChance = Entry.ResolveChance();

		if (ResolvedEntry.ResourceId.IsNone() || ResolvedEntry.SelectionChance <= 0.0f)
		{
			continue;
		}

		ResolvedEntry.InitialQuantityScaled = 0;
		ResolvedEntry.RemainingQuantityScaled = 0;
		ResolvedEntries.Add(ResolvedEntry);
	}

	if (ResolvedEntries.Num() > 0)
	{
		int32 DistributedCapacityScaled = FMath::Max(0, RolledCapacityScaled);
		if (bInfiniteNode && DistributedCapacityScaled <= 0)
		{
			DistributedCapacityScaled = AgentResource::WholeUnitsToScaled(FMath::Max(1, ResolvedEntries.Num()));
		}

		auto ChooseEntryByChance = [&]() -> int32
		{
			float TotalChance = 0.0f;
			for (const FResolvedNodeEntry& Entry : ResolvedEntries)
			{
				TotalChance += FMath::Max(0.0f, Entry.SelectionChance);
			}

			if (TotalChance <= KINDA_SMALL_NUMBER)
			{
				return INDEX_NONE;
			}

			const float Pick = FMath::FRandRange(0.0f, TotalChance);
			float RunningChance = 0.0f;

			for (int32 EntryIndex = 0; EntryIndex < ResolvedEntries.Num(); ++EntryIndex)
			{
				const FResolvedNodeEntry& Entry = ResolvedEntries[EntryIndex];
				const float EntryChance = FMath::Max(0.0f, Entry.SelectionChance);
				if (EntryChance <= 0.0f)
				{
					continue;
				}

				RunningChance += EntryChance;
				if (Pick <= RunningChance)
				{
					return EntryIndex;
				}
			}

			return INDEX_NONE;
		};

		int32 RemainingToAllocateScaled = DistributedCapacityScaled;
		while (RemainingToAllocateScaled >= AllocationChunkScaled)
		{
			const int32 EntryIndex = ChooseEntryByChance();
			if (!ResolvedEntries.IsValidIndex(EntryIndex))
			{
				break;
			}

			ResolvedEntries[EntryIndex].InitialQuantityScaled += AllocationChunkScaled;
			ResolvedEntries[EntryIndex].RemainingQuantityScaled += AllocationChunkScaled;
			RemainingToAllocateScaled -= AllocationChunkScaled;
		}

		if (RemainingToAllocateScaled > 0)
		{
			const int32 EntryIndex = ChooseEntryByChance();
			if (ResolvedEntries.IsValidIndex(EntryIndex))
			{
				ResolvedEntries[EntryIndex].InitialQuantityScaled += RemainingToAllocateScaled;
				ResolvedEntries[EntryIndex].RemainingQuantityScaled += RemainingToAllocateScaled;
				RemainingToAllocateScaled = 0;
			}
		}

		for (const FResolvedNodeEntry& Entry : ResolvedEntries)
		{
			InitialTotalQuantityScaled += FMath::Max(0, Entry.InitialQuantityScaled);
			RemainingTotalQuantityScaled += FMath::Max(0, Entry.RemainingQuantityScaled);
		}
	}

	RemainingTotalQuantityScaled = bInfiniteNode ? InitialTotalQuantityScaled : RemainingTotalQuantityScaled;
	QuantityScaleMultiplier = ResolveQuantityScaleMultiplier();
	bNodeInitialized = true;
	RefreshDepletionVisuals();
}

bool AMaterialNodeActor::IsDepleted() const
{
	return !bInfiniteNode && bNodeInitialized && RemainingTotalQuantityScaled <= 0;
}

float AMaterialNodeActor::GetRemainingRatio01() const
{
	if (bInfiniteNode)
	{
		return 1.0f;
	}

	if (!bNodeInitialized || InitialTotalQuantityScaled <= 0)
	{
		return 0.0f;
	}

	return FMath::Clamp(
		static_cast<float>(RemainingTotalQuantityScaled) / static_cast<float>(InitialTotalQuantityScaled),
		0.0f,
		1.0f);
}

float AMaterialNodeActor::GetRemainingUnits() const
{
	return AgentResource::ScaledToUnits(RemainingTotalQuantityScaled);
}

bool AMaterialNodeActor::ExtractResourcesScaled(int32 MaxRequestScaled, TMap<FName, int32>& OutExtractedResourcesScaled)
{
	OutExtractedResourcesScaled.Reset();

	if (!bNodeInitialized)
	{
		InitializeNodeState();
	}

	const int32 SafeRequestScaled = FMath::Max(0, MaxRequestScaled);
	if (SafeRequestScaled <= 0 || ResolvedEntries.Num() == 0)
	{
		return false;
	}

	const int32 ChunkSizeScaled = FMath::Max(1, AgentResource::WholeUnitsToScaled(1));

	auto ChooseWeightedEntryIndex = [&](bool bUseRemainingQuantitiesAsWeight) -> int32
	{
		float TotalWeight = 0.0f;
		for (const FResolvedNodeEntry& Entry : ResolvedEntries)
		{
			if (Entry.ResourceId.IsNone())
			{
				continue;
			}

			const float CandidateWeight = bUseRemainingQuantitiesAsWeight
				? static_cast<float>(FMath::Max(0, Entry.RemainingQuantityScaled))
				: FMath::Max(0.0f, Entry.SelectionChance);
			if (CandidateWeight <= 0.0f)
			{
				continue;
			}

			TotalWeight += CandidateWeight;
		}

		if (TotalWeight <= KINDA_SMALL_NUMBER)
		{
			return INDEX_NONE;
		}

		const float Pick = FMath::FRandRange(0.0f, TotalWeight);
		float RunningWeight = 0.0f;

		for (int32 EntryIndex = 0; EntryIndex < ResolvedEntries.Num(); ++EntryIndex)
		{
			const FResolvedNodeEntry& Entry = ResolvedEntries[EntryIndex];
			if (Entry.ResourceId.IsNone())
			{
				continue;
			}

			const float CandidateWeight = bUseRemainingQuantitiesAsWeight
				? static_cast<float>(FMath::Max(0, Entry.RemainingQuantityScaled))
				: FMath::Max(0.0f, Entry.SelectionChance);
			if (CandidateWeight <= 0.0f)
			{
				continue;
			}

			RunningWeight += CandidateWeight;
			if (Pick <= RunningWeight)
			{
				return EntryIndex;
			}
		}

		return INDEX_NONE;
	};

	if (bInfiniteNode)
	{
		int32 RemainingRequestScaled = SafeRequestScaled;
		while (RemainingRequestScaled > 0)
		{
			const int32 EntryIndex = ChooseWeightedEntryIndex(false);
			if (!ResolvedEntries.IsValidIndex(EntryIndex))
			{
				break;
			}

			const FResolvedNodeEntry& Entry = ResolvedEntries[EntryIndex];
			const int32 GrantedScaled = FMath::Clamp(RemainingRequestScaled, 1, ChunkSizeScaled);
			OutExtractedResourcesScaled.FindOrAdd(Entry.ResourceId) += GrantedScaled;
			RemainingRequestScaled -= GrantedScaled;
		}

		return OutExtractedResourcesScaled.Num() > 0;
	}

	int32 RemainingRequestScaled = SafeRequestScaled;
	while (RemainingRequestScaled > 0)
	{
		const int32 EntryIndex = ChooseWeightedEntryIndex(true);
		if (!ResolvedEntries.IsValidIndex(EntryIndex))
		{
			break;
		}

		FResolvedNodeEntry& Entry = ResolvedEntries[EntryIndex];
		const int32 AvailableScaled = FMath::Max(0, Entry.RemainingQuantityScaled);
		if (AvailableScaled <= 0)
		{
			continue;
		}

		const int32 RequestedChunkScaled = FMath::Clamp(RemainingRequestScaled, 1, ChunkSizeScaled);
		const int32 GrantedScaled = FMath::Min(AvailableScaled, RequestedChunkScaled);
		Entry.RemainingQuantityScaled = FMath::Max(0, AvailableScaled - GrantedScaled);
		RemainingTotalQuantityScaled = FMath::Max(0, RemainingTotalQuantityScaled - GrantedScaled);
		RemainingRequestScaled -= GrantedScaled;
		OutExtractedResourcesScaled.FindOrAdd(Entry.ResourceId) += GrantedScaled;
	}

	if (OutExtractedResourcesScaled.Num() == 0)
	{
		return false;
	}

	RefreshDepletionVisuals();
	return true;
}

void AMaterialNodeActor::AddResourcesBack(const TMap<FName, int32>& ReturnedResourcesScaled)
{
	if (bInfiniteNode || ReturnedResourcesScaled.Num() == 0 || ResolvedEntries.Num() == 0)
	{
		return;
	}

	bool bDidRestoreAny = false;
	for (const TPair<FName, int32>& ReturnedPair : ReturnedResourcesScaled)
	{
		const FName ResourceId = ReturnedPair.Key;
		int32 RemainingToRestoreScaled = FMath::Max(0, ReturnedPair.Value);
		if (ResourceId.IsNone() || RemainingToRestoreScaled <= 0)
		{
			continue;
		}

		for (FResolvedNodeEntry& Entry : ResolvedEntries)
		{
			if (RemainingToRestoreScaled <= 0)
			{
				break;
			}

			if (Entry.ResourceId != ResourceId)
			{
				continue;
			}

			const int32 EntryCapacityScaled = FMath::Max(0, Entry.InitialQuantityScaled - Entry.RemainingQuantityScaled);
			if (EntryCapacityScaled <= 0)
			{
				continue;
			}

			const int32 RestoredScaled = FMath::Min(EntryCapacityScaled, RemainingToRestoreScaled);
			Entry.RemainingQuantityScaled += RestoredScaled;
			RemainingToRestoreScaled -= RestoredScaled;
			RemainingTotalQuantityScaled += RestoredScaled;
			bDidRestoreAny = true;
		}
	}

	RemainingTotalQuantityScaled = FMath::Clamp(RemainingTotalQuantityScaled, 0, FMath::Max(0, InitialTotalQuantityScaled));

	if (bDidRestoreAny)
	{
		RefreshDepletionVisuals();
	}
}

void AMaterialNodeActor::GetResourcePreview(TArray<FMaterialNodeResourcePreview>& OutPreview) const
{
	OutPreview.Reset();

	TMap<FName, FMaterialNodeResourcePreview> PreviewByResourceId;
	for (const FResolvedNodeEntry& Entry : ResolvedEntries)
	{
		if (Entry.ResourceId.IsNone())
		{
			continue;
		}

		FMaterialNodeResourcePreview& Preview = PreviewByResourceId.FindOrAdd(Entry.ResourceId);
		Preview.ResourceId = Entry.ResourceId;
		Preview.InitialQuantityScaled += FMath::Max(0, Entry.InitialQuantityScaled);
		Preview.RemainingQuantityScaled += FMath::Max(0, Entry.RemainingQuantityScaled);
	}

	TArray<FName> ResourceIds;
	PreviewByResourceId.GetKeys(ResourceIds);
	ResourceIds.Sort([](const FName& Left, const FName& Right)
	{
		return Left.LexicalLess(Right);
	});

	for (const FName& ResourceId : ResourceIds)
	{
		if (const FMaterialNodeResourcePreview* FoundPreview = PreviewByResourceId.Find(ResourceId))
		{
			OutPreview.Add(*FoundPreview);
		}
	}
}

void AMaterialNodeActor::RefreshDepletionVisuals()
{
	if (!bNodeInitialized)
	{
		return;
	}

	const float RemainingRatio01 = GetRemainingRatio01();
	const float QuantizedRemainingRatio01 = ResolveQuantizedRemainingRatio(RemainingRatio01);

	float DepletionScaleMultiplier = 1.0f;
	if (DepletionVisualMode == EMaterialNodeDepletionVisualMode::Scale)
	{
		const float SafeMinScale = FMath::Clamp(MinDepletionScale, 0.01f, 1.0f);
		DepletionScaleMultiplier = FMath::Lerp(SafeMinScale, 1.0f, QuantizedRemainingRatio01);
	}

	if (DepletionVisualMode == EMaterialNodeDepletionVisualMode::StagedMeshes)
	{
		UpdateStageMeshFromRatio(QuantizedRemainingRatio01);
	}

	const float FinalScaleMultiplier = FMath::Max(0.01f, QuantityScaleMultiplier * DepletionScaleMultiplier);
	SetNodeScaleFromMultiplier(FinalScaleMultiplier);
	OnNodeVisualsUpdated(RemainingRatio01, QuantizedRemainingRatio01, FinalScaleMultiplier);
}

FName AMaterialNodeActor::ResolveResourceIdFromEntry(const FMaterialNodeEntry& Entry) const
{
	if (Entry.BlueprintClass.IsNull())
	{
		return NAME_None;
	}

	UClass* EntryClass = Entry.BlueprintClass.LoadSynchronous();
	return ResolveResourceIdFromClass(EntryClass);
}

FName AMaterialNodeActor::ResolveResourceIdFromClass(UClass* EntryClass) const
{
	if (!EntryClass)
	{
		return NAME_None;
	}

	AActor* ClassDefaults = EntryClass->GetDefaultObject<AActor>();
	if (!ClassDefaults)
	{
		return NAME_None;
	}

	const UMaterialComponent* MaterialData = ClassDefaults->FindComponentByClass<UMaterialComponent>();
	if (!MaterialData)
	{
		return NAME_None;
	}

	for (const FMaterialEntry& MaterialEntry : MaterialData->Materials)
	{
		if (!MaterialEntry.Material)
		{
			continue;
		}

		const FName ResourceId = MaterialEntry.Material->GetResolvedMaterialId();
		if (!ResourceId.IsNone())
		{
			return ResourceId;
		}
	}

	return NAME_None;
}

float AMaterialNodeActor::ResolveQuantityScaleMultiplier() const
{
	if (!bScaleWithTotalMaterialQty)
	{
		return 1.0f;
	}

	const float SafeRefMinUnits = FMath::Max(0.0f, QuantityScaleReferenceMinUnits);
	const float SafeRefMaxUnits = FMath::Max(SafeRefMinUnits + KINDA_SMALL_NUMBER, QuantityScaleReferenceMaxUnits);
	const float SafeScaleMin = FMath::Max(0.05f, QuantityScaleAtMinUnits);
	const float SafeScaleMax = FMath::Max(SafeScaleMin, QuantityScaleAtMaxUnits);
	const float TotalUnits = AgentResource::ScaledToUnits(InitialTotalQuantityScaled);
	const float Alpha = FMath::Clamp((TotalUnits - SafeRefMinUnits) / (SafeRefMaxUnits - SafeRefMinUnits), 0.0f, 1.0f);

	return FMath::Lerp(SafeScaleMin, SafeScaleMax, Alpha);
}

int32 AMaterialNodeActor::ResolveRolledCapacityScaled() const
{
	const float SafeMinUnits = FMath::Max(0.0f, CapacityMinUnits);
	const float SafeMaxUnits = FMath::Max(SafeMinUnits, CapacityMaxUnits);
	const float RolledCapacityUnits = FMath::IsNearlyEqual(SafeMinUnits, SafeMaxUnits, KINDA_SMALL_NUMBER)
		? SafeMinUnits
		: FMath::FRandRange(SafeMinUnits, SafeMaxUnits);
	return FMath::Max(0, AgentResource::UnitsToScaled(RolledCapacityUnits));
}

float AMaterialNodeActor::ResolveQuantizedRemainingRatio(float RemainingRatio01) const
{
	if (!(DepletionVisualMode == EMaterialNodeDepletionVisualMode::Scale && bQuantizeDepletionScale))
	{
		return FMath::Clamp(RemainingRatio01, 0.0f, 1.0f);
	}

	const float SafeStep = FMath::Clamp(DepletionScaleStepPercent, 0.1f, 100.0f) / 100.0f;
	const float SafeRatio = FMath::Clamp(RemainingRatio01, 0.0f, 1.0f);
	const float QuantizedRatio = FMath::FloorToFloat(SafeRatio / SafeStep) * SafeStep;
	return FMath::Clamp(QuantizedRatio, 0.0f, 1.0f);
}

void AMaterialNodeActor::UpdateStageMeshFromRatio(float QuantizedRemainingRatio01)
{
	if (!StageMeshComponent || DepletionStageMeshes.Num() == 0)
	{
		return;
	}

	const int32 MaxIndex = DepletionStageMeshes.Num() - 1;
	const float DepletionAlpha = 1.0f - FMath::Clamp(QuantizedRemainingRatio01, 0.0f, 1.0f);
	const int32 StageIndex = FMath::Clamp(FMath::RoundToInt(DepletionAlpha * static_cast<float>(MaxIndex)), 0, MaxIndex);
	StageMeshComponent->SetStaticMesh(DepletionStageMeshes[StageIndex]);
}

void AMaterialNodeActor::SetNodeScaleFromMultiplier(float FinalScaleMultiplier)
{
	const float SafeScaleMultiplier = FMath::Max(0.01f, FinalScaleMultiplier);
	SetActorScale3D(InitialActorScale * SafeScaleMultiplier);
}
