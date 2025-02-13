// Copyright (c) 2023 Jared Taylor. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "SprintMovement.generated.h"

class ASprintCharacter;

struct PREDICTEDMOVEMENT_API FSprintNetworkMoveData : FCharacterNetworkMoveData
{  // Client ➜ Server
public:
	typedef FCharacterNetworkMoveData Super;
 
	FSprintNetworkMoveData()
		: bWantsToSprint(false)
	{}

	bool bWantsToSprint;
	
	virtual void ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType) override;
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType) override;
};
 
struct PREDICTEDMOVEMENT_API FSprintNetworkMoveDataContainer : FCharacterNetworkMoveDataContainer
{  // Client ➜ Server
public:
	typedef FCharacterNetworkMoveDataContainer Super;
 
	FSprintNetworkMoveDataContainer()
	{
		NewMoveData = &MoveData[0];
		PendingMoveData = &MoveData[1];
		OldMoveData = &MoveData[2];
	}
 
private:
	FSprintNetworkMoveData MoveData[3];
};

/**
 * Identical to the main branch implementation, except using move containers instead of compressed flags
 * This exists for teaching purposes, to show how to use move containers that are 1:1 with the compressed flags
 * Typically compressed flags are only used for a boolean, and move containers are used for more complex data
 * However, compressed flags are a lot cheaper so we wouldn't typically use move containers for a boolean
 */
UCLASS()
class PREDICTEDMOVEMENT_API USprintMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()
	
private:
	/** Character movement component belongs to */
	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<ASprintCharacter> SprintCharacterOwner;

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
	USprintMovement(const FObjectInitializer& ObjectInitializer);

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	
	virtual bool HasValidData() const override;
	virtual void PostLoad() override;
	virtual void OnRegister() override;
	virtual void SetUpdatedComponent(USceneComponent* NewUpdatedComponent) override;

public:
	virtual bool IsSprintingAtSpeed() const;
	virtual bool IsSprintingInEffect() const { return IsSprintingAtSpeed() && IsSprintWithinAllowableInputAngle(); }

	virtual float GetMaxAcceleration() const override;
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxBrakingDeceleration() const override;
	
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	virtual void ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration) override;

public:
	UFUNCTION(BlueprintCallable, Category="Character Movement: Walking")
	void SetMaxInputAngleSprint(float InMaxAngleSprint);

public:
	virtual bool IsSprinting() const;

	/**
	 * Call CharacterOwner->OnStartSprint() if successful.
	 * In general you should set bWantsToSprint instead to have the Sprint persist during movement, or just use the Sprint functions on the owning Character.
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

	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void UpdateCharacterStateAfterMovement(float DeltaSeconds) override;

	virtual void ServerMove_PerformMovement(const FCharacterNetworkMoveData& MoveData) override;

protected:
	virtual bool ClientUpdatePositionAfterServerUpdate() override;

private:
	FSprintNetworkMoveDataContainer SprintMoveDataContainer;
	
public:
	/** Get prediction data for a client game. Should not be used if not running as a client. Allocates the data on demand and can be overridden to allocate a custom override if desired. Result must be a FNetworkPredictionData_Client_Character. */
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;
};

class PREDICTEDMOVEMENT_API FSavedMove_Character_Sprint : public FSavedMove_Character
{
	using Super = FSavedMove_Character;

public:
	FSavedMove_Character_Sprint()
		: bWantsToSprint(0)
	{
	}

	virtual ~FSavedMove_Character_Sprint() override
	{}

	uint32 bWantsToSprint:1;
		
	/** Clear saved move properties, so it can be re-used. */
	virtual void Clear() override;

	/** Called to set up this saved move (when initially created) to make a predictive correction. */
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character & ClientData) override;

	/** Returns true if this move can be combined with NewMove for replication without changing any behavior */
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;

	/** Set the properties describing the position, etc. of the moved pawn at the start of the move. */
	virtual void SetInitialPosition(ACharacter* C) override;
};

class PREDICTEDMOVEMENT_API FNetworkPredictionData_Client_Character_Sprint : public FNetworkPredictionData_Client_Character
{
	using Super = FNetworkPredictionData_Client_Character;

public:
	FNetworkPredictionData_Client_Character_Sprint(const UCharacterMovementComponent& ClientMovement)
	: Super(ClientMovement)
	{}

	virtual FSavedMovePtr AllocateNewMove() override;
};
