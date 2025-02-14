// Copyright (c) 2023 Jared Taylor. All Rights Reserved.

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