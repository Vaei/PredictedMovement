// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "PredTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "System/PredictedMovementVersioning.h"
#include "PredictedCharacterMovement.generated.h"

class APredictedCharacter;

struct PREDICTEDMOVEMENT_API FPredMoveResponseDataContainer : FCharacterMoveResponseDataContainer
{  // Server ➜ Client
	using Super = FCharacterMoveResponseDataContainer;

	float Stamina;
	bool bStaminaDrained;
	
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
 * @TODO description
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
	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void UpdateCharacterStateAfterMovement(float DeltaSeconds) override;

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

protected:
	virtual bool ClientUpdatePositionAfterServerUpdate() override;

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

	uint8 bWantsToAimDownSights:1;
	
	uint8 bWantsToProne:1;
	uint8 bProneLocked:1;
	
	uint8 bWantsToStroll:1;
	uint8 bWantsToWalk:1;
	uint8 bWantsToSprint:1;
	uint8 bStaminaDrained:1;
	
	float StartStamina;
	float EndStamina;

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
