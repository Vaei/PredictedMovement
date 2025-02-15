// Copyright (c) 2023 Jared Taylor

#include "PredTags.h"


namespace FPredTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Modifier_Type_Buff_SlowFall,		"Modifier.Type.Buff.SlowFall", "Reduce Gravity to a % of its normal value");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Modifier_Type_Buff_Boost,		"Modifier.Type.Buff.Boost", "Increase Movement Speed");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Modifier_Type_Buff_Haste,		"Modifier.Type.Buff.Haste", "Increase Movement Speed but only while Sprinting");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Modifier_Type_Debuff_Slow,		"Modifier.Type.Debuff.Slow", "Reduce Movement Speed");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Modifier_Type_Debuff_Snare,		"Modifier.Type.Debuff.Snare", "Reduced Movement Speed applied by server");

	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Buff_SlowFall_Test,		"Modifier.Type.Buff.SlowFall.Test");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Buff_Boost_Test,			"Modifier.Type.Buff.Boost.Test");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Buff_Haste_Test,			"Modifier.Type.Buff.Haste.Test");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Debuff_Slow_Test,			"Modifier.Type.Debuff.Slow.Test");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Debuff_Snare_Test,			"Modifier.Type.Debuff.Snare.Test");
}