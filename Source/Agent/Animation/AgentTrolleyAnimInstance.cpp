// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AgentTrolleyAnimInstance.h"
#include "AgentCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"

void UAgentTrolleyAnimInstance::NativeInitializeAnimation()
{
	Super::NativeInitializeAnimation();
	RefreshCachedCharacter();
	if (!CachedAgentCharacter.IsValid())
	{
		ResetRuntimeValues();
	}
}

void UAgentTrolleyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	(void)DeltaSeconds;

	if (!CachedAgentCharacter.IsValid())
	{
		RefreshCachedCharacter();
	}

	const AAgentCharacter* AgentCharacter = CachedAgentCharacter.Get();
	if (!AgentCharacter)
	{
		ResetRuntimeValues();
		return;
	}

	bTrolleyPoseActive = AgentCharacter->IsTrolleyPoseActive();
	bTrolleyGripBroken = AgentCharacter->IsTrolleyGripBroken();
	TrolleyPoseBlendAlpha = AgentCharacter->GetTrolleyPoseBlendAlpha();
	TrolleyLeftHandIKAlpha = AgentCharacter->GetTrolleyLeftHandIKAlpha();
	TrolleyRightHandIKAlpha = AgentCharacter->GetTrolleyRightHandIKAlpha();
	TrolleyHandleLeftMeshSpace = AgentCharacter->GetTrolleyHandleLeftMeshSpace();
	TrolleyHandleRightMeshSpace = AgentCharacter->GetTrolleyHandleRightMeshSpace();
	TrolleyChestTargetMeshSpace = AgentCharacter->GetTrolleyChestTargetMeshSpace();
	TrolleyHandleLeftWorldTransform = AgentCharacter->GetTrolleyHandleLeftWorldTransform();
	TrolleyHandleRightWorldTransform = AgentCharacter->GetTrolleyHandleRightWorldTransform();
	TrolleyChestTargetWorldTransform = AgentCharacter->GetTrolleyChestTargetWorldTransform();
	TrolleyHandleLeftMeshTransform = AgentCharacter->GetTrolleyHandleLeftMeshTransform();
	TrolleyHandleRightMeshTransform = AgentCharacter->GetTrolleyHandleRightMeshTransform();
	TrolleyChestTargetMeshTransform = AgentCharacter->GetTrolleyChestTargetMeshTransform();
	TrolleyForwardBlend = AgentCharacter->GetTrolleyForwardBlend();
	TrolleyRightBlend = AgentCharacter->GetTrolleyRightBlend();
	TrolleyStrainAlpha = AgentCharacter->GetTrolleyStrainAlpha();
	TrolleyLeanPitchDeg = AgentCharacter->GetTrolleyLeanPitchDeg();
	TrolleyLeanRollDeg = AgentCharacter->GetTrolleyLeanRollDeg();
	TrolleySpeedNormalized = AgentCharacter->GetTrolleySpeedNormalized();
	TrolleyForwardSpeedLocal = AgentCharacter->GetTrolleyForwardSpeedLocal();
	TrolleyRightSpeedLocal = AgentCharacter->GetTrolleyRightSpeedLocal();
	TrolleyForwardInput = AgentCharacter->GetTrolleyForwardInput();
	TrolleyRightInput = AgentCharacter->GetTrolleyRightInput();
}

void UAgentTrolleyAnimInstance::RefreshCachedCharacter()
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

void UAgentTrolleyAnimInstance::ResetRuntimeValues()
{
	bTrolleyPoseActive = false;
	bTrolleyGripBroken = false;
	TrolleyPoseBlendAlpha = 0.0f;
	TrolleyLeftHandIKAlpha = 0.0f;
	TrolleyRightHandIKAlpha = 0.0f;
	TrolleyHandleLeftMeshSpace = FVector::ZeroVector;
	TrolleyHandleRightMeshSpace = FVector::ZeroVector;
	TrolleyChestTargetMeshSpace = FVector::ZeroVector;
	TrolleyHandleLeftWorldTransform = FTransform::Identity;
	TrolleyHandleRightWorldTransform = FTransform::Identity;
	TrolleyChestTargetWorldTransform = FTransform::Identity;
	TrolleyHandleLeftMeshTransform = FTransform::Identity;
	TrolleyHandleRightMeshTransform = FTransform::Identity;
	TrolleyChestTargetMeshTransform = FTransform::Identity;
	TrolleyForwardBlend = 0.0f;
	TrolleyRightBlend = 0.0f;
	TrolleyStrainAlpha = 0.0f;
	TrolleyLeanPitchDeg = 0.0f;
	TrolleyLeanRollDeg = 0.0f;
	TrolleySpeedNormalized = 0.0f;
	TrolleyForwardSpeedLocal = 0.0f;
	TrolleyRightSpeedLocal = 0.0f;
	TrolleyForwardInput = 0.0f;
	TrolleyRightInput = 0.0f;
}
