// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AgentCharacterAnimInstance.h"
#include "AgentCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "UObject/UnrealType.h"

void UAgentCharacterAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	RefreshCachedCharacter();
	ResetProxyLocomotionState();
}

void UAgentCharacterAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);

	if (!CachedAgentCharacter.IsValid())
	{
		RefreshCachedCharacter();
	}

	ApplyAttachedLocomotionProxy(DeltaSeconds);
}

void UAgentCharacterAnimInstance::RefreshCachedCharacter()
{
	if (APawn* PawnOwner = TryGetPawnOwner())
	{
		CachedAgentCharacter = Cast<AAgentCharacter>(PawnOwner);
		if (CachedAgentCharacter.IsValid())
		{
			return;
		}
	}

	if (AActor* OwningActor = GetOwningActor())
	{
		CachedAgentCharacter = Cast<AAgentCharacter>(OwningActor);
		if (CachedAgentCharacter.IsValid())
		{
			return;
		}
	}

	if (USkeletalMeshComponent* SkeletalMeshComponent = GetSkelMeshComponent())
	{
		CachedAgentCharacter = Cast<AAgentCharacter>(SkeletalMeshComponent->GetOwner());
		if (CachedAgentCharacter.IsValid())
		{
			return;
		}
	}

	CachedAgentCharacter.Reset();
}

void UAgentCharacterAnimInstance::ResetProxyLocomotionState()
{
	PreviousProxyVelocity = FVector::ZeroVector;
}

void UAgentCharacterAnimInstance::ApplyAttachedLocomotionProxy(float DeltaSeconds)
{
	const AAgentCharacter* AgentCharacter = CachedAgentCharacter.Get();
	if (!AgentCharacter || !AgentCharacter->ShouldUseAttachedLocomotionProxy())
	{
		ResetProxyLocomotionState();
		return;
	}

	const FVector ProxyVelocity = AgentCharacter->GetAttachedLocomotionVelocityWorld();
	const float GroundSpeed = FVector(ProxyVelocity.X, ProxyVelocity.Y, 0.0f).Size();
	const bool bShouldMove = GroundSpeed > 3.0f;
	const FVector ProxyAcceleration = DeltaSeconds > KINDA_SMALL_NUMBER
		? (ProxyVelocity - PreviousProxyVelocity) / DeltaSeconds
		: FVector::ZeroVector;
	const bool bIsAccelerating = ProxyAcceleration.SizeSquared2D() > FMath::Square(3.0f);
	const bool bIsFalling = AgentCharacter->GetCharacterMovement()
		? AgentCharacter->GetCharacterMovement()->IsFalling()
		: false;
	PreviousProxyVelocity = ProxyVelocity;

	SetVectorVariableIfPresent(TEXT("Velocity"), ProxyVelocity);
	SetFloatVariableIfPresent(TEXT("GroundSpeed"), GroundSpeed);
	SetFloatVariableIfPresent(TEXT("Speed"), GroundSpeed);
	SetBoolVariableIfPresent(TEXT("ShouldMove"), bShouldMove);
	SetVectorVariableIfPresent(TEXT("Acceleration"), ProxyAcceleration);
	SetBoolVariableIfPresent(TEXT("IsAccelerating"), bIsAccelerating);
	SetBoolVariableIfPresent(TEXT("HasAcceleration"), bIsAccelerating);
	SetBoolVariableIfPresent(TEXT("IsFalling"), bIsFalling);
}

bool UAgentCharacterAnimInstance::SetFloatVariableIfPresent(FName PropertyName, float Value)
{
	if (FFloatProperty* FloatProperty = FindFProperty<FFloatProperty>(GetClass(), PropertyName))
	{
		FloatProperty->SetFloatingPointPropertyValue(FloatProperty->ContainerPtrToValuePtr<void>(this), Value);
		return true;
	}

	if (FDoubleProperty* DoubleProperty = FindFProperty<FDoubleProperty>(GetClass(), PropertyName))
	{
		DoubleProperty->SetFloatingPointPropertyValue(DoubleProperty->ContainerPtrToValuePtr<void>(this), static_cast<double>(Value));
		return true;
	}

	return false;
}

bool UAgentCharacterAnimInstance::SetBoolVariableIfPresent(FName PropertyName, bool Value)
{
	if (FBoolProperty* BoolProperty = FindFProperty<FBoolProperty>(GetClass(), PropertyName))
	{
		BoolProperty->SetPropertyValue(BoolProperty->ContainerPtrToValuePtr<void>(this), Value);
		return true;
	}

	return false;
}

bool UAgentCharacterAnimInstance::SetVectorVariableIfPresent(FName PropertyName, const FVector& Value)
{
	if (FStructProperty* StructProperty = FindFProperty<FStructProperty>(GetClass(), PropertyName))
	{
		if (StructProperty->Struct == TBaseStructure<FVector>::Get())
		{
			if (FVector* VectorValue = StructProperty->ContainerPtrToValuePtr<FVector>(this))
			{
				*VectorValue = Value;
				return true;
			}
		}
	}

	return false;
}
