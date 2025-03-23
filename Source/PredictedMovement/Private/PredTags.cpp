// Copyright (c) 2023 Jared Taylor

#include "PredTags.h"


namespace FPredTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Modifier_Type_Local_SlowFall,	"Modifier.Type.Local.SlowFall", "Reduce Gravity to a % of its normal value");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Modifier_Type_Local_Boost,		"Modifier.Type.Local.Boost", "Increase Movement Speed");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Modifier_Type_Local_Haste,		"Modifier.Type.Local.Haste", "Increase Movement Speed but only while Sprinting");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Modifier_Type_Server_Slow,		"Modifier.Type.Server.Slow", "Reduce Movement Speed");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Modifier_Type_Server_Snare,		"Modifier.Type.Server.Snare", "Reduced Movement Speed applied by server");

	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Local_SlowFall_Test,		"Modifier.Type.Local.SlowFall.Test");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Local_Boost_Test,			"Modifier.Type.Local.Boost.Test");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Local_Haste_Test,			"Modifier.Type.Local.Haste.Test");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Server_Slow_Test,			"Modifier.Type.Server.Slow.Test");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Server_Snare_Test,			"Modifier.Type.Server.Snare.Test");
}