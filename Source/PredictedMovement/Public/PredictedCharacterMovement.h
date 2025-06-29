// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "PredTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Modifier/ModifierImpl.h"
#include "Modifier/ModifierTypes.h"
#include "System/PredictedMovementVersioning.h"
#include "PredictedCharacterMovement.generated.h"

class APredictedCharacter;

using TMod_Local = FMovementModifier_LocalPredicted;
using TMod_LocalCorrection = FMovementModifier_WithCorrection;
using TMod_Server = FMovementModifier_WithCorrection;

struct PREDICTEDMOVEMENT_API FPredictedMoveResponseDataContainer : FCharacterMoveResponseDataContainer
{  // Server ➜ Client
	using Super = FCharacterMoveResponseDataContainer;

	float Stamina;
	bool bStaminaDrained;

	/*
	 * Used by the server to send Modifier data to the client
	 * LocalPredicted modifiers are not sent, as the server does not correct input states
	 */
	
	FModifierMoveResponse BoostCorrection;		// Boost
	FModifierMoveResponse HasteCorrection;		// Haste
	FModifierMoveResponse SlowCorrection; 		// Slow
	FModifierMoveResponse SnareServer; 			// Snare
	FModifierMoveResponse SlowFallCorrection; 	// SlowFall

	/** Tell the client how much location authority they have */
	float ClientAuthAlpha = 0.f;

	/** No need to send the float if the client has no authority */
	bool bHasClientAuthAlpha;

	virtual void ServerFillResponseData(const UCharacterMovementComponent& CharacterMovement, const FClientAdjustment& PendingAdjustment) override;
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap) override;
};

struct PREDICTEDMOVEMENT_API FPredictedNetworkMoveData : FCharacterNetworkMoveData
{  // Client ➜ Server
public:
	using Super = FCharacterNetworkMoveData;
 
	FPredictedNetworkMoveData()
		: Stamina(0)
	{}

	float Stamina;

	/*
	 * Used by the client to send Modifier data to the server
	 * If local predicted, this data is based on player input, and the server will apply it
	 * Otherwise, the server will compare the client and server data to know when to send a correction
	 */
	
	FModifierMoveData_LocalPredicted BoostLocal; 			// Boost
	FModifierMoveData_WithCorrection BoostCorrection;		// Boost
	FModifierMoveData_LocalPredicted HasteLocal; 			// Haste
	FModifierMoveData_WithCorrection HasteCorrection; 		// Haste
	FModifierMoveData_LocalPredicted SlowLocal; 			// Slow
	FModifierMoveData_WithCorrection SlowCorrection;		// Slow
	FModifierMoveData_ServerInitiated SnareServer;	 		// Snare
	FModifierMoveData_LocalPredicted SlowFallLocal; 		// SlowFall
	FModifierMoveData_WithCorrection SlowFallCorrection;	// SlowFall

	virtual void ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType) override;
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType) override;
};
 
struct PREDICTEDMOVEMENT_API FPredictedNetworkMoveDataContainer : FCharacterNetworkMoveDataContainer
{  // Client ➜ Server
public:
	using Super = FCharacterNetworkMoveDataContainer;
 
	FPredictedNetworkMoveDataContainer()
	{
		NewMoveData = &MoveData[0];
		PendingMoveData = &MoveData[1];
		OldMoveData = &MoveData[2];
	}
 
private:
	FPredictedNetworkMoveData MoveData[3];
};

/**
 * A complete character movement component combined from the PredictedMovement repo.
 *
 * Separates movement property getters between base and scalar functions
 * 
 * Stackable modifiers: Boost, Haste, Slow, Snare, and SlowFall
 *	Supports local predicted, local predicted with corrections, server-initiated
 * Partial client authority for server-initiated movement
 * 
 * Gait modes: Stroll, Walk, Run, Sprint
 * Features: Prone, Stamina, AimDownSights
 */
UCLASS()
class PREDICTEDMOVEMENT_API UPredictedCharacterMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()
	
private:
	/** Character movement component belongs to */
	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<APredictedCharacter> PredCharacterOwner;

public:
	/** Max Acceleration (rate of change of velocity) */
	UPROPERTY(Category="Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	float MaxAccelerationRunning;

	/** The maximum ground speed when Running. */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="cm/s"))
	float MaxWalkSpeedRunning;

	/**
	 * Deceleration when walking and not applying acceleration. This is a constant opposing force that directly lowers velocity by a constant value.
	 * @see GroundFriction, MaxAcceleration
	 */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	float BrakingDecelerationRunning;

	/**
	 * Setting that affects movement control. Higher values allow faster changes in direction.
	 * If bUseSeparateBrakingFriction is false, also affects the ability to stop more quickly when braking (whenever Acceleration is zero), where it is multiplied by BrakingFrictionFactor.
	 * When braking, this property allows you to control how much friction is applied when moving across the ground, applying an opposing force that scales with current velocity.
	 * This can be used to simulate slippery surfaces such as ice or oil by changing the value (possibly based on the material pawn is standing on).
	 * @see BrakingDecelerationWalking, BrakingFriction, bUseSeparateBrakingFriction, BrakingFrictionFactor
	 */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	float GroundFrictionRunning;

	/**
     * When struggling to surpass walk speed, which can occur with heavy rotation and low acceleration, we
     * mitigate the check so there isn't a constant re-entry that can occur as an edge case.
     * This can optionally be used inversely, to require you to considerably exceed MaxSpeedWalking before Running
     * will actually take effect.
     */
    UPROPERTY(Category="Character Movement: Walking", AdvancedDisplay, EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
    float VelocityCheckMitigatorRunning;

	/**
	 * Friction (drag) coefficient applied when braking (whenever Acceleration = 0, or if character is exceeding max speed); actual value used is this multiplied by BrakingFrictionFactor.
	 * When braking, this property allows you to control how much friction is applied when moving across the ground, applying an opposing force that scales with current velocity.
	 * Braking is composed of friction (velocity-dependent drag) and constant deceleration.
	 * This is the current value, used in all movement modes; if this is not desired, override it or bUseSeparateBrakingFriction when movement mode changes.
	 * @note Only used if bUseSeparateBrakingFriction setting is true, otherwise current friction such as GroundFriction is used.
	 * @see bUseSeparateBrakingFriction, BrakingFrictionFactor, GroundFriction, BrakingDecelerationWalking
	 */
	UPROPERTY(Category="Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", EditCondition="bUseSeparateBrakingFriction"))
	float BrakingFrictionRunning;

public:
	/** Max Acceleration (rate of change of velocity) */
	UPROPERTY(Category="Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	float MaxAccelerationStrolling;

	/** The maximum ground speed when Strolling. */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="cm/s"))
	float MaxWalkSpeedStrolling;

	/**
	 * Deceleration when walking and not applying acceleration. This is a constant opposing force that directly lowers velocity by a constant value.
	 * @see GroundFriction, MaxAcceleration
	 */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	float BrakingDecelerationStrolling;

	/**
	 * Setting that affects movement control. Higher values allow faster changes in direction.
	 * If bUseSeparateBrakingFriction is false, also affects the ability to stop more quickly when braking (whenever Acceleration is zero), where it is multiplied by BrakingFrictionFactor.
	 * When braking, this property allows you to control how much friction is applied when moving across the ground, applying an opposing force that scales with current velocity.
	 * This can be used to simulate slippery surfaces such as ice or oil by changing the value (possibly based on the material pawn is standing on).
	 * @see BrakingDecelerationWalking, BrakingFriction, bUseSeparateBrakingFriction, BrakingFrictionFactor
	 */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	float GroundFrictionStrolling;

	/**
     * When struggling to surpass walk speed, which can occur with heavy rotation and low acceleration, we
     * mitigate the check so there isn't a constant re-entry that can occur as an edge case.
     * This can optionally be used inversely, to require you to considerably exceed MaxSpeedWalking before Strolling
     * will actually take effect.
     */
    UPROPERTY(Category="Character Movement: Walking", AdvancedDisplay, EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
    float VelocityCheckMitigatorWalking;

	/**
	 * Friction (drag) coefficient applied when braking (whenever Acceleration = 0, or if character is exceeding max speed); actual value used is this multiplied by BrakingFrictionFactor.
	 * When braking, this property allows you to control how much friction is applied when moving across the ground, applying an opposing force that scales with current velocity.
	 * Braking is composed of friction (velocity-dependent drag) and constant deceleration.
	 * This is the current value, used in all movement modes; if this is not desired, override it or bUseSeparateBrakingFriction when movement mode changes.
	 * @note Only used if bUseSeparateBrakingFriction setting is true, otherwise current friction such as GroundFriction is used.
	 * @see bUseSeparateBrakingFriction, BrakingFrictionFactor, GroundFriction, BrakingDecelerationWalking
	 */
	UPROPERTY(Category="Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", EditCondition="bUseSeparateBrakingFriction"))
	float BrakingFrictionStrolling;

public:
	/** If true, try to Walk (or keep Walking) on next update. If false, try to stop Walking on next update. */
	UPROPERTY(Category="Character Movement (General Settings)", VisibleInstanceOnly, BlueprintReadOnly)
	uint8 bWantsToWalk:1;

	/** If true, try to Stroll (or keep Strolling) on next update. If false, try to stop Walking on next update. */
	UPROPERTY(Category="Character Movement (General Settings)", VisibleInstanceOnly, BlueprintReadOnly)
	uint8 bWantsToStroll:1;

public:
	/** If true, sprinting acceleration will only be applied when IsSprintingAtSpeed() returns true */
	UPROPERTY(Category="Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite)
	bool bUseMaxAccelerationSprintingOnlyAtSpeed;
	
	/** Max Acceleration (rate of change of velocity) */
	UPROPERTY(Category="Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	float MaxAccelerationSprinting;
	
	/** The maximum ground speed when Sprinting. */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="cm/s"))
	float MaxWalkSpeedSprinting;

	/**
	 * Deceleration when walking and not applying acceleration. This is a constant opposing force that directly lowers velocity by a constant value.
	 * @see GroundFriction, MaxAcceleration
	 */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	float BrakingDecelerationSprinting;

	/**
	 * Setting that affects movement control. Higher values allow faster changes in direction.
	 * If bUseSeparateBrakingFriction is false, also affects the ability to stop more quickly when braking (whenever Acceleration is zero), where it is multiplied by BrakingFrictionFactor.
	 * When braking, this property allows you to control how much friction is applied when moving across the ground, applying an opposing force that scales with current velocity.
	 * This can be used to simulate slippery surfaces such as ice or oil by changing the value (possibly based on the material pawn is standing on).
	 * @see BrakingDecelerationWalking, BrakingFriction, bUseSeparateBrakingFriction, BrakingFrictionFactor
	 */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	float GroundFrictionSprinting;

	/**
     * When struggling to surpass walk speed, which can occur with heavy rotation and low acceleration, we
     * mitigate the check so there isn't a constant re-entry that can occur as an edge case.
     * This can optionally be used inversely, to require you to considerably exceed MaxSpeedWalking before sprinting
     * will actually take effect.
     */
    UPROPERTY(Category="Character Movement: Walking", AdvancedDisplay, EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
    float VelocityCheckMitigatorSprinting;
	
	/**
	 * Friction (drag) coefficient applied when braking (whenever Acceleration = 0, or if character is exceeding max speed); actual value used is this multiplied by BrakingFrictionFactor.
	 * When braking, this property allows you to control how much friction is applied when moving across the ground, applying an opposing force that scales with current velocity.
	 * Braking is composed of friction (velocity-dependent drag) and constant deceleration.
	 * This is the current value, used in all movement modes; if this is not desired, override it or bUseSeparateBrakingFriction when movement mode changes.
	 * @note Only used if bUseSeparateBrakingFriction setting is true, otherwise current friction such as GroundFriction is used.
	 * @see bUseSeparateBrakingFriction, BrakingFrictionFactor, GroundFriction, BrakingDecelerationWalking
	 */
	UPROPERTY(Category="Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", EditCondition="bUseSeparateBrakingFriction"))
	float BrakingFrictionSprinting;

	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(InlineEditConditionToggle))
	bool bRestrictSprintInputAngle;

	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, meta=(EditCondition="bRestrictSprintInputAngle", ClampMin="0.0", ClampMax="180.0", UIMin = "0.0", UIMax = "180.0", ForceUnits="degrees"))
	float MaxInputAngleSprint;

	UPROPERTY(Category="Character Movement: Walking", VisibleAnywhere)
	float MaxInputNormalSprint;
	
public:
	/** If true, try to Sprint (or keep Sprinting) on next update. If false, try to stop Sprinting on next update. */
	UPROPERTY(Category="Character Movement (General Settings)", VisibleInstanceOnly, BlueprintReadOnly)
	uint8 bWantsToSprint:1;

public:
	/** How much Stamina to start with */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	float BaseMaxStamina;
	
	/** Modify maximum speed when stamina is drained. */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="x"))
	float MaxWalkSpeedScalarStaminaDrained;

	/** Modify maximum acceleration when stamina is drained. */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="x"))
	float MaxAccelerationScalarStaminaDrained;

	/** Modify maximum braking deceleration when stamina is drained. */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="x"))
	float MaxBrakingDecelerationScalarStaminaDrained;
	
	/** The rate at which stamina is drained while sprinting */
	UPROPERTY(Category="Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite)
	float SprintStaminaDrainRate;

	/** The rate at which stamina is regenerated when not being drained */
	UPROPERTY(Category="Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite)
	float StaminaRegenRate;

	/** The rate at which stamina is regenerated when in a drained state */
	UPROPERTY(Category="Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite)
	float StaminaDrainedRegenRate;

	/** If true, stamina recovery from drained state is based on percentage instead of amount */
	UPROPERTY(Category="Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite)
	bool bStaminaRecoveryFromPct;
	
	/** Amount of Stamina to recover before being considered recovered */
	UPROPERTY(Category="Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", EditCondition="!bStaminaRecoveryFromPct", EditConditionHides))
	float StaminaRecoveryAmount;

	/** Percentage of Stamina to recover before being considered recovered */
	UPROPERTY(Category="Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ClampMax="1", UIMax="1", ForceUnits="%", EditCondition="bStaminaRecoveryFromPct", EditConditionHides))
	float StaminaRecoveryPct;

	/** If Stamina Pct is below this value then cannot start sprinting */
	UPROPERTY(Category="Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ClampMax="1", UIMax="1", ForceUnits="%", EditCondition="bStaminaRecoveryFromPct", EditConditionHides))
	float StartSprintStaminaPct;
	
public:
	/** Maximum stamina difference that is allowed between client and server before a correction occurs. */
	UPROPERTY(Category="Character Movement (Networking)", EditDefaultsOnly, meta=(ClampMin="0.0", UIMin="0.0"))
	float NetworkStaminaCorrectionThreshold;
	
protected:
	/** THIS SHOULD ONLY BE MODIFIED IN DERIVED CLASSES FROM OnStaminaChanged AND NOWHERE ELSE */
	UPROPERTY()
	float Stamina;

private:
	UPROPERTY()
	float MaxStamina;

	UPROPERTY()
	bool bStaminaDrained;

public:
	/** Scale the max Acceleration (rate of change of velocity) */
	UPROPERTY(Category="Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="x"))
	float MaxAccelerationAimingDownSightsScalar;
	
	/** Scale the maximum ground speed when AimingDownSights. */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="x"))
	float MaxWalkSpeedAimingDownSightsScalar;

	/**
	 * Scale the deceleration when walking and not applying acceleration. This is a constant opposing force that directly lowers velocity by a constant value.
	 * @see GroundFriction, MaxAcceleration
	 */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="x"))
	float BrakingDecelerationAimingDownSightsScalar;

	/**
	 * Setting that affects movement control. Higher values allow faster changes in direction.
	 * If bUseSeparateBrakingFriction is false, also affects the ability to stop more quickly when braking (whenever Acceleration is zero), where it is multiplied by BrakingFrictionFactor.
	 * When braking, this property allows you to control how much friction is applied when moving across the ground, applying an opposing force that scales with current velocity.
	 * This can be used to simulate slippery surfaces such as ice or oil by changing the value (possibly based on the material pawn is standing on).
	 * @see BrakingDecelerationWalking, BrakingFriction, bUseSeparateBrakingFriction, BrakingFrictionFactor
	 */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="x"))
	float GroundFrictionAimingDownSightsScalar;

	/**
	 * Friction (drag) coefficient applied when braking (whenever Acceleration = 0, or if character is exceeding max speed); actual value used is this multiplied by BrakingFrictionFactor.
	 * When braking, this property allows you to control how much friction is applied when moving across the ground, applying an opposing force that scales with current velocity.
	 * Braking is composed of friction (velocity-dependent drag) and constant deceleration.
	 * This is the current value, used in all movement modes; if this is not desired, override it or bUseSeparateBrakingFriction when movement mode changes.
	 * @note Only used if bUseSeparateBrakingFriction setting is true, otherwise current friction such as GroundFriction is used.
	 * @see bUseSeparateBrakingFriction, BrakingFrictionFactor, GroundFriction, BrakingDecelerationWalking
	 */
	UPROPERTY(Category="Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="x", EditCondition="bUseSeparateBrakingFriction"))
	float BrakingFrictionAimingDownSightsScalar;

	/** If true, Character can sprint when aiming down sights. */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite)
	uint8 bCanSprintDuringAimDownSights:1;
	
public:
	/** If true, try to AimDownSights (or keep AimingDownSights) on next update. If false, try to stop AimingDownSights on next update. */
	UPROPERTY(Category="Character Movement (General Settings)", VisibleInstanceOnly, BlueprintReadOnly)
	uint8 bWantsToAimDownSights:1;

public:
	/** Max Acceleration (rate of change of velocity) */
	UPROPERTY(Category="Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	float MaxAccelerationCrouched;
	
	/** Max Acceleration (rate of change of velocity) */
	UPROPERTY(Category="Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	float MaxAccelerationProned;
	
	/** The maximum ground speed when Proned. */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="cm/s"))
	float MaxWalkSpeedProned;

	/**
	 * Deceleration when walking and not applying acceleration. This is a constant opposing force that directly lowers velocity by a constant value.
	 * @see GroundFriction, MaxAcceleration
	 */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	float BrakingDecelerationCrouched;
	
	/**
	 * Deceleration when walking and not applying acceleration. This is a constant opposing force that directly lowers velocity by a constant value.
	 * @see GroundFriction, MaxAcceleration
	 */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	float BrakingDecelerationProned;

	/**
	 * Setting that affects movement control. Higher values allow faster changes in direction.
	 * If bUseSeparateBrakingFriction is false, also affects the ability to stop more quickly when braking (whenever Acceleration is zero), where it is multiplied by BrakingFrictionFactor.
	 * When braking, this property allows you to control how much friction is applied when moving across the ground, applying an opposing force that scales with current velocity.
	 * This can be used to simulate slippery surfaces such as ice or oil by changing the value (possibly based on the material pawn is standing on).
	 * @see BrakingDecelerationWalking, BrakingFriction, bUseSeparateBrakingFriction, BrakingFrictionFactor
	 */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	float GroundFrictionCrouched;
	
	/**
	 * Setting that affects movement control. Higher values allow faster changes in direction.
	 * If bUseSeparateBrakingFriction is false, also affects the ability to stop more quickly when braking (whenever Acceleration is zero), where it is multiplied by BrakingFrictionFactor.
	 * When braking, this property allows you to control how much friction is applied when moving across the ground, applying an opposing force that scales with current velocity.
	 * This can be used to simulate slippery surfaces such as ice or oil by changing the value (possibly based on the material pawn is standing on).
	 * @see BrakingDecelerationWalking, BrakingFriction, bUseSeparateBrakingFriction, BrakingFrictionFactor
	 */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0"))
	float GroundFrictionProned;

	/**
	 * Friction (drag) coefficient applied when braking (whenever Acceleration = 0, or if character is exceeding max speed); actual value used is this multiplied by BrakingFrictionFactor.
	 * When braking, this property allows you to control how much friction is applied when moving across the ground, applying an opposing force that scales with current velocity.
	 * Braking is composed of friction (velocity-dependent drag) and constant deceleration.
	 * This is the current value, used in all movement modes; if this is not desired, override it or bUseSeparateBrakingFriction when movement mode changes.
	 * @note Only used if bUseSeparateBrakingFriction setting is true, otherwise current friction such as GroundFriction is used.
	 * @see bUseSeparateBrakingFriction, BrakingFrictionFactor, GroundFriction, BrakingDecelerationWalking
	 */
	UPROPERTY(Category="Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", EditCondition="bUseSeparateBrakingFriction"))
	float BrakingFrictionCrouched;
	
	/**
	 * Friction (drag) coefficient applied when braking (whenever Acceleration = 0, or if character is exceeding max speed); actual value used is this multiplied by BrakingFrictionFactor.
	 * When braking, this property allows you to control how much friction is applied when moving across the ground, applying an opposing force that scales with current velocity.
	 * Braking is composed of friction (velocity-dependent drag) and constant deceleration.
	 * This is the current value, used in all movement modes; if this is not desired, override it or bUseSeparateBrakingFriction when movement mode changes.
	 * @note Only used if bUseSeparateBrakingFriction setting is true, otherwise current friction such as GroundFriction is used.
	 * @see bUseSeparateBrakingFriction, BrakingFrictionFactor, GroundFriction, BrakingDecelerationWalking
	 */
	UPROPERTY(Category="Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", EditCondition="bUseSeparateBrakingFriction"))
	float BrakingFrictionProned;
	
	/** Collision half-height when proned (component scale is applied separately) */
	UPROPERTY(Category="Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits=cm))
	float PronedHalfHeight;

	/** Collision half-height when proned (component scale is applied separately) */
	UPROPERTY(Category="Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits=cm))
	float PronedRadius;

	/**
	 * Cannot leave prone for this duration
	 * @see ClearProneLock
	 */
	UPROPERTY(Category="Character Movement (General Settings)", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits=cm))
	float ProneLockDuration;
	
	/** If true, Character can walk off a ledge when proned. */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite)
	uint8 bCanWalkOffLedgesWhenProned:1;

	/** If true, Character can sprint when prone. */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite)
	uint8 bCanSprintDuringProne:1;

	/** If true, Character can sprint when crouching. */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite)
	uint8 bCanSprintDuringCrouch:1;
	
	/** If true, Character can jump when prone. */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite)
	uint8 bCanJumpDuringProne:1;
	
	/** If true, Character can jump when crouching. */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite)
	uint8 bCanJumpDuringCrouch:1;
	
public:
	/** If true, try to Prone (or keep Proned) on next update. If false, try to stop Proned on next update. */
	UPROPERTY(Category="Character Movement (General Settings)", VisibleInstanceOnly, BlueprintReadOnly)
	uint8 bWantsToProne:1;

	UPROPERTY(Category="Character Movement (General Settings)", VisibleInstanceOnly, BlueprintReadOnly)
	uint8 bProneLocked:1;

protected:
	float ProneLockTimestamp = -1.f;
	
public:
	/**
	 * Boost modifies movement properties such as speed and acceleration
	 * Scaling applied on a per-Boost-level basis
	 */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite)
	TMap<FGameplayTag, FMovementModifierParams> Boost;

	/**
	 * Limits the maximum number of Boost levels that can be applied to the character
	 * This value is shared between each type of Boost
	 * It limits both the number being serialized and sent over the network, as well as having gameplay implications
	 * Priority is granted in order, because modifiers consume the remaining slots, so LocalPredicted -> WithCorrection - ServerInitiated
	 */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite, meta=(InlineEditConditionToggle))
	bool bLimitMaxBoosts = true;
	
	/**
	 * Maximum number of Boost levels that can be applied to the character
	 * This value is shared between each type of Boost
	 * It limits both the number being serialized and sent over the network, as well as having gameplay implications
	 * Priority is granted in order, because modifiers consume the remaining slots, so LocalPredicted -> WithCorrection - ServerInitiated
	 */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite, meta=(ClampMin=1, UIMin=1, UIMax=32, EditCondition="bLimitMaxBoosts"))
	int32 MaxBoosts = 8;

	/** Indexed list of Boost levels, used to determine the current Boost level based on index */
	UPROPERTY()
	TArray<FGameplayTag> BoostLevels;

	/** The method used to calculate Boost levels */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite)
	EModifierLevelMethod BoostLevelMethod;
	
	/** Local Predicted Boost based on Player Input */
	TMod_Local BoostLocal;

	/** Local Predicted Boost based on Player Input, that can be corrected by the server when a mismatch occurs */
	TMod_LocalCorrection BoostCorrection;

public:
	/**
	 * Haste modifies movement properties such as speed and acceleration
	 * Scaling applied on a per-Haste-level basis
	 */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite)
	TMap<FGameplayTag, FMovementModifierParams> Haste;

	/**
	 * Limits the maximum number of Haste levels that can be applied to the character
	 * This value is shared between each type of Haste
	 * It limits both the number being serialized and sent over the network, as well as having gameplay implications
	 * Priority is granted in order, because modifiers consume the remaining slots, so LocalPredicted -> WithCorrection - ServerInitiated
	 */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite, meta=(InlineEditConditionToggle))
	bool bLimitMaxHastes = true;
	
	/**
	 * Maximum number of Haste levels that can be applied to the character
	 * This value is shared between each type of Haste
	 * It limits both the number being serialized and sent over the network, as well as having gameplay implications
	 * Priority is granted in order, because modifiers consume the remaining slots, so LocalPredicted -> WithCorrection - ServerInitiated
	 */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite, meta=(ClampMin=1, UIMin=1, UIMax=32, EditCondition="bLimitMaxHastes"))
	int32 MaxHastes = 8;

	/** Indexed list of Haste levels, used to determine the current Haste level based on index */
	UPROPERTY()
	TArray<FGameplayTag> HasteLevels;

	/** The method used to calculate Haste levels */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite)
	EModifierLevelMethod HasteLevelMethod;
	
	/** Local Predicted Haste based on Player Input */
	TMod_Local HasteLocal;

	/** Local Predicted Haste based on Player Input, that can be corrected by the server when a mismatch occurs */
	TMod_LocalCorrection HasteCorrection;

public:
	/**
	 * Slow modifies movement properties such as speed and acceleration
	 * Scaling applied on a per-Slow-level basis
	 */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite)
	TMap<FGameplayTag, FMovementModifierParams> Slow;

	/**
	 * Limits the maximum number of Slow levels that can be applied to the character
	 * This value is shared between each type of Slow
	 * It limits both the number being serialized and sent over the network, as well as having gameplay implications
	 * Priority is granted in order, because modifiers consume the remaining slots, so LocalPredicted -> WithCorrection - ServerInitiated
	 */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite, meta=(InlineEditConditionToggle))
	bool bLimitMaxSlows = true;
	
	/**
	 * Maximum number of Slow levels that can be applied to the character
	 * This value is shared between each type of Slow
	 * It limits both the number being serialized and sent over the network, as well as having gameplay implications
	 * Priority is granted in order, because modifiers consume the remaining slots, so LocalPredicted -> WithCorrection - ServerInitiated
	 */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite, meta=(ClampMin=1, UIMin=1, UIMax=32, EditCondition="bLimitMaxSlows"))
	int32 MaxSlows = 8;

	/** Indexed list of Slow levels, used to determine the current Slow level based on index */
	UPROPERTY()
	TArray<FGameplayTag> SlowLevels;

	/** The method used to calculate Slow levels */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite)
	EModifierLevelMethod SlowLevelMethod;
	
	/** Local Predicted Slow based on Player Input */
	TMod_Local SlowLocal;

	/** Local Predicted Slow based on Player Input, that can be corrected by the server when a mismatch occurs */
	TMod_LocalCorrection SlowCorrection;
	
public:
	/**
	 * Snare modifies movement properties such as speed and acceleration
	 * Scaling applied on a per-Snare-level basis
	 */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite)
	TMap<FGameplayTag, FMovementModifierParams> Snare;

	/**
	 * Limits the maximum number of Snare levels that can be applied to the character
	 * This value is shared between each type of Snare
	 * It limits both the number being serialized and sent over the network, as well as having gameplay implications
	 */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite, meta=(InlineEditConditionToggle))
	bool bLimitMaxSnares = true;
	
	/**
	 * Maximum number of Snare levels that can be applied to the character
	 * This value is shared between each type of Snare
	 * It limits both the number being serialized and sent over the network, as well as having gameplay implications
	 */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite, meta=(ClampMin=1, UIMin=1, UIMax=32, EditCondition="bLimitMaxSnares"))
	int32 MaxSnares = 8;

	/** Indexed list of Snare levels, used to determine the current Snare level based on index */
	UPROPERTY()
	TArray<FGameplayTag> SnareLevels;

	/** The method used to calculate Snare levels */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite)
	EModifierLevelMethod SnareLevelMethod;

	/** Server Initiated Snare that is sent to the Client via a correction */
	TMod_Server SnareServer;

public:
	/**
	 * SlowFall changes falling properties, such as gravity and air control
	 * Scaling applied on a per-SlowFall-level basis
	 */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite)
	TMap<FGameplayTag, FFallingModifierParams> SlowFall;

	/**
	 * Limits the maximum number of SlowFall levels that can be applied to the character
	 * This value is shared between each type of SlowFall
	 * It limits both the number being serialized and sent over the network, as well as having gameplay implications
	 * Priority is granted in order, because modifiers consume the remaining slots, so LocalPredicted -> WithCorrection - ServerInitiated
	 */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite, meta=(InlineEditConditionToggle))
	bool bLimitMaxSlowFalls = true;
	
	/**
	 * Maximum number of SlowFall levels that can be applied to the character
	 * This value is shared between each type of SlowFall
	 * It limits both the number being serialized and sent over the network, as well as having gameplay implications
	 * Priority is granted in order, because modifiers consume the remaining slots, so LocalPredicted -> WithCorrection - ServerInitiated
	 */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite, meta=(ClampMin=1, UIMin=1, UIMax=32, EditCondition="bLimitMaxSlowFalls"))
	int32 MaxSlowFalls = 8;

	/** Indexed list of SlowFall levels, used to determine the current SlowFall level */
	UPROPERTY()
	TArray<FGameplayTag> SlowFallLevels;

	/** The method used to calculate SlowFall levels */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite)
	EModifierLevelMethod SlowFallLevelMethod;

	/** Local Predicted SlowFall based on Player Input */
	TMod_Local SlowFallLocal;

	/** Local Predicted SlowFall based on Player Input, that can be corrected by the server when a mismatch occurs */
	TMod_LocalCorrection SlowFallCorrection;
	
public:
	/** Client auth parameters mapped to a source gameplay tag */
	UPROPERTY(Category="Character Movement (Networking)", EditAnywhere, BlueprintReadOnly)
	TMap<FGameplayTag, FClientAuthParams> ClientAuthParams;

	UPROPERTY()
	FClientAuthStack ClientAuthStack;

	UPROPERTY()
	float ClientAuthAlpha = 0.f;

	UPROPERTY()
	uint64 ClientAuthIdCounter = 0;
	
public:
	UPredictedCharacterMovement(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	
	virtual bool HasValidData() const override;
	virtual void PostLoad() override;
	virtual void OnRegister() override;
	virtual void SetUpdatedComponent(USceneComponent* NewUpdatedComponent) override;
	virtual void SetUpdatedCharacter();

	virtual void BeginPlay() override;
	
public:
	virtual EPredGaitMode GetGaitMode() const;
	virtual EPredGaitMode GetGaitSpeed() const;

	virtual bool IsGaitAtSpeed(float Mitigator) const;
	virtual bool IsStrolling() const;
	virtual bool IsWalk() const;  // Do not mistake this for UCharacterMovementComponent::IsWalking
	virtual bool IsWalkingAtSpeed() const { return IsWalk() && IsGaitAtSpeed(VelocityCheckMitigatorWalking); }
	virtual bool IsRunning() const;
	virtual bool IsRunningAtSpeed() const { return IsRunning() && IsGaitAtSpeed(VelocityCheckMitigatorRunning); }
	virtual bool IsSprintingAtSpeed() const { return IsSprinting() && IsGaitAtSpeed(VelocityCheckMitigatorSprinting); }
	virtual bool IsSprintingInEffect() const { return IsSprintingAtSpeed() && IsSprintWithinAllowableInputAngle(); }

public:
	// Movement scalars

	/** Max speed scalar without sprinting checks */
	virtual float GetGaitSpeedFactor() const;
	
	virtual float GetMaxAccelerationScalar() const;
	virtual float GetMaxSpeedScalar() const;
	virtual float GetMaxBrakingDecelerationScalar() const;
	virtual float GetGroundFrictionScalar() const;
	virtual float GetBrakingFrictionScalar() const;
	virtual float GetGravityZScalar() const;
	virtual float GetRootMotionTranslationScalar() const;

	// Base movement values
	
	virtual float GetBaseMaxAcceleration() const;
	virtual float GetBaseMaxSpeed() const;
	virtual float GetBaseMaxBrakingDeceleration() const;
	virtual float GetBaseGroundFriction(float DefaultGroundFriction) const;
	virtual float GetBaseBrakingFriction() const;

	// Final movement values
	
	virtual float GetMaxAcceleration() const override { return GetBaseMaxAcceleration() * GetMaxAccelerationScalar(); }
	virtual float GetMaxSpeed() const override { return GetBaseMaxSpeed() * GetMaxSpeedScalar(); }
	virtual float GetMaxBrakingDeceleration() const override { return GetBaseMaxBrakingDeceleration() * GetMaxBrakingDecelerationScalar(); }
	virtual float GetGroundFriction(float DefaultGroundFriction) const { return GetBaseGroundFriction(DefaultGroundFriction) * GetGroundFrictionScalar(); }
	virtual float GetBrakingFriction() const { return GetBaseBrakingFriction() * GetBrakingFrictionScalar(); }

	// Falling
	
	virtual float GetGravityZ() const override;
	virtual FVector GetAirControl(float DeltaTime, float TickAirControl, const FVector& FallAcceleration) override;

public:	
	virtual void CalcStamina(float DeltaTime);
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	virtual void ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration) override;

public:
	virtual bool CanWalkOffLedges() const override;
	virtual bool CanAttemptJump() const override;

public:
	/**
	 * Call CharacterOwner->OnStartStroll() if successful.
	 * In general, you should set bWantsToStroll instead to have the Stroll persist during movement, or just use the Stroll functions on the owning Character.
	 * @param	bClientSimulation	true when called when bIsStrolled is replicated to non owned clients.
	 */
	virtual void Stroll(bool bClientSimulation = false);

	/**
	 * Checks if default capsule size fits (no encroachment), and trigger OnEndStroll() on the owner if successful.
	 * @param	bClientSimulation	true when called when bIsStrolled is replicated to non owned clients.
	 */
	virtual void UnStroll(bool bClientSimulation = false);

	/** Returns true if the character is allowed to Stroll in the current state. */
	virtual bool CanStrollInCurrentState() const;
	
public:
	/**
	 * Call CharacterOwner->OnStartWalk() if successful.
	 * In general, you should set bWantsToWalk instead to have the Walk persist during movement, or just use the Walk functions on the owning Character.
	 * @param	bClientSimulation	true when called when bIsWalked is replicated to non owned clients.
	 */
	virtual void Walk(bool bClientSimulation = false);

	/**
	 * Checks if default capsule size fits (no encroachment), and trigger OnEndWalk() on the owner if successful.
	 * @param	bClientSimulation	true when called when bIsWalked is replicated to non owned clients.
	 */
	virtual void UnWalk(bool bClientSimulation = false);

	/** Returns true if the character is allowed to Walk in the current state. */
	virtual bool CanWalkInCurrentState() const;
	
public:
	UFUNCTION(BlueprintCallable, Category="Character Movement: Walking")
	void SetMaxInputAngleSprint(float InMaxAngleSprint);
	
	virtual bool IsSprinting() const;

	/**
	 * Call CharacterOwner->OnStartSprint() if successful.
	 * In general, you should set bWantsToSprint instead to have the Sprint persist during movement, or just use the Sprint functions on the owning Character.
	 * @param	bClientSimulation	true when called when bIsSprinted is replicated to non owned clients.
	 */
	virtual void Sprint(bool bClientSimulation = false);
	
	/**
	 * Checks if default capsule size fits (no encroachment), and trigger OnEndSprint() on the owner if successful.
	 * @param	bClientSimulation	true when called when bIsSprinted is replicated to non owned clients.
	 */
	virtual void UnSprint(bool bClientSimulation = false);

	/** Returns true if the character is allowed to Sprint in the current state. */
	virtual bool CanSprintInCurrentState() const;

	/**
	 * This check ensures that we are not sprinting backward or sideways, while allowing leeway 
	 * This angle allows sprinting when holding forward, forward left, forward right
	 * but not left or right or backward)
	 */
	virtual bool IsSprintWithinAllowableInputAngle() const;

public:
	float GetStamina() const { return Stamina; }
	float GetStaminaPct() const { return Stamina / MaxStamina; }
	float GetMaxStamina() const { return MaxStamina; }
	bool IsStaminaDrained() const { return bStaminaDrained; }
	bool IsStaminaRecovered() const
	{
		return bStaminaRecoveryFromPct ? GetStaminaPct() >= StaminaRecoveryPct : GetStamina() >= StaminaRecoveryAmount;
	}

	void SetStamina(float NewStamina);
	void SetMaxStamina(float NewMaxStamina);
	void SetStaminaDrained(bool bNewValue);
	
protected:
	/*
	 * Drain state entry and exit is handled here. Drain state is used to prevent rapid re-entry of sprinting or other
	 * such abilities before sufficient stamina has regenerated. However, in the default implementation, 100%
	 * stamina must be regenerated. Consider overriding this, check the implementation's comment for more information.
	 */
	virtual void OnStaminaChanged(float PrevValue, float NewValue);
	virtual void OnMaxStaminaChanged(float PrevValue, float NewValue);
	virtual void OnStaminaDrained();
	virtual void OnStaminaDrainRecovered();

public:
	virtual bool IsAimingDownSights() const;

	/**
	 * Call CharacterOwner->OnStartAimDownSights() if successful.
	 * In general, you should set bWantsToAimDownSights instead to have the AimDownSights persist during movement, or just use the AimDownSights functions on the owning Character.
	 * @param	bClientSimulation	true when called when bIsAimDownSightsed is replicated to non owned clients.
	 */
	virtual void AimDownSights(bool bClientSimulation = false);
	
	/**
	 * Checks if default capsule size fits (no encroachment), and trigger OnEndAimDownSights() on the owner if successful.
	 * @param	bClientSimulation	true when called when bIsAimDownSightsed is replicated to non owned clients.
	 */
	virtual void UnAimDownSights(bool bClientSimulation = false);

	/** Returns true if the character is allowed to AimDownSights in the current state. By default it is allowed when walking or falling. */
	virtual bool CanAimDownSightsInCurrentState() const;

public:
	UFUNCTION(BlueprintPure, Category="Character Movement")
	bool IsProneLocked() const;

	bool IsProneLockOnTimer() const;

	// Prone lock timer
	float GetRemainingProneLockCooldown() const;

	void SetProneLock(bool bLock);

	float GetTimestamp() const;

public:
	virtual bool IsProned() const;

	/**
	 * Call CharacterOwner->OnStartProne() if successful.
	 * In general, you should set bWantsToProne instead to have the Prone persist during movement, or just use the Prone functions on the owning Character.
	 * @param	bClientSimulation	true when called when bIsProned is replicated to non owned clients.
	 */
	virtual void Prone(bool bClientSimulation = false);
	
	/**
	 * Checks if default capsule size fits (no encroachment), and trigger OnEndProne() on the owner if successful.
	 * @param	bClientSimulation	true when called when bIsProned is replicated to non owned clients.
	 */
	virtual void UnProne(bool bClientSimulation = false);

	/** Returns true if the character is allowed to Prone in the current state. By default it is allowed when walking or falling. */
	virtual bool CanProneInCurrentState() const;

	virtual bool CanCrouchInCurrentState() const override;
	
public:
	/* Boost Implementation */

	uint8 BoostLevel = NO_MODIFIER;
	bool IsBoostActive() const { return BoostLevel != NO_MODIFIER; }
	const FMovementModifierParams* GetBoostParams() const { return Boost.Find(GetBoostLevel()); }
	FGameplayTag GetBoostLevel() const { return BoostLevels.IsValidIndex(BoostLevel) ? BoostLevels[BoostLevel] : FGameplayTag::EmptyTag; }
	uint8 GetBoostLevelIndex(const FGameplayTag& Level) const { return BoostLevels.IndexOfByKey(Level) > INDEX_NONE ? BoostLevels.IndexOfByKey(Level) : NO_MODIFIER; }
	virtual bool CanBoostInCurrentState() const;

	float GetBoostSpeedScalar() const { return GetBoostParams() ? GetBoostParams()->MaxWalkSpeed : 1.f; }
	float GetBoostAccelScalar() const { return GetBoostParams() ? GetBoostParams()->MaxAcceleration : 1.f; }
	float GetBoostBrakingScalar() const { return GetBoostParams() ? GetBoostParams()->BrakingDeceleration : 1.f; }
	float GetBoostGroundFrictionScalar() const { return GetBoostParams() ? GetBoostParams()->GroundFriction : 1.f; }
	float GetBoostBrakingFrictionScalar() const { return GetBoostParams() ? GetBoostParams()->BrakingFriction : 1.f; }
	bool BoostAffectsRootMotion() const { return GetBoostParams() ? GetBoostParams()->bAffectsRootMotion : false; }
	
	/* ~Boost Implementation */

public:
	/* Haste Implementation */

	uint8 HasteLevel = NO_MODIFIER;
	bool IsHasteActive() const { return HasteLevel != NO_MODIFIER; }
	const FMovementModifierParams* GetHasteParams() const { return Haste.Find(GetHasteLevel()); }
	FGameplayTag GetHasteLevel() const { return HasteLevels.IsValidIndex(HasteLevel) ? HasteLevels[HasteLevel] : FGameplayTag::EmptyTag; }
	uint8 GetHasteLevelIndex(const FGameplayTag& Level) const { return HasteLevels.IndexOfByKey(Level) > INDEX_NONE ? HasteLevels.IndexOfByKey(Level) : NO_MODIFIER; }
	virtual bool CanHasteInCurrentState() const;

	float GetHasteSpeedScalar() const { return GetHasteParams() ? GetHasteParams()->MaxWalkSpeed : 1.f; }
	float GetHasteAccelScalar() const { return GetHasteParams() ? GetHasteParams()->MaxAcceleration : 1.f; }
	float GetHasteBrakingScalar() const { return GetHasteParams() ? GetHasteParams()->BrakingDeceleration : 1.f; }
	float GetHasteGroundFrictionScalar() const { return GetHasteParams() ? GetHasteParams()->GroundFriction : 1.f; }
	float GetHasteBrakingFrictionScalar() const { return GetHasteParams() ? GetHasteParams()->BrakingFriction : 1.f; }
	bool HasteAffectsRootMotion() const { return GetHasteParams() ? GetHasteParams()->bAffectsRootMotion : false; }
	
	/* ~Haste Implementation */

public:
	/* Slow Implementation */

	uint8 SlowLevel = NO_MODIFIER;
	bool IsSlowActive() const { return SlowLevel != NO_MODIFIER; }
	const FMovementModifierParams* GetSlowParams() const { return Slow.Find(GetSlowLevel()); }
	FGameplayTag GetSlowLevel() const { return SlowLevels.IsValidIndex(SlowLevel) ? SlowLevels[SlowLevel] : FGameplayTag::EmptyTag; }
	uint8 GetSlowLevelIndex(const FGameplayTag& Level) const { return SlowLevels.IndexOfByKey(Level) > INDEX_NONE ? SlowLevels.IndexOfByKey(Level) : NO_MODIFIER; }
	virtual bool CanSlowInCurrentState() const;

	float GetSlowSpeedScalar() const { return GetSlowParams() ? GetSlowParams()->MaxWalkSpeed : 1.f; }
	float GetSlowAccelScalar() const { return GetSlowParams() ? GetSlowParams()->MaxAcceleration : 1.f; }
	float GetSlowBrakingScalar() const { return GetSlowParams() ? GetSlowParams()->BrakingDeceleration : 1.f; }
	float GetSlowGroundFrictionScalar() const { return GetSlowParams() ? GetSlowParams()->GroundFriction : 1.f; }
	float GetSlowBrakingFrictionScalar() const { return GetSlowParams() ? GetSlowParams()->BrakingFriction : 1.f; }
	bool SlowAffectsRootMotion() const { return GetSlowParams() ? GetSlowParams()->bAffectsRootMotion : false; }
	
	/* ~Slow Implementation */
	
public:
	/* Snare Implementation */

	uint8 SnareLevel = NO_MODIFIER;
	bool IsSnareActive() const { return SnareLevel != NO_MODIFIER; }
	const FMovementModifierParams* GetSnareParams() const { return Snare.Find(GetSnareLevel()); }
	FGameplayTag GetSnareLevel() const { return SnareLevels.IsValidIndex(SnareLevel) ? SnareLevels[SnareLevel] : FGameplayTag::EmptyTag; }
	uint8 GetSnareLevelIndex(const FGameplayTag& Level) const { return SnareLevels.IndexOfByKey(Level) > INDEX_NONE ? SnareLevels.IndexOfByKey(Level) : NO_MODIFIER; }
	virtual bool CanSnareInCurrentState() const;

	float GetSnareSpeedScalar() const { return GetSnareParams() ? GetSnareParams()->MaxWalkSpeed : 1.f; }
	float GetSnareAccelScalar() const { return GetSnareParams() ? GetSnareParams()->MaxAcceleration : 1.f; }
	float GetSnareBrakingScalar() const { return GetSnareParams() ? GetSnareParams()->BrakingDeceleration : 1.f; }
	float GetSnareGroundFrictionScalar() const { return GetSnareParams() ? GetSnareParams()->GroundFriction : 1.f; }
	float GetSnareBrakingFrictionScalar() const { return GetSnareParams() ? GetSnareParams()->BrakingFriction : 1.f; }
	bool SnareAffectsRootMotion() const { return GetSnareParams() ? GetSnareParams()->bAffectsRootMotion : false; }
	
	/* ~Snare Implementation */

public:
	/* SlowFall Implementation */

	uint8 SlowFallLevel = NO_MODIFIER;
	bool IsSlowFallActive() const { return SlowFallLevel != NO_MODIFIER; }
	const FFallingModifierParams* GetSlowFallParams() const { return SlowFall.Find(GetSlowFallLevel()); }
	FGameplayTag GetSlowFallLevel() const { return SlowFallLevels.IsValidIndex(SlowFallLevel) ? SlowFallLevels[SlowFallLevel] : FGameplayTag::EmptyTag; }
	uint8 GetSlowFallLevelIndex(const FGameplayTag& Level) const { return SlowFallLevels.IndexOfByKey(Level) > INDEX_NONE ? SlowFallLevels.IndexOfByKey(Level) : NO_MODIFIER; }
	virtual bool CanSlowFallInCurrentState() const;

	virtual float GetSlowFallGravityZScalar() const { return GetSlowFallParams() ? GetSlowFallParams()->GetGravityScalar(Velocity) : 1.f; }
	virtual bool RemoveVelocityZOnSlowFallStart() const;

	/* ~SlowFall Implementation */
	
public:
	virtual void ProcessModifierMovementState();
	virtual void UpdateModifierMovementState();

	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void UpdateCharacterStateAfterMovement(float DeltaSeconds) override;

public:
	/* Client Auth Implementation */

	virtual FClientAuthData* ProcessClientAuthData();
	FClientAuthParams* GetClientAuthParamsForSource(const FGameplayTag& Source) { return ClientAuthParams.Find(Source); }
	virtual FClientAuthParams GetClientAuthParams(const FClientAuthData* ClientAuthData);

protected:
	/**
	 * Called when the client's position is rejected by the server entirely due to excessive difference
	 * @param ClientLoc The client's location
	 * @param ServerLoc The server's location
	 * @param LocDiff The difference between the client and server locations
	 */
	virtual void OnClientAuthRejected(const FVector& ClientLoc, const FVector& ServerLoc, const FVector& LocDiff) {}

public:
	/** 
	 * Grant the client position authority, based on the current state of the character.
	 * @param ClientAuthSource What the client is requesting authority for, not used by default, requires override
	 * @param OverrideDuration Override the default client authority time, -1.f to use default
	 */
	virtual void GrantClientAuthority(FGameplayTag ClientAuthSource, float OverrideDuration = -1.f);

protected:
	virtual bool ServerShouldGrantClientPositionAuthority(FVector& ClientLoc, FClientAuthData*& AuthData);
	
	/* ~Client Auth Implementation */

public:
	virtual void ServerMove_PerformMovement(const FCharacterNetworkMoveData& MoveData) override;

protected:
	virtual bool ServerCheckClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel,
		const FVector& ClientWorldLocation, const FVector& RelativeClientLocation,
		UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode) override;

	virtual void ServerMoveHandleClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel,
		const FVector& RelativeClientLocation, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName,
		uint8 ClientMovementMode) override;

public:
	virtual void ClientAdjustPosition_Implementation(float TimeStamp, FVector NewLoc, FVector NewVel,
		UPrimitiveComponent* NewBase, FName NewBaseBoneName, bool bHasBase, bool bBaseRelativePosition,
		uint8 ServerMovementMode, TOptional<FRotator> OptionalRotation = TOptional<FRotator>()) override;

protected:
	virtual void OnClientCorrectionReceived(class FNetworkPredictionData_Client_Character& ClientData, float TimeStamp,
		FVector NewLocation, FVector NewVelocity, UPrimitiveComponent* NewBase, FName NewBaseBoneName, bool bHasBase,
		bool bBaseRelativePosition, uint8 ServerMovementMode
#if UE_5_03_OR_LATER
		, FVector ServerGravityDirection) override;
#else
	) override;
#endif

	virtual bool ClientUpdatePositionAfterServerUpdate() override;

protected:
	virtual void TickCharacterPose(float DeltaTime) override;  // ACharacter::GetAnimRootMotionTranslationScale() is non-virtual so we have to duplicate this entire function
	
private:
	FPredictedNetworkMoveDataContainer PredMoveDataContainer;
	FPredictedMoveResponseDataContainer PredMoveResponseDataContainer;
	
public:
	/** Get prediction data for a client game. Should not be used if not running as a client. Allocates the data on demand and can be overridden to allocate a custom override if desired. Result must be a FNetworkPredictionData_Client_Character. */
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;

protected:
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
};

class PREDICTEDMOVEMENT_API FPredictedSavedMove : public FSavedMove_Character
{
	using Super = FSavedMove_Character;

public:
	FPredictedSavedMove()
		: bWantsToAimDownSights(0)
		, bWantsToProne(0)
		, bProneLocked(0)
		, bWantsToStroll(0)
		, bWantsToWalk(0)
		, bWantsToSprint(0)
		, bStaminaDrained(false)
		, StartStamina(0)
		, EndStamina(0)
	{}

	virtual ~FPredictedSavedMove() override
	{}

	uint8 bWantsToAimDownSights:1;
	
	uint8 bWantsToProne:1;
	uint8 bProneLocked:1;
	
	uint8 bWantsToStroll:1;
	uint8 bWantsToWalk:1;
	uint8 bWantsToSprint:1;
	uint8 bStaminaDrained:1;
	
	float StartStamina;
	float EndStamina;

	// Movement Modifiers
	
	FModifierSavedMove BoostLocal;							// Boost
	FModifierSavedMove_WithCorrection BoostCorrection;		// Boost
	FModifierSavedMove HasteLocal;							// Haste
	FModifierSavedMove_WithCorrection HasteCorrection;		// Haste
	FModifierSavedMove SlowLocal;							// Slow
	FModifierSavedMove_WithCorrection SlowCorrection;		// Slow
	FModifierSavedMove_ServerInitiated SnareServer;			// Snare
	FModifierSavedMove SlowFallLocal; 						// SlowFall
	FModifierSavedMove_WithCorrection SlowFallCorrection;	// SlowFall

	uint8 BoostLevel = NO_MODIFIER;
	uint8 HasteLevel = NO_MODIFIER;
	uint8 SlowLevel = NO_MODIFIER;
	uint8 SnareLevel = NO_MODIFIER;
	uint8 SlowFallLevel = NO_MODIFIER;

	/** Returns a byte containing encoded special movement information (jumping, crouching, etc.)	 */
	virtual uint8 GetCompressedFlags() const override;
	
	/** Clear saved move properties, so it can be re-used. */
	virtual void Clear() override;

	/** Called to set up this saved move (when initially created) to make a predictive correction. */
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character & ClientData) override;

	/** Called before ClientUpdatePosition uses this SavedMove to make a predictive correction	 */
	virtual void PrepMoveFor(ACharacter* C) override;
	
	/** Returns true if this move can be combined with NewMove for replication without changing any behavior */
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
	
	/** Set the properties describing the position, etc. of the moved pawn at the start of the move. */
	virtual void SetInitialPosition(ACharacter* C) override;

	/** Combine this move with an older move and update relevant state. */
	virtual void CombineWith(const FSavedMove_Character* OldMove, ACharacter* InCharacter, APlayerController* PC, const FVector& OldStartLocation) override;

	/** Set the properties describing the final position, etc. of the moved pawn. */
	virtual void PostUpdate(ACharacter* C, EPostUpdateMode PostUpdateMode) override;

	/** Returns true if this move is an "important" move that should be sent again if not acked by the server */
	virtual bool IsImportantMove(const FSavedMovePtr& LastAckedMove) const override;
};

class PREDICTEDMOVEMENT_API FPredictedNetworkPredictionData_Client : public FNetworkPredictionData_Client_Character
{
	using Super = FNetworkPredictionData_Client_Character;

public:
	FPredictedNetworkPredictionData_Client(const UCharacterMovementComponent& ClientMovement)
		: Super(ClientMovement)
	{}

	virtual FSavedMovePtr AllocateNewMove() override;
};
