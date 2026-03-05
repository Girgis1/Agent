// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Factory/ResourceActor.h"
#include "ResourceSalvageActor.generated.h"

UCLASS(meta=(DeprecatedNode, DeprecationMessage="Use AResourceActor instead."))
class AResourceSalvageActor : public AResourceActor
{
	GENERATED_BODY()

public:
	AResourceSalvageActor();
};
