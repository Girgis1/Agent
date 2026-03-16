// Copyright Epic Games, Inc. All Rights Reserved.

#include "Camera/AgentAimFocusCameraModifier.h"

#include "Engine/Scene.h"

UAgentAimFocusCameraModifier::UAgentAimFocusCameraModifier()
{
	Priority = 127;
}

void UAgentAimFocusCameraModifier::SetAimFocusState(
	float InBlendWeight,
	const FPostProcessSettings& InPostProcessSettings)
{
	BlendWeight = FMath::Clamp(InBlendWeight, 0.0f, 1.0f);
	FocusPostProcessSettings = InPostProcessSettings;
}

void UAgentAimFocusCameraModifier::ModifyPostProcess(
	float DeltaTime,
	float& PostProcessBlendWeight,
	FPostProcessSettings& PostProcessSettings)
{
	Super::ModifyPostProcess(DeltaTime, PostProcessBlendWeight, PostProcessSettings);

	if (BlendWeight <= KINDA_SMALL_NUMBER)
	{
		PostProcessBlendWeight = 0.0f;
		return;
	}

	PostProcessBlendWeight = BlendWeight;
	PostProcessSettings = FocusPostProcessSettings;
}
