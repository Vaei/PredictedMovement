// Copyright (c) 2023 Jared Taylor. All Rights Reserved.


#include "Modifier/ModifierTypes.h"

#include "Modifier/ModifierCharacter.h"
#include "Algo/Accumulate.h"
#include "Algo/MaxElement.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModifierTypes)

namespace FModifierTags
{
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Debuff_Snare, "Modifier.Type.Debuff.Snare");
}

int32 FModifierData::GetNumModifiersByLevel(uint8 Level) const
{
	if (!ensureAlwaysMsgf(ModifierType.IsValid(), TEXT("Modifier type %s is not valid"), *ModifierType.ToString()))
	{
		return 0;
	}
	
	// Can't apply a modifier of level 0 in the first place
	if (!ensureAlways(Level > 0))
	{
		return 0;
	}
	
	return Modifiers.FilterByPredicate([Level](uint8 ModifierLevel)
	{
		return ModifierLevel == Level;
	}).Num();
}

void FModifierData::Initialize(AModifierCharacter* InCharacterOwner)
{
	CharacterOwner = InCharacterOwner;
	bHasInitialized = true;
}

void FModifierData::AddModifier(uint8 Level)
{
	if (!ensureAlwaysMsgf(ModifierType.IsValid(), TEXT("Modifier type %s is not valid"), *ModifierType.ToString()))
	{
		return;
	}
	
	if (!ensureAlwaysMsgf(bHasInitialized, TEXT("Modifier %s has not been initialized"), *ModifierType.ToString()))
	{
		return;
	}
	
	if (!CharacterOwner.IsValid())
	{
		return;
	}

	if (ensureAlwaysMsgf(MaxModifiers > 0, TEXT("Cannot apply a modifier %s with max modifiers set to 0, use SetModifierLevel instead"), *ModifierType.ToString()))
	{
		return;
	}
	
	if (!ensureAlwaysMsgf(Level > 0, TEXT("Cannot apply a modifier %s of level 0"), *ModifierType.ToString()))
	{
		return;
	}

	// Remove the oldest modifier if we're at the max
	if (Modifiers.Num() == MaxModifiers)
	{
		Modifiers.RemoveAt(0);
	}

	// Add the new modifier
	Modifiers.Add(Level);
	OnModifiersChanged();
}

void FModifierData::RemoveModifier(uint8 Level)
{
	if (!ensureAlwaysMsgf(ModifierType.IsValid(), TEXT("Modifier type %s is not valid"), *ModifierType.ToString()))
	{
		return;
	}
	
	if (!ensureAlwaysMsgf(bHasInitialized, TEXT("Modifier %s has not been initialized"), *ModifierType.ToString()))
	{
		return;
	}
	
	if (!CharacterOwner.IsValid())
	{
		return;
	}
	
	if (ensureAlwaysMsgf(MaxModifiers > 0, TEXT("Cannot remove a modifier %s with max modifiers set to 0, use SetModifierLevel instead"), *ModifierType.ToString()))
	{
		return;
	}
	
	if (!ensureAlwaysMsgf(Level > 0, TEXT("Cannot remove a modifier of level 0")))
	{
		return;
	}

	if (Modifiers.Contains(Level))
	{
		Modifiers.RemoveSingle(Level);
		OnModifiersChanged();
	}
}

void FModifierData::RemoveAllModifiers()
{
	if (!ensureAlwaysMsgf(ModifierType.IsValid(), TEXT("Modifier type %s is not valid"), *ModifierType.ToString()))
	{
		return;
	}
	
	if (!ensureAlwaysMsgf(bHasInitialized, TEXT("Modifier %s has not been initialized"), *ModifierType.ToString()))
	{
		return;
	}
	
	if (ensureAlwaysMsgf(MaxModifiers > 0, TEXT("Cannot remove a modifier %s with max modifiers set to 0, use SetModifierLevel instead"), *ModifierType.ToString()))
	{
		return;
	}
	
	if (Modifiers.Num() > 0)
	{
		Modifiers.Reset();
		OnModifiersChanged();
	}
}

void FModifierData::RemoveAllModifiersByLevel(uint8 Level)
{
	if (!ensureAlwaysMsgf(ModifierType.IsValid(), TEXT("Modifier type %s is not valid"), *ModifierType.ToString()))
	{
		return;
	}
	
	if (!ensureAlwaysMsgf(bHasInitialized, TEXT("Modifier %s has not been initialized"), *ModifierType.ToString()))
	{
		return;
	}
	
	if (ensureAlwaysMsgf(MaxModifiers > 0, TEXT("Cannot remove a modifier %s with max modifiers set to 0, use SetModifierLevel instead"), *ModifierType.ToString()))
	{
		return;
	}
	
	if (GetNumModifiersByLevel(Level) > 0)
	{
		Modifiers.Remove(Level);
		OnModifiersChanged();
	}
}

void FModifierData::RemoveAllModifiersExceptLevel(uint8 Level)
{
	if (!ensureAlwaysMsgf(ModifierType.IsValid(), TEXT("Modifier type %s is not valid"), *ModifierType.ToString()))
	{
		return;
	}
	
	if (!ensureAlwaysMsgf(bHasInitialized, TEXT("Modifier %s has not been initialized"), *ModifierType.ToString()))
	{
		return;
	}
	
	if (ensureAlwaysMsgf(MaxModifiers > 0, TEXT("Cannot remove a modifier %s with max modifiers set to 0, use SetModifierLevel instead"), *ModifierType.ToString()))
	{
		return;
	}
	
	Modifiers.RemoveAll([Level](uint8 ModifierLevel)
	{
		return ModifierLevel != Level;
	});
}

void FModifierData::SetModifiers(const TArray<uint8>& NewModifiers)
{
	if (!ensureAlwaysMsgf(ModifierType.IsValid(), TEXT("Modifier type %s is not valid"), *ModifierType.ToString()))
	{
		return;
	}
	
	if (ensureAlwaysMsgf(MaxModifiers > 0, TEXT("Cannot set modifiers %s with max modifiers set to 0, use SetModifierLevel instead"), *ModifierType.ToString()))
	{
		return;
	}
	
	if (Modifiers != NewModifiers)
	{
		Modifiers = NewModifiers;
		OnModifiersChanged();
	}
}

void FModifierData::SetModifierLevel(uint8 Level)
{
	if (!ensureAlwaysMsgf(ModifierType.IsValid(), TEXT("Modifier type %s is not valid"), *ModifierType.ToString()))
	{
		return;
	}
	
	if (!CharacterOwner.IsValid())
	{
		return;
	}
	
	if (Level != ModifierLevel)
	{
		const uint8 PrevLevel = ModifierLevel;
		ModifierLevel = Level;

		CharacterOwner->OnModifierChanged(ModifierType, ModifierLevel, PrevLevel);
	}
}

void FModifierData::OnModifiersChanged()
{
	if (MaxModifiers == 0)
	{
		return;
	}
	
	// Determine modifier level
	uint8 NewLevel = 0;
	switch (LevelMethod)
	{
	case EModifierLevelMethod::Max:
		NewLevel = Modifiers.Num() > 0 ? *Algo::MaxElement(Modifiers) : 0;
		break;
	case EModifierLevelMethod::Stack:
		for (const uint8& Level : Modifiers)
		{
			NewLevel += Level;
		}
		break;
	case EModifierLevelMethod::Average:
		NewLevel = Modifiers.Num() > 0 ? (uint8)(Algo::Accumulate(Modifiers, 0) / Modifiers.Num()) : 0;
		break;
	}
	
	SetModifierLevel(NewLevel);
}

bool FModifierData::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	// Serialize the modifier level
	SerializeOptionalValue<uint8>(Ar.IsSaving(), Ar, ModifierLevel, 0);

	// Don't serialize modifier stack if the max is 0
	if (MaxModifiers == 0)
	{
		return !Ar.IsError();
	}
	
	// Serialize the number of elements
	int32 NumModifiers = Modifiers.Num();
	if (Ar.IsSaving())
	{
		NumModifiers = FMath::Min(MaxModifiers, NumModifiers);
	}
	Ar << NumModifiers;

	// Resize the array if needed
	if (Ar.IsLoading())
	{
		if (!ensureMsgf(NumModifiers <= MaxModifiers,
			TEXT("Deserializing modifier %s array with %d elements when max is %d -- Check packet serialization logic"), *ModifierType.ToString(), NumModifiers, MaxModifiers))
		{
			NumModifiers = MaxModifiers;
		}
		Modifiers.SetNum(NumModifiers);
	}

	// Serialize the elements
	for (int32 i = 0; i < NumModifiers; ++i)
	{
		Ar << Modifiers[i];
	}

	return !Ar.IsError();
}
