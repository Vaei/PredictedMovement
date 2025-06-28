// Copyright (c) Jared Taylor


#include "Modifier/ModifierImpl.h"
#include "Algo/Accumulate.h"
#include "Algo/MaxElement.h"
#include "Algo/MinElement.h"

bool FModifierMoveData_LocalPredicted::Serialize(FArchive& Ar, const FString& ErrorName,
	uint8 MaxSerializedModifiers)
{
	return FModifierStatics::NetSerialize(WantsModifiers, Ar, ErrorName, MaxSerializedModifiers);
}

bool FModifierMoveData_WithCorrection::Serialize(FArchive& Ar, const FString& ErrorName,
	uint8 MaxSerializedModifiers)
{
	return FModifierStatics::NetSerialize(WantsModifiers, Ar, ErrorName, MaxSerializedModifiers) &&
		FModifierStatics::NetSerialize(Modifiers, Ar, ErrorName, MaxSerializedModifiers);
}

bool FModifierMoveData_ServerInitiated::Serialize(FArchive& Ar, const FString& ErrorName, uint8 MaxSerializedModifiers)
{
	return FModifierStatics::NetSerialize(Modifiers, Ar, ErrorName, MaxSerializedModifiers);
}

void FMovementModifier::LimitNumModifiers(TModifierStack& Modifiers, int32& RemainingModifiers)
{
	if (Modifiers.Num() > RemainingModifiers)
	{
		// Limit the number of modifiers to the maximum allowed
		if (RemainingModifiers <= 0)
		{
			// If MaxModifiers is 0 or less, we can't have any modifiers
			Modifiers.Reset();
		}
		else
		{
			// Remove the oldest entries (from the start)
			const int32 NumToRemove = Modifiers.Num() - RemainingModifiers;
			Modifiers.RemoveAt(0, NumToRemove, EAllowShrinking::No);
		}
	}

	RemainingModifiers = FMath::Max(RemainingModifiers - Modifiers.Num(), 0);
}

bool FModifierStatics::NetSerialize(TModifierStack& Modifiers, FArchive& Ar, const FString& ErrorName, uint8 MaxSerializedModifiers)
{
	// Don't serialize modifier stack if the max is 0
	if (MaxSerializedModifiers <= 1)
	{
		return !Ar.IsError();
	}
	
	// Serialize the number of elements
	TModSize NumModifiers = Modifiers.Num();
	if (Ar.IsSaving())
	{
		NumModifiers = FMath::Min(MaxSerializedModifiers, NumModifiers);
	}
	Ar << NumModifiers;

	// Resize the array if needed
	if (Ar.IsLoading())
	{
		if (!ensureMsgf(NumModifiers <= MaxSerializedModifiers,
			TEXT("Deserializing modifier %s array with %d elements when max is %d -- Check packet serialization logic"), *ErrorName, NumModifiers, MaxSerializedModifiers))
		{
			NumModifiers = MaxSerializedModifiers;
		}
		Modifiers.SetNum(NumModifiers);
	}

	// Serialize the elements
	for (TModSize i = 0; i < NumModifiers; ++i)
	{
		Ar << Modifiers[i];
	}

	return !Ar.IsError();
}

TModSize FModifierStatics::UpdateModifierLevel(EModifierLevelMethod Method, const TModifierStack& Modifiers,
	TModSize MaxLevel, TModSize InvalidLevel)
{
	if (Modifiers.IsEmpty())
	{
		return InvalidLevel;
	}

	TModSize NewLevel = 0;

	switch (Method)
	{
	case EModifierLevelMethod::Max:
		NewLevel = *Algo::MaxElement(Modifiers);
		break;

	case EModifierLevelMethod::Min:
		NewLevel = *Algo::MinElement(Modifiers);
		break;

	case EModifierLevelMethod::Stack:
		for (const TModSize& Level : Modifiers)
		{
			// We are only counting the number of modifiers, so we add 1 because levels are 0-based
			NewLevel += Level + 1;
		}
		break;

	case EModifierLevelMethod::Average:
		{
			uint64 Total = 0;
			for (const TModSize& Level : Modifiers)
			{
				// We don't add 1 here because we are averaging the levels, not counting them
				Total += Level;
			}
			NewLevel = static_cast<TModSize>(Total / Modifiers.Num());
		}
		break;

	default:
		return InvalidLevel;
	}

	// Clamp to max allowed
	return FMath::Min(NewLevel, MaxLevel);
}

TModSize FModifierStatics::CombineModifierLevels(EModifierLevelMethod Method, const TModifierStack& ModifierLevels,
	TModSize MaxLevel, TModSize InvalidLevel)
{
	if (ModifierLevels.IsEmpty())
	{
		return InvalidLevel;
	}

	TModSize NewLevel = 0;

	switch (Method)
	{
	case EModifierLevelMethod::Max:
		NewLevel = *Algo::MaxElement(ModifierLevels);
		break;

	case EModifierLevelMethod::Min:
		NewLevel = *Algo::MinElement(ModifierLevels);
		break;

	case EModifierLevelMethod::Stack:
		for (const TModSize& Level : ModifierLevels)
		{
			NewLevel += Level;
		}
		break;

	case EModifierLevelMethod::Average:
		{
			uint64 Total = 0;
			for (const TModSize& Level : ModifierLevels)
			{
				Total += Level;
			}
			NewLevel = static_cast<TModSize>(Total / ModifierLevels.Num());
		}
		break;

	default:
		return InvalidLevel;
	}

	// Clamp to max allowed
	return FMath::Min(NewLevel, MaxLevel);
}
