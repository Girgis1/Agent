// Copyright Epic Games, Inc. All Rights Reserved.

#include "Objects/Actors/ObjectFragmentActor.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
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
	if (!PhysicsProxy || !ItemMesh)
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

UPrimitiveComponent* AObjectFragmentActor::ResolveMaterialRuntimePrimitive() const
{
	return PhysicsProxy ? Cast<UPrimitiveComponent>(PhysicsProxy) : Cast<UPrimitiveComponent>(ItemMesh);
}

void AObjectFragmentActor::HandlePostItemMeshConfiguration()
{
	RefreshPhysicsProxyFromItemMesh();
}
