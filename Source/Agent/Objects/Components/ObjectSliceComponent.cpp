// Copyright Epic Games, Inc. All Rights Reserved.

#include "Objects/Components/ObjectSliceComponent.h"
#include "Components/BoxComponent.h"
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
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/MeshQueryFunctions.h"
#include "GeometryScript/MeshSpatialFunctions.h"
#include "GeometryScript/PolygonFunctions.h"
#include "GeometryScript/SceneUtilityFunctions.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Material/MaterialComponent.h"
#include "Materials/MaterialInterface.h"
#include "Objects/Actors/ObjectFragmentActor.h"
#include "Objects/Components/ObjectFractureComponent.h"
#include "Objects/Components/ObjectHealthComponent.h"
#include "Objects/Types/ObjectBreakUtilities.h"
#include "Algo/Reverse.h"

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

FBox TransformBoundsToWorld(const FBox& LocalBounds, const FTransform& Transform)
{
	if (!LocalBounds.IsValid)
	{
		return FBox(EForceInit::ForceInit);
	}

	const FVector Min = LocalBounds.Min;
	const FVector Max = LocalBounds.Max;
	FBox WorldBounds(EForceInit::ForceInit);
	WorldBounds += Transform.TransformPosition(FVector(Min.X, Min.Y, Min.Z));
	WorldBounds += Transform.TransformPosition(FVector(Min.X, Min.Y, Max.Z));
	WorldBounds += Transform.TransformPosition(FVector(Min.X, Max.Y, Min.Z));
	WorldBounds += Transform.TransformPosition(FVector(Min.X, Max.Y, Max.Z));
	WorldBounds += Transform.TransformPosition(FVector(Max.X, Min.Y, Min.Z));
	WorldBounds += Transform.TransformPosition(FVector(Max.X, Min.Y, Max.Z));
	WorldBounds += Transform.TransformPosition(FVector(Max.X, Max.Y, Min.Z));
	WorldBounds += Transform.TransformPosition(FVector(Max.X, Max.Y, Max.Z));
	return WorldBounds;
}

double ComputeSignedPolygonArea2D(const TArray<FVector2D>& PolygonVertices)
{
	if (PolygonVertices.Num() < 3)
	{
		return 0.0;
	}

	double TwiceArea = 0.0;
	for (int32 VertexIndex = 0; VertexIndex < PolygonVertices.Num(); ++VertexIndex)
	{
		const FVector2D& CurrentVertex = PolygonVertices[VertexIndex];
		const FVector2D& NextVertex = PolygonVertices[(VertexIndex + 1) % PolygonVertices.Num()];
		TwiceArea += (static_cast<double>(CurrentVertex.X) * static_cast<double>(NextVertex.Y))
			- (static_cast<double>(NextVertex.X) * static_cast<double>(CurrentVertex.Y));
	}

	return TwiceArea * 0.5;
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

	FGeometryScriptMeshPlaneSliceOptions SliceOptions;
	SliceOptions.bFillHoles = true;
	SliceOptions.bFillSpans = true;
	SliceOptions.HoleFillMaterialID = INDEX_NONE;
	SliceOptions.GapWidth = FMath::Max(0.01f, SliceGapWidthCm / ResolveAverageScale(SourceTransform));
	if (SliceCapMaterial)
	{
		TArray<UMaterialInterface*> ResultMaterialSet;
		CollectSourceMaterialSet(ResultMaterialSet);
		SliceOptions.HoleFillMaterialID = ResultMaterialSet.Num();
	}

	const FTransform CutFrame(FRotationMatrix::MakeFromXZ(LocalBeamAxis, LocalPlaneNormal).ToQuat(), LocalPlaneOrigin);
	UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshPlaneSlice(WorkingMesh, CutFrame, SliceOptions, nullptr);
	const FVector SliceMidpointWorld = (CurrentEntryPointWorld + CurrentExitPointWorld) * 0.5f;
	return FinalizeSliceFromWorkingMesh(
		WorkingMesh,
		PlaneNormalWorld,
		SliceMidpointWorld,
		SliceInstigator,
		OutFailureReason);
}

bool UObjectSliceComponent::TryExecuteSliceFromStrokeCutter(
	const TArray<FVector>& StrokePointsWorld,
	const FVector& ExtrudeDirectionTowardRayStartWorld,
	float CutDepthCm,
	float StrokeWidthCm,
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

	if (StrokePointsWorld.Num() < 2)
	{
		OutFailureReason = TEXT("Stroke requires at least two points");
		return false;
	}

	const FVector SafeExtrudeDirectionWorld = ExtrudeDirectionTowardRayStartWorld.GetSafeNormal();
	if (SafeExtrudeDirectionWorld.IsNearlyZero())
	{
		OutFailureReason = TEXT("Stroke cutter extrusion direction is invalid");
		return false;
	}

	const FTransform SourceTransform = SourcePrimitive->GetComponentTransform();
	const float SafeAverageScale = ResolveAverageScale(SourceTransform);
	const float LocalPointMergeTolerance = FMath::Max(0.01f, 0.25f / SafeAverageScale);
	const float LocalPointMergeToleranceSq = FMath::Square(LocalPointMergeTolerance);

	TArray<FVector> LocalStrokePoints;
	LocalStrokePoints.Reserve(StrokePointsWorld.Num());
	for (const FVector& PointWorld : StrokePointsWorld)
	{
		const FVector LocalPoint = SourceTransform.InverseTransformPosition(PointWorld);
		if (LocalStrokePoints.Num() == 0 || FVector::DistSquared(LocalStrokePoints.Last(), LocalPoint) > LocalPointMergeToleranceSq)
		{
			LocalStrokePoints.Add(LocalPoint);
		}
	}

	if (LocalStrokePoints.Num() < 2)
	{
		OutFailureReason = TEXT("Stroke collapsed to a single point");
		return false;
	}

	const FVector LocalExtrudeDirection = SourceTransform.InverseTransformVector(SafeExtrudeDirectionWorld).GetSafeNormal();
	if (LocalExtrudeDirection.IsNearlyZero())
	{
		OutFailureReason = TEXT("Unable to transform stroke extrusion direction");
		return false;
	}

	FVector LocalPathAxis = FVector::ZeroVector;
	double BestProjectedSegmentLengthSq = 0.0;
	for (int32 PointIndex = 1; PointIndex < LocalStrokePoints.Num(); ++PointIndex)
	{
		FVector Segment = LocalStrokePoints[PointIndex] - LocalStrokePoints[PointIndex - 1];
		Segment -= FVector::DotProduct(Segment, LocalExtrudeDirection) * LocalExtrudeDirection;
		const double SegmentLengthSq = Segment.SizeSquared();
		if (SegmentLengthSq > BestProjectedSegmentLengthSq)
		{
			BestProjectedSegmentLengthSq = SegmentLengthSq;
			LocalPathAxis = Segment;
		}
	}

	if (LocalPathAxis.IsNearlyZero())
	{
		LocalPathAxis = FVector::CrossProduct(LocalExtrudeDirection, FVector::UpVector);
		if (LocalPathAxis.IsNearlyZero())
		{
			LocalPathAxis = FVector::CrossProduct(LocalExtrudeDirection, FVector::RightVector);
		}
	}
	LocalPathAxis = LocalPathAxis.GetSafeNormal();
	if (LocalPathAxis.IsNearlyZero())
	{
		OutFailureReason = TEXT("Stroke cutter path axis is degenerate");
		return false;
	}

	FVector LocalPathBinormal = FVector::CrossProduct(LocalExtrudeDirection, LocalPathAxis).GetSafeNormal();
	if (LocalPathBinormal.IsNearlyZero())
	{
		OutFailureReason = TEXT("Stroke cutter frame could not be constructed");
		return false;
	}
	LocalPathAxis = FVector::CrossProduct(LocalPathBinormal, LocalExtrudeDirection).GetSafeNormal();
	if (LocalPathAxis.IsNearlyZero())
	{
		OutFailureReason = TEXT("Stroke cutter frame collapsed");
		return false;
	}

	const FVector LocalPathOrigin = LocalStrokePoints[0];
	TArray<FVector2D> Path2D;
	Path2D.Reserve(LocalStrokePoints.Num());
	for (const FVector& LocalPoint : LocalStrokePoints)
	{
		const FVector Delta = LocalPoint - LocalPathOrigin;
		const FVector2D ProjectedPoint(
			FVector::DotProduct(Delta, LocalPathAxis),
			FVector::DotProduct(Delta, LocalPathBinormal));
		if (Path2D.Num() == 0 || FVector2D::Distance(Path2D.Last(), ProjectedPoint) > LocalPointMergeTolerance)
		{
			Path2D.Add(ProjectedPoint);
		}
	}

	if (Path2D.Num() < 2)
	{
		OutFailureReason = TEXT("Stroke projection is too short for a cutter");
		return false;
	}

	FGeometryScriptOpenPathOffsetOptions OffsetOptions;
	OffsetOptions.JoinType = EGeometryScriptPolyOffsetJoinType::Round;
	OffsetOptions.EndType = EGeometryScriptPathOffsetEndType::Round;

	bool bPathOffsetSuccess = false;
	const double LocalStrokeHalfWidth = FMath::Max(
		0.25,
		static_cast<double>(FMath::Max(0.1f, StrokeWidthCm) / SafeAverageScale) * 0.5);
	const FGeometryScriptGeneralPolygonList OffsetPolygonList =
		UGeometryScriptLibrary_PolygonListFunctions::CreatePolygonsFromPathOffset(
			Path2D,
			OffsetOptions,
			LocalStrokeHalfWidth,
			bPathOffsetSuccess,
			true);
	if (!bPathOffsetSuccess)
	{
		OutFailureReason = TEXT("Failed to offset stroke path into a cutter polygon");
		return false;
	}

	const int32 PolygonCount = UGeometryScriptLibrary_PolygonListFunctions::GetPolygonCount(OffsetPolygonList);
	if (PolygonCount <= 0)
	{
		OutFailureReason = TEXT("Stroke offset produced no cutter polygon");
		return false;
	}

	int32 SelectedPolygonIndex = INDEX_NONE;
	double SelectedPolygonArea = 0.0;
	for (int32 PolygonIndex = 0; PolygonIndex < PolygonCount; ++PolygonIndex)
	{
		bool bValidPolygon = false;
		const double PolygonArea = FMath::Abs(
			UGeometryScriptLibrary_PolygonListFunctions::GetPolygonArea(
				OffsetPolygonList,
				bValidPolygon,
				PolygonIndex));
		if (bValidPolygon && (SelectedPolygonIndex == INDEX_NONE || PolygonArea > SelectedPolygonArea))
		{
			SelectedPolygonIndex = PolygonIndex;
			SelectedPolygonArea = PolygonArea;
		}
	}

	if (SelectedPolygonIndex == INDEX_NONE || SelectedPolygonArea <= KINDA_SMALL_NUMBER)
	{
		OutFailureReason = TEXT("Stroke cutter polygon is invalid");
		return false;
	}

	TArray<FVector2D> CutterPolygonVertices;
	bool bValidPolygonIndices = false;
	UGeometryScriptLibrary_PolygonListFunctions::GetPolygonVertices(
		OffsetPolygonList,
		CutterPolygonVertices,
		bValidPolygonIndices,
		SelectedPolygonIndex,
		-1);
	if (!bValidPolygonIndices || CutterPolygonVertices.Num() < 3)
	{
		OutFailureReason = TEXT("Stroke cutter polygon has insufficient vertices");
		return false;
	}

	if (ComputeSignedPolygonArea2D(CutterPolygonVertices) < 0.0)
	{
		Algo::Reverse(CutterPolygonVertices);
	}

	const float LocalCutDepth = FMath::Max(1.0f, CutDepthCm / SafeAverageScale);
	UDynamicMesh* CutterMesh = NewObject<UDynamicMesh>(GetTransientPackage(), NAME_None, RF_Transient);
	if (!CutterMesh)
	{
		OutFailureReason = TEXT("Failed to allocate stroke cutter mesh");
		return false;
	}

	const FTransform CutterTransform(
		FRotationMatrix::MakeFromXZ(LocalPathAxis, LocalExtrudeDirection).ToQuat(),
		LocalPathOrigin);
	FGeometryScriptPrimitiveOptions PrimitiveOptions;
	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendSimpleExtrudePolygon(
		CutterMesh,
		PrimitiveOptions,
		CutterTransform,
		CutterPolygonVertices,
		LocalCutDepth,
		0,
		true,
		EGeometryScriptPrimitiveOriginMode::Base,
		nullptr);
	if (CutterMesh->IsEmpty())
	{
		OutFailureReason = TEXT("Failed to build the stroke cutter mesh");
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

	FGeometryScriptMeshBooleanOptions BooleanOptions;
	BooleanOptions.bFillHoles = true;
	BooleanOptions.bSimplifyOutput = true;
	BooleanOptions.OutputTransformSpace = EGeometryScriptBooleanOutputSpace::TargetTransformSpace;
	UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean(
		WorkingMesh,
		FTransform::Identity,
		CutterMesh,
		FTransform::Identity,
		EGeometryScriptBooleanOperation::Subtract,
		BooleanOptions,
		nullptr);

	if (WorkingMesh->IsEmpty())
	{
		OutFailureReason = TEXT("Stroke cutter removed the entire mesh");
		return false;
	}

	FVector SliceMidpointWorld = FVector::ZeroVector;
	for (const FVector& StrokePoint : StrokePointsWorld)
	{
		SliceMidpointWorld += StrokePoint;
	}
	SliceMidpointWorld /= static_cast<float>(StrokePointsWorld.Num());

	return FinalizeSliceFromWorkingMesh(
		WorkingMesh,
		SafeExtrudeDirectionWorld,
		SliceMidpointWorld,
		SliceInstigator,
		OutFailureReason);
}

bool UObjectSliceComponent::FinalizeSliceFromWorkingMesh(
	UDynamicMesh* WorkingMesh,
	const FVector& PlaneNormalWorld,
	const FVector& SliceMidpointWorld,
	AActor* SliceInstigator,
	FString& OutFailureReason)
{
	if (!WorkingMesh || WorkingMesh->IsEmpty())
	{
		OutFailureReason = TEXT("Slice produced an empty working mesh");
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

	TArray<UDynamicMesh*> PieceMeshes;
	UGeometryScriptLibrary_MeshDecompositionFunctions::SplitMeshByComponents(WorkingMesh, PieceMeshes, nullptr, nullptr);
	UGeometryScriptLibrary_MeshDecompositionFunctions::SortMeshesByVolume(PieceMeshes, false, EArraySortOrder::Descending);

	if (PieceMeshes.Num() < 2)
	{
		OutFailureReason = TEXT("Slice did not produce separate pieces");
		return false;
	}

	const FTransform SourceTransform = SourcePrimitive->GetComponentTransform();
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

	const double SourceVolume = CachedSourceVolumeCm3 > KINDA_SMALL_NUMBER
		? static_cast<double>(CachedSourceVolumeCm3)
		: TotalPieceVolume;
	if (SourceVolume <= KINDA_SMALL_NUMBER || TotalPieceVolume <= KINDA_SMALL_NUMBER)
	{
		OutFailureReason = TEXT("Slice pieces have invalid volume");
		return false;
	}

	TArray<bool> PieceBelowMinimumThreshold;
	PieceBelowMinimumThreshold.SetNumZeroed(PieceMeshes.Num());
	TArray<bool> PieceAnchored;
	PieceAnchored.SetNumZeroed(PieceMeshes.Num());

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
		const FBox PieceLocalBounds = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(PieceMesh);
		const FVector WorldExtent = ResolveWorldExtent(PieceLocalBounds.GetExtent(), SourceTransform);
		const float MinWorldExtent = WorldExtent.GetMin();

		const bool bBelowMinVolume = MinPieceVolumeCm3 > 0.0f && PieceVolumeAbs < MinPieceVolumeCm3;
		const bool bBelowMinExtent = MinPieceExtentCm > 0.0f && MinWorldExtent < MinPieceExtentCm;
		PieceBelowMinimumThreshold[PieceIndex] = bBelowMinVolume || bBelowMinExtent;
		PieceAnchored[PieceIndex] = IsPieceAnchored(PieceLocalBounds, SourceTransform);
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

	TArray<FComponentReference> ResolvedSupportBoxColliders = SupportBoxColliders;
	for (FComponentReference& SupportBoxReference : ResolvedSupportBoxColliders)
	{
		// Keep support boxes bound to the original owner actor when they are local references.
		if (!SupportBoxReference.OtherActor.IsValid())
		{
			SupportBoxReference.OtherActor = OwnerActor;
		}
	}

	TArray<UMaterialInterface*> ResultMaterialSet;
	CollectSourceMaterialSet(ResultMaterialSet);
	if (SliceCapMaterial)
	{
		ResultMaterialSet.Add(SliceCapMaterial);
	}

	FObjectFragmentCollisionGenerationSettings FragmentCollisionSettings;
	FragmentCollisionSettings.bUseLongObjectProfile = bUseLongObjectProfile;
	FragmentCollisionSettings.LongObjectMaxConvexHulls = LongObjectMaxConvexHulls;
	FragmentCollisionSettings.LongObjectMaxShapeCount = LongObjectMaxShapeCount;
	FragmentCollisionSettings.LongObjectMinThicknessCm = LongObjectMinCollisionThicknessCm;
	const float EffectiveLongObjectCollisionIgnoreSeconds =
		bUseLongObjectProfile
			? FMath::Max(0.0f, LongObjectSiblingCollisionIgnoreSeconds)
			: 0.0f;

	TArray<TMap<FName, int32>> SplitResourceQuantitiesScaled;
	AgentObjectBreak::SplitResourceQuantitiesExact(SourceState.SourceResourceQuantitiesScaled, NormalizedWeights, SplitResourceQuantitiesScaled);

	TArray<AActor*> SpawnedActors;
	SpawnedActors.Reserve(PieceMeshes.Num());
	TArray<TWeakObjectPtr<UPrimitiveComponent>> SpawnedPrimitives;
	SpawnedPrimitives.SetNum(PieceMeshes.Num());
	TArray<TTuple<TWeakObjectPtr<UPrimitiveComponent>, ECollisionResponse>> PendingPhysicsBodyCollisionRestore;

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

		const bool bInitializedPiece = SpawnedPiece->InitializeFromDynamicMesh(
			PieceMeshes[PieceIndex],
			ResultMaterialSet,
			FragmentCollisionSettings);
		if (!bInitializedPiece)
		{
			SpawnedPiece->Destroy();
			OutFailureReason = TEXT("Failed to initialize a slice result mesh");
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
			SpawnedHealthComponent->bHealthEnabled = false;
			SpawnedHealthComponent->bCanTakeDamage = false;
			SpawnedHealthComponent->bCanDie = false;
			SpawnedHealthComponent->bDestroyOwnerWhenDepleted = false;
			SpawnedHealthComponent->bAutoInitializeOnBeginPlay = false;
			SpawnedHealthComponent->bDeferMassInitializationToNextTick = false;
		}

		if (UObjectSliceComponent* SpawnedSliceComponent = SpawnedPiece->FindComponentByClass<UObjectSliceComponent>())
		{
			SpawnedSliceComponent->bSlicingEnabled = true;
			SpawnedSliceComponent->SliceResultActorClass = SliceResultActorClass;
			SpawnedSliceComponent->SliceCapMaterial = SliceCapMaterial;
			SpawnedSliceComponent->MaxRuntimeSlices = 0;
			SpawnedSliceComponent->bAllowNaniteStaticMeshes = bAllowNaniteStaticMeshes;
			SpawnedSliceComponent->MaxSourceLod0Triangles = MaxSourceLod0Triangles;
			SpawnedSliceComponent->MinPieceVolumeCm3 = MinPieceVolumeCm3;
			SpawnedSliceComponent->MinPieceExtentCm = MinPieceExtentCm;
			SpawnedSliceComponent->MinimumPenetrationThicknessCm = MinimumPenetrationThicknessCm;
			SpawnedSliceComponent->SliceGapWidthCm = SliceGapWidthCm;
			SpawnedSliceComponent->SliceSeparationImpulse = SliceSeparationImpulse;
			SpawnedSliceComponent->SmallPieceLifespanSeconds = SmallPieceLifespanSeconds;
			SpawnedSliceComponent->bUseLongObjectProfile = bUseLongObjectProfile;
			SpawnedSliceComponent->LongObjectMaxConvexHulls = LongObjectMaxConvexHulls;
			SpawnedSliceComponent->LongObjectMaxShapeCount = LongObjectMaxShapeCount;
			SpawnedSliceComponent->LongObjectMinCollisionThicknessCm = LongObjectMinCollisionThicknessCm;
			SpawnedSliceComponent->LongObjectSiblingCollisionIgnoreSeconds = LongObjectSiblingCollisionIgnoreSeconds;
			SpawnedSliceComponent->AnchorMode = AnchorMode;
			SpawnedSliceComponent->StaticAnchorMaxLocalZCm = StaticAnchorMaxLocalZCm;
			SpawnedSliceComponent->SupportBoxColliders = ResolvedSupportBoxColliders;
			SpawnedSliceComponent->MinSupportOverlapCm = MinSupportOverlapCm;
			SpawnedSliceComponent->bDisableSlicingOnAnchoredPieces = bDisableSlicingOnAnchoredPieces;
			SpawnedSliceComponent->InvalidateSourceMeshCache();

			const bool bPieceBelowThreshold = PieceBelowMinimumThreshold.IsValidIndex(PieceIndex) && PieceBelowMinimumThreshold[PieceIndex];
			const bool bPieceAnchored = PieceAnchored.IsValidIndex(PieceIndex) && PieceAnchored[PieceIndex];
			if (bPieceBelowThreshold || (bPieceAnchored && bDisableSlicingOnAnchoredPieces))
			{
				SpawnedSliceComponent->bSlicingEnabled = false;
			}
		}

		DisableFractureOnActor(SpawnedPiece);
		UGameplayStatics::FinishSpawningActor(SpawnedPiece, SourceTransform);

		if (PieceBelowMinimumThreshold.IsValidIndex(PieceIndex)
			&& PieceBelowMinimumThreshold[PieceIndex]
			&& SmallPieceLifespanSeconds > KINDA_SMALL_NUMBER)
		{
			SpawnedPiece->SetLifeSpan(SmallPieceLifespanSeconds);
		}

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

	const FVector SafePlaneNormalWorld = PlaneNormalWorld.GetSafeNormal();
	for (int32 PieceIndex = 0; PieceIndex < SpawnedPrimitives.Num(); ++PieceIndex)
	{
		if (UPrimitiveComponent* SpawnedPrimitive = SpawnedPrimitives[PieceIndex].Get())
		{
			const bool bPieceAnchored = PieceAnchored.IsValidIndex(PieceIndex) && PieceAnchored[PieceIndex];
			SpawnedPrimitive->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
			if (bPieceAnchored)
			{
				SpawnedPrimitive->SetSimulatePhysics(false);
				SpawnedPrimitive->SetPhysicsLinearVelocity(FVector::ZeroVector);
				SpawnedPrimitive->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
				continue;
			}

			SpawnedPrimitive->SetSimulatePhysics(true);
			SpawnedPrimitive->SetPhysicsLinearVelocity(SourceState.SourceLinearVelocity);
			SpawnedPrimitive->SetPhysicsAngularVelocityInDegrees(SourceState.SourceAngularVelocityDeg);

			if (EffectiveLongObjectCollisionIgnoreSeconds > KINDA_SMALL_NUMBER)
			{
				PendingPhysicsBodyCollisionRestore.Add(
					MakeTuple(
						TWeakObjectPtr<UPrimitiveComponent>(SpawnedPrimitive),
						SpawnedPrimitive->GetCollisionResponseToChannel(ECC_PhysicsBody)));
				SpawnedPrimitive->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
			}

			if (!SafePlaneNormalWorld.IsNearlyZero() && SliceSeparationImpulse > KINDA_SMALL_NUMBER)
			{
				const FVector PieceCenterWorld = SourceTransform.TransformPosition(PieceCentersLocal[PieceIndex]);
				const float Side = FVector::DotProduct(PieceCenterWorld - SliceMidpointWorld, SafePlaneNormalWorld) >= 0.0f ? 1.0f : -1.0f;
				SpawnedPrimitive->AddImpulse(SafePlaneNormalWorld * (SliceSeparationImpulse * Side), NAME_None, true);
			}
		}
	}

	if (PendingPhysicsBodyCollisionRestore.Num() > 0)
	{
		FTimerHandle RestoreCollisionTimerHandle;
		World->GetTimerManager().SetTimer(
			RestoreCollisionTimerHandle,
			[PendingPhysicsBodyCollisionRestore]()
			{
				for (const TTuple<TWeakObjectPtr<UPrimitiveComponent>, ECollisionResponse>& PendingEntry : PendingPhysicsBodyCollisionRestore)
				{
					if (UPrimitiveComponent* Primitive = PendingEntry.Get<0>().Get())
					{
						Primitive->SetCollisionResponseToChannel(ECC_PhysicsBody, PendingEntry.Get<1>());
					}
				}
			},
			EffectiveLongObjectCollisionIgnoreSeconds,
			false);
	}

	InvalidateSourceMeshCache();
	return true;
}

void UObjectSliceComponent::InvalidateSourceMeshCache()
{
	bSourceCacheValid = false;
	CachedSourceMesh = nullptr;
	CachedSourceVolumeCm3 = 0.0f;
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

		if (USceneComponent* RootComponent = OwnerActor->GetRootComponent())
		{
			if (Cast<UMeshComponent>(RootComponent))
			{
				return RootComponent;
			}
		}

		TArray<UMeshComponent*> MeshComponents;
		OwnerActor->GetComponents<UMeshComponent>(MeshComponents);
		for (UMeshComponent* MeshComponent : MeshComponents)
		{
			if (MeshComponent)
			{
				return MeshComponent;
			}
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

	if (const UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(SourceSceneComponent))
	{
		if (const UStaticMesh* StaticMesh = StaticMeshComponent->GetStaticMesh())
		{
			const bool bIsNaniteMesh = StaticMesh->IsNaniteEnabled();
			if (bIsNaniteMesh && !bAllowNaniteStaticMeshes)
			{
				OutFailureReason = TEXT("Nanite mesh blocked. Enable 'Nanite Sliceable (Experimental)' on ObjectSliceComponent to test.");
				return false;
			}

			const bool bShouldCheckTriangleCap = MaxSourceLod0Triangles > 0 && !(bIsNaniteMesh && bAllowNaniteStaticMeshes);
			if (bShouldCheckTriangleCap)
			{
				const int32 Lod0Triangles = StaticMesh->GetNumTriangles(0);
				if (Lod0Triangles > MaxSourceLod0Triangles)
				{
					OutFailureReason = FString::Printf(
						TEXT("Slice source LOD0 triangle count %d exceeds limit %d"),
						Lod0Triangles,
						MaxSourceLod0Triangles);
					return false;
				}
			}
		}
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

	float SurfaceArea = 0.0f;
	UGeometryScriptLibrary_MeshQueryFunctions::GetMeshVolumeArea(CachedSourceMesh, SurfaceArea, CachedSourceVolumeCm3);
	CachedSourceVolumeCm3 = FMath::Abs(CachedSourceVolumeCm3);

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

	DisableFractureOnActor(OwnerActor);

	TSet<const UPrimitiveComponent*> AnchorSupportPrimitives;
	for (const FComponentReference& SupportBoxReference : SupportBoxColliders)
	{
		if (UPrimitiveComponent* SupportPrimitive = Cast<UPrimitiveComponent>(SupportBoxReference.GetComponent(OwnerActor)))
		{
			AnchorSupportPrimitives.Add(SupportPrimitive);
		}
	}

	TArray<UPrimitiveComponent*> PrimitiveComponents;
	OwnerActor->GetComponents<UPrimitiveComponent>(PrimitiveComponents);
	for (UPrimitiveComponent* PrimitiveComponent : PrimitiveComponents)
	{
		if (!PrimitiveComponent)
		{
			continue;
		}

		if (AnchorSupportPrimitives.Contains(PrimitiveComponent))
		{
			continue;
		}

		PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PrimitiveComponent->SetSimulatePhysics(false);
	}

	OwnerActor->SetActorHiddenInGame(true);
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

bool UObjectSliceComponent::IsPieceAnchored(const FBox& PieceLocalBounds, const FTransform& SourceTransform) const
{
	switch (AnchorMode)
	{
	case EObjectSliceAnchorMode::OriginBand:
		return IsAnchoredByOriginBand(PieceLocalBounds, SourceTransform);
	case EObjectSliceAnchorMode::SupportVolumes:
		return IsAnchoredBySupportBoxes(PieceLocalBounds, SourceTransform);
	default:
		return false;
	}
}

bool UObjectSliceComponent::IsAnchoredByOriginBand(const FBox& PieceLocalBounds, const FTransform& SourceTransform) const
{
	if (!PieceLocalBounds.IsValid || StaticAnchorMaxLocalZCm <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	const float SafeScaleZ = FMath::Max(KINDA_SMALL_NUMBER, SourceTransform.GetScale3D().GetAbs().Z);
	const float LocalThresholdZ = StaticAnchorMaxLocalZCm / SafeScaleZ;
	return PieceLocalBounds.Min.Z <= LocalThresholdZ;
}

bool UObjectSliceComponent::IsAnchoredBySupportBoxes(const FBox& PieceLocalBounds, const FTransform& SourceTransform) const
{
	if (!PieceLocalBounds.IsValid || SupportBoxColliders.Num() == 0)
	{
		return false;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return false;
	}

	const FBox PieceWorldBounds = TransformBoundsToWorld(PieceLocalBounds, SourceTransform);
	if (!PieceWorldBounds.IsValid)
	{
		return false;
	}

	for (const FComponentReference& SupportBoxReference : SupportBoxColliders)
	{
		const UBoxComponent* SupportBoxComponent = Cast<UBoxComponent>(SupportBoxReference.GetComponent(OwnerActor));
		if (!IsValid(SupportBoxComponent) || !SupportBoxComponent->IsCollisionEnabled())
		{
			continue;
		}

		const FBox SupportBounds = SupportBoxComponent->Bounds.GetBox();
		if (!SupportBounds.IsValid || !PieceWorldBounds.Intersect(SupportBounds))
		{
			continue;
		}

		if (MinSupportOverlapCm <= KINDA_SMALL_NUMBER)
		{
			return true;
		}

		const FVector OverlapMin(
			FMath::Max(PieceWorldBounds.Min.X, SupportBounds.Min.X),
			FMath::Max(PieceWorldBounds.Min.Y, SupportBounds.Min.Y),
			FMath::Max(PieceWorldBounds.Min.Z, SupportBounds.Min.Z));
		const FVector OverlapMax(
			FMath::Min(PieceWorldBounds.Max.X, SupportBounds.Max.X),
			FMath::Min(PieceWorldBounds.Max.Y, SupportBounds.Max.Y),
			FMath::Min(PieceWorldBounds.Max.Z, SupportBounds.Max.Z));
		if (OverlapMax.X <= OverlapMin.X || OverlapMax.Y <= OverlapMin.Y || OverlapMax.Z <= OverlapMin.Z)
		{
			continue;
		}

		const FVector OverlapSize = OverlapMax - OverlapMin;
		if (OverlapSize.GetMin() >= MinSupportOverlapCm)
		{
			return true;
		}
	}

	return false;
}
