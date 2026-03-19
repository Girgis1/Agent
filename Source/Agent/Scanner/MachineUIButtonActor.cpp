// Copyright Epic Games, Inc. All Rights Reserved.

#include "MachineUIButtonActor.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

AMachineUIButtonActor::AMachineUIButtonActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	ButtonVisualRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ButtonVisualRoot"));
	ButtonVisualRoot->SetupAttachment(SceneRoot);

	ButtonMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ButtonMesh"));
	ButtonMesh->SetupAttachment(ButtonVisualRoot);
	ButtonMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ButtonMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
	ButtonMesh->SetGenerateOverlapEvents(false);
	ButtonMesh->SetCastShadow(false);
	ButtonMesh->SetCanEverAffectNavigation(false);

	HoverCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("HoverCollision"));
	HoverCollision->SetupAttachment(SceneRoot);
	HoverCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HoverCollision->SetCollisionObjectType(ECC_WorldDynamic);
	HoverCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	HoverCollision->SetGenerateOverlapEvents(true);
	HoverCollision->SetCanEverAffectNavigation(false);
	HoverCollision->SetBoxExtent(FVector(12.0f, 12.0f, 3.0f));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> DefaultMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (DefaultMesh.Succeeded())
	{
		ButtonMesh->SetStaticMesh(DefaultMesh.Object);
		ButtonMesh->SetRelativeScale3D(FVector(0.12f, 0.12f, 0.025f));
	}
}

void AMachineUIButtonActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (ButtonVisualRoot)
	{
		BaseButtonRelativeLocation = ButtonVisualRoot->GetRelativeLocation();
		BaseButtonRelativeScale = ButtonVisualRoot->GetRelativeScale3D();
	}

	RefreshInteractionCollision();
	UpdateVisualTransform(0.0f);
}

void AMachineUIButtonActor::BeginPlay()
{
	Super::BeginPlay();

	if (ButtonVisualRoot)
	{
		BaseButtonRelativeLocation = ButtonVisualRoot->GetRelativeLocation();
		BaseButtonRelativeScale = ButtonVisualRoot->GetRelativeScale3D();
	}

	if (HoverCollision)
	{
		HoverCollision->OnComponentBeginOverlap.AddUniqueDynamic(this, &AMachineUIButtonActor::HandleHoverCollisionBegin);
		HoverCollision->OnComponentEndOverlap.AddUniqueDynamic(this, &AMachineUIButtonActor::HandleHoverCollisionEnd);
	}

	RefreshInteractionCollision();
	RefreshVisualState();
	UpdateVisualTransform(0.0f);
}

void AMachineUIButtonActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bPressed && PressTimeRemaining > 0.0f)
	{
		PressTimeRemaining = FMath::Max(0.0f, PressTimeRemaining - FMath::Max(0.0f, DeltaSeconds));
		if (PressTimeRemaining <= KINDA_SMALL_NUMBER)
		{
			SetPressed(false);
		}
	}

	if (bAutoHoverFromOverlap)
	{
		HoveringActors.Remove(nullptr);
		SetHovered(HoveringActors.Num() > 0);
	}

	UpdateVisualTransform(DeltaSeconds);
}

void AMachineUIButtonActor::SetHovered(bool bInHovered)
{
	if (bHovered == bInHovered)
	{
		return;
	}

	bHovered = bInHovered;
	RefreshVisualState();
}

void AMachineUIButtonActor::SetPressed(bool bInPressed)
{
	if (bPressed == bInPressed)
	{
		return;
	}

	bPressed = bInPressed;
	RefreshVisualState();
}

void AMachineUIButtonActor::TriggerClick()
{
	SetPressed(true);
	PressTimeRemaining = FMath::Max(0.01f, ClickPressedDuration);
	OnButtonClicked.Broadcast(Command);
}

void AMachineUIButtonActor::SetRayTraceInteractionEnabled(bool bEnabled)
{
	if (bRayTraceInteractionEnabled == bEnabled)
	{
		return;
	}

	bRayTraceInteractionEnabled = bEnabled;
	if (!bRayTraceInteractionEnabled)
	{
		HoveringActors.Reset();
		SetHovered(false);
		SetPressed(false);
	}

	RefreshInteractionCollision();
}

void AMachineUIButtonActor::HandleHoverCollisionBegin(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	(void)OverlappedComponent;
	(void)OtherComp;
	(void)OtherBodyIndex;
	(void)bFromSweep;
	(void)SweepResult;

	if (!bAutoHoverFromOverlap || !OtherActor || OtherActor == this)
	{
		return;
	}

	if (bOnlyHoverPawns && !Cast<APawn>(OtherActor))
	{
		return;
	}

	HoveringActors.Add(OtherActor);
	SetHovered(true);
}

void AMachineUIButtonActor::HandleHoverCollisionEnd(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex)
{
	(void)OverlappedComponent;
	(void)OtherComp;
	(void)OtherBodyIndex;

	if (!bAutoHoverFromOverlap || !OtherActor)
	{
		return;
	}

	HoveringActors.Remove(OtherActor);
	SetHovered(HoveringActors.Num() > 0);
}

void AMachineUIButtonActor::RefreshVisualState()
{
	const EMachineUIButtonVisualState NewState = bPressed
		? EMachineUIButtonVisualState::Pressed
		: (bHovered ? EMachineUIButtonVisualState::Hovered : EMachineUIButtonVisualState::Idle);

	if (CurrentVisualState == NewState)
	{
		return;
	}

	CurrentVisualState = NewState;
	OnVisualStateChanged.Broadcast(CurrentVisualState);
}

void AMachineUIButtonActor::RefreshInteractionCollision()
{
	if (!HoverCollision)
	{
		return;
	}

	if (!bRayTraceInteractionEnabled)
	{
		HoverCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return;
	}

	HoverCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HoverCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
	if (bAutoHoverFromOverlap)
	{
		HoverCollision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	}

	if (bEnableRayTraceHit)
	{
		HoverCollision->SetCollisionResponseToChannel(
			static_cast<ECollisionChannel>(RayTraceChannel.GetValue()),
			ECR_Block);
	}
}

void AMachineUIButtonActor::UpdateVisualTransform(float DeltaSeconds)
{
	if (!ButtonVisualRoot)
	{
		return;
	}

	const float HoverTarget = bHovered ? 1.0f : 0.0f;
	const float PressTarget = bPressed ? 1.0f : 0.0f;

	if (DeltaSeconds <= KINDA_SMALL_NUMBER)
	{
		HoverAlpha = HoverTarget;
		PressAlpha = PressTarget;
	}
	else
	{
		HoverAlpha = FMath::FInterpTo(HoverAlpha, HoverTarget, DeltaSeconds, FMath::Max(0.0f, HoverInterpSpeed));
		PressAlpha = FMath::FInterpTo(PressAlpha, PressTarget, DeltaSeconds, FMath::Max(0.0f, PressInterpSpeed));
	}

	const FVector SafeButtonNormal = ButtonNormalLocal.IsNearlyZero() ? FVector(1.0f, 0.0f, 0.0f) : ButtonNormalLocal.GetSafeNormal();
	const float LiftDistance = FMath::Max(0.0f, HoverLiftDistance) * HoverAlpha;
	const float PressDistance = FMath::Max(0.0f, PressDepthDistance) * PressAlpha;
	const FVector TargetOffset = SafeButtonNormal * (LiftDistance - PressDistance);

	const float HoverScale = FMath::Lerp(1.0f, FMath::Max(0.01f, HoverScaleMultiplier), HoverAlpha);
	const float PressScale = FMath::Lerp(1.0f, FMath::Max(0.01f, PressScaleMultiplier), PressAlpha);
	const FVector TargetScale = BaseButtonRelativeScale * (HoverScale * PressScale);

	ButtonVisualRoot->SetRelativeLocation(BaseButtonRelativeLocation + TargetOffset);
	ButtonVisualRoot->SetRelativeScale3D(TargetScale);
}
