// Copyright Epic Games, Inc. All Rights Reserved.

#include "Interact/Components/InteractVolumeComponent.h"

UInteractVolumeComponent::UInteractVolumeComponent()
{
	InitBoxExtent(FVector(110.0f, 90.0f, 90.0f));
	SetGenerateOverlapEvents(true);
}

AActor* UInteractVolumeComponent::GetInteractableActor() const
{
	return InteractableActorOverride ? InteractableActorOverride.Get() : GetOwner();
}

void UInteractVolumeComponent::BeginPlay()
{
	Super::BeginPlay();

	OnComponentBeginOverlap.AddUniqueDynamic(this, &UInteractVolumeComponent::HandleBeginOverlap);
	OnComponentEndOverlap.AddUniqueDynamic(this, &UInteractVolumeComponent::HandleEndOverlap);
}

void UInteractVolumeComponent::OnRegister()
{
	Super::OnRegister();
	ApplyVolumeDefaults();
}

void UInteractVolumeComponent::ApplyVolumeDefaults()
{
	SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	SetCollisionObjectType(ECC_WorldDynamic);
	SetCollisionResponseToAllChannels(ECR_Ignore);
	SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	SetGenerateOverlapEvents(true);
}

void UInteractVolumeComponent::HandleBeginOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	// Kept intentionally lightweight. Query-based interaction does not depend on overlap state.
}

void UInteractVolumeComponent::HandleEndOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	// Kept intentionally lightweight. Query-based interaction does not depend on overlap state.
}
