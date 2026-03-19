// Copyright Epic Games, Inc. All Rights Reserved.

#include "MachineUIBlueprintLibrary.h"
#include "MachineUIScannerTargeting.h"
#include "Machine/MachineComponent.h"
#include "Material/RecipeAsset.h"

namespace
{
FText ResolveMachineRecipeDisplayName(const URecipeAsset* RecipeAsset, int32 RecipeIndex)
{
	if (!RecipeAsset)
	{
		return FText::Format(
			NSLOCTEXT("MachineUI", "RecipeFallbackFormat", "Recipe {0}"),
			FText::AsNumber(RecipeIndex + 1));
	}

	if (!RecipeAsset->DisplayName.IsEmpty())
	{
		return RecipeAsset->DisplayName;
	}

	return FText::FromString(RecipeAsset->GetName());
}

bool ResolveCommandBoolValue(const FMachineUICommand& Command)
{
	if (Command.IntValue != 0)
	{
		return true;
	}

	if (!FMath::IsNearlyZero(Command.FloatValue))
	{
		return Command.FloatValue > 0.5f;
	}

	const FString NormalizedName = Command.NameValue.ToString().ToLower();
	return NormalizedName == TEXT("on")
		|| NormalizedName == TEXT("true")
		|| NormalizedName == TEXT("enabled");
}

int32 WrapIndex(int32 Index, int32 Count)
{
	if (Count <= 0)
	{
		return INDEX_NONE;
	}

	const int32 ModValue = Index % Count;
	return ModValue < 0 ? ModValue + Count : ModValue;
}
}

bool UMachineUIBlueprintLibrary::ResolveMachineComponentFromActor(
	const AActor* SourceActor,
	UMachineComponent*& OutMachineComponent,
	AActor*& OutMachineActor)
{
	OutMachineComponent = nullptr;
	OutMachineActor = nullptr;

	if (!SourceActor)
	{
		return false;
	}

	OutMachineComponent = AgentMachineUITargeting::ResolveMachineComponentFromCandidate(const_cast<AActor*>(SourceActor));
	if (!OutMachineComponent)
	{
		return false;
	}

	OutMachineActor = OutMachineComponent->GetOwner();
	return true;
}

bool UMachineUIBlueprintLibrary::BuildMachineUIStateFromMachineComponent(
	const UMachineComponent* MachineComponent,
	FMachineUIState& OutState)
{
	OutState = FMachineUIState();
	if (!MachineComponent)
	{
		return false;
	}

	const AActor* MachineActor = MachineComponent->GetOwner();
	OutState.MachineName = FText::FromString(MachineActor ? MachineActor->GetName() : TEXT("Machine"));
	OutState.RuntimeStatus = MachineComponent->RuntimeStatus;
	OutState.bPowerEnabled = MachineComponent->bEnabled;
	OutState.CurrentSpeed = MachineComponent->Speed;
	OutState.MinSpeed = 0.01f;
	OutState.MaxSpeed = 8.0f;
	OutState.EffectiveSpeed = MachineComponent->bEnabled ? MachineComponent->Speed : 0.0f;
	OutState.StoredUnits = MachineComponent->GetTotalStoredUnits();
	OutState.CapacityUnits = MachineComponent->CapacityUnits > 0
		? static_cast<float>(MachineComponent->CapacityUnits)
		: -1.0f;
	OutState.InputFillPercent01 = OutState.CapacityUnits > KINDA_SMALL_NUMBER
		? FMath::Clamp(OutState.StoredUnits / OutState.CapacityUnits, 0.0f, 1.0f)
		: 0.0f;
	OutState.CraftProgress01 = FMath::Clamp(MachineComponent->GetCraftProgress01(), 0.0f, 1.0f);
	OutState.CraftRemainingSeconds = MachineComponent->GetCraftRemainingSeconds();
	if (!FMath::IsFinite(OutState.CraftRemainingSeconds))
	{
		OutState.CraftRemainingSeconds = -1.0f;
	}

	OutState.bRecipeAny = MachineComponent->bRecipeAny;
	OutState.RecipeCount = MachineComponent->Recipes.Num();
	OutState.RecipeIndex = INDEX_NONE;
	OutState.ActiveRecipeName = NSLOCTEXT("MachineUI", "NoRecipeLabel", "No Recipe");
	OutState.RecipeOptions.Reset();
	OutState.RecipeOptions.Reserve(OutState.RecipeCount);

	const int32 SafeActiveRecipeIndex = OutState.RecipeCount > 0
		? FMath::Clamp(MachineComponent->ActiveRecipeIndex, 0, OutState.RecipeCount - 1)
		: INDEX_NONE;

	for (int32 RecipeIndex = 0; RecipeIndex < OutState.RecipeCount; ++RecipeIndex)
	{
		const URecipeAsset* RecipeAsset = MachineComponent->Recipes.IsValidIndex(RecipeIndex)
			? MachineComponent->Recipes[RecipeIndex].Get()
			: nullptr;

		FMachineUIRecipeOption RecipeOption;
		RecipeOption.RecipeIndex = RecipeIndex;
		RecipeOption.RecipeId = RecipeAsset ? RecipeAsset->RecipeId : NAME_None;
		RecipeOption.DisplayName = ResolveMachineRecipeDisplayName(RecipeAsset, RecipeIndex);
		RecipeOption.CraftTimeSeconds = RecipeAsset ? RecipeAsset->GetResolvedCraftTimeSeconds() : 0.0f;
		RecipeOption.bIsActive = !MachineComponent->bRecipeAny && RecipeIndex == SafeActiveRecipeIndex;
		OutState.RecipeOptions.Add(RecipeOption);
	}

	if (OutState.RecipeCount > 0)
	{
		if (MachineComponent->bRecipeAny)
		{
			OutState.ActiveRecipeName = NSLOCTEXT("MachineUI", "AnyRecipeLabel", "Auto (Any Recipe)");
		}
		else
		{
			OutState.RecipeIndex = SafeActiveRecipeIndex;
			if (OutState.RecipeOptions.IsValidIndex(SafeActiveRecipeIndex))
			{
				OutState.ActiveRecipeName = OutState.RecipeOptions[SafeActiveRecipeIndex].DisplayName;
			}
		}
	}

	return true;
}

bool UMachineUIBlueprintLibrary::BuildMachineUIStateFromActor(const AActor* SourceActor, FMachineUIState& OutState)
{
	UMachineComponent* MachineComponent = nullptr;
	AActor* MachineActor = nullptr;
	if (!ResolveMachineComponentFromActor(SourceActor, MachineComponent, MachineActor))
	{
		OutState = FMachineUIState();
		return false;
	}

	return BuildMachineUIStateFromMachineComponent(MachineComponent, OutState);
}

bool UMachineUIBlueprintLibrary::ApplyMachineUICommandToMachineComponent(
	UMachineComponent* MachineComponent,
	const FMachineUICommand& Command,
	float DefaultSpeedStep)
{
	if (!MachineComponent)
	{
		return false;
	}

	const int32 RecipeCount = MachineComponent->Recipes.Num();
	switch (Command.CommandType)
	{
	case EMachineUICommandType::TogglePower:
		MachineComponent->bEnabled = !MachineComponent->bEnabled;
		return true;

	case EMachineUICommandType::SetPowerEnabled:
		MachineComponent->bEnabled = ResolveCommandBoolValue(Command);
		return true;

	case EMachineUICommandType::AdjustSpeed:
	{
		float SpeedDelta = Command.FloatValue;
		if (FMath::IsNearlyZero(SpeedDelta))
		{
			SpeedDelta = Command.IntValue != 0 ? static_cast<float>(Command.IntValue) : DefaultSpeedStep;
		}

		MachineComponent->SetSpeed(MachineComponent->Speed + SpeedDelta);
		return true;
	}

	case EMachineUICommandType::SetSpeed:
	{
		float DesiredSpeed = Command.FloatValue;
		if (DesiredSpeed <= 0.0f && Command.IntValue > 0)
		{
			DesiredSpeed = static_cast<float>(Command.IntValue);
		}

		if (DesiredSpeed <= 0.0f)
		{
			return false;
		}

		MachineComponent->SetSpeed(DesiredSpeed);
		return true;
	}

	case EMachineUICommandType::CycleRecipe:
	{
		if (RecipeCount <= 0)
		{
			return false;
		}

		const int32 Step = Command.IntValue != 0 ? Command.IntValue : 1;
		const int32 CurrentIndex = FMath::Clamp(MachineComponent->ActiveRecipeIndex, 0, RecipeCount - 1);
		const int32 NextIndex = WrapIndex(CurrentIndex + Step, RecipeCount);
		MachineComponent->bRecipeAny = false;
		MachineComponent->SetActiveRecipeIndex(NextIndex);
		return true;
	}

	case EMachineUICommandType::SetRecipeAny:
		MachineComponent->bRecipeAny = ResolveCommandBoolValue(Command);
		if (!MachineComponent->bRecipeAny && RecipeCount > 0)
		{
			const int32 SafeIndex = FMath::Clamp(MachineComponent->ActiveRecipeIndex, 0, RecipeCount - 1);
			MachineComponent->SetActiveRecipeIndex(SafeIndex);
		}
		return true;

	case EMachineUICommandType::SelectRecipeIndex:
	{
		if (RecipeCount <= 0)
		{
			return false;
		}

		int32 DesiredIndex = Command.IntValue;
		if (DesiredIndex == 0 && !FMath::IsNearlyZero(Command.FloatValue))
		{
			DesiredIndex = FMath::RoundToInt(Command.FloatValue);
		}

		if (DesiredIndex < 0)
		{
			return false;
		}

		MachineComponent->bRecipeAny = false;
		MachineComponent->SetActiveRecipeIndex(FMath::Clamp(DesiredIndex, 0, RecipeCount - 1));
		return true;
	}

	case EMachineUICommandType::InstallUpgrade:
	case EMachineUICommandType::RemoveUpgrade:
	case EMachineUICommandType::Custom:
	case EMachineUICommandType::None:
	default:
		return false;
	}
}

bool UMachineUIBlueprintLibrary::ApplyMachineUICommandToActor(
	AActor* SourceActor,
	const FMachineUICommand& Command,
	float DefaultSpeedStep)
{
	UMachineComponent* MachineComponent = nullptr;
	AActor* MachineActor = nullptr;
	if (!ResolveMachineComponentFromActor(SourceActor, MachineComponent, MachineActor))
	{
		return false;
	}

	return ApplyMachineUICommandToMachineComponent(MachineComponent, Command, DefaultSpeedStep);
}

