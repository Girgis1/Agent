// Copyright Epic Games, Inc. All Rights Reserved.

#include "Tools/ToolWorldComponent.h"

#include "AgentCharacter.h"
#include "Dirty/DirtBrushComponent.h"
#include "Tools/ToolBehaviorComponent.h"
#include "Tools/ToolDefinition.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogToolWorld, Log, All);

UToolWorldComponent::UToolWorldComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UToolWorldComponent::BeginPlay()
{
	Super::BeginPlay();

	if (UPrimitiveComponent* PrimitiveComponent = ResolvePhysicsPrimitive())
	{
		bHasStoredPhysicsState = true;
		bStoredSimulatePhysics = PrimitiveComponent->IsSimulatingPhysics();
		bStoredGravityEnabled = PrimitiveComponent->IsGravityEnabled();
		StoredCollisionEnabled = PrimitiveComponent->GetCollisionEnabled();
		StoredCollisionProfileName = PrimitiveComponent->GetCollisionProfileName();
		StoredLinearDamping = PrimitiveComponent->GetLinearDamping();
		StoredAngularDamping = PrimitiveComponent->GetAngularDamping();
	}

	FString SetupErrorReason;
	if (!ValidateToolSetup(&SetupErrorReason))
	{
		LogToolSetupFailure(SetupErrorReason);
	}
}

bool UToolWorldComponent::ValidateToolSetup(FString* OutErrorReason) const
{
	auto SetError = [&](const TCHAR* Message)
	{
		if (OutErrorReason)
		{
			*OutErrorReason = Message;
		}
		return false;
	};

	if (!ToolDefinition)
	{
		return SetError(TEXT("ToolDefinition is not assigned."));
	}

	if (!ResolvePhysicsPrimitive())
	{
		return SetError(TEXT("PhysicsPrimitiveReference is not assigned to a primitive component."));
	}

	if (!ResolveGripPoint())
	{
		return SetError(TEXT("GripPointReference is not assigned."));
	}

	if (!ResolveHeadPoint())
	{
		return SetError(TEXT("HeadPointReference is not assigned."));
	}

	if (!ResolveBehavior())
	{
		return SetError(TEXT("BehaviorComponentReference is not assigned to a ToolBehaviorComponent."));
	}

	return true;
}

FString UToolWorldComponent::GetToolSetupValidationError() const
{
	FString SetupErrorReason;
	return ValidateToolSetup(&SetupErrorReason) ? FString() : SetupErrorReason;
}

bool UToolWorldComponent::CanBeEquippedBy(const AAgentCharacter* AgentCharacter) const
{
	if (!AgentCharacter || IsEquipped())
	{
		return false;
	}

	FString SetupErrorReason;
	if (!ValidateToolSetup(&SetupErrorReason))
	{
		LogToolSetupFailure(SetupErrorReason);
		return false;
	}

	bHasLoggedSetupFailure = false;

	const float AllowedDistance = FMath::Max(1.0f, ToolDefinition ? ToolDefinition->MaxEquipDistance : 1.0f);
	return FVector::DistSquared(AgentCharacter->GetActorLocation(), GetGripLocation()) <= FMath::Square(AllowedDistance);
}

bool UToolWorldComponent::OnEquipped(AAgentCharacter* AgentCharacter)
{
	FString SetupErrorReason;
	if (!AgentCharacter || !ValidateToolSetup(&SetupErrorReason))
	{
		if (!SetupErrorReason.IsEmpty())
		{
			LogToolSetupFailure(SetupErrorReason);
		}
		return false;
	}

	bHasLoggedSetupFailure = false;
	EquippedByCharacter = AgentCharacter;

	if (UPrimitiveComponent* PrimitiveComponent = ResolvePhysicsPrimitive())
	{
		bHasStoredPhysicsState = true;
		bStoredSimulatePhysics = PrimitiveComponent->IsSimulatingPhysics();
		bStoredGravityEnabled = PrimitiveComponent->IsGravityEnabled();
		StoredCollisionEnabled = PrimitiveComponent->GetCollisionEnabled();
		StoredCollisionProfileName = PrimitiveComponent->GetCollisionProfileName();
		StoredLinearDamping = PrimitiveComponent->GetLinearDamping();
		StoredAngularDamping = PrimitiveComponent->GetAngularDamping();

		PrimitiveComponent->SetPhysicsLinearVelocity(FVector::ZeroVector);
		PrimitiveComponent->SetPhysicsAngularVelocityInDegrees(FVector::ZeroVector);
		PrimitiveComponent->SetSimulatePhysics(false);
		PrimitiveComponent->SetEnableGravity(false);

		if (ToolDefinition)
		{
			if (ToolDefinition->bDisableCollisionWhileEquipped)
			{
				PrimitiveComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			}
			else
			{
				PrimitiveComponent->SetCollisionEnabled(ToolDefinition->EquippedCollisionEnabled);
			}
		}
	}

	return true;
}

void UToolWorldComponent::OnDropped(const FVector& LinearVelocity, const FVector& AngularVelocity)
{
	EquippedByCharacter.Reset();

	UPrimitiveComponent* PrimitiveComponent = ResolvePhysicsPrimitive();
	if (!PrimitiveComponent)
	{
		return;
	}

	const bool bRestoreSimulatePhysics = bHasStoredPhysicsState ? bStoredSimulatePhysics : true;
	const bool bRestoreGravityEnabled = bHasStoredPhysicsState ? bStoredGravityEnabled : true;
	const ECollisionEnabled::Type CollisionToRestore = bHasStoredPhysicsState
		? StoredCollisionEnabled
		: ECollisionEnabled::QueryAndPhysics;
	const FName CollisionProfileToRestore = bHasStoredPhysicsState
		? StoredCollisionProfileName
		: NAME_None;

	if (CollisionProfileToRestore != NAME_None)
	{
		PrimitiveComponent->SetCollisionProfileName(CollisionProfileToRestore);
	}
	PrimitiveComponent->SetCollisionEnabled(CollisionToRestore);
	PrimitiveComponent->SetLinearDamping(bHasStoredPhysicsState ? StoredLinearDamping : PrimitiveComponent->GetLinearDamping());
	PrimitiveComponent->SetAngularDamping(bHasStoredPhysicsState ? StoredAngularDamping : PrimitiveComponent->GetAngularDamping());
	PrimitiveComponent->SetEnableGravity(bRestoreGravityEnabled);
	PrimitiveComponent->SetSimulatePhysics(bRestoreSimulatePhysics);

	if (bRestoreSimulatePhysics)
	{
		PrimitiveComponent->WakeAllRigidBodies();
		PrimitiveComponent->SetPhysicsLinearVelocity(LinearVelocity);
		PrimitiveComponent->SetPhysicsAngularVelocityInDegrees(AngularVelocity);
	}
}

void UToolWorldComponent::LogToolSetupFailure(const FString& Reason) const
{
	if (!bLogSetupFailures || bHasLoggedSetupFailure || Reason.IsEmpty())
	{
		return;
	}

	const AActor* OwnerActor = GetOwner();
	const FString OwnerPath = OwnerActor ? OwnerActor->GetPathName() : GetPathName();
	UE_LOG(LogToolWorld, Warning, TEXT("Tool setup invalid for '%s': %s"), *OwnerPath, *Reason);
	bHasLoggedSetupFailure = true;
}

UPrimitiveComponent* UToolWorldComponent::ResolvePhysicsPrimitive() const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}

	return Cast<UPrimitiveComponent>(PhysicsPrimitiveReference.GetComponent(OwnerActor));
}

USceneComponent* UToolWorldComponent::ResolveGripPoint() const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}

	return Cast<USceneComponent>(GripPointReference.GetComponent(OwnerActor));
}

USceneComponent* UToolWorldComponent::ResolveHeadPoint() const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}

	return Cast<USceneComponent>(HeadPointReference.GetComponent(OwnerActor));
}

UToolBehaviorComponent* UToolWorldComponent::ResolveBehavior() const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}

	return Cast<UToolBehaviorComponent>(BehaviorComponentReference.GetComponent(OwnerActor));
}

UDirtBrushComponent* UToolWorldComponent::ResolveDirtBrush() const
{
	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return nullptr;
	}

	return Cast<UDirtBrushComponent>(DirtBrushReference.GetComponent(OwnerActor));
}

FVector UToolWorldComponent::GetGripLocation() const
{
	if (USceneComponent* GripPoint = ResolveGripPoint())
	{
		return GripPoint->GetComponentLocation();
	}

	if (const AActor* OwnerActor = GetOwner())
	{
		return OwnerActor->GetActorLocation();
	}

	return FVector::ZeroVector;
}
