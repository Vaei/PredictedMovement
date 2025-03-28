// Copyright (c) 2023 Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "PredTypes.generated.h"

UENUM(BlueprintType)
enum class EPredGaitMode : uint8
{
	Stroll,
	Walk,
	Run,
	Sprint,
};

UENUM(BlueprintType)
enum class EPredStance : uint8
{
	Stand,
	Crouch,
	Prone,
};