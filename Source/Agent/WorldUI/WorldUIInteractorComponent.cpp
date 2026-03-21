// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldUIInteractorComponent.h"
#include "AgentCharacter.h"
#include "WorldUIActor.h"
#include "WorldUIHostComponent.h"
#include "WorldUIRegistrySubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

namespace
{
float ScoreWorldUIHost(const UWorldUIHostComponent* HostComponent, float DistanceToHost, bool bIsAimed)
{
	const float PriorityScore = static_cast<float>(HostComponent ? HostComponent->Priority : 0) * 100000.0f;
	const float AimScore = bIsAimed ? 20000.0f : 0.0f;
	const float DistancePenalty = DistanceToHost;
	return PriorityScore + AimScore - DistancePenalty;
}
}

UWorldUIInteractorComponent::UWorldUIInteractorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UWorldUIInteractorComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UWorldUIInteractorComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearWorldUI(true);
	Super::EndPlay(EndPlayReason);
}

void UWorldUIInteractorComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bWorldUIEnabled)
	{
		ClearWorldUI(false);
		return;
	}

	FVector ViewLocation = FVector::ZeroVector;
	FVector ViewDirection = FVector::ForwardVector;
	if (!ResolveViewerPose(ViewLocation, ViewDirection))
	{
		ClearWorldUI(false);
		return;
	}

	UpdateActiveWorldUI(DeltaTime, ViewLocation, ViewDirection, ResolveScannerModeActive());
}

void UWorldUIInteractorComponent::ClearWorldUI(bool bDestroyActor)
{
	if (AWorldUIActor* UIActor = ActiveUIActor.Get())
	{
		UIActor->ResetButtonInteractionState();
		UIActor->HideReadout();
		if (bDestroyActor)
		{
			UIActor->Destroy();
		}
	}

	if (bDestroyActor)
	{
		ActiveUIActor = nullptr;
	}

	ActiveHostComponent = nullptr;
	ActiveModel = FWorldUIModel();
	TimeUntilModelRefresh = 0.0f;
}

bool UWorldUIInteractorComponent::ResolveViewerPose(FVector& OutViewLocation, FVector& OutViewDirection) const
{
	OutViewLocation = FVector::ZeroVector;
	OutViewDirection = FVector::ForwardVector;

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const APlayerController* PlayerController = World->GetFirstPlayerController();
	if (!PlayerController)
	{
		return false;
	}

	FRotator ViewRotation = FRotator::ZeroRotator;
	PlayerController->GetPlayerViewPoint(OutViewLocation, ViewRotation);
	OutViewDirection = ViewRotation.Vector();
	return !OutViewLocation.IsNearlyZero() && !OutViewDirection.IsNearlyZero();
}

bool UWorldUIInteractorComponent::ResolveScannerModeActive() const
{
	const AAgentCharacter* AgentCharacter = Cast<AAgentCharacter>(GetOwner());
	if (AgentCharacter)
	{
		return AgentCharacter->IsScannerModeActive();
	}

	const UWorld* World = GetWorld();
	const APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	return PlayerController && PlayerController->IsInputKeyDown(EKeys::RightMouseButton);
}

UWorldUIHostComponent* UWorldUIInteractorComponent::ResolveBestHost(
	const FVector& ViewLocation,
	const FVector& ViewDirection,
	bool bScannerModeActive) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	const UWorldUIRegistrySubsystem* Registry = World->GetSubsystem<UWorldUIRegistrySubsystem>();
	if (!Registry)
	{
		return nullptr;
	}

	TArray<UWorldUIHostComponent*> Hosts;
	Registry->GetRegisteredHosts(Hosts);

	UWorldUIHostComponent* BestHost = nullptr;
	float BestScore = -FLT_MAX;
	for (UWorldUIHostComponent* HostComponent : Hosts)
	{
		if (!HostComponent || HostComponent->ActivationMode == EWorldUIActivationMode::Disabled)
		{
			continue;
		}

		float DistanceToHost = 0.0f;
		bool bIsAimed = false;
		if (!DoesHostPassVisibility(HostComponent, ViewLocation, ViewDirection, bScannerModeActive, DistanceToHost, bIsAimed))
		{
			continue;
		}

		const float Score = ScoreWorldUIHost(HostComponent, DistanceToHost, bIsAimed);
		if (!BestHost || Score > BestScore)
		{
			BestHost = HostComponent;
			BestScore = Score;
		}
	}

	return BestHost;
}

bool UWorldUIInteractorComponent::DoesHostPassVisibility(
	UWorldUIHostComponent* HostComponent,
	const FVector& ViewLocation,
	const FVector& ViewDirection,
	bool bScannerModeActive,
	float& OutDistanceToHost,
	bool& bOutIsAimed) const
{
	OutDistanceToHost = 0.0f;
	bOutIsAimed = false;

	if (!HostComponent || !HostComponent->IsActive())
	{
		return false;
	}

	if (HostComponent->bRequireScannerMode && !bScannerModeActive)
	{
		return false;
	}

	const FVector HostLocation = HostComponent->GetPanelWorldLocation();
	const FVector ToHost = HostLocation - ViewLocation;
	OutDistanceToHost = ToHost.Size();
	if (ToHost.IsNearlyZero())
	{
		return false;
	}

	const FVector ToHostDirection = ToHost.GetSafeNormal();
	const bool bWithinRange = HostComponent->ActivationRange <= KINDA_SMALL_NUMBER
		|| OutDistanceToHost <= HostComponent->ActivationRange;
	const bool bWithinAimDistance = OutDistanceToHost <= FMath::Max(100.0f, HostComponent->AimTraceDistance);
	const float DotToHost = FVector::DotProduct(ViewDirection.GetSafeNormal(), ToHostDirection);
	const bool bAimed = bWithinAimDistance && DotToHost >= FMath::Clamp(HostComponent->AimDotThreshold, -1.0f, 1.0f);

	bool bRangeVisible = bWithinRange;
	if (bRangeVisible && HostComponent->bRequireLineOfSightInRange)
	{
		bRangeVisible = HasLineOfSightToHost(HostComponent, ViewLocation, HostLocation);
	}

	bool bAimVisible = bAimed;
	if (bAimVisible && HostComponent->bRequireLineOfSightWhenAimed)
	{
		bAimVisible = HasLineOfSightToHost(HostComponent, ViewLocation, HostLocation);
	}

	bOutIsAimed = bAimVisible;

	switch (HostComponent->ActivationMode)
	{
	case EWorldUIActivationMode::InRange:
		return bRangeVisible;

	case EWorldUIActivationMode::Aimed:
		return bAimVisible;

	case EWorldUIActivationMode::InRangeAndAimed:
		return bRangeVisible && bAimVisible;

	case EWorldUIActivationMode::InRangeOrAimed:
		return bRangeVisible || bAimVisible;

	case EWorldUIActivationMode::Disabled:
	default:
		return false;
	}
}

bool UWorldUIInteractorComponent::HasLineOfSightToHost(
	UWorldUIHostComponent* HostComponent,
	const FVector& ViewLocation,
	const FVector& TargetLocation) const
{
	UWorld* World = GetWorld();
	AActor* OwnerActor = GetOwner();
	if (!World || !HostComponent)
	{
		return false;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WorldUIVisibilityTrace), false);
	if (OwnerActor)
	{
		QueryParams.AddIgnoredActor(OwnerActor);
	}

	FHitResult HitResult;
	const bool bHasHit = World->LineTraceSingleByChannel(
		HitResult,
		ViewLocation,
		TargetLocation,
		ECC_Visibility,
		QueryParams);
	return !bHasHit || IsActorPartOfHost(HitResult.GetActor(), HostComponent);
}

bool UWorldUIInteractorComponent::IsActorPartOfHost(AActor* CandidateActor, const UWorldUIHostComponent* HostComponent) const
{
	if (!CandidateActor || !HostComponent)
	{
		return false;
	}

	const AActor* OwnerActor = HostComponent->GetOwner();
	for (AActor* CurrentActor = CandidateActor; CurrentActor; CurrentActor = CurrentActor->GetAttachParentActor())
	{
		if (CurrentActor == OwnerActor || CurrentActor->GetOwner() == OwnerActor)
		{
			return true;
		}
	}

	return CandidateActor == OwnerActor;
}

AWorldUIActor* UWorldUIInteractorComponent::EnsureUIActorForHost(UWorldUIHostComponent* HostComponent)
{
	if (!HostComponent)
	{
		return nullptr;
	}

	UClass* DesiredClass = HostComponent->UIActorClass ? HostComponent->UIActorClass.Get() : AWorldUIActor::StaticClass();
	AWorldUIActor* ExistingActor = ActiveUIActor.Get();
	if (ExistingActor && ExistingActor->GetClass() == DesiredClass)
	{
		return ExistingActor;
	}

	if (ExistingActor)
	{
		ExistingActor->Destroy();
		ActiveUIActor = nullptr;
	}

	UWorld* World = GetWorld();
	if (!World || !DesiredClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = GetOwner();
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AWorldUIActor* SpawnedActor = World->SpawnActor<AWorldUIActor>(
		DesiredClass,
		HostComponent->GetComponentTransform(),
		SpawnParameters);
	ActiveUIActor = SpawnedActor;
	return SpawnedActor;
}

void UWorldUIInteractorComponent::UpdateActiveWorldUI(float DeltaTime, const FVector& ViewLocation, const FVector& ViewDirection, bool bScannerModeActive)
{
	UWorldUIHostComponent* BestHost = ResolveBestHost(ViewLocation, ViewDirection, bScannerModeActive);
	if (!BestHost)
	{
		ClearWorldUI(false);
		return;
	}

	AWorldUIActor* UIActor = EnsureUIActorForHost(BestHost);
	if (!UIActor)
	{
		ClearWorldUI(false);
		return;
	}

	const bool bHostChanged = ActiveHostComponent.Get() != BestHost;
	ActiveHostComponent = BestHost;
	UIActor->SetHostComponent(BestHost);
	UIActor->SetNeutralFacingRotation(BestHost->GetPanelWorldRotation());

	TimeUntilModelRefresh = bHostChanged
		? 0.0f
		: FMath::Max(0.0f, TimeUntilModelRefresh - FMath::Max(0.0f, DeltaTime));

	if (bHostChanged || TimeUntilModelRefresh <= KINDA_SMALL_NUMBER)
	{
		if (!BestHost->BuildWorldUIModel(ActiveModel))
		{
			ClearWorldUI(false);
			return;
		}

		TimeUntilModelRefresh = FMath::Max(0.01f, ModelRefreshInterval);
	}

	UIActor->UpdateWorldUI(
		ActiveModel,
		BestHost->GetPanelWorldLocation(),
		BestHost->GetLeaderAnchorWorldLocation(),
		ViewLocation,
		1.0f,
		true);

	UWorld* World = GetWorld();
	APlayerController* PlayerController = World ? World->GetFirstPlayerController() : nullptr;
	if (!World || !PlayerController || !BestHost->bAllowInteraction)
	{
		UIActor->ResetButtonInteractionState();
		return;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(WorldUIButtonTrace), false);
	if (AActor* OwnerActor = GetOwner())
	{
		QueryParams.AddIgnoredActor(OwnerActor);
	}

	const FVector TraceEnd = ViewLocation + (ViewDirection.GetSafeNormal() * FMath::Max(100.0f, InteractionTraceDistance));
	TArray<FHitResult> HitResults;
	World->LineTraceMultiByChannel(
		HitResults,
		ViewLocation,
		TraceEnd,
		static_cast<ECollisionChannel>(InteractionTraceChannel.GetValue()),
		QueryParams);

	AActor* HitButtonActor = nullptr;
	for (const FHitResult& HitResult : HitResults)
	{
		if (UIActor->ResolveButtonFromHitActor(HitResult.GetActor()))
		{
			HitButtonActor = HitResult.GetActor();
			break;
		}
	}

	const bool bClickHeld = PlayerController->IsInputKeyDown(EKeys::LeftMouseButton);
	UIActor->UpdateButtonInteractionFromTrace(HitButtonActor, true, bClickHeld);
}
