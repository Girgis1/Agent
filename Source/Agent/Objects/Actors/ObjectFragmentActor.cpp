// Copyright Epic Games, Inc. All Rights Reserved.

#include "Objects/Actors/ObjectFragmentActor.h"
#include "Components/BoxComponent.h"
#include "Components/DynamicMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GeometryScript/CollisionFunctions.h"
#include "KismetProceduralMeshLibrary.h"
#include "Objects/Components/ObjectSliceComponent.h"
#include "UDynamicMesh.h"
#include "Engine/StaticMesh.h"

AObjectFragmentActor::AObjectFragmentActor()
{
	PhysicsProxy = CreateDefaultSubobject<UBoxComponent>(TEXT("PhysicsProxy"));
	SetRootComponent(PhysicsProxy);

	PhysicsProxy->SetSimulatePhysics(true);
	PhysicsProxy->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PhysicsProxy->SetCollisionObjectType(ECC_PhysicsBody);
	PhysicsProxy->SetCollisionResponseToAllChannels(ECR_Block);
	PhysicsProxy->SetGenerateOverlapEvents(true);
	PhysicsProxy->SetCanEverAffectNavigation(false);
	PhysicsProxy->SetLinearDamping(0.3f);
	PhysicsProxy->SetAngularDamping(1.8f);
	PhysicsProxy->SetBoxExtent(FVector(10.0f));

	DynamicMeshPiece = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("DynamicMeshPiece"));
	DynamicMeshPiece->SetupAttachment(PhysicsProxy);
	DynamicMeshPiece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DynamicMeshPiece->SetCollisionObjectType(ECC_PhysicsBody);
	DynamicMeshPiece->SetCollisionResponseToAllChannels(ECR_Block);
	DynamicMeshPiece->SetGenerateOverlapEvents(true);
	DynamicMeshPiece->SetCanEverAffectNavigation(false);
	DynamicMeshPiece->SetSimulatePhysics(false);
	DynamicMeshPiece->SetLinearDamping(0.3f);
	DynamicMeshPiece->SetAngularDamping(1.8f);
	DynamicMeshPiece->CollisionType = CTF_UseSimpleAsComplex;
	DynamicMeshPiece->bEnableComplexCollision = false;
	DynamicMeshPiece->SetHiddenInGame(true);

	ProceduralMeshPiece = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("ProceduralMeshPiece"));
	ProceduralMeshPiece->SetupAttachment(PhysicsProxy);
	ProceduralMeshPiece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProceduralMeshPiece->SetCollisionObjectType(ECC_PhysicsBody);
	ProceduralMeshPiece->SetCollisionResponseToAllChannels(ECR_Block);
	ProceduralMeshPiece->SetGenerateOverlapEvents(true);
	ProceduralMeshPiece->SetCanEverAffectNavigation(false);
	ProceduralMeshPiece->SetSimulatePhysics(false);
	ProceduralMeshPiece->SetLinearDamping(0.3f);
	ProceduralMeshPiece->SetAngularDamping(1.8f);
	ProceduralMeshPiece->bUseComplexAsSimpleCollision = false;
	ProceduralMeshPiece->bUseAsyncCooking = true;
	ProceduralMeshPiece->SetHiddenInGame(true);

	ObjectSlice = CreateDefaultSubobject<UObjectSliceComponent>(TEXT("ObjectSlice"));
	ObjectSlice->bSlicingEnabled = true;
	ObjectSlice->MaxRuntimeSlices = 0;

	if (ItemMesh)
	{
		ItemMesh->SetupAttachment(PhysicsProxy);
		ItemMesh->SetSimulatePhysics(false);
		ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ItemMesh->SetGenerateOverlapEvents(false);
	}
}

void AObjectFragmentActor::RefreshPhysicsProxyFromItemMesh()
{
	if (bUsingDynamicMeshPiece || !PhysicsProxy || !ItemMesh)
	{
		return;
	}

	const UStaticMesh* StaticMesh = ItemMesh->GetStaticMesh();
	if (!StaticMesh)
	{
		ItemMesh->SetRelativeLocation(FVector::ZeroVector);
		PhysicsProxy->SetBoxExtent(FVector(10.0f), true);
		return;
	}

	const FBoxSphereBounds MeshBounds = StaticMesh->GetBounds();
	const FVector RelativeScale = ItemMesh->GetRelativeScale3D().GetAbs();
	const FVector MeshCenter = MeshBounds.Origin * RelativeScale;
	const FVector MeshExtents = MeshBounds.BoxExtent * RelativeScale;
	const float SafeExtentScale = FMath::Clamp(PhysicsProxyExtentScale, 0.5f, 1.0f);
	const FVector SafeExtents(
		FMath::Max(1.0f, MeshExtents.X * SafeExtentScale),
		FMath::Max(1.0f, MeshExtents.Y * SafeExtentScale),
		FMath::Max(1.0f, MeshExtents.Z * SafeExtentScale));

	ItemMesh->SetRelativeLocation(-MeshCenter);
	PhysicsProxy->SetBoxExtent(SafeExtents, true);
}

bool AObjectFragmentActor::InitializeFromDynamicMesh(
	const UDynamicMesh* SourceMesh,
	const TArray<UMaterialInterface*>& MaterialSet)
{
	FObjectFragmentCollisionGenerationSettings DefaultCollisionSettings;
	return InitializeFromDynamicMesh(SourceMesh, MaterialSet, DefaultCollisionSettings);
}

bool AObjectFragmentActor::InitializeFromDynamicMesh(
	const UDynamicMesh* SourceMesh,
	const TArray<UMaterialInterface*>& MaterialSet,
	const FObjectFragmentCollisionGenerationSettings& CollisionSettings)
{
	if (!SourceMesh || !DynamicMeshPiece)
	{
		return false;
	}

	SourceMesh->ProcessMesh([&](const UE::Geometry::FDynamicMesh3& Mesh)
	{
		DynamicMeshPiece->SetMesh(UE::Geometry::FDynamicMesh3(Mesh));
	});

	DynamicMeshPiece->ConfigureMaterialSet(MaterialSet, true);
	const FTransform PieceWorldTransform = DynamicMeshPiece->GetComponentTransform();
	DynamicMeshPiece->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	SetRootComponent(DynamicMeshPiece);
	SetActorTransform(PieceWorldTransform, false, nullptr, ETeleportType::TeleportPhysics);

	if (PhysicsProxy)
	{
		PhysicsProxy->AttachToComponent(DynamicMeshPiece, FAttachmentTransformRules::KeepWorldTransform);
		PhysicsProxy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PhysicsProxy->SetSimulatePhysics(false);
		PhysicsProxy->SetHiddenInGame(true);
	}

	if (ItemMesh)
	{
		ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ItemMesh->SetGenerateOverlapEvents(false);
		ItemMesh->SetSimulatePhysics(false);
		ItemMesh->SetHiddenInGame(true);
	}

	if (ProceduralMeshPiece)
	{
		ProceduralMeshPiece->ClearAllMeshSections();
		ProceduralMeshPiece->ClearCollisionConvexMeshes();
		ProceduralMeshPiece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ProceduralMeshPiece->SetSimulatePhysics(false);
		ProceduralMeshPiece->SetHiddenInGame(true);
	}

	if (GeneratedProceduralOtherHalf)
	{
		GeneratedProceduralOtherHalf->DestroyComponent();
		GeneratedProceduralOtherHalf = nullptr;
	}

	FGeometryScriptCollisionFromMeshOptions CollisionOptions;
	CollisionOptions.bEmitTransaction = false;
	if (CollisionSettings.bUseLongObjectProfile)
	{
		CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::ConvexHulls;
		CollisionOptions.MaxConvexHullsPerMesh = FMath::Max(1, CollisionSettings.LongObjectMaxConvexHulls);
		CollisionOptions.MaxShapeCount = FMath::Max(1, CollisionSettings.LongObjectMaxShapeCount);
		CollisionOptions.MinThickness = FMath::Max(0.01f, CollisionSettings.LongObjectMinThicknessCm);
		CollisionOptions.bSimplifyHulls = true;
		CollisionOptions.ConvexHullTargetFaceCount = 32;
	}
	else
	{
		CollisionOptions.Method = EGeometryScriptCollisionGenerationMethod::MinVolumeShapes;
		CollisionOptions.MaxShapeCount = 1;
		CollisionOptions.MaxConvexHullsPerMesh = 1;
		CollisionOptions.MinThickness = 0.5f;
	}

	UGeometryScriptLibrary_CollisionFunctions::SetDynamicMeshCollisionFromMesh(
		DynamicMeshPiece->GetDynamicMesh(),
		DynamicMeshPiece,
		CollisionOptions,
		nullptr);
	DynamicMeshPiece->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	DynamicMeshPiece->SetSimulatePhysics(true);
	DynamicMeshPiece->SetHiddenInGame(false);
	ActiveProceduralMeshPiece = nullptr;
	bUsingDynamicMeshPiece = true;
	bUsingProceduralMeshPiece = false;
	DisableInactiveVisualComponents();
	return true;
}

bool AObjectFragmentActor::InitializeFromStaticMeshSlice(
	UStaticMeshComponent* SourceStaticMeshComponent,
	const FVector& PlanePositionWorld,
	const FVector& PlaneNormalWorld,
	bool bKeepPositiveHalf,
	UMaterialInterface* CapMaterial)
{
	if (!SourceStaticMeshComponent || !SourceStaticMeshComponent->GetStaticMesh() || !ProceduralMeshPiece)
	{
		return false;
	}

	const FVector SafePlaneNormal = PlaneNormalWorld.GetSafeNormal();
	if (SafePlaneNormal.IsNearlyZero())
	{
		return false;
	}

	if (GeneratedProceduralOtherHalf)
	{
		GeneratedProceduralOtherHalf->DestroyComponent();
		GeneratedProceduralOtherHalf = nullptr;
	}

	ProceduralMeshPiece->ClearAllMeshSections();
	ProceduralMeshPiece->ClearCollisionConvexMeshes();
	ProceduralMeshPiece->SetHiddenInGame(true);
	ProceduralMeshPiece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ProceduralMeshPiece->SetSimulatePhysics(false);

	UKismetProceduralMeshLibrary::CopyProceduralMeshFromStaticMeshComponent(SourceStaticMeshComponent, 0, ProceduralMeshPiece, true);
	if (ProceduralMeshPiece->GetNumSections() <= 0)
	{
		return false;
	}

	UProceduralMeshComponent* OtherHalf = nullptr;
	UKismetProceduralMeshLibrary::SliceProceduralMesh(
		ProceduralMeshPiece,
		PlanePositionWorld,
		SafePlaneNormal,
		true,
		OtherHalf,
		CapMaterial ? EProcMeshSliceCapOption::CreateNewSectionForCap : EProcMeshSliceCapOption::UseLastSectionForCap,
		CapMaterial ? CapMaterial : SourceStaticMeshComponent->GetMaterial(0));
	if (ProceduralMeshPiece->GetNumSections() <= 0 || !OtherHalf || OtherHalf->GetNumSections() <= 0)
	{
		if (OtherHalf)
		{
			OtherHalf->DestroyComponent();
		}

		ProceduralMeshPiece->ClearAllMeshSections();
		ProceduralMeshPiece->ClearCollisionConvexMeshes();
		return false;
	}

	GeneratedProceduralOtherHalf = OtherHalf;

	UProceduralMeshComponent* ActiveMesh = bKeepPositiveHalf ? ProceduralMeshPiece.Get() : OtherHalf;
	UProceduralMeshComponent* InactiveMesh = bKeepPositiveHalf ? OtherHalf : ProceduralMeshPiece.Get();
	if (!ActiveMesh)
	{
		return false;
	}

	ActiveMesh->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
	SetRootComponent(ActiveMesh);
	ActiveMesh->SetRelativeTransform(FTransform::Identity);
	ActiveMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	ActiveMesh->SetCollisionObjectType(ECC_PhysicsBody);
	ActiveMesh->SetCollisionResponseToAllChannels(ECR_Block);
	ActiveMesh->SetGenerateOverlapEvents(true);
	ActiveMesh->SetCanEverAffectNavigation(false);
	ActiveMesh->SetSimulatePhysics(true);
	ActiveMesh->SetLinearDamping(0.3f);
	ActiveMesh->SetAngularDamping(1.8f);
	ActiveMesh->SetHiddenInGame(false);

	if (InactiveMesh)
	{
		InactiveMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		InactiveMesh->SetSimulatePhysics(false);
		InactiveMesh->SetHiddenInGame(true);
		if (InactiveMesh == GeneratedProceduralOtherHalf)
		{
			InactiveMesh->DestroyComponent();
			GeneratedProceduralOtherHalf = nullptr;
		}
	}

	ActiveProceduralMeshPiece = ActiveMesh;
	bUsingProceduralMeshPiece = true;
	bUsingDynamicMeshPiece = false;
	DisableInactiveVisualComponents();
	return true;
}

UPrimitiveComponent* AObjectFragmentActor::ResolveMaterialRuntimePrimitive() const
{
	if (bUsingProceduralMeshPiece && ActiveProceduralMeshPiece)
	{
		return ActiveProceduralMeshPiece;
	}

	if (bUsingDynamicMeshPiece && DynamicMeshPiece)
	{
		return DynamicMeshPiece;
	}

	return PhysicsProxy ? Cast<UPrimitiveComponent>(PhysicsProxy) : Cast<UPrimitiveComponent>(ItemMesh);
}

void AObjectFragmentActor::HandlePostItemMeshConfiguration()
{
	if (!bUsingDynamicMeshPiece && !bUsingProceduralMeshPiece)
	{
		RefreshPhysicsProxyFromItemMesh();
	}
}

void AObjectFragmentActor::DisableInactiveVisualComponents()
{
	if (PhysicsProxy && GetRootComponent() != PhysicsProxy)
	{
		PhysicsProxy->AttachToComponent(Cast<USceneComponent>(GetRootComponent()), FAttachmentTransformRules::KeepRelativeTransform);
		PhysicsProxy->SetRelativeTransform(FTransform::Identity);
		PhysicsProxy->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PhysicsProxy->SetSimulatePhysics(false);
		PhysicsProxy->SetHiddenInGame(true);
	}

	if (ItemMesh)
	{
		ItemMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		ItemMesh->SetGenerateOverlapEvents(false);
		ItemMesh->SetSimulatePhysics(false);
		ItemMesh->SetHiddenInGame(true);
	}

	if (DynamicMeshPiece)
	{
		const bool bDynamicMeshIsActive = bUsingDynamicMeshPiece && GetRootComponent() == DynamicMeshPiece;
		if (!bDynamicMeshIsActive)
		{
			DynamicMeshPiece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			DynamicMeshPiece->SetSimulatePhysics(false);
			DynamicMeshPiece->SetHiddenInGame(true);
		}
	}

	if (ProceduralMeshPiece)
	{
		const bool bProceduralMeshIsActive = bUsingProceduralMeshPiece && (ProceduralMeshPiece == ActiveProceduralMeshPiece);
		if (!bProceduralMeshIsActive)
		{
			ProceduralMeshPiece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			ProceduralMeshPiece->SetSimulatePhysics(false);
			ProceduralMeshPiece->SetHiddenInGame(true);
		}
	}
}
