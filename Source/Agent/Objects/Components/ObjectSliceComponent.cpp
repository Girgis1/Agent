// Copyright Epic Games, Inc. All Rights Reserved.

#include "Objects/Components/ObjectSliceComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "UDynamicMesh.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "GeometryScript/CollisionFunctions.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshDecompositionFunctions.h"
#include "GeometryScript/MeshQueryFunctions.h"
#include "GeometryScript/MeshSpatialFunctions.h"
#include "GeometryScript/SceneUtilityFunctions.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Material/MaterialComponent.h"
#include "Materials/MaterialInterface.h"
#include "Objects/Actors/ObjectFragmentActor.h"
#include "Objects/Components/ObjectFractureComponent.h"
#include "Objects/Components/ObjectHealthComponent.h"
#include "Objects/Types/ObjectBreakUtilities.h"

namespace
{
float ResolveAverageScale(const FTransform& Transform)
{
	const FVector Scale3D = Transform.GetScale3D().GetAbs();
	return FMath::Max(KINDA_SMALL_NUMBER, (Scale3D.X + Scale3D.Y + Scale3D.Z) / 3.0f);
}

FVector ResolveWorldExtent(const FVector& LocalExtent, const FTransform& Transform)
{
	const FVector Scale3D = Transform.GetScale3D().GetAbs();
	return FVector(LocalExtent.X * Scale3D.X, LocalExtent.Y * Scale3D.Y, LocalExtent.Z * Scale3D.Z);
}
}

UObjectSliceComponent::UObjectSliceComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SliceResultActorClass = AObjectFragmentActor::StaticClass();
}

UObjectSliceComponent* UObjectSliceComponent::FindObjectSliceComponent(AActor* Actor, const UPrimitiveComponent* PreferredPrimitive)
{
	if (!Actor)
	{
		return nullptr;
	}

	TInlineComponentArray<UObjectSliceComponent*> SliceComponents(Actor);
	if (SliceComponents.Num() == 0)
	{
		return nullptr;
	}

	if (!PreferredPrimitive)
	{
		for (UObjectSliceComponent* SliceComponent : SliceComponents)
		{
			if (SliceComponent && SliceComponent->bSlicingEnabled)
			{
				return SliceComponent;
			}
		}

		return SliceComponents[0];
	}

	for (UObjectSliceComponent* SliceComponent : SliceComponents)
	{
		if (SliceComponent && SliceComponent->GetSliceSourcePrimitive() == PreferredPrimitive)
		{
			return SliceComponent;
		}
	}

	return nullptr;
}

void UObjectSliceComponent::BeginPlay()
{
	Super::BeginPlay();
	InvalidateSourceMeshCache();
}

bool UObjectSliceComponent::HasRemainingSlices() const
{
	return MaxRuntimeSlices <= 0 || CompletedSliceCount < MaxRuntimeSlices;
}

UPrimitiveComponent* UObjectSliceComponent::GetSliceSourcePrimitive() const
{
	return Cast<UPrimitiveComponent>(ResolveSliceSourceSceneComponent());
}

bool UObjectSliceComponent::CanSliceNow(FString& OutFailureReason) const
{
	OutFailureReason.Reset();

	if (!bSlicingEnabled)
	{
		OutFailureReason = TEXT("Slicing disabled");
		return false;
	}

	if (!GetOwner())
	{
		OutFailureReason = TEXT("Missing owner");
		return false;
	}

	if (!HasRemainingSlices())
	{
		OutFailureReason = TEXT("No slices remaining");
		return false;
	}

	if (!GetSliceSourcePrimitive())
	{
		OutFailureReason = TEXT("Missing slice source component");
		return false;
	}

	if (!SliceResultActorClass)
	{
		OutFailureReason = TEXT("Missing slice result actor class");
		return false;
	}

	return true;
}

bool UObjectSliceComponent::QueryBeamPenetration(
	const FVector& BeamOriginWorld,
	const FVector& BeamDirectionWorld,
	float MaxDistanceCm,
	FObjectSliceBeamIntersection& OutIntersection,
	FString& OutFailureReason)
{
	OutIntersection = FObjectSliceBeamIntersection();

	if (!BuildSliceSourceCache(OutFailureReason))
	{
		return false;
	}

	UPrimitiveComponent* SourcePrimitive = GetSliceSourcePrimitive();
	if (!SourcePrimitive)
	{
		OutFailureReason = TEXT("Missing slice source primitive");
		return false;
	}

	const FVector SafeBeamDirection = BeamDirectionWorld.GetSafeNormal();
	if (SafeBeamDirection.IsNearlyZero())
	{
		OutFailureReason = TEXT("Beam direction is invalid");
		return false;
	}

	const FVector BeamEndWorld = BeamOriginWorld + (SafeBeamDirection * FMath::Max(1.0f, MaxDistanceCm));
	const FTransform SourceTransform = SourcePrimitive->GetComponentTransform();
	const FVector LocalOrigin = SourceTransform.InverseTransformPosition(BeamOriginWorld);
	const FVector LocalEnd = SourceTransform.InverseTransformPosition(BeamEndWorld);
	const FVector LocalDirection = (LocalEnd - LocalOrigin).GetSafeNormal();
	const float LocalTraceLength = (LocalEnd - LocalOrigin).Size();
	if (LocalDirection.IsNearlyZero() || LocalTraceLength <= KINDA_SMALL_NUMBER)
	{
		OutFailureReason = TEXT("Beam does not cross the slice source");
		return false;
	}

	FGeometryScriptSpatialQueryOptions QueryOptions;
	QueryOptions.MaxDistance = LocalTraceLength;

	bool bOriginInside = false;
	EGeometryScriptContainmentOutcomePins ContainmentOutcome = EGeometryScriptContainmentOutcomePins::Outside;
	UGeometryScriptLibrary_MeshSpatial::IsPointInsideMesh(
		CachedSourceMesh,
		CachedSourceMeshBVH,
		LocalOrigin,
		QueryOptions,
		bOriginInside,
		ContainmentOutcome,
		nullptr);
	if (bOriginInside)
	{
		OutFailureReason = TEXT("BeamStart is inside the slice target");
		return false;
	}

	FGeometryScriptRayHitResult EntryHit;
	EGeometryScriptSearchOutcomePins EntryOutcome = EGeometryScriptSearchOutcomePins::NotFound;
	UGeometryScriptLibrary_MeshSpatial::FindNearestRayIntersectionWithMesh(
		CachedSourceMesh,
		CachedSourceMeshBVH,
		LocalOrigin,
		LocalDirection,
		QueryOptions,
		EntryHit,
		EntryOutcome,
		nullptr);
	if (EntryOutcome != EGeometryScriptSearchOutcomePins::Found || !EntryHit.bHit)
	{
		OutFailureReason = TEXT("Slice beam has not entered the target");
		return false;
	}

	const FVector ReverseDirection = (LocalOrigin - LocalEnd).GetSafeNormal();
	FGeometryScriptRayHitResult ExitHit;
	EGeometryScriptSearchOutcomePins ExitOutcome = EGeometryScriptSearchOutcomePins::NotFound;
	UGeometryScriptLibrary_MeshSpatial::FindNearestRayIntersectionWithMesh(
		CachedSourceMesh,
		CachedSourceMeshBVH,
		LocalEnd,
		ReverseDirection,
		QueryOptions,
		ExitHit,
		ExitOutcome,
		nullptr);
	if (ExitOutcome != EGeometryScriptSearchOutcomePins::Found || !ExitHit.bHit)
	{
		OutFailureReason = TEXT("Slice beam has not exited the target");
		return false;
	}

	FVector EntryPointWorld = SourceTransform.TransformPosition(EntryHit.HitPosition);
	FVector ExitPointWorld = SourceTransform.TransformPosition(ExitHit.HitPosition);

	float EntryDistance = FVector::DotProduct(EntryPointWorld - BeamOriginWorld, SafeBeamDirection);
	float ExitDistance = FVector::DotProduct(ExitPointWorld - BeamOriginWorld, SafeBeamDirection);
	if (ExitDistance < EntryDistance)
	{
		Swap(EntryDistance, ExitDistance);
		Swap(EntryPointWorld, ExitPointWorld);
	}

	const float ThicknessCm = FMath::Max(0.0f, ExitDistance - EntryDistance);
	if (ThicknessCm < FMath::Max(0.0f, MinimumPenetrationThicknessCm))
	{
		OutFailureReason = TEXT("Target is too thin to define a stable slice");
		return false;
	}

	OutIntersection.bHasPenetration = true;
	OutIntersection.EntryPoint = EntryPointWorld;
	OutIntersection.ExitPoint = ExitPointWorld;
	OutIntersection.ThicknessCm = ThicknessCm;
	return true;
}

bool UObjectSliceComponent::TryExecuteSliceFromBeamGesture(
	const FVector& BeamStartWorld,
	const FVector& FirstEntryPointWorld,
	const FVector& CurrentEntryPointWorld,
	const FVector& CurrentExitPointWorld,
	AActor* SliceInstigator,
	FString& OutFailureReason)
{
	if (!BuildSliceSourceCache(OutFailureReason))
	{
		return false;
	}

	UPrimitiveComponent* SourcePrimitive = GetSliceSourcePrimitive();
	UWorld* World = GetWorld();
	AActor* OwnerActor = GetOwner();
	if (!SourcePrimitive || !World || !OwnerActor)
	{
		OutFailureReason = TEXT("Slice target is missing runtime state");
		return false;
	}

	const FVector FirstGestureVector = FirstEntryPointWorld - BeamStartWorld;
	const FVector CurrentGestureVector = CurrentEntryPointWorld - BeamStartWorld;
	const FVector PlaneNormalWorld = FVector::CrossProduct(FirstGestureVector, CurrentGestureVector).GetSafeNormal();
	if (PlaneNormalWorld.IsNearlyZero())
	{
		OutFailureReason = TEXT("Slice plane is degenerate");
		return false;
	}

	const FVector BeamAxisWorld = (CurrentExitPointWorld - CurrentEntryPointWorld).GetSafeNormal();
	if (BeamAxisWorld.IsNearlyZero())
	{
		OutFailureReason = TEXT("Slice beam axis is invalid");
		return false;
	}

	const FTransform SourceTransform = SourcePrimitive->GetComponentTransform();
	const FVector LocalPlaneOrigin = SourceTransform.InverseTransformPosition((CurrentEntryPointWorld + CurrentExitPointWorld) * 0.5f);
	const FVector LocalPlaneNormal = SourceTransform.InverseTransformVector(PlaneNormalWorld).GetSafeNormal();
	const FVector LocalBeamAxis = SourceTransform.InverseTransformVector(BeamAxisWorld).GetSafeNormal();
	if (LocalPlaneNormal.IsNearlyZero() || LocalBeamAxis.IsNearlyZero())
	{
		OutFailureReason = TEXT("Slice plane could not be transformed into local space");
		return false;
	}

	if (FMath::Abs(FVector::DotProduct(LocalPlaneNormal, LocalBeamAxis)) > 0.999f)
	{
		OutFailureReason = TEXT("Slice plane collapsed onto the beam axis");
		return false;
	}

	UDynamicMesh* WorkingMesh = NewObject<UDynamicMesh>(GetTransientPackage(), NAME_None, RF_Transient);
	if (!WorkingMesh)
	{
		OutFailureReason = TEXT("Failed to allocate working slice mesh");
		return false;
	}

	CachedSourceMesh->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& SourceMesh)
	{
		WorkingMesh->SetMesh(UE::Geometry::FDynamicMesh3(SourceMesh));
	});

	TArray<UMaterialInterface*> ResultMaterialSet;
	CollectSourceMaterialSet(ResultMaterialSet);
	int32 HoleFillMaterialId = INDEX_NONE;
	if (SliceCapMaterial)
	{
		HoleFillMaterialId = ResultMaterialSet.Num();
		ResultMaterialSet.Add(SliceCapMaterial);
	}

	FGeometryScriptMeshPlaneSliceOptions SliceOptions;
	SliceOptions.bFillHoles = true;
	SliceOptions.bFillSpans = true;
	SliceOptions.HoleFillMaterialID = HoleFillMaterialId;
	SliceOptions.GapWidth = FMath::Max(0.01f, SliceGapWidthCm / ResolveAverageScale(SourceTransform));

	const FTransform CutFrame(FRotationMatrix::MakeFromXZ(LocalBeamAxis, LocalPlaneNormal).ToQuat(), LocalPlaneOrigin);
	UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshPlaneSlice(WorkingMesh, CutFrame, SliceOptions, nullptr);

	TArray<UDynamicMesh*> PieceMeshes;
	UGeometryScriptLibrary_MeshDecompositionFunctions::SplitMeshByComponents(WorkingMesh, PieceMeshes, nullptr, nullptr);
	UGeometryScriptLibrary_MeshDecompositionFunctions::SortMeshesByVolume(PieceMeshes, false, EArraySortOrder::Descending);

	if (PieceMeshes.Num() < 2)
	{
		OutFailureReason = TEXT("Slice did not produce separate pieces");
		return false;
	}

	if (MaxGeneratedPiecesPerSlice > 0 && PieceMeshes.Num() > MaxGeneratedPiecesPerSlice)
	{
		OutFailureReason = TEXT("Slice produced too many loose pieces");
		return false;
	}

	TArray<double> VolumeRatios;
	TArray<FVector> PieceCentersLocal;
	VolumeRatios.Reserve(PieceMeshes.Num());
	PieceCentersLocal.Reserve(PieceMeshes.Num());

	double TotalPieceVolume = 0.0;
	for (UDynamicMesh* PieceMesh : PieceMeshes)
	{
		float PieceArea = 0.0f;
		float PieceVolume = 0.0f;
		FVector PieceCenterLocal = FVector::ZeroVector;
		UGeometryScriptLibrary_MeshQueryFunctions::GetMeshVolumeAreaCenter(PieceMesh, PieceArea, PieceVolume, PieceCenterLocal);
		const double SafePieceVolume = FMath::Abs(static_cast<double>(PieceVolume));
		VolumeRatios.Add(SafePieceVolume);
		PieceCentersLocal.Add(PieceCenterLocal);
		TotalPieceVolume += SafePieceVolume;
	}

	const FVector SliceMidpointWorld = (CurrentEntryPointWorld + CurrentExitPointWorld) * 0.5f;
	TArray<bool> PiecePositiveSides;
	PiecePositiveSides.Reserve(PieceMeshes.Num());
	int32 PositivePieceCount = 0;
	int32 NegativePieceCount = 0;
	for (int32 PieceIndex = 0; PieceIndex < PieceCentersLocal.Num(); ++PieceIndex)
	{
		const FVector PieceCenterWorld = SourceTransform.TransformPosition(PieceCentersLocal[PieceIndex]);
		const bool bPositiveSide = FVector::DotProduct(PieceCenterWorld - SliceMidpointWorld, PlaneNormalWorld) >= 0.0f;
		PiecePositiveSides.Add(bPositiveSide);
		PositivePieceCount += bPositiveSide ? 1 : 0;
		NegativePieceCount += bPositiveSide ? 0 : 1;
	}

	const double SourceVolume = CachedSourceVolumeCm3 > KINDA_SMALL_NUMBER
		? static_cast<double>(CachedSourceVolumeCm3)
		: TotalPieceVolume;
	if (SourceVolume <= KINDA_SMALL_NUMBER || TotalPieceVolume <= KINDA_SMALL_NUMBER)
	{
		OutFailureReason = TEXT("Slice pieces have invalid volume");
		return false;
	}

	for (int32 PieceIndex = 0; PieceIndex < PieceMeshes.Num(); ++PieceIndex)
	{
		UDynamicMesh* PieceMesh = PieceMeshes[PieceIndex];
		if (!PieceMesh)
		{
			OutFailureReason = TEXT("Slice generated an invalid mesh piece");
			return false;
		}

		float PieceArea = 0.0f;
		float PieceVolume = 0.0f;
		UGeometryScriptLibrary_MeshQueryFunctions::GetMeshVolumeArea(PieceMesh, PieceArea, PieceVolume);
		const float PieceVolumeAbs = FMath::Abs(PieceVolume);
		const double PieceRatio = VolumeRatios[PieceIndex] / SourceVolume;
		const float PieceMassKg = AgentObjectBreak::ResolvePrimitiveMassKgWithoutWarning(SourcePrimitive) * static_cast<float>(PieceRatio);

		const FVector WorldExtent = ResolveWorldExtent(UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(PieceMesh).GetExtent(), SourceTransform);
		const float MinWorldExtent = WorldExtent.GetMin();
		if (MinPieceVolumeCm3 > 0.0f && PieceVolumeAbs < MinPieceVolumeCm3)
		{
			OutFailureReason = TEXT("Slice would create a piece below the minimum volume");
			return false;
		}

		if (MinPieceMassKg > 0.0f && PieceMassKg < MinPieceMassKg)
		{
			OutFailureReason = TEXT("Slice would create a piece below the minimum mass");
			return false;
		}

		if (MinPieceExtentCm > 0.0f && MinWorldExtent < MinPieceExtentCm)
		{
			OutFailureReason = TEXT("Slice would create a piece below the minimum extent");
			return false;
		}

		if (MinPieceVolumeRatio > 0.0f && PieceRatio < MinPieceVolumeRatio)
		{
			OutFailureReason = TEXT("Slice would create a sliver piece");
			return false;
		}
	}

	UStaticMeshComponent* SourceStaticMeshComponent = Cast<UStaticMeshComponent>(SourcePrimitive);
	const bool bCanUseProceduralStaticMeshSlice = SourceStaticMeshComponent
		&& SourceStaticMeshComponent->GetStaticMesh()
		&& SourceStaticMeshComponent->GetStaticMesh()->bAllowCPUAccess;
	if (bCanUseProceduralStaticMeshSlice)
	{
		if (PieceMeshes.Num() != 2)
		{
			OutFailureReason = TEXT("Static mesh slicing currently requires exactly two halves");
			return false;
		}

		if (PositivePieceCount != 1 || NegativePieceCount != 1)
		{
			OutFailureReason = TEXT("Slice could not resolve unique positive and negative halves");
			return false;
		}
	}

	FResolvedObjectBreakSourceState SourceState;
	AgentObjectBreak::BuildSourceState(
		OwnerActor,
		SourcePrimitive,
		OwnerActor->FindComponentByClass<UMaterialComponent>(),
		UObjectHealthComponent::FindObjectHealthComponent(OwnerActor),
		SourceState);

	TArray<double> NormalizedWeights;
	NormalizedWeights.Reserve(VolumeRatios.Num());
	for (const double VolumeRatioValue : VolumeRatios)
	{
		NormalizedWeights.Add(VolumeRatioValue / TotalPieceVolume);
	}

	TArray<TMap<FName, int32>> SplitResourceQuantitiesScaled;
	AgentObjectBreak::SplitResourceQuantitiesExact(SourceState.SourceResourceQuantitiesScaled, NormalizedWeights, SplitResourceQuantitiesScaled);

	TArray<AActor*> SpawnedActors;
	SpawnedActors.Reserve(PieceMeshes.Num());
	TArray<TWeakObjectPtr<UPrimitiveComponent>> SpawnedPrimitives;
	SpawnedPrimitives.SetNum(PieceMeshes.Num());

	for (int32 PieceIndex = 0; PieceIndex < PieceMeshes.Num(); ++PieceIndex)
	{
		AObjectFragmentActor* SpawnedPiece = World->SpawnActorDeferred<AObjectFragmentActor>(
			SliceResultActorClass,
			SourceTransform,
			SliceInstigator ? SliceInstigator : OwnerActor,
			nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!SpawnedPiece)
		{
			OutFailureReason = TEXT("Failed to spawn a slice result actor");
			for (AActor* SpawnedActor : SpawnedActors)
			{
				if (SpawnedActor)
				{
					SpawnedActor->Destroy();
				}
			}
			return false;
		}

		const bool bInitializedPiece = bCanUseProceduralStaticMeshSlice
			? SpawnedPiece->InitializeFromStaticMeshSlice(
				SourceStaticMeshComponent,
				SliceMidpointWorld,
				PlaneNormalWorld,
				PiecePositiveSides.IsValidIndex(PieceIndex) ? PiecePositiveSides[PieceIndex] : true,
				SliceCapMaterial)
			: SpawnedPiece->InitializeFromDynamicMesh(PieceMeshes[PieceIndex], ResultMaterialSet);
		if (!bInitializedPiece)
		{
			SpawnedPiece->Destroy();
			OutFailureReason = bCanUseProceduralStaticMeshSlice
				? TEXT("Failed to initialize a procedural slice half. Ensure the source static mesh has Allow CPU Access enabled.")
				: TEXT("Failed to initialize a slice result mesh");
			for (AActor* SpawnedActor : SpawnedActors)
			{
				if (SpawnedActor)
				{
					SpawnedActor->Destroy();
				}
			}
			return false;
		}

		if (UMaterialComponent* SpawnedMaterialComponent = SpawnedPiece->FindComponentByClass<UMaterialComponent>())
		{
			const float Weight = static_cast<float>(NormalizedWeights[PieceIndex]);
			const float SpawnedMaterialGlobalMultiplier = FMath::Max(KINDA_SMALL_NUMBER, SpawnedMaterialComponent->GetResolvedGlobalMassMultiplier());
			const float PieceBaseMassBeforeMultiplierKg = SourceState.SourceMaterialComponent
				? (SourceState.SourceBaseMassBeforeMultiplierKg * Weight)
				: ((SourceState.SourceCurrentMassKg * Weight) / SpawnedMaterialGlobalMultiplier);
			SpawnedMaterialComponent->SetExplicitBaseMassKgOverride(PieceBaseMassBeforeMultiplierKg);

			if (SplitResourceQuantitiesScaled.IsValidIndex(PieceIndex) && SplitResourceQuantitiesScaled[PieceIndex].Num() > 0)
			{
				SpawnedMaterialComponent->ConfigureResourcesById(SplitResourceQuantitiesScaled[PieceIndex]);
			}
			else
			{
				SpawnedMaterialComponent->ClearConfiguredResources();
			}
		}

		if (UObjectHealthComponent* SpawnedHealthComponent = SpawnedPiece->FindComponentByClass<UObjectHealthComponent>())
		{
			const bool bSourceHealthEnabled = SourceState.SourceHealthComponent ? SourceState.SourceHealthComponent->bHealthEnabled : false;
			SpawnedHealthComponent->bHealthEnabled = bSourceHealthEnabled;
			SpawnedHealthComponent->InitializationMode = EObjectHealthInitializationMode::FromCurrentMass;
			SpawnedHealthComponent->bAutoInitializeOnBeginPlay = true;
			SpawnedHealthComponent->bDeferMassInitializationToNextTick = true;
			if (bSourceHealthEnabled)
			{
				SpawnedHealthComponent->SetInheritedDamagedPenaltyPercent(SourceState.InheritedDamagedPenaltyPercent);
			}
		}

		DisableFractureOnActor(SpawnedPiece);

		UGameplayStatics::FinishSpawningActor(SpawnedPiece, SourceTransform);
		SpawnedActors.Add(SpawnedPiece);

		if (UPrimitiveComponent* SpawnedPrimitive = AgentObjectBreak::ResolveBestPrimitiveOnActor(SpawnedPiece))
		{
			SpawnedPrimitive->SetSimulatePhysics(false);
			SpawnedPrimitive->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			SpawnedPrimitives[PieceIndex] = SpawnedPrimitive;
		}
	}

	++CompletedSliceCount;
	ApplySourceActorPostSlice();

	for (int32 PieceIndex = 0; PieceIndex < SpawnedPrimitives.Num(); ++PieceIndex)
	{
		if (UPrimitiveComponent* SpawnedPrimitive = SpawnedPrimitives[PieceIndex].Get())
		{
			SpawnedPrimitive->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			SpawnedPrimitive->SetSimulatePhysics(true);
			SpawnedPrimitive->SetPhysicsLinearVelocity(SourceState.SourceLinearVelocity);
			SpawnedPrimitive->SetPhysicsAngularVelocityInDegrees(SourceState.SourceAngularVelocityDeg);

			if (SliceSeparationImpulse > KINDA_SMALL_NUMBER)
			{
				const FVector PieceCenterWorld = SourceTransform.TransformPosition(PieceCentersLocal[PieceIndex]);
				const float Side = FVector::DotProduct(PieceCenterWorld - SliceMidpointWorld, PlaneNormalWorld) >= 0.0f ? 1.0f : -1.0f;
				SpawnedPrimitive->AddImpulse(PlaneNormalWorld * (SliceSeparationImpulse * Side), NAME_None, true);
			}
		}
	}

	InvalidateSourceMeshCache();
	return true;
}

void UObjectSliceComponent::InvalidateSourceMeshCache()
{
	bSourceCacheValid = false;
	CachedSourceMesh = nullptr;
	CachedSourceLocalBounds = FBox(EForceInit::ForceInit);
	CachedSourceVolumeCm3 = 0.0f;
	bCachedSourceClosedMesh = false;
	CachedSourceConnectedComponents = 0;
	CachedSourceTriangleCount = 0;
	UGeometryScriptLibrary_MeshSpatial::ResetBVH(CachedSourceMeshBVH);
}

USceneComponent* UObjectSliceComponent::ResolveSliceSourceSceneComponent() const
{
	if (AActor* OwnerActor = GetOwner())
	{
		if (USceneComponent* ExplicitComponent = Cast<USceneComponent>(SliceSourceComponent.GetComponent(OwnerActor)))
		{
			return ExplicitComponent;
		}

		return OwnerActor->GetRootComponent();
	}

	return nullptr;
}

UMeshComponent* UObjectSliceComponent::ResolveSliceSourceMeshComponent() const
{
	return Cast<UMeshComponent>(ResolveSliceSourceSceneComponent());
}

bool UObjectSliceComponent::BuildSliceSourceCache(FString& OutFailureReason)
{
	if (bSourceCacheValid && CachedSourceMesh)
	{
		return true;
	}

	if (!CanSliceNow(OutFailureReason))
	{
		return false;
	}

	USceneComponent* SourceSceneComponent = ResolveSliceSourceSceneComponent();
	if (!SourceSceneComponent)
	{
		OutFailureReason = TEXT("Missing slice source scene component");
		return false;
	}

	CachedSourceMesh = NewObject<UDynamicMesh>(this, NAME_None, RF_Transient);
	if (!CachedSourceMesh)
	{
		OutFailureReason = TEXT("Failed to allocate the cached slice mesh");
		return false;
	}

	FGeometryScriptCopyMeshFromComponentOptions CopyOptions;
	CopyOptions.bWantNormals = true;
	CopyOptions.bWantTangents = true;

	FTransform LocalToWorld = FTransform::Identity;
	EGeometryScriptOutcomePins Outcome = EGeometryScriptOutcomePins::Failure;
	UGeometryScriptLibrary_SceneUtilityFunctions::CopyMeshFromComponent(
		SourceSceneComponent,
		CachedSourceMesh,
		CopyOptions,
		false,
		LocalToWorld,
		Outcome,
		nullptr);
	if (Outcome != EGeometryScriptOutcomePins::Success || CachedSourceMesh->IsEmpty())
	{
		OutFailureReason = TEXT("Failed to read the slice source mesh");
		InvalidateSourceMeshCache();
		return false;
	}

	CachedSourceTriangleCount = CachedSourceMesh->GetTriangleCount();
	if (MaxSourceTriangleCount > 0 && CachedSourceTriangleCount > MaxSourceTriangleCount)
	{
		OutFailureReason = TEXT("Slice source mesh is too dense for runtime slicing");
		InvalidateSourceMeshCache();
		return false;
	}

	CachedSourceLocalBounds = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(CachedSourceMesh);
	float SurfaceArea = 0.0f;
	UGeometryScriptLibrary_MeshQueryFunctions::GetMeshVolumeArea(CachedSourceMesh, SurfaceArea, CachedSourceVolumeCm3);
	CachedSourceVolumeCm3 = FMath::Abs(CachedSourceVolumeCm3);
	bCachedSourceClosedMesh = UGeometryScriptLibrary_MeshQueryFunctions::GetIsClosedMesh(CachedSourceMesh);
	CachedSourceConnectedComponents = UGeometryScriptLibrary_MeshQueryFunctions::GetNumConnectedComponents(CachedSourceMesh);

	if (!ValidateCachedSourceMesh(OutFailureReason))
	{
		InvalidateSourceMeshCache();
		return false;
	}

	UGeometryScriptLibrary_MeshSpatial::BuildBVHForMesh(CachedSourceMesh, CachedSourceMeshBVH, nullptr);
	bSourceCacheValid = true;
	return true;
}

bool UObjectSliceComponent::ValidateCachedSourceMesh(FString& OutFailureReason) const
{
	if (!CachedSourceMesh || CachedSourceMesh->IsEmpty())
	{
		OutFailureReason = TEXT("Slice source mesh is empty");
		return false;
	}

	if (bRequireClosedMesh && !bCachedSourceClosedMesh)
	{
		OutFailureReason = TEXT("Slice source mesh must be closed");
		return false;
	}

	if (bRequireSingleConnectedSourceIsland && CachedSourceConnectedComponents > 1)
	{
		OutFailureReason = TEXT("Slice source mesh must be one connected island");
		return false;
	}

	if (MinSourceVolumeCm3 > 0.0f && CachedSourceVolumeCm3 < MinSourceVolumeCm3)
	{
		OutFailureReason = TEXT("Slice source mesh is below the minimum volume");
		return false;
	}

	if (MinSourceMassKg > 0.0f)
	{
		const float SourceMassKg = AgentObjectBreak::ResolvePrimitiveMassKgWithoutWarning(GetSliceSourcePrimitive());
		if (SourceMassKg < MinSourceMassKg)
		{
			OutFailureReason = TEXT("Slice source mesh is below the minimum mass");
			return false;
		}
	}

	return true;
}

bool UObjectSliceComponent::CollectSourceMaterialSet(TArray<UMaterialInterface*>& OutMaterialSet) const
{
	OutMaterialSet.Reset();

	if (const UMeshComponent* MeshComponent = ResolveSliceSourceMeshComponent())
	{
		OutMaterialSet = MeshComponent->GetMaterials();
		for (int32 Index = OutMaterialSet.Num() - 1; Index >= 0; --Index)
		{
			if (OutMaterialSet[Index] == nullptr)
			{
				OutMaterialSet.RemoveAt(Index);
			}
		}
	}

	return OutMaterialSet.Num() > 0;
}

void UObjectSliceComponent::ApplySourceActorPostSlice() const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	if (bDisableSourceFractureAfterSlice)
	{
		DisableFractureOnActor(OwnerActor);
	}

	if (bDisableSourceCollisionOnSlice)
	{
		TArray<UPrimitiveComponent*> PrimitiveComponents;
		OwnerActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
		for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
		{
			if (!PrimitiveComponent)
			{
				continue;
			}

			PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			PrimitiveComponent->SetSimulatePhysics(false);
		}
	}

	if (bHideSourceActorOnSlice)
	{
		OwnerActor->SetActorHiddenInGame(true);
	}

	if (bDestroySourceActorAfterSlice)
	{
		OwnerActor->Destroy();
	}
}

void UObjectSliceComponent::DisableFractureOnActor(AActor* Actor) const
{
	if (!Actor)
	{
		return;
	}

	if (UObjectFractureComponent* FractureComponent = Actor->FindComponentByClass<UObjectFractureComponent>())
	{
		FractureComponent->bFractureEnabled = false;
		FractureComponent->bAutoFractureOnDepleted = false;
	}
}
