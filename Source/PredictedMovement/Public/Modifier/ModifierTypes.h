// Copyright (c) 2023 Jared Taylor. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "ModifierTypes.generated.h"

class AModifierCharacter;
class UModifierMovement;

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Modifier)
	EModifierLevelMethod LevelMethod;

	/**
	 * The maximum number of modifiers that can be applied at a single time
	 * Set to 0 to disable stacking
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category=Modifier, meta=(UIMin="0", ClampMin="0"))
	int32 MaxModifiers;
	
public:
	/** The current modifier level */
	UPROPERTY()
	uint8 ModifierLevel;

	/** The modifier stack that is currently applied */
	UPROPERTY()
	TArray<uint8> Modifiers;

	UPROPERTY(Transient)
	TWeakObjectPtr<AModifierCharacter> CharacterOwner;

public:
	template<typename T>
	T GetModifierLevel() const
	{
		return static_cast<T>(ModifierLevel);
	}

	bool HasModifier() const
	{
		return ModifierLevel > 0;
	}

	int32 GetNumModifiers() const	{ return Modifiers.Num();	}
	int32 GetNumModifiersByLevel(uint8 Level) const;

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

public:
	bool ServerCheckClientError(const FModifierData& Data) const { return ModifierLevel != Data.ModifierLevel || Modifiers != Data.Modifiers; }
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
