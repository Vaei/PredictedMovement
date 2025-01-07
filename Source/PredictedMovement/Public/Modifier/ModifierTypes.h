// Copyright (c) 2023 Jared Taylor. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "ModifierTypes.generated.h"

class AModifierCharacter;
class UModifierMovement;

#define LEVEL_NONE UINT8_MAX

/**
 * The source of the modifier, such as whether it was applied externally or predicted
 */
UENUM(BlueprintType)
enum class EModifierSource : uint8
{
	External		UMETA(ToolTip="The modifier was applied externally, such as from a server event"),
	Predicted		UMETA(ToolTip="The modifier was applied predictively, such as from a self-activated event"),
};

/**
 * The method used to calculate modifier levels
 */
UENUM(BlueprintType)
enum class EModifierLevelMethod : uint8
{
	Max				UMETA(ToolTip="The highest active modifier level will be applied"),
	Stack			UMETA(ToolTip="Level stacks by each modifier that is added. e.g. add a level 1 modifier and a level 4 modifier, and it applies level 5"),
	Average 		UMETA(ToolTip="The average modifier level will be applied"),
};

/**
 * The data type used to represent levels
 */
UENUM()
enum class EModifierLevelType : uint8
{
	FGameplayTag,
	UEnum,
};

USTRUCT(BlueprintType)
struct PREDICTEDMOVEMENT_API FCommonModifierParams
{
	GENERATED_BODY()

	FCommonModifierParams(float InMaxWalkSpeed = 1.f, float InMaxAcceleration = 1.f, float InBrakingDeceleration = 1.f, float InGroundFriction = 1.f, float InBrakingFriction = 1.f)
		: MaxWalkSpeed(InMaxWalkSpeed)
		, MaxAcceleration(InMaxAcceleration)
		, BrakingDeceleration(InBrakingDeceleration)
		, GroundFriction(InGroundFriction)
		, BrakingFriction(InBrakingFriction)
	{}
	
	/** The maximum ground speed when walking. Also determines maximum lateral speed when falling. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Modifier, meta=(ClampMin="0", UIMin="0", ForceUnits="x"))
	float MaxWalkSpeed;
	
	/** Max Acceleration (rate of change of velocity) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Modifier, meta=(ClampMin="0", UIMin="0", ForceUnits="x"))
	float MaxAcceleration;
	
	/**
	 * Deceleration when walking and not applying acceleration. This is a constant opposing force that directly lowers velocity by a constant value.
	 * @see GroundFriction, MaxAcceleration
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Modifier, meta=(ClampMin="0", UIMin="0", ForceUnits="x"))
	float BrakingDeceleration;

	/**
	 * Setting that affects movement control. Higher values allow faster changes in direction.
	 * If bUseSeparateBrakingFriction is false, also affects the ability to stop more quickly when braking (whenever Acceleration is zero), where it is multiplied by BrakingFrictionFactor.
	 * When braking, this property allows you to control how much friction is applied when moving across the ground, applying an opposing force that scales with current velocity.
	 * This can be used to simulate slippery surfaces such as ice or oil by changing the value (possibly based on the material pawn is standing on).
	 * @see BrakingDecelerationWalking, BrakingFriction, bUseSeparateBrakingFriction, BrakingFrictionFactor
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Modifier, meta=(ClampMin="0", UIMin="0", ForceUnits="x"))
	float GroundFriction;
	
	/**
	 * Friction (drag) coefficient applied when braking (whenever Acceleration = 0, or if character is exceeding max speed); actual value used is this multiplied by BrakingFrictionFactor.
	 * When braking, this property allows you to control how much friction is applied when moving across the ground, applying an opposing force that scales with current velocity.
	 * Braking is composed of friction (velocity-dependent drag) and constant deceleration.
	 * This is the current value, used in all movement modes; if this is not desired, override it or bUseSeparateBrakingFriction when movement mode changes.
	 * @note Only used if bUseSeparateBrakingFriction setting is true, otherwise current friction such as GroundFriction is used.
	 * @see bUseSeparateBrakingFriction, BrakingFrictionFactor, GroundFriction, BrakingDecelerationWalking
	 */
	UPROPERTY(Category="Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", EditCondition="bUseSeparateBrakingFriction"))
	float BrakingFriction;
};

/**
 * Represents a single modifier that can be applied to a character
 */
USTRUCT(BlueprintType)
struct PREDICTEDMOVEMENT_API FModifierData
{
	GENERATED_BODY()

	FModifierData(const EModifierLevelType InLevelType = EModifierLevelType::FGameplayTag, int32 InMaxModifiers = 3,
		EModifierLevelMethod InLevelMethod = EModifierLevelMethod::Max)
		: ModifierType(FGameplayTag::EmptyTag)
		, LevelMethod(InLevelMethod)
		, MaxModifiers(InMaxModifiers)
		, LevelType(InLevelType)
		, RequestedModifierLevel(0)
		, ModifierLevel(0)
		, CharacterOwner(nullptr)
	{}

	FModifierData(const FModifierData& Clone)
	{
		*this = Clone;
	}

	/** Only copies replicated data */
	FModifierData& operator<<(const FModifierData& Clone);

	/** We don't check type here, we only want to ensure we're sufficiently matched for prediction purposes */
	bool operator==(const FModifierData& Other) const
	{
		return RequestedModifierLevel == Other.RequestedModifierLevel && ModifierLevel == Other.ModifierLevel && Modifiers == Other.Modifiers;
	}

	/** We don't check type here, we only want to ensure we're sufficiently matched for prediction purposes */
	bool operator!=(const FModifierData& Other) const
	{
		return !(*this == Other);
	}

protected:
	/** The type of modifier */
	UPROPERTY(BlueprintReadOnly, Category=Modifier)
	FGameplayTag ModifierType;

public:
	const FGameplayTag& GetModifierType() const { return ModifierType; }

	/** The method used to calculate modifier levels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Modifier, meta=(EditCondition="MaxModifiers > 1", EditConditionHides))
	EModifierLevelMethod LevelMethod;

	/**
	 * The maximum number of modifiers that can be applied at a single time
	 * Set to 1 to disable stacking
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Modifier, meta=(UIMin="1", ClampMin="1"))
	int32 MaxModifiers;

	/** Whether to use UEnum or FGameplayTag to represent levels */
	UPROPERTY()
	EModifierLevelType LevelType;
	
	/**
	 * When using FGameplayTag instead of Enum for Levels, the levels here will be available for use in CMC
	 * Any levels not defined here will be considered invalid, all levels utilized by the modifier must be defined here
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Modifier, meta=(EditCondition="LevelType == EModifierLevelType::FGameplayTag", EditConditionHides))
	FGameplayTagContainer ModifierLevelTags;

protected:
	UPROPERTY(Transient)
	TArray<FGameplayTag> ModifierLevels;

public:
	/** The requested modifier level, similar to the concept of bWantsSprint */
	UPROPERTY()
	uint8 RequestedModifierLevel;

	/** The current modifier level, similar to the concept of bIsSprinting */
	UPROPERTY()
	uint8 ModifierLevel;

	/** The modifier stack that is currently applied */
	UPROPERTY()
	TArray<uint8> Modifiers;

	UPROPERTY(Transient)
	TWeakObjectPtr<AModifierCharacter> CharacterOwner;

public:
	// Use either UEnum or FGameplayTag to represent levels, not both
	
	/** Used for casting when level is UEnum based */
	template<typename T>
	T GetModifierLevel() const
	{
		if (!ensureAlwaysMsgf(LevelType == EModifierLevelType::UEnum, TEXT("Should not be called when using FGameplayTag levels")))
		{
			return LEVEL_NONE;
		}
		return static_cast<T>(ModifierLevel);
	}

	/** Convert UEnum Level Type to byte level */
	template <typename T>
	uint8 GetModifierLevelByte(T EnumLevel) const
	{
		if (!ensureAlwaysMsgf(LevelType == EModifierLevelType::UEnum, TEXT("Should not be called when using FGameplayTag levels")))
		{
			return LEVEL_NONE;
		}
		return static_cast<uint8>(EnumLevel);
	}

	/** Used for gameplay tag conversion when level is gameplay tag based */
	const FGameplayTag& GetModifierLevel() const
	{
		if (!ensureAlwaysMsgf(LevelType == EModifierLevelType::FGameplayTag, TEXT("Should not be called when using UEnum levels")))
		{
			return FGameplayTag::EmptyTag;
		}
		return ModifierLevels.IsValidIndex(ModifierLevel) ? ModifierLevels[ModifierLevel] : FGameplayTag::EmptyTag;
	}

	/** Convert Gameplay Tag Level Type to byte level */
	uint8 GetModifierLevelByte(const FGameplayTag& Level) const
	{
		if (!ensureAlwaysMsgf(LevelType == EModifierLevelType::FGameplayTag, TEXT("Should not be called when using UEnum levels")))
		{
			return LEVEL_NONE;
		}
	
		// Did you pass the levels as tags in Initialize()?
		if (ensureAlwaysMsgf(ModifierLevels.Contains(Level), TEXT("Modifier level %s is not valid"), *Level.ToString()))
		{
			return ModifierLevels.IndexOfByKey(Level);
		}
		return LEVEL_NONE;
	}

public:
	bool HasModifier() const
	{
		return ModifierLevel > 0;
	}

	int32 GetNumModifiers() const	{ return Modifiers.Num();	}
	int32 GetNumModifiersByLevel(uint8 Level) const;

	/**
	 * Initialization is mandatory
	 * @param InCharacterOwner The character that owns this modifier
	 * @param InModifierType The type of modifier
	 */
	void Initialize(AModifierCharacter* InCharacterOwner, const FGameplayTag& InModifierType);
	bool HasInitialized() const { return ModifierType.IsValid(); }

	void AddModifier(uint8 Level);
	void RemoveModifier(uint8 Level);
	void RemoveAllModifiers();
	void RemoveAllModifiersByLevel(uint8 Level);
	void RemoveAllModifiersExceptLevel(uint8 Level);
	
	void SetModifiers(const TArray<uint8>& NewModifiers);
	void SetModifierLevel(uint8 Level);

	void OnModifiersChanged();

	void UpdateModifierLevel();

public:
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FModifierData> : public TStructOpsTypeTraitsBase2<FModifierData>
{
	enum
	{
		WithNetSerializer = true
	};
};
