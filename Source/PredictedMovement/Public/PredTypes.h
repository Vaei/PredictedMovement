// Copyright (c) Jared Taylor

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

/** How AimDownSights changes movement properties */
UENUM(BlueprintType)
enum class EPredAimDownSightsMode : uint8
{
	Scalar		UMETA(ToolTip="Multiply movement properties by the AimingDownSights scalars"),
	Gait		UMETA(ToolTip="Restrict the gait mode while AimingDownSights, the scalars are not applied"),
};

UENUM(BlueprintType)
enum class EPredStance : uint8
{
	Stand,
	Crouch,
	Prone,
};

UENUM(BlueprintType)
enum class EPredTerminalVelocityMode : uint8
{
	Default				UMETA(ToolTip="Get the terminal velocity from the current or default physics volume"),
	DirectSet			UMETA(ToolTip="Set the terminal velocity directly on the character movement component"),
};