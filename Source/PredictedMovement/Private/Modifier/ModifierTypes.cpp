﻿// Copyright (c) 2023 Jared Taylor. All Rights Reserved.


#include "Modifier/ModifierTypes.h"

#include "Modifier/ModifierCharacter.h"
#include "Algo/Accumulate.h"
#include "Algo/MaxElement.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModifierTypes)

FMovementModifier& FMovementModifier::operator<<(const FMovementModifier& Clone)
{
	RequestedModifierLevel = Clone.RequestedModifierLevel;
	Modifiers = Clone.Modifiers;
	return *this;
}

uint8 FMovementModifier::GetNumModifiersByLevel(uint8 Level) const
{
	// Did you call initialize?
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

void FMovementModifier::Initialize(AModifierCharacter* InCharacterOwner, const FGameplayTag& InModifierType)
{
	CharacterOwner = InCharacterOwner;
	ModifierType = InModifierType;

	// Only if we're using gameplay tag levels instead of enum levels
	if (LevelType == EModifierLevelType::FGameplayTag)
	{
		if (ensureAlwaysMsgf(ModifierLevelTags.Num() > 0, TEXT("Modifier %s has no levels set"), *ModifierType.ToString()))
		{
			ModifierLevelTags.GetGameplayTagArray(ModifierLevels);
			ModifierLevels.Insert(FGameplayTag::EmptyTag, 0);  // Level 0
		}
	}
}

void FMovementModifier::AddModifier(uint8 Level)
{
	if (!ensureAlwaysMsgf(HasInitialized(), TEXT("Modifier %s has not been initialized"), *ModifierType.ToString()))
	{
		return;
	}
	
	if (!HasValidData())
	{
		return;
	}

	if (!ensureAlwaysMsgf(MaxModifiers > 1, TEXT("Cannot apply a modifier %s with max modifiers set to 0, use SetModifierLevel instead"), *ModifierType.ToString()))
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

void FMovementModifier::RemoveModifier(uint8 Level)
{
	if (!ensureAlwaysMsgf(HasInitialized(), TEXT("Modifier %s has not been initialized"), *ModifierType.ToString()))
	{
		return;
	}
	
	if (!HasValidData())
	{
		return;
	}
	
	if (!ensureAlwaysMsgf(MaxModifiers > 1, TEXT("Cannot remove a modifier %s with max modifiers set to 0, use SetModifierLevel instead"), *ModifierType.ToString()))
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

void FMovementModifier::RemoveAllModifiers()
{
	if (!ensureAlwaysMsgf(HasInitialized(), TEXT("Modifier %s has not been initialized"), *ModifierType.ToString()))
	{
		return;
	}
	
	if (!ensureAlwaysMsgf(MaxModifiers > 1, TEXT("Cannot remove a modifier %s with max modifiers set to 0, use SetModifierLevel instead"), *ModifierType.ToString()))
	{
		return;
	}
	
	if (Modifiers.Num() > 0)
	{
		Modifiers.Reset();
		OnModifiersChanged();
	}
}

void FMovementModifier::RemoveAllModifiersByLevel(uint8 Level)
{
	if (!ensureAlwaysMsgf(HasInitialized(), TEXT("Modifier %s has not been initialized"), *ModifierType.ToString()))
	{
		return;
	}
	
	if (!ensureAlwaysMsgf(MaxModifiers > 1, TEXT("Cannot remove a modifier %s with max modifiers set to 0, use SetModifierLevel instead"), *ModifierType.ToString()))
	{
		return;
	}
	
	if (GetNumModifiersByLevel(Level) > 0)
	{
		Modifiers.Remove(Level);
		OnModifiersChanged();
	}
}

void FMovementModifier::RemoveAllModifiersExceptLevel(uint8 Level)
{
	if (!ensureAlwaysMsgf(HasInitialized(), TEXT("Modifier %s has not been initialized"), *ModifierType.ToString()))
	{
		return;
	}
	
	if (!ensureAlwaysMsgf(MaxModifiers > 1, TEXT("Cannot remove a modifier %s with max modifiers set to 0, use SetModifierLevel instead"), *ModifierType.ToString()))
	{
		return;
	}
	
	Modifiers.RemoveAll([Level](uint8 ModifierLevel)
	{
		return ModifierLevel != Level;
	});
}

void FMovementModifier::SetModifiers(const TArray<uint8>& NewModifiers)
{
	if (!ensureAlwaysMsgf(HasInitialized(), TEXT("Modifier %s has not been initialized"), *ModifierType.ToString()))
	{
		return;
	}
	
	if (!ensureAlwaysMsgf(MaxModifiers > 1, TEXT("Cannot set modifiers %s with max modifiers set to 0, use SetModifierLevel instead"), *ModifierType.ToString()))
	{
		return;
	}
	
	if (Modifiers != NewModifiers)
	{
		Modifiers = NewModifiers;
		OnModifiersChanged();
	}
}

void FMovementModifier::SetModifierLevel(uint8 Level)
{
	if (!ensureAlwaysMsgf(HasInitialized(), TEXT("Modifier %s has not been initialized"), *ModifierType.ToString()))
	{
		return;
	}
	
	if (!HasValidData())
	{
		return;
	}
	
	if (Level != RequestedModifierLevel)
	{
		RequestedModifierLevel = Level;
	}
}

void FMovementModifier::OnModifiersChanged()
{
	if (MaxModifiers <= 1)
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

void FMovementModifier::StartModifier(uint8 Level, bool bCanApplyModifier, bool bClientSimulation)
{
	if (!HasValidData())
	{
		return;
	}
	
	if (!bClientSimulation && !bCanApplyModifier)
	{
		return;
	}
	
	const uint8 PrevLevel = ModifierLevel;
	if (!bClientSimulation)
	{
		ModifierLevel = Level;
	}
	CharacterOwner->OnModifierChanged(ModifierType, Level, PrevLevel);
}

void FMovementModifier::EndModifier(bool bClientSimulation)
{
	if (!HasValidData())
	{
		return;
	}
	
	if (!bClientSimulation)
	{
		ModifierLevel = 0;
	}
	CharacterOwner->OnModifierChanged(ModifierType, 0, ModifierLevel);
}

void FMovementModifier::UpdateCharacterStateBeforeMovement(bool bCanApplyModifier)
{
	if (!HasValidData())
	{
		return;
	}
	
	if (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		if (HasModifier() && (!WantsModifier() || !bCanApplyModifier))
		{
			EndModifier();
		}
		else if (!HasModifier() && WantsModifier() && bCanApplyModifier)
		{
			StartModifier(RequestedModifierLevel, bCanApplyModifier);
		}
	}
}

void FMovementModifier::UpdateCharacterStateAfterMovement(bool bCanApplyModifier)
{
	if (!HasValidData())
	{
		return;
	}
	
	if (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		if (HasModifier() && !bCanApplyModifier)
		{
			EndModifier();
		}
	}
}

bool FMovementModifier::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	// Serialize the modifier level
	SerializeOptionalValue<uint8>(Ar.IsSaving(), Ar, RequestedModifierLevel, 0);

	// Don't serialize modifier stack if the max is 0
	if (MaxModifiers <= 1)
	{
		return !Ar.IsError();
	}
	
	// Serialize the number of elements
	uint8 NumModifiers = Modifiers.Num();
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
	for (uint8 i = 0; i < NumModifiers; ++i)
	{
		Ar << Modifiers[i];
	}

	return !Ar.IsError();
}

bool FClientAuthData::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	Ar << Alpha;
	
	return !Ar.IsError();
}

bool FClientAuthStack::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	// Serialize the number of elements
	static constexpr uint8 MaxStack = 8;

	uint8 StackNum = Stack.Num();
	if (Ar.IsSaving())
	{
		StackNum = FMath::Min(MaxStack, StackNum);
	}
	Ar << StackNum;
	
	// Resize the array if needed
	if (Ar.IsLoading())
	{
		if (!ensureMsgf(StackNum <= MaxStack,
			TEXT("Deserializing client auth stack array with %d elements when max is %d -- Check packet serialization logic"), StackNum, MaxStack))
		{
			StackNum = MaxStack;
		}
		Stack.SetNum(StackNum);
	}

	// Serialize the elements
	for (uint8 i = 0; i < StackNum; ++i)
	{
		Stack[i].NetSerialize(Ar, Map, bOutSuccess);
	}

	return !Ar.IsError();
}
