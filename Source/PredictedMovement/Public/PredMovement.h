// Copyright (c) 2023 Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "ModifierTypes.h"
#include "PredTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "System/PredictedMovementVersioning.h"
#include "PredMovement.generated.h"

class APredCharacter;

struct PREDICTEDMOVEMENT_API FPredMoveResponseDataContainer : FCharacterMoveResponseDataContainer
{  // Server ➜ Client
	using Super = FCharacterMoveResponseDataContainer;

	float Stamina;
	bool bStaminaDrained;
	
	FMovementModifier Snare;
	
	virtual void ServerFillResponseData(const UCharacterMovementComponent& CharacterMovement, const FClientAdjustment& PendingAdjustment) override;
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap) override;
};

struct PREDICTEDMOVEMENT_API FPredNetworkMoveData : FCharacterNetworkMoveData
{  // Client ➜ Server
public:
	using Super = FCharacterNetworkMoveData;
 
	FPredNetworkMoveData()
		: Stamina(0)
	{}

	float Stamina;

	FMovementModifier Boost;
	FMovementModifier Haste;
	FMovementModifier Slow;
	FMovementModifier SlowFall;
	FMovementModifier Snare;

	virtual void ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType) override;
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType) override;
};
 
struct PREDICTEDMOVEMENT_API FPredNetworkMoveDataContainer : FCharacterNetworkMoveDataContainer
{  // Client ➜ Server
public:
	using Super = FCharacterNetworkMoveDataContainer;
 
	FPredNetworkMoveDataContainer()
	{
		NewMoveData = &MoveData[0];
		PendingMoveData = &MoveData[1];
		OldMoveData = &MoveData[2];
	}
 
private:
	FPredNetworkMoveData MoveData[3];
};

/**
 * Identical to the main branch implementation, except using move containers instead of compressed flags
 * This exists for teaching purposes, to show how to use move containers that are 1:1 with the compressed flags
 * Typically compressed flags are only used for a boolean, and move containers are used for more complex data
 * However, compressed flags are a lot cheaper so we wouldn't typically use move containers for a boolean
 */
UCLASS()
class PREDICTEDMOVEMENT_API UPredMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()
	
private:
	/** Character movement component belongs to */
	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<APredCharacter> PredCharacterOwner;

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
	UPredMovement(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

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
	virtual EPredGaitMode GetGaitModeAtSpeed() const;
	
	virtual bool IsStrolling() const;
	virtual bool IsWalk() const;  // Do not mistake this for UCharacterMovementComponent::IsWalking
	virtual bool IsWalkingAtSpeed() const;
	virtual bool IsRunning() const;
	virtual bool IsRunningAtSpeed() const;
	virtual bool IsSprintingAtSpeed() const;
	virtual float GetGaitSpeedFactor() const;
	
	virtual bool IsSprintingInEffect() const { return IsSprintingAtSpeed() && IsSprintWithinAllowableInputAngle(); }
	virtual float GetMaxAccelerationScalar() const;
	virtual float GetMaxSpeedScalar() const;
	virtual float GetMaxBrakingDecelerationScalar() const;
	virtual float GetGroundFrictionScalar() const;
	virtual float GetBrakingFrictionScalar() const;
	virtual float GetGravityZScalar() const;
	virtual float GetRootMotionTranslationScalar() const;

	virtual float GetMaxAcceleration() const override;
	virtual float GetBasicMaxSpeed() const;
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxBrakingDeceleration() const override;
	virtual float GetGroundFriction(float DefaultGroundFriction) const;
	virtual float GetBrakingFriction() const;
	
	virtual float GetGravityZ() const override;
	virtual FVector GetAirControl(float DeltaTime, float TickAirControl, const FVector& FallAcceleration) override;

	virtual void CalcStamina(float DeltaTime);
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	virtual void ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration) override;

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
	/* Boost (Non-Generic) Implementation */
	
	/**
	 * Boost increases movement speed and acceleration
	 * Scaling applied on a per-boost-level basis
	 * Every tag defined here must also be defined in the FModifierData Boost property
	 */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite)
	TMap<FGameplayTag, FMovementModifierParams> BoostLevels;

	/**
	 * If True, Boost will affect root motion
	 * This allows boosts to scale up root motion translation
	 * This is disabled by default because it can increase attack range, dodge range, etc.
	 */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite)
	bool bBoostAffectsRootMotion = false;

	/** Example implementation of a local predicted buff modifier that can stack */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite)
	FMovementModifier Boost;
	
	UFUNCTION(BlueprintCallable, Category="Character Movement: Walking")
	const FGameplayTag& GetBoostLevel() const
	{
		return Boost.GetModifierLevel();
	}

	UFUNCTION(BlueprintPure, Category="Character Movement: Walking")
	bool IsBoosted() const
	{
		return Boost.HasModifier();
	}

	UFUNCTION(BlueprintPure, Category="Character Movement: Walking")
	bool WantsBoost() const
	{
		return Boost.WantsModifier();
	}

	/**
	 * @param ModifierLevel The level to check for - optional, leave empty if check is generic
	 * @return True if we can boost
	 */
	UFUNCTION(BlueprintPure, Category="Character Movement: Walking", meta=(FilterGameplayTag="Modifier.Type.Buff.Boost"))
	virtual bool CanBoostInCurrentState(FGameplayTag ModifierLevel) const;

	virtual void OnStartBoost() {}
	virtual void OnEndBoost() {}

	/* ~Boost (Non-Generic) Implementation */

public:
	/* Haste (Non-Generic) Implementation */
	
	/**
	 * Haste increases movement speed and acceleration but only while Sprinting is in effect
	 * Scaling applied on a per-haste-level basis
	 * Every tag defined here must also be defined in the FModifierData Haste property
	 */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite)
	TMap<FGameplayTag, FMovementModifierParams> HasteLevels;

	/** Example implementation of a local predicted buff modifier that can stack */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite)
	FMovementModifier Haste;
	
	UFUNCTION(BlueprintCallable, Category="Character Movement: Walking")
	const FGameplayTag& GetHasteLevel() const
	{
		return Haste.GetModifierLevel();
	}

	UFUNCTION(BlueprintPure, Category="Character Movement: Walking")
	bool IsHaste() const
	{
		return Haste.HasModifier();
	}

	UFUNCTION(BlueprintPure, Category="Character Movement: Walking")
	bool WantsHaste() const
	{
		return Haste.WantsModifier();
	}

	/**
	 * @param ModifierLevel The level to check for - optional, leave empty if check is generic
	 * @return True if we can Haste
	 */
	UFUNCTION(BlueprintPure, Category="Character Movement: Walking", meta=(FilterGameplayTag="Modifier.Type.Buff.Haste"))
	virtual bool CanHasteInCurrentState(FGameplayTag ModifierLevel) const;

	virtual void OnStartHaste() {}
	virtual void OnEndHaste() {}

	/* ~Haste (Non-Generic) Implementation */

public:
	/* Slow (Non-Generic) Implementation */
	
	/**
	 * Slow decreases movement speed and acceleration
	 * Scaling applied on a per-haste-level basis
	 * Every tag defined here must also be defined in the FModifierData Slow property
	 */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite)
	TMap<FGameplayTag, FMovementModifierParams> SlowLevels;

	/**
	 * If True, Slow will affect root motion
	 * This allows Slows to scale up root motion translation
	 * This is disabled by default because it can increase attack range, dodge range, etc.
	 */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite)
	bool bSlowAffectsRootMotion = false;

	/** Example implementation of a local predicted buff modifier that can stack */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite)
	FMovementModifier Slow;
	
	UFUNCTION(BlueprintCallable, Category="Character Movement: Walking")
	const FGameplayTag& GetSlowLevel() const
	{
		return Slow.GetModifierLevel();
	}

	UFUNCTION(BlueprintPure, Category="Character Movement: Walking")
	bool IsSlowed() const
	{
		return Slow.HasModifier();
	}

	UFUNCTION(BlueprintPure, Category="Character Movement: Walking")
	bool WantsSlow() const
	{
		return Slow.WantsModifier();
	}

	/**
	 * @param ModifierLevel The level to check for - optional, leave empty if check is generic
	 * @return True if we can Slow
	 */
	UFUNCTION(BlueprintPure, Category="Character Movement: Walking", meta=(FilterGameplayTag="Modifier.Type.Debuff.Slow"))
	virtual bool CanSlowInCurrentState(FGameplayTag ModifierLevel) const;

	virtual void OnStartSlow() {}
	virtual void OnEndSlow() {}

	/* ~Slow (Non-Generic) Implementation */
	
public:
	/* SlowFall (Non-Generic) Implementation */
	
	/**
	 * SlowFall reduces falling speed and acceleration
	 * Scaling applied on a per-slow-fall-level basis
	 * Every tag defined here must also be defined in the FModifierData SlowFall property
	 */
	UPROPERTY(Category="Character Movement: Jumping / Falling", EditAnywhere, BlueprintReadWrite)
	TMap<FGameplayTag, FFallingModifierParams> SlowFallLevels;

	/** Example implementation of a local predicted buff modifier that can stack */
	UPROPERTY(Category="Character Movement: Jumping / Falling", EditAnywhere, BlueprintReadWrite)
	FMovementModifier SlowFall;
	
	UFUNCTION(BlueprintCallable, Category="Character Movement: Jumping / Falling")
	const FGameplayTag& GetSlowFallLevel() const
	{
		return SlowFall.GetModifierLevel();
	}

	UFUNCTION(BlueprintPure, Category="Character Movement: Jumping / Falling")
	bool IsSlowFall() const
	{
		return SlowFall.HasModifier();
	}
	
	UFUNCTION(BlueprintPure, Category="Character Movement: Jumping / Falling")
	bool WantsSlowFall() const
	{
		return SlowFall.WantsModifier();
	}

	/**
	 * @param ModifierLevel The level to check for - optional, leave empty if check is generic
	 * @return True if we can slow fall
	 */
	UFUNCTION(BlueprintPure, Category="Character Movement: Jumping / Falling", meta=(FilterGameplayTag="Modifier.Type.Buff.SlowFall"))
	virtual bool CanSlowFallInCurrentState(FGameplayTag ModifierLevel) const;

	virtual void OnStartSlowFall();
	virtual void OnEndSlowFall() {}

	/* ~SlowFall (Non-Generic) Implementation */

public:
	/* Snare (Non-Generic) Implementation */

	/**
	 * Snare reduces movement speed and acceleration
	 * Scaling applied on a per-snare-level basis
	 * Every tag defined here must also be defined in the FModifierData Snare property
	 */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite)
	TMap<FGameplayTag, FMovementModifierParams> SnareLevels;

	/**
	 * If True, Snare will affect root motion
	 * This allows snares to scale down root motion translation
	 */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ForceUnits="x"))
	bool bSnareAffectsRootMotion = true;

	/** Example implementation of an externally applied non-predicted debuff modifier that can stack */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite)
	FMovementModifier Snare;

	UFUNCTION(BlueprintCallable, Category="Character Movement: Walking")
	const FGameplayTag& GetSnareLevel() const
	{
		return Snare.GetModifierLevel();
	}

	UFUNCTION(BlueprintPure, Category="Character Movement: Walking")
	bool IsSnared() const
	{
		return Snare.HasModifier();
	}
	
	UFUNCTION(BlueprintPure, Category="Character Movement: Walking")
	bool WantsSnare() const
	{
		return Snare.WantsModifier();
	}

	/**
	 * @param ModifierLevel The level to check for - optional, leave empty if check is generic
	 * @return True if we can be snared
	 */
	UFUNCTION(BlueprintPure, Category="Character Movement: Walking", meta=(FilterGameplayTag="Modifier.Type.Debuff.Snare"))
	virtual bool CanBeSnaredInCurrentState(FGameplayTag ModifierLevel) const;

	virtual void OnStartSnare() {}
	virtual void OnEndSnare() {}

	/* ~SlowFall (Non-Generic) Implementation */

public:
	const FMovementModifierParams* GetBoostLevelParams() const { return BoostLevels.Find(GetBoostLevel()); }
	const FMovementModifierParams* GetHasteLevelParams() const { return HasteLevels.Find(GetHasteLevel()); }
	const FMovementModifierParams* GetSlowLevelParams() const { return SlowLevels.Find(GetSlowLevel()); }
	const FFallingModifierParams* GetSlowFallLevelParams() const { return SlowFallLevels.Find(GetSlowFallLevel()); }
	const FMovementModifierParams* GetSnareLevelParams() const { return SnareLevels.Find(GetSnareLevel()); }
		
	virtual float GetBoostSpeedScalar() const { return GetBoostLevelParams() ? GetBoostLevelParams()->MaxWalkSpeed : 1.f; }
	virtual float GetBoostAccelScalar() const { return GetBoostLevelParams() ? GetBoostLevelParams()->MaxAcceleration : 1.f; }
	virtual float GetBoostBrakingScalar() const { return GetBoostLevelParams() ? GetBoostLevelParams()->BrakingDeceleration : 1.f; }
	virtual float GetBoostGroundFrictionScalar() const { return GetBoostLevelParams() ? GetBoostLevelParams()->GroundFriction : 1.f; }
	virtual float GetBoostBrakingFrictionScalar() const { return GetBoostLevelParams() ? GetBoostLevelParams()->BrakingFriction : 1.f; }

	virtual float GetHasteSpeedScalar() const { return GetHasteLevelParams() ? GetHasteLevelParams()->MaxWalkSpeed : 1.f; }
	virtual float GetHasteAccelScalar() const { return GetHasteLevelParams() ? GetHasteLevelParams()->MaxAcceleration : 1.f; }
	virtual float GetHasteBrakingScalar() const { return GetHasteLevelParams() ? GetHasteLevelParams()->BrakingDeceleration : 1.f; }
	virtual float GetHasteGroundFrictionScalar() const { return GetHasteLevelParams() ? GetHasteLevelParams()->GroundFriction : 1.f; }
	virtual float GetHasteBrakingFrictionScalar() const { return GetHasteLevelParams() ? GetHasteLevelParams()->BrakingFriction : 1.f; }

	virtual float GetSlowSpeedScalar() const { return GetSlowLevelParams() ? GetSlowLevelParams()->MaxWalkSpeed : 1.f; }
	virtual float GetSlowAccelScalar() const { return GetSlowLevelParams() ? GetSlowLevelParams()->MaxAcceleration : 1.f; }
	virtual float GetSlowBrakingScalar() const { return GetSlowLevelParams() ? GetSlowLevelParams()->BrakingDeceleration : 1.f; }
	virtual float GetSlowGroundFrictionScalar() const { return GetSlowLevelParams() ? GetSlowLevelParams()->GroundFriction : 1.f; }
	virtual float GetSlowBrakingFrictionScalar() const { return GetSlowLevelParams() ? GetSlowLevelParams()->BrakingFriction : 1.f; }

	virtual float GetSlowFallGravityZScalar() const { return GetSlowFallLevelParams() ? GetSlowFallLevelParams()->GetGravityScalar(Velocity) : 1.f; }
	
	virtual float GetSnareSpeedScalar() const { return GetSnareLevelParams() ? GetSnareLevelParams()->MaxWalkSpeed : 1.f; }
	virtual float GetSnareAccelScalar() const { return GetSnareLevelParams() ? GetSnareLevelParams()->MaxAcceleration : 1.f; }
	virtual float GetSnareBrakingScalar() const { return GetSnareLevelParams() ? GetSnareLevelParams()->BrakingDeceleration : 1.f; }
	virtual float GetSnareGroundFrictionScalar() const { return GetSnareLevelParams() ? GetSnareLevelParams()->GroundFriction : 1.f; }
	virtual float GetSnareBrakingFrictionScalar() const { return GetSnareLevelParams() ? GetSnareLevelParams()->BrakingFriction : 1.f; }
	
public:
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void UpdateCharacterStateAfterMovement(float DeltaSeconds) override;

	virtual void ServerMove_PerformMovement(const FCharacterNetworkMoveData& MoveData) override;
	virtual void ClientHandleMoveResponse(const FCharacterMoveResponseDataContainer& MoveResponse) override;
	
	/*
	 * ClientHandleMoveResponse appears more correct because it has a MoveResponse passed in, and
	 * it doesn't require the ugly versioning pre-compiler macro, but it occurs at the wrong point
	 * in the execution, causing IsCorrection() to fail and introducing de-sync
	 */
	// virtual void ClientHandleMoveResponse(const FCharacterMoveResponseDataContainer& MoveResponse) override;

	virtual void OnClientCorrectionReceived(class FNetworkPredictionData_Client_Character& ClientData, float TimeStamp,
		FVector NewLocation, FVector NewVelocity, UPrimitiveComponent* NewBase, FName NewBaseBoneName, bool bHasBase,
		bool bBaseRelativePosition, uint8 ServerMovementMode
#if UE_5_03_OR_LATER
		, FVector ServerGravityDirection) override;
#else
		) override;
#endif

	virtual bool ServerCheckClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel,
		const FVector& ClientWorldLocation, const FVector& RelativeClientLocation,
		UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode) override;

	virtual void ServerMoveHandleClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel,
		const FVector& RelativeClientLocation, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName,
		uint8 ClientMovementMode) override;
	
protected:
	virtual bool ClientUpdatePositionAfterServerUpdate() override;
	virtual bool CanDelaySendingMove(const FSavedMovePtr& NewMove) override;

public:
	/** Client auth parameters mapped to a source gameplay tag */
	UPROPERTY(Category="Character Movement (Networking)", EditAnywhere, BlueprintReadOnly)
	TMap<FGameplayTag, FClientAuthParams> ClientAuthParams;

	UPROPERTY()
	FClientAuthStack ClientAuthStack;

	virtual FClientAuthData* GetClientAuthData();
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
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Character Movement (Networking)")
	virtual void InitClientAuthority(FGameplayTag ClientAuthSource, float OverrideDuration = -1.f);

protected:
	virtual bool ServerShouldGrantClientPositionAuthority(FVector& ClientLoc);

protected:
	virtual void TickCharacterPose(float DeltaTime) override;  // ACharacter::GetAnimRootMotionTranslationScale() is non-virtual so we have to duplicate this entire function
	
private:
	FPredNetworkMoveDataContainer PredMoveDataContainer;
	FPredMoveResponseDataContainer PredMoveResponseDataContainer;
	
public:
	/** Get prediction data for a client game. Should not be used if not running as a client. Allocates the data on demand and can be overridden to allocate a custom override if desired. Result must be a FNetworkPredictionData_Client_Character. */
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;

protected:
	virtual void UpdateFromCompressedFlags(uint8 Flags) override;
};

class PREDICTEDMOVEMENT_API FSavedMove_Character_Pred : public FSavedMove_Character
{
	using Super = FSavedMove_Character;

public:
	FSavedMove_Character_Pred()
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

	virtual ~FSavedMove_Character_Pred() override
	{}

	uint32 bWantsToAimDownSights:1;
	
	uint32 bWantsToProne:1;
	uint32 bProneLocked:1;
	
	uint32 bWantsToStroll:1;
	uint32 bWantsToWalk:1;
	uint32 bWantsToSprint:1;
	uint32 bStaminaDrained : 1;
	
	float StartStamina;
	float EndStamina;

	FMovementModifier Boost;
	FMovementModifier EndBoost;
	FMovementModifier Haste;
	FMovementModifier EndHaste;
	FMovementModifier Slow;
	FMovementModifier EndSlow;
	FMovementModifier SlowFall;
	FMovementModifier EndSlowFall;
	FMovementModifier Snare;
	FMovementModifier EndSnare;

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
	
	/** Combine this move with an older move and update relevant state. */
	virtual void CombineWith(const FSavedMove_Character* OldMove, ACharacter* InCharacter, APlayerController* PC, const FVector& OldStartLocation) override;

	/** Set the properties describing the position, etc. of the moved pawn at the start of the move. */
	virtual void SetInitialPosition(ACharacter* C) override;

	/** Set the properties describing the final position, etc. of the moved pawn. */
	virtual void PostUpdate(ACharacter* C, EPostUpdateMode PostUpdateMode) override;

	/** Returns true if this move is an "important" move that should be sent again if not acked by the server */
	virtual bool IsImportantMove(const FSavedMovePtr& LastAckedMove) const override;
};

class PREDICTEDMOVEMENT_API FNetworkPredictionData_Client_Character_Pred : public FNetworkPredictionData_Client_Character
{
	using Super = FNetworkPredictionData_Client_Character;

public:
	FNetworkPredictionData_Client_Character_Pred(const UCharacterMovementComponent& ClientMovement)
		: Super(ClientMovement)
	{}

	virtual FSavedMovePtr AllocateNewMove() override;
};
