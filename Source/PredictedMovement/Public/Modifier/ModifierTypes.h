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
 * Represents a single modifier that can be applied to a character
 */
USTRUCT(BlueprintType)
struct PREDICTEDMOVEMENT_API FModifierData
{
	GENERATED_BODY()

	FModifierData(int32 InMaxModifiers = 3, EModifierLevelMethod InLevelMethod = EModifierLevelMethod::Max)
		: ModifierType(FGameplayTag::EmptyTag)
		, LevelMethod(InLevelMethod)
		, MaxModifiers(InMaxModifiers)
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
		return ModifierLevel == Other.ModifierLevel && Modifiers == Other.Modifiers;
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
	
public:
	/** The current modifier level */
	UPROPERTY()
	uint8 ModifierLevel;

	/** The modifier stack that is currently applied */
	UPROPERTY()
	TArray<uint8> Modifiers;

	/** Cached levels used to lookup tags from uint8 */
	UPROPERTY(Transient)
	TArray<FGameplayTag> ModifierLevelTags;
	
	UPROPERTY(Transient)
	TWeakObjectPtr<AModifierCharacter> CharacterOwner;

public:
	// Use either enum or FGameplayTag to represent levels, not both
	
	/** Used for enum casting when level is enum based */
	template<typename T>
	T GetModifierLevel() const { return static_cast<T>(ModifierLevel); }

	/** Convert Enum Level Type to byte level */
	template <typename T>
	static uint8 GetModifierLevelByte(T EnumLevel)
	{
		return static_cast<uint8>(EnumLevel);
	}

	/** Used for gameplay tag conversion when level is gameplay tag based */
	const FGameplayTag& GetModifierLevel() const
	{
		return ModifierLevelTags.IsValidIndex(ModifierLevel) ? ModifierLevelTags[ModifierLevel] : FGameplayTag::EmptyTag;
	}

	/** Convert Gameplay Tag Level Type to byte level */
	uint8 GetModifierLevelByte(const FGameplayTag& Level) const;

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
	 * @param InModifierLevels The levels of the modifier -- not required if using enum based levels
	 */
	void Initialize(AModifierCharacter* InCharacterOwner, const FGameplayTag& InModifierType, const FGameplayTagContainer& InModifierLevels);
	bool HasInitialized() const { return ModifierType.IsValid(); }

	void AddModifier(uint8 Level);
	void RemoveModifier(uint8 Level);
	void RemoveAllModifiers();
	void RemoveAllModifiersByLevel(uint8 Level);
	void RemoveAllModifiersExceptLevel(uint8 Level);
	
	void SetModifiers(const TArray<uint8>& NewModifiers);
	void SetModifierLevel(uint8 Level);

	void OnModifiersChanged();

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
