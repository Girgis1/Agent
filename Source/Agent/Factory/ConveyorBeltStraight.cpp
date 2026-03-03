// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factory/ConveyorBeltStraight.h"
#include "Factory/FactoryPlacementHelpers.h"
#include "Components/ArrowComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "UObject/ConstructorHelpers.h"

float AConveyorBeltStraight::MasterBeltSpeed = 150.0f;

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

void AConveyorBeltStraight::SetMasterConveyorSettings(float InBeltSpeed)
{
	MasterBeltSpeed = FMath::Max(0.0f, InBeltSpeed);
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

	TArray<TWeakObjectPtr<ACharacter>> CharactersToRemove;
	for (const TWeakObjectPtr<ACharacter>& CharacterEntry : ActiveCharacters)
	{
		ACharacter* Character = CharacterEntry.Get();
		if (!IsCharacterOccupantValid(Character))
		{
			CharactersToRemove.Add(CharacterEntry);
			continue;
		}

		ApplyBeltDriveToCharacter(Character);
	}

	for (const TWeakObjectPtr<ACharacter>& CharacterToRemove : CharactersToRemove)
	{
		ActiveCharacters.Remove(CharacterToRemove);
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

	if (IsPayloadValid(OtherComp))
	{
		ActivePayloads.Add(OtherComp);
	}

	if (ACharacter* Character = Cast<ACharacter>(OtherActor))
	{
		if (OtherComp == Character->GetCapsuleComponent() && IsCharacterOccupantValid(Character))
		{
			ActiveCharacters.Add(Character);
		}
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

	if (ACharacter* Character = Cast<ACharacter>(OtherActor))
	{
		if (OtherComp == Character->GetCapsuleComponent())
		{
			ActiveCharacters.Remove(Character);
		}
	}

	UpdateTickState();
}

void AConveyorBeltStraight::UpdateTickState()
{
	SetActorTickEnabled(ActivePayloads.Num() > 0 || ActiveCharacters.Num() > 0);
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

bool AConveyorBeltStraight::IsCharacterOccupantValid(const ACharacter* Character) const
{
	return IsValid(Character)
		&& Character->GetCharacterMovement() != nullptr
		&& Character->GetRootComponent() != nullptr
		&& Character->GetRootComponent()->IsRegistered();
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
	const float TargetSpeed = bUseMasterSpeedSettings ? MasterBeltSpeed : FMath::Max(0.0f, BeltSpeed);
	const FVector NonConveyorVelocity = CurrentVelocity - (DriveDirection * CurrentAlongBeltSpeed);
	const FVector DesiredVelocity = NonConveyorVelocity + (DriveDirection * TargetSpeed);
	PrimitiveComponent->SetPhysicsLinearVelocity(DesiredVelocity, false);
}

void AConveyorBeltStraight::ApplyBeltDriveToCharacter(ACharacter* Character) const
{
	if (!IsCharacterOccupantValid(Character))
	{
		return;
	}

	UCharacterMovementComponent* CharacterMovement = Character->GetCharacterMovement();
	if (!CharacterMovement || !CharacterMovement->IsMovingOnGround())
	{
		return;
	}

	const UCapsuleComponent* CapsuleComponent = Character->GetCapsuleComponent();
	const float CapsuleHalfHeight = CapsuleComponent ? CapsuleComponent->GetScaledCapsuleHalfHeight() : 88.0f;
	const FVector SupportPoint = Character->GetActorLocation() - FVector(0.0f, 0.0f, CapsuleHalfHeight - 2.0f);
	if (!IsWorldPointSupportedByBelt(SupportPoint))
	{
		return;
	}

	const FVector DriveDirection = DirectionArrow ? DirectionArrow->GetForwardVector() : GetActorForwardVector();
	const FVector CurrentVelocity = CharacterMovement->Velocity;
	const float CurrentAlongBeltSpeed = FVector::DotProduct(CurrentVelocity, DriveDirection);
	const float TargetSpeed = bUseMasterSpeedSettings ? MasterBeltSpeed : FMath::Max(0.0f, BeltSpeed);
	const FVector NonConveyorVelocity = CurrentVelocity - (DriveDirection * CurrentAlongBeltSpeed);
	CharacterMovement->Velocity = NonConveyorVelocity + (DriveDirection * TargetSpeed);
}
