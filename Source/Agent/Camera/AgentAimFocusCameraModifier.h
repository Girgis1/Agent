// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraModifier.h"
#include "AgentAimFocusCameraModifier.generated.h"

UCLASS()
class AGENT_API UAgentAimFocusCameraModifier : public UCameraModifier
{
	GENERATED_BODY()

public:
	UAgentAimFocusCameraModifier();

	void SetAimFocusState(float InBlendWeight, const FPostProcessSettings& InPostProcessSettings);

protected:
	virtual void ModifyPostProcess(float DeltaTime, float& PostProcessBlendWeight, FPostProcessSettings& PostProcessSettings) override;

	float BlendWeight = 0.0f;
	FPostProcessSettings FocusPostProcessSettings;
};
