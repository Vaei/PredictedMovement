// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Curves/CurveFloat.h"
#include "ModifierTypes.generated.h"

#define NO_MODIFIER UINT8_MAX

/**
 * The network type of the modifier, which determines how it is applied and synchronized across clients and servers
 */
UENUM(BlueprintType)
enum class EModifierNetType : uint8
{
	LocalPredicted UMETA(DisplayName="Local Predicted", ToolTip="Locally predicted modifier that respects player input, e.g. Sprinting"),
	WithCorrection UMETA(DisplayName="Local Predicted + Correction", ToolTip="Locally predicted modifier, but corrected by server when a mismatch occurs, e.g. Equipping a knife that results in higher speed"),
	ServerInitiated UMETA(DisplayName="Server Initiated", ToolTip="Modifier is applied by the server and sent to the client, e.g. Snared from a damage event on the server"),
};

/**
 * The network type of the modifier, which determines how it is applied and synchronized across clients and servers
 */
UENUM(BlueprintType)
enum class EModifierNetTypeLocal : uint8
{
	LocalPredicted UMETA(DisplayName="Local Predicted", ToolTip="Locally predicted modifier that respects player input, e.g. Sprinting"),
	WithCorrection UMETA(DisplayName="Local Predicted + Correction", ToolTip="Locally predicted modifier, but corrected by server when a mismatch occurs, e.g. Equipping a knife that results in higher speed"),
};

/**
 * The method used to calculate modifier levels
 */
UENUM(BlueprintType)
enum class EModifierLevelMethod : uint8
{
	Max					UMETA(ToolTip="The highest active modifier level will be applied"),
	Min					UMETA(ToolTip="The lowest active modifier level will be applied"),
	Stack				UMETA(ToolTip="Level stacks by each modifier that is added. e.g. add a level 1 modifier and a level 4 modifier, and it applies level 5"),
	Average 			UMETA(ToolTip="The average modifier level will be applied"),
};

UENUM(BlueprintType)
enum class EModifierFallZ : uint8
{
	Disabled			UMETA(ToolTip="Do not remove Velocity.Z when modifier starts"),
	Enabled				UMETA(ToolTip="Remove Velocity.Z when modifier starts"),
	Falling				UMETA(ToolTip="Remove Velocity.Z when modifier starts, but only if the character is falling (Velocity.Z < 0)"),
	Rising				UMETA(ToolTip="Remove Velocity.Z when modifier starts, but only if the character is rising (Velocity.Z > 0)"),
};

/**
 * Parameters for a modifier that affects character movement
 */
USTRUCT(BlueprintType)
struct PREDICTEDMOVEMENT_API FMovementModifierParams
{
	GENERATED_BODY()

	FMovementModifierParams(float InMaxWalkSpeed = 1.f, float InMaxAcceleration = 1.f, float InBrakingDeceleration = 1.f,
		float InGroundFriction = 1.f, float InBrakingFriction = 1.f, bool bInAffectsRootMotion = false)
		: MaxWalkSpeed(InMaxWalkSpeed)
		, MaxAcceleration(InMaxAcceleration)
		, BrakingDeceleration(InBrakingDeceleration)
		, GroundFriction(InGroundFriction)
		, BrakingFriction(InBrakingFriction)
		, bAffectsRootMotion(bInAffectsRootMotion)
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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Modifier, meta=(ClampMin="0", UIMin="0", ForceUnits="x"))
	float BrakingFriction;

	/** If true, this modifier's MaxWalkSpeed scalar affects root motion translation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Modifier, meta=(ClampMin="0", UIMin="0", ForceUnits="x"))
	bool bAffectsRootMotion;
};

/**
 * Parameters for a modifier that affects falling
 */
USTRUCT(BlueprintType)
struct PREDICTEDMOVEMENT_API FFallingModifierParams
{
	GENERATED_BODY()

	FFallingModifierParams(float InGravityScalar = 1.f, EModifierFallZ InRemoveVelocityZ = EModifierFallZ::Disabled)
		: bGravityScalarFromVelocityZ(false)
		, GravityScalar(InGravityScalar)
		, GravityScalarFallVelocityCurve(nullptr)
		, RemoveVelocityZOnStart(InRemoveVelocityZ)
		, bOverrideAirControl(false)
		, AirControlScalar(1.f)
		, AirControlOverride(1.f)
	{}

	/** If true, use GravityScalarFallVelocityCurve */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Modifier)
	bool bGravityScalarFromVelocityZ;

	/** Gravity is multiplied by this amount */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Modifier, meta=(ClampMin="0", UIMin="0", ForceUnits="x", EditCondition="!bGravityScalarFromVelocityZ", EditConditionHides))
	float GravityScalar;

	/** Gravity scale curve based on fall velocity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Modifier, meta=(EditCondition="bGravityScalarFromVelocityZ", EditConditionHides))
	UCurveFloat* GravityScalarFallVelocityCurve;

	/** Set Velocity.Z = 0.f when air fall starts */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Modifier, meta=(DisplayName="Remove Velocity Z On Start"))
	EModifierFallZ RemoveVelocityZOnStart;

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

	/**
	 * Get the gravity scalar based on the current velocity.
	 * If bGravityScalarFromVelocityZ is true, uses GravityScalarFallVelocityCurve to determine the scalar based on Velocity.Z.
	 * Otherwise, returns GravityScalar.
	 */
	float GetGravityScalar(const FVector& Velocity) const
	{
		if (!ensureMsgf(!bGravityScalarFromVelocityZ || GravityScalarFallVelocityCurve != nullptr, TEXT("GravityScalarFallVelocityCurve must be set")))
		{
			return 1.f;
		}
		return bGravityScalarFromVelocityZ ? GravityScalarFallVelocityCurve->GetFloatValue(Velocity.Z) : GravityScalar;
	}

	/**
	 * Get the air control value based on the current air control scalar.
	 * If bOverrideAirControl is true, returns AirControlOverride.
	 * Otherwise, returns AirControlScalar multiplied by CurrentAirControl.
	 * @param CurrentAirControl The current air control value to scale.
	 */
	float GetAirControl(float CurrentAirControl) const
	{
		return bOverrideAirControl ? AirControlOverride : AirControlScalar * CurrentAirControl;
	}
};

/**
 * Client auth parameters for providing client with partial positional authority
 * These parameters can be used to configure how the client can send position updates to the server
 * Useful for short bursts of movement that are difficult to sync over the network
 */
USTRUCT(BlueprintType)
struct PREDICTEDMOVEMENT_API FClientAuthParams
{
	GENERATED_BODY()

	FClientAuthParams(int32 InPriority = 99)
		: bEnableClientAuth(true)
		, ClientAuthTime(1.2f)
		, MaxClientAuthDistance(35.f)
		, RejectClientAuthDistance(500.f)
		, Priority(InPriority)
	{}

	FClientAuthParams(bool bInEnableClientAuth, float InClientAuthTime, float InMaxClientAuthDistance, float InRejectClientAuthDistance, int32 InPriority)
		: bEnableClientAuth(bInEnableClientAuth)
		, ClientAuthTime(InClientAuthTime)
		, MaxClientAuthDistance(InMaxClientAuthDistance)
		, RejectClientAuthDistance(InRejectClientAuthDistance)
		, Priority(InPriority)
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

	/**
	 * Optional property that can be used for GetClientAuthData() to determine priority
	 */
	UPROPERTY(Category="Character Movement (Networking)", EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", UIMin="0", EditCondition="bEnableClientAuth", EditConditionHides))
	int32 Priority;
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
		, Id(0)
		, Source(FGameplayTag::EmptyTag)
		, Priority(99)
	{}

	FClientAuthData(const FGameplayTag& InSource, float InTimeRemaining, int32 InPriority, uint64 InId)
		: Alpha(0.f)
		, TimeRemaining(InTimeRemaining)
		, Id(InId)
		, Source(InSource)
		, Priority(InPriority)
	{}

	FClientAuthData(const FGameplayTag& InSource, float InTimeRemaining, float InAlpha, int32 InPriority, uint64 InId)
		: Alpha(InAlpha)
		, TimeRemaining(InTimeRemaining)
		, Id(InId)
		, Source(InSource)
		, Priority(InPriority)
	{}

	/** The alpha value of the client auth data, used to determine how much authority the client has */
	UPROPERTY()
	float Alpha;

	/** The time remaining for the client to have positional authority */
	UPROPERTY()
	float TimeRemaining;

	UPROPERTY()
	uint64 Id;

	/** The source of the client auth data, used to determine which gameplay tag this data is associated with */
	UPROPERTY()
	FGameplayTag Source;

	/**
	 * The priority of the client auth data, used to determine which data to use when multiple sources are present
	 * Lower values are more important
	 */
	UPROPERTY()
	int32 Priority;

	bool IsValid() const
	{
		return Id != 0 && Source.IsValid();
	}

	bool operator==(const FClientAuthData& Other) const
	{
		return IsValid() && Id == Other.Id;
	}

	bool operator!=(const FClientAuthData& Other) const
	{
		return !(*this == Other);
	}
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

	/** Stack of client auth data */
	UPROPERTY()
	TArray<FClientAuthData> Stack;

	bool operator==(const FClientAuthStack& Other) const
	{
		return Stack == Other.Stack;
	}

	bool operator!=(const FClientAuthStack& Other) const
	{
		return !(*this == Other);
	}

	/**
	 * Sorts the stack by priority, in ascending order
	 * Lower priority values are more important
	 */
	void SortByPriority()
	{
		Stack.Sort([](const FClientAuthData& A, const FClientAuthData& B)
		{
			return A.Priority < B.Priority;
		});
	}

	/**
	 * Filters the stack by priority, returning only the data with the specified priority
	 * @param Priority The priority to filter by
	 * @return An array of FClientAuthData with the specified priority
	 */
	TArray<FClientAuthData> FilterPriority(int32 Priority) const
	{
		return Stack.FilterByPredicate([Priority](const FClientAuthData& AuthData)
		{
			return AuthData.Priority == Priority;
		});
	}

	/**
	 * Determines the lowest priority in the stack
	 */
	int32 DetermineLowestPriority()
	{
		if (Stack.Num() == 0)
		{
			return INT32_MAX;
		}
		SortByPriority();
		return Stack[0].Priority;
	}

	TArray<FClientAuthData> GetLowestPriority()
	{
		return FilterPriority(DetermineLowestPriority());
	}

	FClientAuthData* GetFirst()
	{
		return Stack.Num() > 0 ? &Stack[0] : nullptr;
	}

	const FClientAuthData* GetFirst() const
	{
		return Stack.Num() > 0 ? &Stack[0] : nullptr;
	}

	FClientAuthData* GetLatest()
	{
		return Stack.Num() > 0 ? &Stack.Last() : nullptr;
	}

	void RemoveFirst()
	{
		if (Stack.Num() > 0)
		{
			Stack.RemoveAt(0);
		}
	}
	
	const FClientAuthData* GetLatest() const
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

	/**
	 * Updates the time remaining for each client auth data in the stack
	 * And removes any data that has expired (TimeRemaining <= 0)
	 */
	void Update(float DeltaTime)
	{
		Stack.RemoveAll([DeltaTime](FClientAuthData& Data)
		{
			Data.TimeRemaining -= DeltaTime;
			return Data.TimeRemaining <= 0.f;
		});
	}
};
