// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/ConveyorBeltStraight.h"
#include "Factory/ConveyorSurfaceVelocityComponent.h"
#include "Factory/FactoryPlacementHelpers.h"
#include "Factory/FactoryWorldConfig.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/ConstructorHelpers.h"

AConveyorBeltStraight::AConveyorBeltStraight()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	SupportCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("SupportCollision"));
	SupportCollision->SetupAttachment(SceneRoot);
	SupportCollision->SetBoxExtent(SupportBoxExtent);
	SupportCollision->SetRelativeLocation(FVector(0.0f, 0.0f, SupportBoxExtent.Z));
	SupportCollision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	SupportCollision->SetCollisionObjectType(AgentFactory::FactoryBuildableChannel);
	SupportCollision->SetCollisionResponseToAllChannels(ECR_Block);
	SupportCollision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	SupportCollision->SetGenerateOverlapEvents(false);
	SupportCollision->SetCanEverAffectNavigation(false);

	ConveyorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ConveyorMesh"));
	ConveyorMesh->SetupAttachment(SupportCollision);
	ConveyorMesh->SetRelativeLocation(FVector::ZeroVector);
	ConveyorMesh->SetRelativeScale3D(FVector(1.0f, 1.0f, 0.2f));
	ConveyorMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ConveyorMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	ConveyorMesh->SetGenerateOverlapEvents(false);
	ConveyorMesh->SetCanEverAffectNavigation(false);

	PayloadDriveVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("PayloadDriveVolume"));
	PayloadDriveVolume->SetupAttachment(SceneRoot);
	PayloadDriveVolume->SetBoxExtent(PayloadDriveExtent);
	PayloadDriveVolume->SetRelativeLocation(FVector(0.0f, 0.0f, SupportBoxExtent.Z * 2.0f + PayloadDriveExtent.Z * 0.5f));
	PayloadDriveVolume->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PayloadDriveVolume->SetCollisionObjectType(ECC_WorldDynamic);
	PayloadDriveVolume->SetCollisionResponseToAllChannels(ECR_Ignore);
	PayloadDriveVolume->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Overlap);
	PayloadDriveVolume->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	PayloadDriveVolume->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	PayloadDriveVolume->SetGenerateOverlapEvents(true);
	PayloadDriveVolume->SetCanEverAffectNavigation(false);

	DirectionArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("DirectionArrow"));
	DirectionArrow->SetupAttachment(SceneRoot);
	DirectionArrow->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
	DirectionArrow->SetArrowColor(FLinearColor(FColor::Yellow));
	DirectionArrow->ArrowSize = 1.2f;
	DirectionArrow->SetCanEverAffectNavigation(false);

	PayloadDriveVolume->OnComponentBeginOverlap.AddDynamic(this, &AConveyorBeltStraight::OnPayloadBeginOverlap);
	PayloadDriveVolume->OnComponentEndOverlap.AddDynamic(this, &AConveyorBeltStraight::OnPayloadEndOverlap);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConveyorMeshAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (ConveyorMeshAsset.Succeeded())
	{
		ConveyorMesh->SetStaticMesh(ConveyorMeshAsset.Object);
	}
}

FVector AConveyorBeltStraight::GetSurfaceVelocity() const
{
	const FVector DriveDirection = DirectionArrow ? DirectionArrow->GetForwardVector() : GetActorForwardVector();
	return DriveDirection.GetSafeNormal() * GetResolvedBeltSpeed();
}

void AConveyorBeltStraight::BeginPlay()
{
	Super::BeginPlay();

	if (SupportCollision)
	{
		SupportCollision->SetBoxExtent(SupportBoxExtent);
		SupportCollision->SetRelativeLocation(FVector(0.0f, 0.0f, SupportBoxExtent.Z));
	}

	if (PayloadDriveVolume)
	{
		PayloadDriveVolume->SetBoxExtent(PayloadDriveExtent);
		PayloadDriveVolume->SetRelativeLocation(FVector(0.0f, 0.0f, SupportBoxExtent.Z * 2.0f + PayloadDriveExtent.Z * 0.5f));
	}

	UpdateSupportPhysicalMaterial();
	UpdateTickState();
}

void AConveyorBeltStraight::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	TArray<TWeakObjectPtr<UPrimitiveComponent>> PayloadsToRemove;
	for (const TWeakObjectPtr<UPrimitiveComponent>& PayloadEntry : ActivePayloads)
	{
		UPrimitiveComponent* PrimitiveComponent = PayloadEntry.Get();
		if (!IsPayloadValid(PrimitiveComponent))
		{
			PayloadsToRemove.Add(PayloadEntry);
			continue;
		}

		ApplyBeltDrive(PrimitiveComponent, DeltaSeconds);
	}

	for (const TWeakObjectPtr<UPrimitiveComponent>& PayloadToRemove : PayloadsToRemove)
	{
		ActivePayloads.Remove(PayloadToRemove);
	}

	const FVector SurfaceVelocity = GetSurfaceVelocity();
	TArray<TWeakObjectPtr<UConveyorSurfaceVelocityComponent>> ConsumersToRemove;
	for (const TWeakObjectPtr<UConveyorSurfaceVelocityComponent>& ConsumerEntry : ActiveSurfaceVelocityConsumers)
	{
		UConveyorSurfaceVelocityComponent* SurfaceVelocityComponent = ConsumerEntry.Get();
		if (!IsValid(SurfaceVelocityComponent))
		{
			ConsumersToRemove.Add(ConsumerEntry);
			continue;
		}

		SurfaceVelocityComponent->SetConveyorSurfaceVelocity(this, SurfaceVelocity);
	}

	for (const TWeakObjectPtr<UConveyorSurfaceVelocityComponent>& ConsumerToRemove : ConsumersToRemove)
	{
		ActiveSurfaceVelocityConsumers.Remove(ConsumerToRemove);
	}

	UpdateTickState();
}

void AConveyorBeltStraight::OnPayloadBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	(void)OverlappedComponent;
	(void)OtherActor;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;

	if (UConveyorSurfaceVelocityComponent* SurfaceVelocityConsumer = GetSurfaceVelocityConsumer(OtherActor))
	{
		ActiveSurfaceVelocityConsumers.Add(SurfaceVelocityConsumer);
		SurfaceVelocityConsumer->SetConveyorSurfaceVelocity(this, GetSurfaceVelocity());
	}
	else if (IsPayloadValid(OtherComp))
	{
		ActivePayloads.Add(OtherComp);
	}

	UpdateTickState();
}

void AConveyorBeltStraight::OnPayloadEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	(void)OverlappedComponent;
	(void)OtherActor;
	(void)OtherBodyIndex;

	if (OtherComp)
	{
		ActivePayloads.Remove(OtherComp);
	}

	if (UConveyorSurfaceVelocityComponent* SurfaceVelocityConsumer = GetSurfaceVelocityConsumer(OtherActor))
	{
		ActiveSurfaceVelocityConsumers.Remove(SurfaceVelocityConsumer);
		SurfaceVelocityConsumer->ClearConveyorSurfaceVelocity(this);
	}

	UpdateTickState();
}

void AConveyorBeltStraight::UpdateTickState()
{
	SetActorTickEnabled(ActivePayloads.Num() > 0 || ActiveSurfaceVelocityConsumers.Num() > 0);
}

void AConveyorBeltStraight::UpdateSupportPhysicalMaterial()
{
	if (!SupportCollision)
	{
		return;
	}

	if (!RuntimeSupportPhysicalMaterial)
	{
		RuntimeSupportPhysicalMaterial = NewObject<UPhysicalMaterial>(this, TEXT("RuntimeSupportPhysicalMaterial"));
	}

	if (!RuntimeSupportPhysicalMaterial)
	{
		return;
	}

	RuntimeSupportPhysicalMaterial->Friction = FMath::Max(0.0f, BeltSurfaceFriction);
	RuntimeSupportPhysicalMaterial->StaticFriction = FMath::Max(0.0f, BeltSurfaceFriction);
	RuntimeSupportPhysicalMaterial->Restitution = FMath::Clamp(BeltSurfaceRestitution, 0.0f, 1.0f);
	RuntimeSupportPhysicalMaterial->bOverrideFrictionCombineMode = true;
	RuntimeSupportPhysicalMaterial->FrictionCombineMode = EFrictionCombineMode::Min;
	RuntimeSupportPhysicalMaterial->bOverrideRestitutionCombineMode = true;
	RuntimeSupportPhysicalMaterial->RestitutionCombineMode = EFrictionCombineMode::Min;

	SupportCollision->SetPhysMaterialOverride(RuntimeSupportPhysicalMaterial);
}

bool AConveyorBeltStraight::IsPayloadValid(const UPrimitiveComponent* PrimitiveComponent) const
{
	return IsValid(PrimitiveComponent)
		&& PrimitiveComponent->IsSimulatingPhysics()
		&& PrimitiveComponent->IsRegistered();
}

float AConveyorBeltStraight::GetResolvedBeltSpeed() const
{
	if (!bUseMasterSpeedSettings)
	{
		return FMath::Max(0.0f, BeltSpeed);
	}

	if (const AFactoryWorldConfig* WorldConfig = GetFactoryWorldConfig())
	{
		return FMath::Max(0.0f, WorldConfig->ConveyorMasterBeltSpeed);
	}

	if (const AFactoryWorldConfig* DefaultConfig = GetDefault<AFactoryWorldConfig>())
	{
		return FMath::Max(0.0f, DefaultConfig->ConveyorMasterBeltSpeed);
	}

	return FMath::Max(0.0f, BeltSpeed);
}

UConveyorSurfaceVelocityComponent* AConveyorBeltStraight::GetSurfaceVelocityConsumer(AActor* OtherActor) const
{
	return IsValid(OtherActor) ? OtherActor->FindComponentByClass<UConveyorSurfaceVelocityComponent>() : nullptr;
}

AFactoryWorldConfig* AConveyorBeltStraight::GetFactoryWorldConfig() const
{
	if (CachedWorldConfig.IsValid())
	{
		return CachedWorldConfig.Get();
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AFactoryWorldConfig> It(World); It; ++It)
	{
		CachedWorldConfig = *It;
		return CachedWorldConfig.Get();
	}

	return nullptr;
}

bool AConveyorBeltStraight::IsWorldPointSupportedByBelt(const FVector& WorldPoint) const
{
	if (!SupportCollision)
	{
		return false;
	}

	const FVector LocalPoint = GetActorTransform().InverseTransformPosition(WorldPoint);
	const FVector SupportCenter = SupportCollision->GetRelativeLocation();
	const float SupportTopZ = SupportCenter.Z + SupportBoxExtent.Z;
	constexpr float SupportXYTolerance = 2.0f;
	constexpr float SupportSurfaceTolerance = 14.0f;

	const bool bWithinX = FMath::Abs(LocalPoint.X - SupportCenter.X) <= (SupportBoxExtent.X + SupportXYTolerance);
	const bool bWithinY = FMath::Abs(LocalPoint.Y - SupportCenter.Y) <= (SupportBoxExtent.Y + SupportXYTolerance);
	const bool bNearSurface = FMath::Abs(LocalPoint.Z - SupportTopZ) <= SupportSurfaceTolerance;

	return bWithinX && bWithinY && bNearSurface;
}

void AConveyorBeltStraight::ApplyBeltDrive(UPrimitiveComponent* PrimitiveComponent, float DeltaSeconds) const
{
	(void)DeltaSeconds;

	if (!PrimitiveComponent || DeltaSeconds <= 0.0f)
	{
		return;
	}

	const FVector SupportPoint = PrimitiveComponent->Bounds.Origin - FVector(0.0f, 0.0f, PrimitiveComponent->Bounds.BoxExtent.Z - 2.0f);
	if (!IsWorldPointSupportedByBelt(SupportPoint))
	{
		return;
	}

	const FVector DriveDirection = DirectionArrow ? DirectionArrow->GetForwardVector() : GetActorForwardVector();
	const FVector CurrentVelocity = PrimitiveComponent->GetPhysicsLinearVelocity();
	const float CurrentAlongBeltSpeed = FVector::DotProduct(CurrentVelocity, DriveDirection);
	const float TargetSpeed = GetResolvedBeltSpeed();
	const FVector NonConveyorVelocity = CurrentVelocity - (DriveDirection * CurrentAlongBeltSpeed);
	const FVector DesiredVelocity = NonConveyorVelocity + (DriveDirection * TargetSpeed);
	PrimitiveComponent->SetPhysicsLinearVelocity(DesiredVelocity, false);
}
