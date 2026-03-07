// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "AgentCharacterAnimInstance.generated.h"

class AAgentCharacter;

UCLASS(BlueprintType, Blueprintable, Transient)
class AGENT_API UAgentCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

private:
	void RefreshCachedCharacter();
	void ResetProxyLocomotionState();
	void ApplyAttachedLocomotionProxy(float DeltaSeconds);
	bool SetFloatVariableIfPresent(FName PropertyName, float Value);
	bool SetBoolVariableIfPresent(FName PropertyName, bool Value);
	bool SetVectorVariableIfPresent(FName PropertyName, const FVector& Value);

	UPROPERTY(Transient)
	TWeakObjectPtr<AAgentCharacter> CachedAgentCharacter;

	UPROPERTY(Transient)
	FVector PreviousProxyVelocity = FVector::ZeroVector;
};
