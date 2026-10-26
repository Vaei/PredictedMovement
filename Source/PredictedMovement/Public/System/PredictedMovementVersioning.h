// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"

#include "Misc/EngineVersionComparison.h"

#ifndef UE_5_03_OR_LATER
#if !UE_VERSION_OLDER_THAN(5, 3, 0)
#define UE_5_03_OR_LATER 1
#else
#define UE_5_03_OR_LATER 0
#endif
#endif

#ifndef UE_5_04_OR_LATER
#if !UE_VERSION_OLDER_THAN(5, 4, 0)
#define UE_5_04_OR_LATER 1
#else
#define UE_5_04_OR_LATER 0
#endif
#endif

#ifndef UE_5_05_OR_LATER
#if !UE_VERSION_OLDER_THAN(5, 5, 0)
#define UE_5_05_OR_LATER 1
#else
#define UE_5_05_OR_LATER 0
#endif
#endif

#ifndef UE_5_06_OR_LATER
#if !UE_VERSION_OLDER_THAN(5, 6, 0)
#define UE_5_06_OR_LATER 1
#else
#define UE_5_06_OR_LATER 0
#endif
#endif

#ifndef UE_5_07_OR_LATER
#if !UE_VERSION_OLDER_THAN(5, 7, 0)
#define UE_5_07_OR_LATER 1
#else
#define UE_5_07_OR_LATER 0
#endif
#endif

#ifndef UE_5_08_OR_LATER
#if !UE_VERSION_OLDER_THAN(5, 8, 0)
#define UE_5_08_OR_LATER 1
#else
#define UE_5_08_OR_LATER 0
#endif
#endif