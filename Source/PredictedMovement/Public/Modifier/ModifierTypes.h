// Copyright (c) 2023 Jared Taylor. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "ModifierTypes.generated.h"

class AModifierCharacter;
class UModifierMovement;

namespace FModifierTags
{
	PREDICTEDMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Modifier_Type_Debuff_Snare);
}

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

	FModifierData(const FGameplayTag& InModifierType = FModifierTags::Modifier_Type_Debuff_Snare, EModifierLevelMethod InLevelMethod = EModifierLevelMethod::Max, int32 InMaxModifiers = 3)
		: ModifierType(InModifierType)
		, LevelMethod(InLevelMethod)
		, MaxModifiers(InMaxModifiers)
		, ModifierLevel(0)
		, CharacterOwner(nullptr)
		, bHasInitialized(false)
	{}

	/** The type of modifier */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Modifier, meta=(Categories="Modifier.Type"))
	FGameplayTag ModifierType;

	/** The method used to calculate modifier levels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Modifier)
	EModifierLevelMethod LevelMethod;

	/**
	 * The maximum number of modifiers that can be applied at a single time
	 * Set to 0 to disable stacking
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Modifier, meta=(UIMin="0", ClampMin="0"))
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

	UPROPERTY(Transient)
	bool bHasInitialized;

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
	int32 GetNumModifiersByLevel(uint8 Level = 0) const;

	void Initialize(AModifierCharacter* InCharacterOwner);

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
	
	bool operator==(const FModifierData& Other) const
	{
		return ModifierType == Other.ModifierType && ModifierLevel == Other.ModifierLevel && Modifiers == Other.Modifiers;
	}

	bool operator!=(const FModifierData& Other) const
	{
		return !(*this == Other);
	}
};

template<>
struct TStructOpsTypeTraits<FModifierData> : public TStructOpsTypeTraitsBase2<FModifierData>
{
	enum
	{
		WithNetSerializer = true
	};
};
