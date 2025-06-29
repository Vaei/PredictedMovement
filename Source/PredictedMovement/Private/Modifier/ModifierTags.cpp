// Copyright (c) Jared Taylor


#include "Modifier/ModifierTags.h"


namespace FModifierTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Modifier_Boost,		"Modifier.Boost", "Increase Movement Speed");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Modifier_Haste,		"Modifier.Haste", "Increase Movement Speed but only while Sprinting");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Modifier_Slow,		"Modifier.Slow", "Reduce Movement Speed");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Modifier_SlowFall,	"Modifier.SlowFall", "Reduce Gravity to a % of its normal value");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Modifier_Snare,		"Modifier.Snare", "Reduced Movement Speed applied by server");

	// You may want to fork and delete these, they are used for demo content
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Boost_Test_Level1, "Modifier.Boost.Test.Level1");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Boost_Test_Level2, "Modifier.Boost.Test.Level2");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Boost_Test_Level3, "Modifier.Boost.Test.Level3");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Boost_Test_Level4, "Modifier.Boost.Test.Level4");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Boost_Test_Level5, "Modifier.Boost.Test.Level5");
	
	UE_DEFINE_GAMEPLAY_TAG(ClientAuth_Snare,	"ClientAuth.Snare");
}