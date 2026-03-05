// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "AgentInteractable.generated.h"

class AActor;

UINTERFACE(BlueprintType)
class AGENT_API UAgentInteractable : public UInterface
{
	GENERATED_BODY()
};

class AGENT_API IAgentInteractable
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interact")
	bool CanInteract(AActor* Interactor) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interact")
	FVector GetInteractionLocation(AActor* Interactor) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interact")
	bool BeginInteract(AActor* Interactor);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interact")
	void EndInteract(AActor* Interactor);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interact")
	bool IsInteracting(AActor* Interactor) const;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="Interact")
	FText GetInteractPrompt() const;
};
