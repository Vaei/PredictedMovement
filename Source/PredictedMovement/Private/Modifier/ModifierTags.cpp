// Copyright (c) Jared Taylor


#include "Modifier/ModifierTags.h"


namespace FModifierTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Modifier_Boost,		"Modifier.Boost", "Increase Movement Speed");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Modifier_SlowFall,	"Modifier.SlowFall", "Reduce Gravity to a % of its normal value");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Modifier_Snare,		"Modifier.Snare", "Reduced Movement Speed applied by server");

	UE_DEFINE_GAMEPLAY_TAG(ClientAuth_Snare,	"ClientAuth.Snare");
}