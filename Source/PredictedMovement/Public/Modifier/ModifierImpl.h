// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ModifierTypes.h"

// UINT8_MAX is NO_MODIFIER, so UINT8_MAX-1 is the max for uint8 -- NO_MODIFIER is defined in ModifierTypes.h
using TModSize = uint8;  // If you want more than 254 modifiers, change this to uint16 or uint32
using TModifierStack = TArray<TModSize>;

/**
 * FSavedMove_Character
 */
struct PREDICTEDMOVEMENT_API FModifierSavedMove
{
	TModifierStack WantsModifiers;

	FModifierSavedMove()
	{}
	
	virtual ~FModifierSavedMove() = default;

	virtual void Clear()
	{
		WantsModifiers.Empty();
	}

	void SetMoveFor(const TModifierStack& Modifiers)
	{
		WantsModifiers = Modifiers;
	}

	bool CanCombineWith(const TModifierStack& Modifiers) const
	{
		return WantsModifiers == Modifiers;
	}

	void SetInitialPosition(const TModifierStack& Modifiers)
	{
		WantsModifiers = Modifiers;
	}

	bool IsImportantMove(const TModifierStack& Modifiers) const
	{
		return WantsModifiers != Modifiers;
	}
};

/**
 * FSavedMove_Character
 */
struct PREDICTEDMOVEMENT_API FModifierSavedMove_WithCorrection final : FModifierSavedMove
{
	using Super = FModifierSavedMove;
	
	TModifierStack Modifiers;

	FModifierSavedMove_WithCorrection()
	{}

	virtual void Clear() override
	{
		Super::Clear();
		Modifiers.Empty();
	}

	void PostUpdate(const TModifierStack& InModifiers)
	{
		Modifiers = InModifiers;
	}
};

/**
 * FSavedMove_Character
 */
struct PREDICTEDMOVEMENT_API FModifierSavedMove_ServerInitiated
{
	TModifierStack Modifiers;

	FModifierSavedMove_ServerInitiated()
	{}

	void Clear()
	{
		Modifiers.Empty();
	}

	void PostUpdate(const TModifierStack& InModifiers)
	{
		Modifiers = InModifiers;
	}
};

/**
 * FCharacterMoveResponseDataContainer
 * Only required when using WithCorrection or ServerInitiated modifiers
 */
struct PREDICTEDMOVEMENT_API FModifierMoveResponse
{
	TModifierStack Modifiers;

	void ServerFillResponseData(const TModifierStack& InModifiers)
	{
		Modifiers = InModifiers;
	}
};

/**
 * FCharacterNetworkMoveData
 * Sends wanted modifiers (via input) to the server to be applied to the character
 */
struct PREDICTEDMOVEMENT_API FModifierMoveData_LocalPredicted
{
	FModifierMoveData_LocalPredicted()
	{}
	
	TModifierStack WantsModifiers;

	void ClientFillNetworkMoveData(const TModifierStack& InWantsModifiers)
	{
		WantsModifiers = InWantsModifiers;
	}

	bool Serialize(FArchive& Ar, const FString& ErrorName, uint8 MaxSerializedModifiers=8);
};

/**
 * FCharacterNetworkMoveData
 * Sends wanted modifiers (via input) to the server to be applied to the character
 * Server compares the client and server modifiers to know when to send a net correction to the client with updated modifiers
 */
struct PREDICTEDMOVEMENT_API FModifierMoveData_WithCorrection
{
	FModifierMoveData_WithCorrection()
	{}

	TModifierStack WantsModifiers;
	TModifierStack Modifiers;

	void ClientFillNetworkMoveData(const TModifierStack& InWantsModifiers, const TModifierStack& InModifiers)
	{
		WantsModifiers = InWantsModifiers;
		Modifiers = InModifiers;
	}

	bool Serialize(FArchive& Ar, const FString& ErrorName, uint8 MaxSerializedModifiers=8);
};

/**
 * FCharacterNetworkMoveData
 * Used by server to compare between client and server, to know when to send a net correction to the client with updated modifiers
 */
struct PREDICTEDMOVEMENT_API FModifierMoveData_ServerInitiated
{
	FModifierMoveData_ServerInitiated()
	{}
	
	TModifierStack Modifiers;

	void ClientFillNetworkMoveData(const TModifierStack& InModifiers)
	{
		Modifiers = InModifiers;
	}

	bool Serialize(FArchive& Ar, const FString& ErrorName, uint8 MaxSerializedModifiers=8);
};

/**
 * Represents a single modifier that can be applied to a character
 * This is the base class for all modifiers, which can be local predicted, with correction, or server initiated
 */
struct PREDICTEDMOVEMENT_API FMovementModifier
{
	/** The requested input state, which requests modifiers of the specified level */
	TModifierStack WantsModifiers;
	
	/** The actual state, which represents the actual modifiers applied to the character */
	TModifierStack Modifiers;
	
	/**
	 * Adds a modifier to the stack
	 * @param Level The level of the modifier to add
	 * @return True if the modifier was added
	 */
	bool AddModifier(TModSize Level)
	{
		WantsModifiers.Add(Level);
		return true;
	}

	/**
	 * Removes a modifier from the stack
	 * @param Level The level of the modifier to remove
	 * @param bRemoveAll If true, removes all modifiers of the specified level, otherwise removes only one
	 * @return True if the modifier was removed, false otherwise
	 */
	bool RemoveModifier(TModSize Level, bool bRemoveAll)
	{
		if (WantsModifiers.Contains(Level))
		{
			if (bRemoveAll)
			{
				WantsModifiers.Remove(Level);
			}
			else
			{
				WantsModifiers.RemoveSingle(Level);
			}
			return true;
		}
		return false;
	}

	/**
	 * Removes all modifiers from the stack
	 * @return True if any modifiers were removed, false otherwise
	 */
	bool ResetModifiers()
	{
		if (WantsModifiers.Num() > 0)
		{
			WantsModifiers.Reset();
			return true;
		}
		return false;
	}

	/**
	 * Returns the number of wanted modifiers in the stack that match the specified level
	 * This is the requested modifiers, not the actual modifiers applied to the character
	 * @param Level The level to filter by
	 * @return The number of modifiers that match the specified level
	 */
	TModSize GetNumWantedModifiersByLevel(TModSize Level) const;

	/**
	 * Returns the number of modifiers in the stack that match the specified level
	 * This is the actual modifiers applied to the character, not the requested ones
	 * @param Level The level to filter by
	 * @return The number of modifiers that match the specified level
	 */
	TModSize GetNumModifiersByLevel(TModSize Level) const;

	/**
	 * Limits the number of modifiers in the stack to the specified maximum
	 * Supports limiting between different types of modifiers affecting the same type of movement, e.g. BoostLocal and BoostCorrection
	 */
	static void LimitNumModifiers(TModifierStack& Modifiers, int32& RemainingModifiers);

	/** Applies WantsModifiers to Modifiers based on the current state of the character */
	bool UpdateMovementState(bool bAllowedInCurrentState, bool bClampMax, int32& Remaining);
};

/**
 * Represents a single modifier that can be applied to a character
 * Local Predicted modifier is activated via player input and is predicted on the client
 * e.g. Sprint, Crouch, etc.
 */
struct PREDICTEDMOVEMENT_API FMovementModifier_LocalPredicted : FMovementModifier
{
	FMovementModifier_LocalPredicted()
	{}

	void ServerMove_PerformMovement(const TModifierStack& InWantsModifiers)
	{
		WantsModifiers = InWantsModifiers;
	}

	void CombineWith(const TModifierStack& InWantsModifiers)
	{
		WantsModifiers = InWantsModifiers;
	}
};

/**
 * Represents a single modifier that can be applied to a character
 * 
 * Local Predicted modifier is activated via a predicted state (GAS) and is predicted on the client
 * However, if the server disagrees with the client, it will correct the client
 *
 * Alternatively, initiated by the server and sent to the client
 * 
 * e.g. Speed increase after equipping a knife via predicted inventory ability, etc.
 */
struct PREDICTEDMOVEMENT_API FMovementModifier_WithCorrection final : FMovementModifier_LocalPredicted
{
	bool ServerCheckClientError(const TModifierStack& InModifiers) const
	{
		return Modifiers != InModifiers;
	}

	void OnClientCorrectionReceived(const TModifierStack& InModifiers)
	{
		WantsModifiers = InModifiers;
	}
};

/**
 * Static functions for modifiers
 */
struct PREDICTEDMOVEMENT_API FModifierStatics
{
	/**
	 * Serializes the modifier stack to the archive
	 * @param Modifiers The modifier stack to serialize
	 * @param Ar The archive to serialize to
	 * @param ErrorName The name of the Modifier to report if serialization fails
	 * @param MaxSerializedModifiers The maximum number of modifiers to serialize (default is 8)
	 * @return True if serialization was successful, false otherwise
	 */
	static bool NetSerialize(TModifierStack& Modifiers, FArchive& Ar, const FString& ErrorName, uint8 MaxSerializedModifiers=8);

	/**
	 * Updates the modifier level based on the specified method
	 * @param Method The method to use for updating the modifier level
	 * @param Modifiers The stack of modifiers to update
	 * @param MaxLevel The maximum level of modifiers
	 * @param InvalidLevel The level to return if no valid modifiers are found
	 * @return The updated modifier level
	 */
	static TModSize UpdateModifierLevel(EModifierLevelMethod Method, const TModifierStack& Modifiers, TModSize MaxLevel, TModSize InvalidLevel);

	/**
	 * Combines multiple modifier levels into a single level based on the specified method
	 * @param Method The method to use for combining the modifier levels
	 * @param ModifierLevels The stack of modifier levels to combine
	 * @param MaxLevel The maximum level of modifiers
	 * @param InvalidLevel The level to return if no valid modifiers are found
	 * @return The combined modifier level
	 */
	static TModSize CombineModifierLevels(EModifierLevelMethod Method, const TModifierStack& ModifierLevels, TModSize MaxLevel, TModSize InvalidLevel);

	/**
	 * Processes modifiers based on the specified method and updates the current level
	 * @param CurrentLevel The current modifier level to update
	 * @param Method The method to use for processing modifiers
	 * @param LevelTags The tags representing the levels of modifiers
	 * @param bLimitMaxModifiers Whether to limit the maximum number of modifiers
	 * @param MaxModifiers The maximum number of modifiers allowed
	 * @param InvalidLevel The level to return if no valid modifiers are found
	 * @param Modifiers The array of modifiers to process
	 * @param CanActivateCallback Callback to determine if the modifier can be activated
	 * @return True if the current level changed, false otherwise
	 */
	static bool ProcessModifiers(TModSize& CurrentLevel, EModifierLevelMethod Method, const TArray<FGameplayTag>& LevelTags,
		bool bLimitMaxModifiers, int32 MaxModifiers, TModSize InvalidLevel,	const TArray<FMovementModifier*>& Modifiers,
		const TFunctionRef<bool()>& CanActivateCallback);
};