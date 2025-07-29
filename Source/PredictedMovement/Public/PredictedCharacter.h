// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "PredTypes.h"
#include "GameFramework/Character.h"
#include "Modifier/ModifierTypes.h"
#include "PredictedCharacter.generated.h"

class UPredictedCharacterMovement;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStaminaChangeEvent, float, Stamina, float, PrevStamina);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FStaminaEvent);

/**
 * A complete character class combined from the PredictedMovement repo.
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
class PREDICTEDMOVEMENT_API APredictedCharacter : public ACharacter
{
	GENERATED_BODY()

private:
	/** Movement component used for movement logic in various movement modes (walking, falling, etc), containing relevant settings and functions to control movement. */
	UPROPERTY(BlueprintReadOnly, Category=Character, meta=(AllowPrivateAccess = "true"))
	TObjectPtr<UPredictedCharacterMovement> PredictedMovement;

public:
	UPredictedCharacterMovement* GetPredictedMovement() const { return PredictedMovement; }

protected:
	/** Set by character movement to specify that this Character is currently Strolling. */
	UPROPERTY(BlueprintReadOnly, replicatedUsing=OnRep_IsStrolling, Category=Character)
	uint8 bIsStrolling:1;

	/** Set by character movement to specify that this Character is currently Walking. */
	UPROPERTY(BlueprintReadOnly, replicatedUsing=OnRep_IsWalking, Category=Character)
	uint8 bIsWalking:1;
	
	/** Set by character movement to specify that this Character is currently Sprinting. */
	UPROPERTY(BlueprintReadOnly, replicatedUsing=OnRep_IsSprinting, Category=Character)
	uint8 bIsSprinting:1;
	
public:
	APredictedCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
public:
	template<typename T>
	void NotifyModifierChanged(const FGameplayTag& ModifierType, const FGameplayTag& ModifierLevel,
		const FGameplayTag& PrevModifierLevel, T ModifierLevelValue, T PrevModifierLevelValue, T InvalidLevel)
	{
		if (ModifierLevelValue != InvalidLevel && PrevModifierLevelValue == InvalidLevel)
		{
			OnModifierAdded(ModifierType, ModifierLevel, PrevModifierLevel);
		}
		else if (ModifierLevelValue == InvalidLevel && PrevModifierLevelValue != InvalidLevel)
		{
			OnModifierRemoved(ModifierType, ModifierLevel, PrevModifierLevel);
		}
		
		OnModifierChanged(ModifierType, ModifierLevel, PrevModifierLevel);
	}
	
	virtual void OnModifierChanged(const FGameplayTag& ModifierType, const FGameplayTag& ModifierLevel, const FGameplayTag& PrevModifierLevel);
	virtual void OnModifierAdded(const FGameplayTag& ModifierType, const FGameplayTag& ModifierLevel, const FGameplayTag& PrevModifierLevel);
	virtual void OnModifierRemoved(const FGameplayTag& ModifierType, const FGameplayTag& ModifierLevel, const FGameplayTag& PrevModifierLevel);

	UFUNCTION(BlueprintImplementableEvent, Category=Character, meta=(DisplayName="On Modifier Added"))
	void K2_OnModifierAdded(const FGameplayTag& ModifierType, const FGameplayTag& ModifierLevel, const FGameplayTag& PrevModifierLevel);

	UFUNCTION(BlueprintImplementableEvent, Category=Character, meta=(DisplayName="On Modifier Changed"))
	void K2_OnModifierChanged(const FGameplayTag& ModifierType, const FGameplayTag& ModifierLevel, const FGameplayTag& PrevModifierLevel);

	UFUNCTION(BlueprintImplementableEvent, Category=Character, meta=(DisplayName="On Modifier Removed"))
	void K2_OnModifierRemoved(const FGameplayTag& ModifierType, const FGameplayTag& ModifierLevel, const FGameplayTag& PrevModifierLevel);

public:
	/** 
	 * Grant the client position authority, based on the current state of the character.
	 * @param ClientAuthSource What the client is requesting authority for, not used by default, requires override
	 * @param OverrideDuration Override the default client authority time, -1.f to use default
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category=Character)
	virtual void GrantClientAuthority(FGameplayTag ClientAuthSource, float OverrideDuration = -1.f);
	
	/**
	 * Consume the PendingMove if it exists, by sending it to the server and clearing it
	 * Useful for movement in GAS where we need the server to complete anything pending to resolve de-sync that occurs as a result of a delayed move
	 * @note Doesn't simply trash the PendingMove and there is only ever one, this function is poorly named and comes from Unreal
	 */
	UFUNCTION(BlueprintCallable, Category=Character)
	void FlushServerMoves();
	
public:
	/** Request Gait Mode based on Player's Input */
	UFUNCTION(BlueprintCallable, Category=Character)
	virtual void SetGaitMode(EPredGaitMode NewGaitMode);

	/** @return GaitMode based on Player's Input */
	UFUNCTION(BlueprintPure, Category=Character)
	EPredGaitMode GetGaitMode() const;

	/** @return GaitMode based on current speed */
	UFUNCTION(BlueprintPure, Category=Character)
	EPredGaitMode GetGaitSpeed() const;

	UFUNCTION(BlueprintPure, Category=Character)
	static FString GetGaitModeString(EPredGaitMode GaitMode);

	UFUNCTION(BlueprintPure, Category=Character)
	EPredStance GetStance() const;
	
public:
	void SetIsStrolling(bool bNewStrolling);

	UFUNCTION(BlueprintPure, Category=Character)
	bool IsStrolling() const { return bIsStrolling; }

	/** Handle Strolling replicated from server */
	UFUNCTION()
	virtual void OnRep_IsStrolling();

	/** @return true if this character is currently able to Stroll (and is not currently Strolling) */
	UFUNCTION(BlueprintPure, Category=Character)
	virtual bool CanStroll() const;

	/**
	 * Request the character to start Strolling. The request is processed on the next update of the CharacterMovementComponent.
	 * @see OnStartStroll
	 * @see IsStrolling
	 * @see CharacterMovement->WantsToStroll
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(HidePin="bClientSimulation"))
	virtual void Stroll(bool bClientSimulation = false);

	/**
	 * Request the character to stop Strolling. The request is processed on the next update of the CharacterMovementComponent.
	 * @see OnEndStroll
	 * @see IsStrolling
	 * @see CharacterMovement->WantsToStroll
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(HidePin="bClientSimulation"))
	virtual void UnStroll(bool bClientSimulation = false);

	/** Called when Character Strolls. Called on non-owned Characters through bIsStrolling replication. */
	virtual void OnStartStroll();

	/** Event when Character Strolls. */
	UFUNCTION(BlueprintImplementableEvent, Category=Character, meta=(DisplayName="On Start Stroll"))
	void K2_OnStartStroll();

	/** Called when Character stops Strolling. Called on non-owned Characters through bIsStrolling replication. */
	virtual void OnEndStroll();

	/** Event when Character stops Strolling. */
	UFUNCTION(BlueprintImplementableEvent, Category=Character, meta=(DisplayName="On End Stroll"))
	void K2_OnEndStroll();
	
public:
	public:
	void SetIsWalking(bool bNewWalking);

	UFUNCTION(BlueprintPure, Category=Character)
	bool IsWalking() const { return bIsWalking; }

	/** Handle Walking replicated from server */
	UFUNCTION()
	virtual void OnRep_IsWalking();

	/** @return true if this character is currently able to Walk (and is not currently Walking) */
	UFUNCTION(BlueprintPure, Category=Character)
	virtual bool CanWalk() const;

	/**
	 * Request the character to start Walking. The request is processed on the next update of the CharacterMovementComponent.
	 * @see OnStartWalk
	 * @see IsWalking
	 * @see CharacterMovement->WantsToWalk
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(HidePin="bClientSimulation"))
	virtual void Walk(bool bClientSimulation = false);

	/**
	 * Request the character to stop Walking. The request is processed on the next update of the CharacterMovementComponent.
	 * @see OnEndWalk
	 * @see IsWalking
	 * @see CharacterMovement->WantsToWalk
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(HidePin="bClientSimulation"))
	virtual void UnWalk(bool bClientSimulation = false);

	/** Called when Character Walks. Called on non-owned Characters through bIsWalking replication. */
	virtual void OnStartWalk();

	/** Event when Character Walks. */
	UFUNCTION(BlueprintImplementableEvent, Category=Character, meta=(DisplayName="On Start Walk"))
	void K2_OnStartWalk();

	/** Called when Character stops Walking. Called on non-owned Characters through bIsWalking replication. */
	virtual void OnEndWalk();

	/** Event when Character stops Walking. */
	UFUNCTION(BlueprintImplementableEvent, Category=Character, meta=(DisplayName="On End Walk"))
	void K2_OnEndWalk();

public:
	/**
	 * Request the character to start Running. The request is processed on the next update of the CharacterMovementComponent.
	 * @note Running state is simply the absence of Strolling, Walking, or Sprinting.
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(HidePin="bClientSimulation"))
	virtual void Run(bool bClientSimulation = false);

	UFUNCTION(BlueprintPure, Category=Character)
	bool IsRunning() const { return !IsStrolling() && !IsWalking() && !IsSprinting(); }

	UFUNCTION(BlueprintPure, Category=Character)
	bool IsRunningAtSpeed() const;

public:
	void SetIsSprinting(bool bNewSprinting);

	UFUNCTION(BlueprintPure, Category=Character)
	bool IsSprinting() const { return bIsSprinting; }

	UFUNCTION(BlueprintPure, Category=Character)
	bool IsSprintingAtSpeed() const;
	
	/**
	 * This check ensures that we are not sprinting backward or sideways, while allowing leeway 
	 * This angle allows sprinting when holding forward, forward left, forward right
	 * but not left or right or backward)
	 */
	UFUNCTION(BlueprintPure, Category=Character)
	virtual bool IsSprintWithinAllowableInputAngle() const;
	
	/** @return True if the character is currently Sprinting at or above sprint speed and sprint is within allowable input angle */
	UFUNCTION(BlueprintPure, Category=Character)
	virtual bool IsSprintingInEffect() const;
	
	/** Handle Sprinting replicated from server */
	UFUNCTION()
	virtual void OnRep_IsSprinting();

	/** @return true if this character is currently able to Sprint (and is not currently Sprinting) */
	UFUNCTION(BlueprintPure, Category=Character)
	virtual bool CanSprint() const;

	/**
	 * Request the character to start Sprinting. The request is processed on the next update of the CharacterMovementComponent.
	 * @see OnStartSprint
	 * @see IsSprinting
	 * @see CharacterMovement->WantsToSprint
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(HidePin="bClientSimulation"))
	virtual void Sprint(bool bClientSimulation = false);

	/**
	 * Request the character to stop Sprinting. The request is processed on the next update of the CharacterMovementComponent.
	 * @see OnEndSprint
	 * @see IsSprinting
	 * @see CharacterMovement->WantsToSprint
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(HidePin="bClientSimulation"))
	virtual void UnSprint(bool bClientSimulation = false);

	/** Called when Character Sprints. Called on non-owned Characters through bIsSprinting replication. */
	virtual void OnStartSprint();

	/** Event when Character Sprints. */
	UFUNCTION(BlueprintImplementableEvent, Category=Character, meta=(DisplayName="On Start Sprint"))
	void K2_OnStartSprint();

	/** Called when Character stops Sprinting. Called on non-owned Characters through bIsSprinting replication. */
	virtual void OnEndSprint();

	/** Event when Character stops Sprinting. */
	UFUNCTION(BlueprintImplementableEvent, Category=Character, meta=(DisplayName="On End Sprint"))
	void K2_OnEndSprint();

public:
	UPROPERTY(BlueprintAssignable, Category=Character)
	FStaminaChangeEvent NotifyOnStaminaChanged;

	UPROPERTY(BlueprintAssignable, Category=Character)
	FStaminaChangeEvent NotifyOnMaxStaminaChanged;

	UPROPERTY(BlueprintAssignable, Category=Character)
	FStaminaEvent NotifyOnStaminaDrained;

	UPROPERTY(BlueprintAssignable, Category=Character)
	FStaminaEvent NotifyOnStaminaDrainRecovered;

	virtual void OnStaminaChanged(float Stamina, float PrevStamina);
	virtual void OnMaxStaminaChanged(float MaxStamina, float PrevMaxStamina);
	virtual void OnStaminaDrained();
	virtual void OnStaminaDrainRecovered();

	UFUNCTION(BlueprintImplementableEvent, Category=Character, meta=(DisplayName="OnStaminaChanged"))
	void K2_OnStaminaChanged(float Stamina, float PrevStamina);

	UFUNCTION(BlueprintImplementableEvent, Category=Character, meta=(DisplayName="OnMaxStaminaChanged"))
	void K2_OnMaxStaminaChanged(float MaxStamina, float PrevMaxStamina);

	UFUNCTION(BlueprintImplementableEvent, Category=Character, meta=(DisplayName="OnStaminaDrained"))
	void K2_OnStaminaDrained();

	UFUNCTION(BlueprintImplementableEvent, Category=Character, meta=(DisplayName="OnStaminaDrainRecovered"))
	void K2_OnStaminaDrainRecovered();

	/**
	 * Set the stamina directly
	 * @WARNING DE-SYNC - Calling this from outside CMC prediction will de-sync
	 */
	UFUNCTION(BlueprintCallable, Category=Character)
	void SetStamina(float NewStamina);

	/**
	 * Add stamina
	 * @WARNING DE-SYNC - Calling this from outside CMC prediction will de-sync
	 */
	UFUNCTION(BlueprintCallable, Category=Character)
	void AddStamina(float AddedStamina);
	
	/**
	 * Set the maximum stamina directly
	 * @WARNING DE-SYNC - Calling this from outside CMC prediction will de-sync
	 * Only use this for non-gameplay critical/affecting situations
	 */
	UFUNCTION(BlueprintCallable, Category=Character)
	void SetMaxStamina(float NewMaxStamina);

	/**
	 * Add maximum stamina
	 * @WARNING DE-SYNC - Calling this from outside CMC prediction will de-sync
	 * Only use this for non-gameplay critical/affecting situations
	 */
	UFUNCTION(BlueprintCallable, Category=Character)
	void AddMaxStamina(float AddedMaxStamina);

	/**
	 * Reset maximum stamina to BaseMaxStamina
	 * @WARNING DE-SYNC - Calling this from outside CMC prediction will de-sync
	 * Only use this for non-gameplay critical/affecting situations
	 */
	UFUNCTION(BlueprintCallable, Category=Character)
	void ResetMaxStamina();
	
	/**
	 * Set the stamina drained state directly
	 * @WARNING DE-SYNC - Calling this from outside CMC prediction will de-sync
	 * Only use this for non-gameplay critical/affecting situations
	 */
	UFUNCTION(BlueprintCallable, Category=Character)
	void SetStaminaDrained(bool bNewStaminaDrained);
	
	UFUNCTION(BlueprintPure, Category=Character)
	float GetStamina() const;
	
	UFUNCTION(BlueprintPure, Category=Character)
	float GetMaxStamina() const;
	
	UFUNCTION(BlueprintPure, Category=Character)
	float GetStaminaPct() const;

	UFUNCTION(BlueprintPure, Category=Character)
	bool IsStaminaDrained() const;

protected:
	/** Set by character movement to specify that this Character is currently AimingDownSights. */
	UPROPERTY(BlueprintReadOnly, replicatedUsing=OnRep_IsAimingDownSights, Category=Character)
	uint8 bIsAimingDownSights:1;

public:
	virtual void SetIsAimingDownSights(bool bNewAimingDownSights);

	/** @return true if this character is currently AimingDownSights */
	UFUNCTION(BlueprintPure, Category=Character)
	virtual bool IsAimingDownSights() const { return bIsAimingDownSights; }
	
	/** Handle AimingDownSights replicated from server */
	UFUNCTION()
	virtual void OnRep_IsAimingDownSights();

	/** @return true if this character is currently able to AimDownSights (and is not currently AimingDownSights) */
	UFUNCTION(BlueprintPure, Category=Character)
	virtual bool CanAimDownSights() const;

	/**
	 * Request the character to start AimingDownSights. The request is processed on the next update of the CharacterMovementComponent.
	 * @see OnStartAimDownSights
	 * @see IsAimingDownSights
	 * @see CharacterMovement->WantsToAimDownSights
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(HidePin="bClientSimulation"))
	virtual void AimDownSights(bool bClientSimulation = false);

	/**
	 * Request the character to stop AimingDownSights. The request is processed on the next update of the CharacterMovementComponent.
	 * @see OnEndAimDownSights
	 * @see IsAimingDownSights
	 * @see CharacterMovement->WantsToAimDownSights
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(HidePin="bClientSimulation"))
	virtual void UnAimDownSights(bool bClientSimulation = false);

	/** Called when Character AimsDownSights. Called on non-owned Characters through bIsAimingDownSights replication. */
	virtual void OnStartAimDownSights();

	/** Event when Character AimsDownSights. */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName="On Start Aim Down Sights"))
	void K2_OnStartAimDownSights();

	/** Called when Character stops AimingDownSights. Called on non-owned Characters through bIsAimingDownSights replication. */
	virtual void OnEndAimDownSights();

	/** Event when Character stops AimingDownSights. */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName="On End Aim Down Sights"))
	void K2_OnEndAimDownSights();

public:
	/** Default proned eye height */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Camera)
	float PronedEyeHeight;
	
	/** Set by character movement to specify that this Character is currently Proned. */
	UPROPERTY(BlueprintReadOnly, replicatedUsing=OnRep_IsProned, Category=Character)
	uint8 bIsProned:1;
	
public:
	/** Default standing eye height */
	UFUNCTION(BlueprintPure, Category=Character)
	virtual float GetStandingBaseEyeHeight() const;
	
	virtual float GetBaseEyeHeight() const;
	
	virtual void RecalculateBaseEyeHeight() override;
	
	virtual void SetIsProned(bool bNewProned);

	/** @return true if this character is currently Proned */
	UFUNCTION(BlueprintPure, Category=Character)
	virtual bool IsProned() const { return bIsProned; }

	/** Handle Prone replicated from server */
	UFUNCTION()
	virtual void OnRep_IsProned();

	/** @return true if this character is currently able to Prone (and is not currently Prone) */
	UFUNCTION(BlueprintPure, Category=Character)
	virtual bool CanProne() const;

	virtual void Crouch(bool bClientSimulation = false) override;
	
	/**
	 * Request the character to start Proned. The request is processed on the next update of the CharacterMovementComponent.
	 * @see OnStartProne
	 * @see IsProned
	 * @see CharacterMovement->WantsToProne
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(HidePin="bClientSimulation"))
	virtual void Prone(bool bClientSimulation = false);

	/**
	 * Request the character to stop Proned. The request is processed on the next update of the CharacterMovementComponent.
	 * @see OnEndProne
	 * @see IsProned
	 * @see CharacterMovement->WantsToProne
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(HidePin="bClientSimulation"))
	virtual void UnProne(bool bClientSimulation = false);

	/** Called when Character Prones. Called on non-owned Characters through bIsProned replication. */
	virtual void OnStartProne(float HalfHeightAdjust, float ScaledHalfHeightAdjust);

	/** Event when Character Prones. */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName="On Start Prone"))
	void K2_OnStartProne(float HalfHeightAdjust, float ScaledHalfHeightAdjust);
	
	/** Called when Character stops Proned. Called on non-owned Characters through bIsProned replication. */
	virtual void OnEndProne(float HalfHeightAdjust, float ScaledHalfHeightAdjust);

	/** Event when Character stops Proned. */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName="On End Prone"))
	void K2_OnEndProne(float HalfHeightAdjust, float ScaledHalfHeightAdjust);
	
public:
	/* Boost Implementation */
	
	/** Set by character movement to specify that this Character's Boost level. */
	UPROPERTY(ReplicatedUsing=OnRep_SimulatedBoost)
	uint8 SimulatedBoost = NO_MODIFIER;

	/** Handle Boost replicated from server */
	UFUNCTION()
	virtual void OnRep_SimulatedBoost(uint8 PrevLevel);

	/**
	 * Request the character to start Boost. The request is processed on the next update of the CharacterMovementComponent.
	 * @param Level The level of the Boost to remove.
	 * @param NetType How the Boost is applied, either locally predicted, with correction, or server initiated.
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(GameplayTagFilter="Modifier.Boost"))
	virtual bool Boost(FGameplayTag Level, EModifierNetTypeLocal NetType);

	/**
	 * Request the character to stop Boost. The request is processed on the next update of the CharacterMovementComponent.
	 * @param Level The level of the Boost to remove.
	 * @param NetType How the Boost is applied, either locally predicted, with correction, or server initiated.
	 * @param bRemoveAll If true, removes all Boosts of the specified level, otherwise only removes the first one found.
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(GameplayTagFilter="Modifier.Boost", InvalidEnumValues="ServerInitiated"))
	virtual bool UnBoost(FGameplayTag Level, EModifierNetTypeLocal NetType, bool bRemoveAll=false);

	/**
	 * Reset the Boost for the specified NetType, removing all Boosts of that type.
	 * @return True if any modifiers were removed, false if none were found.
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(InvalidEnumValues="ServerInitiated"))
	virtual bool ResetBoost(EModifierNetTypeLocal NetType);

	/**
	 * Get the current Boost level of the character.
	 * @return Current active Boost level if active, otherwise empty tag.
	 */
	UFUNCTION(BlueprintCallable, Category=Character)
	FGameplayTag GetBoostLevel() const;

	/**
	 * Determine if this character is currently Boosted.
	 * @return True if this character has an active Boost.
	 */
	UFUNCTION(BlueprintCallable, Category=Character)
	bool IsBoostActive() const;
	
	/* ~Boost Implementation */

public:
	/* Haste Implementation */
	
	/** Set by character movement to specify that this Character's Haste level. */
	UPROPERTY(ReplicatedUsing=OnRep_SimulatedHaste)
	uint8 SimulatedHaste = NO_MODIFIER;

	/** Handle Haste replicated from server */
	UFUNCTION()
	virtual void OnRep_SimulatedHaste(uint8 PrevLevel);

	/**
	 * Request the character to start Haste. The request is processed on the next update of the CharacterMovementComponent.
	 * @param Level The level of the Haste to remove.
	 * @param NetType How the Haste is applied, either locally predicted, with correction, or server initiated.
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(GameplayTagFilter="Modifier.Haste", InvalidEnumValues="ServerInitiated"))
	virtual bool Haste(FGameplayTag Level, EModifierNetTypeLocal NetType);

	/**
	 * Request the character to stop Haste. The request is processed on the next update of the CharacterMovementComponent.
	 * @param Level The level of the Haste to remove.
	 * @param NetType How the Haste is applied, either locally predicted, with correction, or server initiated.
	 * @param bRemoveAll If true, removes all Hastes of the specified level, otherwise only removes the first one found.
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(GameplayTagFilter="Modifier.Haste", InvalidEnumValues="ServerInitiated"))
	virtual bool UnHaste(FGameplayTag Level, EModifierNetTypeLocal NetType, bool bRemoveAll=false);

	/**
	 * Reset the Haste for the specified NetType, removing all Hastes of that type.
	 * @return True if any modifiers were removed, false if none were found.
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(InvalidEnumValues="ServerInitiated"))
	virtual bool ResetHaste(EModifierNetTypeLocal NetType);

	/**
	 * Get the current Haste level of the character.
	 * @return Current active Haste level if active, otherwise empty tag.
	 */
	UFUNCTION(BlueprintCallable, Category=Character)
	FGameplayTag GetHasteLevel() const;

	/**
	 * Determine if this character is currently Hasteed.
	 * @return True if this character has an active Haste.
	 */
	UFUNCTION(BlueprintCallable, Category=Character)
	bool IsHasteActive() const;
	
	/* ~Haste Implementation */

public:
	/* Slow Implementation */
	
	/** Set by character movement to specify that this Character's Slow level. */
	UPROPERTY(ReplicatedUsing=OnRep_SimulatedSlow)
	uint8 SimulatedSlow = NO_MODIFIER;

	/** Handle Slow replicated from server */
	UFUNCTION()
	virtual void OnRep_SimulatedSlow(uint8 PrevLevel);

	/**
	 * Request the character to start Slow. The request is processed on the next update of the CharacterMovementComponent.
	 * @param Level The level of the Slow to remove.
	 * @param NetType How the Slow is applied, either locally predicted, with correction, or server initiated.
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(GameplayTagFilter="Modifier.Slow", InvalidEnumValues="ServerInitiated"))
	virtual bool Slow(FGameplayTag Level, EModifierNetTypeLocal NetType);

	/**
	 * Request the character to stop Slow. The request is processed on the next update of the CharacterMovementComponent.
	 * @param Level The level of the Slow to remove.
	 * @param NetType How the Slow is applied, either locally predicted, with correction, or server initiated.
	 * @param bRemoveAll If true, removes all Slows of the specified level, otherwise only removes the first one found.
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(GameplayTagFilter="Modifier.Slow", InvalidEnumValues="ServerInitiated"))
	virtual bool UnSlow(FGameplayTag Level, EModifierNetTypeLocal NetType, bool bRemoveAll=false);

	/**
	 * Reset the Slow for the specified NetType, removing all Slows of that type.
	 * @return True if any modifiers were removed, false if none were found.
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(InvalidEnumValues="ServerInitiated"))
	virtual bool ResetSlow(EModifierNetTypeLocal NetType);

	/**
	 * Get the current Slow level of the character.
	 * @return Current active Slow level if active, otherwise empty tag.
	 */
	UFUNCTION(BlueprintCallable, Category=Character)
	FGameplayTag GetSlowLevel() const;

	/**
	 * Determine if this character is currently Slowed.
	 * @return True if this character has an active Slow.
	 */
	UFUNCTION(BlueprintCallable, Category=Character)
	bool IsSlowActive() const;
	
	/* ~Slow Implementation */
	
public:
	/* Snare Implementation */
	
	/** Set by character movement to specify that this Character's Snare level. */
	UPROPERTY(ReplicatedUsing=OnRep_SimulatedSnare)
	uint8 SimulatedSnare = NO_MODIFIER;

	/** Handle Snare replicated from server */
	UFUNCTION()
	virtual void OnRep_SimulatedSnare(uint8 PrevLevel);

	/**
	 * Request the character to start Modified. The request is processed on the next update of the CharacterMovementComponent.
	 * @see OnStartModifier
	 * @see IsModified
	 * @see CharacterMovement->WantsToModifier
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(GameplayTagFilter="Modifier.Snare"))
	virtual bool Snare(FGameplayTag Level);

	/**
	 * Request the character to stop Modified. The request is processed on the next update of the CharacterMovementComponent.
	 * @see OnEndModifier
	 * @see IsModified
	 * @see CharacterMovement->WantsToModifier
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(GameplayTagFilter="Modifier.Snare"))
	virtual bool UnSnare(FGameplayTag Level, bool bRemoveAll=false);

	/**
	 * Reset the Snare for the specified NetType, removing all Snares of that type.
	 * @return True if any modifiers were removed, false if none were found.
	 */
	UFUNCTION(BlueprintCallable, Category=Character)
	virtual bool ResetSnare();

	/**
	 * Get the current Snare level of the character.
	 * @return Current active Snare level if active, otherwise empty tag.
	 */
	UFUNCTION(BlueprintCallable, Category=Character)
	FGameplayTag GetSnareLevel() const;

	/**
	 * Determine if this character is currently Snareed.
	 * @return True if this character has an active Snare.
	 */
	UFUNCTION(BlueprintCallable, Category=Character)
	bool IsSnareActive() const;
	
	/* ~Snare Implementation */

public:
	/* SlowFall Implementation */
	
	/** Set by character movement to specify that this Character's SlowFall level. */
	UPROPERTY(ReplicatedUsing=OnRep_SimulatedSlowFall)
	uint8 SimulatedSlowFall = NO_MODIFIER;

	/** Handle SlowFall replicated from server */
	UFUNCTION()
	virtual void OnRep_SimulatedSlowFall(uint8 PrevLevel);

	/**
	 * Request the character to start SlowFall. The request is processed on the next update of the CharacterMovementComponent.
	 * @param Level The level of the SlowFall to remove.
	 * @param NetType How the SlowFall is applied, either locally predicted, with correction, or server initiated.
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(GameplayTagFilter="Modifier.SlowFall", InvalidEnumValues="ServerInitiated"))
	virtual bool SlowFall(FGameplayTag Level, EModifierNetTypeLocal NetType);

	/**
	 * Request the character to stop SlowFall. The request is processed on the next update of the CharacterMovementComponent.
	 * @param Level The level of the SlowFall to remove.
	 * @param NetType How the SlowFall is applied, either locally predicted, with correction, or server initiated.
	 * @param bRemoveAll If true, removes all SlowFalls of the specified level, otherwise only removes the first one found.
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(GameplayTagFilter="Modifier.SlowFall", InvalidEnumValues="ServerInitiated"))
	virtual bool UnSlowFall(FGameplayTag Level, EModifierNetTypeLocal NetType, bool bRemoveAll=false);

	/**
	 * Reset the SlowFall for the specified NetType, removing all SlowFalls of that type.
	 * @return True if any modifiers were removed, false if none were found.
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(InvalidEnumValues="ServerInitiated"))
	virtual bool ResetSlowFall(EModifierNetTypeLocal NetType);

	/**
	 * Get the current SlowFall level of the character.
	 * @return Current active SlowFall level if active, otherwise empty tag.
	 */
	UFUNCTION(BlueprintCallable, Category=Character)
	FGameplayTag GetSlowFallLevel() const;

	/**
	 * Determine if this character is currently SlowFalled.
	 * @return True if this character has an active SlowFall.
	 */
	UFUNCTION(BlueprintCallable, Category=Character)
	bool IsSlowFallActive() const;
	
	/* ~SlowFall Implementation */
};
