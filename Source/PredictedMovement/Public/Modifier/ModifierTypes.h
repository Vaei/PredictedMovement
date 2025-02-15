// Copyright (c) 2023 Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "NativeGameplayTags.h"
#include "Curves/CurveFloat.h"
#include "ModifierTypes.generated.h"

class AModifierCharacter;
class UModifierMovement;

#define LEVEL_NONE UINT8_MAX

/**
 * The source of the modifier, such as whether it was applied externally or predicted internally
 */
UENUM(BlueprintType)
enum class EModifierActivationSource : uint8
{
	LocalPredicted		UMETA(ToolTip="The modifier was applied to self predictively, from a self-activated event such as input"),
	ServerInitiated		UMETA(ToolTip="The modifier was applied externally, such as from a server event or a different character"),
};

/**
 * The source of the modifier, such as whether it was applied externally or predicted internally
 */
UENUM(BlueprintType)
enum class EModifierActivationSources : uint8
{
	LocalPredicted		UMETA(ToolTip="The modifier was applied to self predictively, from a self-activated event such as input"),
	ServerInitiated		UMETA(ToolTip="The modifier was applied externally, such as from a server event"),
	Both				UMETA(DisplayName = "Both", ToolTip = "Apply to both Self and External activations"),
};

/**
 * The method used to calculate modifier levels
 */
UENUM(BlueprintType)
enum class EModifierLevelMethod : uint8
{
	Max					UMETA(ToolTip="The highest active modifier level will be applied"),
	Stack				UMETA(ToolTip="Level stacks by each modifier that is added. e.g. add a level 1 modifier and a level 4 modifier, and it applies level 5"),
	Average 			UMETA(ToolTip="The average modifier level will be applied"),
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

/**
 * Parameters for a modifier that affects character movement
 */
USTRUCT(BlueprintType)
struct PREDICTEDMOVEMENT_API FMovementModifierParams
{
	GENERATED_BODY()

	FMovementModifierParams(float InMaxWalkSpeed = 1.f, float InMaxAcceleration = 1.f, float InBrakingDeceleration = 1.f,
		float InGroundFriction = 1.f, float InBrakingFriction = 1.f)
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
 * Parameters for a modifier that affects falling
 */
USTRUCT(BlueprintType)
struct PREDICTEDMOVEMENT_API FFallingModifierParams
{
	GENERATED_BODY()

	FFallingModifierParams(float InGravityScalar = 1.f)
		: bGravityScalarFromVelocityZ(false)
		, GravityScalar(InGravityScalar)
		, GravityScalarFallVelocityCurve(nullptr)
		, bRemoveVelocityZOnStart(false)
		, bOverrideAirControl(false)
		, AirControlScalar(1.f)
		, AirControlOverride(1.f)
	{}

	/** If true, use GravityScalarFallVelocityCurve */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Modifier)
	bool bGravityScalarFromVelocityZ;

	/** Gravity is multiplied by this amount */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Modifier, meta=(ClampMin="0", UIMin="0", ForceUnits="x", EditCondition="!bGravityScalarFromVelocity", EditConditionHides))
	float GravityScalar;

	/** Gravity scale curve based on fall velocity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Modifier, meta=(EditCondition="bGravityScalarFromVelocity", EditConditionHides))
	UCurveFloat* GravityScalarFallVelocityCurve;

	/** Set Velocity.Z = 0.f when air fall starts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Modifier, meta=(DisplayName="Remove Velocity Z On Start"))
	bool bRemoveVelocityZOnStart;

	/** If true, directly set the air control value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Modifier)
	bool bOverrideAirControl;

	/**
	 * When falling, amount of lateral movement control available to the character.
	 * 0 = no control, 1 = full control at max speed of MaxWalkSpeed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Modifier, meta=(ClampMin="0", UIMin="0", ForceUnits="x", EditCondition="!bOverrideAirControl", EditConditionHides))
	float AirControlScalar;

	/**
	 * When falling, amount of lateral movement control available to the character.
	 * 0 = no control, 1 = full control at max speed of MaxWalkSpeed.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Modifier, meta=(EditCondition="bOverrideAirControl", EditConditionHides))
	float AirControlOverride;

	float GetGravityScalar(const FVector& Velocity) const
	{
		if (!ensureMsgf(!bGravityScalarFromVelocityZ || GravityScalarFallVelocityCurve != nullptr, TEXT("GravityScalarFallVelocityCurve must be set")))
		{
			return 1.f;
		}
		return bGravityScalarFromVelocityZ ? GravityScalarFallVelocityCurve->GetFloatValue(Velocity.Z) : GravityScalar;
	}

	float GetAirControl(float CurrentAirControl) const
	{
		return bOverrideAirControl ? AirControlOverride : AirControlScalar * CurrentAirControl;
	}
};

/**
 * Represents a single modifier that can be applied to a character
 */
USTRUCT(BlueprintType)
struct PREDICTEDMOVEMENT_API FMovementModifier
{
	GENERATED_BODY()

	FMovementModifier(const EModifierLevelType InLevelType = EModifierLevelType::FGameplayTag, uint8 InMaxModifiers = 3,
		EModifierLevelMethod InLevelMethod = EModifierLevelMethod::Max)
		: ModifierType(FGameplayTag::EmptyTag)
		, LevelMethod(InLevelMethod)
		, MaxModifiers(InMaxModifiers)
		, ActivationSource(EModifierActivationSource::LocalPredicted)
		, LevelType(InLevelType)
		, RequestedModifierLevel(0)
		, ModifierLevel(0)
		, CharacterOwner(nullptr)
	{}

	FMovementModifier(const FMovementModifier& Clone)
	{
		*this = Clone;
	}

	/** Only copies replicated data */
	FMovementModifier& operator<<(const FMovementModifier& Clone);

	/**
	 * We don't check type here, we only want to ensure we're sufficiently matched for prediction purposes.
	 * We also don't serialize the actual ModifierLevel, which is updated from RequestedModifierLevel.
	 */
	bool operator==(const FMovementModifier& Other) const
	{
		return RequestedModifierLevel == Other.RequestedModifierLevel && Modifiers == Other.Modifiers;
	}

	bool operator!=(const FMovementModifier& Other) const
	{
		return !(*this == Other);
	}

	/** ^= is the same as != but it also evaluates the current level rather than only the requested level */
	bool operator^=(const FMovementModifier& Other) const
	{
		return ModifierLevel != Other.ModifierLevel || RequestedModifierLevel != Other.RequestedModifierLevel || Modifiers != Other.Modifiers;
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
	uint8 MaxModifiers;

	/**
	 * Whether the modifier was activated by the character itself or externally
	 * This can be checked against for wrapper functions that apply modifiers if needed, otherwise it isn't used
	 */
	UPROPERTY()
	EModifierActivationSource ActivationSource;

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

protected:
	bool HasValidData() const { return CharacterOwner.IsValid(); }

public:
	// Use either UEnum or FGameplayTag to represent levels, not both
	
	/** Used for casting when level is UEnum based */
	template<typename T>
	T GetModifierLevel() const
	{
		if (!ensureAlwaysMsgf(LevelType == EModifierLevelType::UEnum, TEXT("Should not be called when using FGameplayTag levels")))
		{
			return static_cast<T>(LEVEL_NONE);
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

	bool WantsModifier() const
	{
		return RequestedModifierLevel > 0;
	}

	uint8 GetNumModifiers() const { return Modifiers.Num();	}
	uint8 GetNumModifiersByLevel(uint8 Level) const;

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

	void StartModifier(uint8 Level, bool bCanApplyModifier, bool bClientSimulation = false, uint8 PrevSimulatedLevel = 0);
	void EndModifier(bool bClientSimulation = false, uint8 PrevSimulatedLevel = 0);

	void UpdateCharacterStateBeforeMovement(bool bCanApplyModifier);
	void UpdateCharacterStateAfterMovement(bool bCanApplyModifier);

public:
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FMovementModifier> : TStructOpsTypeTraitsBase2<FMovementModifier>
{
	enum
	{
		WithNetSerializer = true
	};
};

USTRUCT(BlueprintType)
struct PREDICTEDMOVEMENT_API FClientAuthParams
{
	GENERATED_BODY()

	FClientAuthParams()
		: bEnableClientAuth(true)
		, ClientAuthTime(1.25f)
		, MaxClientAuthDistance(150.f)
		, RejectClientAuthDistance(800.f)
	{}

	/**
	 * If True, the client will be allowed to send position updates to the server
	 * Useful for short bursts of movement that are difficult to sync over the network
	 */
	UPROPERTY(Category="Character Movement (Networking)", EditAnywhere, BlueprintReadOnly)
	bool bEnableClientAuth;
	
	/**
	 * How long to allow client to have positional authority after being Snared
	 */
	UPROPERTY(Category="Character Movement (Networking)", EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", UIMin="0", ForceUnits="s", EditCondition="bEnableClientAuth", EditConditionHides))
	float ClientAuthTime;

	/**
	 * Maximum distance between client and server that will be accepted by server
	 * Values above this will be scaled to the maximum distance
	 */
	UPROPERTY(Category="Character Movement (Networking)", EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", UIMin="0", ForceUnits="cm", EditCondition="bEnableClientAuth", EditConditionHides))
	float MaxClientAuthDistance;

	/**
	 * Maximum distance between client and server that will be accepted by server
	 * Values above this will be rejected entirely, on suspicion of cheating, or excessive error
	 */
	UPROPERTY(Category="Character Movement (Networking)", EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", UIMin="0", ForceUnits="cm", EditCondition="bEnableClientAuth", EditConditionHides))
	float RejectClientAuthDistance;
};

/**
 * Client auth data for providing client with positional authority
 */
USTRUCT()
struct PREDICTEDMOVEMENT_API FClientAuthData
{
	GENERATED_BODY()

	FClientAuthData()
		: Alpha(0.f)
		, TimeRemaining(0.f)
		, Id(FGuid())
		, Source(FGameplayTag::EmptyTag)
	{}

	FClientAuthData(const FGameplayTag& InSource, float InTimeRemaining)
		: Alpha(0.f)
		, TimeRemaining(InTimeRemaining)
		, Id(FGuid::NewGuid())
		, Source(InSource)
	{}

	UPROPERTY()
	float Alpha;
	
	UPROPERTY(NotReplicated)
	float TimeRemaining;

	UPROPERTY(NotReplicated)
	FGuid Id;
	
	UPROPERTY(NotReplicated)
	FGameplayTag Source;

	bool operator==(const FClientAuthData& Other) const
	{
		return Id.IsValid() && Id == Other.Id;
	}

	bool operator!=(const FClientAuthData& Other) const
	{
		return !(*this == Other);
	}
	
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FClientAuthData> : TStructOpsTypeTraitsBase2<FClientAuthData>
{
	enum
	{
		WithNetSerializer = true
	};
};

/**
 * Stack of client auth data for providing client with positional authority
 */
USTRUCT()
struct PREDICTEDMOVEMENT_API FClientAuthStack
{
	GENERATED_BODY()

	FClientAuthStack()
	{}

	UPROPERTY()
	TArray<FClientAuthData> Stack;

	FClientAuthData* GetLatest()
	{
		return Stack.Num() > 0 ? &Stack.Last() : nullptr;
	}

	void RemoveLatest()
	{
		if (Stack.Num() > 0)
		{
			Stack.RemoveAt(Stack.Num() - 1);
		}
	}

	void RemoveData(const FClientAuthData* Data)
	{
		if (Data)
		{
			Stack.Remove(*Data);
		}
	}

	void RemoveAllDataForSource(const FGameplayTag& Source)
	{
		Stack.RemoveAll([Source](const FClientAuthData& Data)
		{
			return Data.Source == Source;
		});
	}

	void Update(float DeltaTime)
	{
		Stack.RemoveAll([DeltaTime](FClientAuthData& Data)
		{
			Data.TimeRemaining -= DeltaTime;
			return Data.TimeRemaining <= 0.f;
		});
	}

	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);
};

template<>
struct TStructOpsTypeTraits<FClientAuthStack> : TStructOpsTypeTraitsBase2<FClientAuthStack>
{
	enum
	{
		WithNetSerializer = true
	};
};