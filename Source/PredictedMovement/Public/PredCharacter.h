﻿// Copyright (c) 2023 Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "PredTypes.h"
#include "GameFramework/Character.h"
#include "PredCharacter.generated.h"

struct FGameplayTag;
class UPredMovement;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStaminaChangeEvent, float, Stamina, float, PrevStamina);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FStaminaEvent);

UCLASS()
class PREDICTEDMOVEMENT_API APredCharacter : public ACharacter
{
	GENERATED_BODY()

private:
	/** Movement component used for movement logic in various movement modes (walking, falling, etc), containing relevant settings and functions to control movement. */
	UPROPERTY(BlueprintReadOnly, Category=Character, meta=(AllowPrivateAccess = "true"))
	TObjectPtr<UPredMovement> PredMovement;

	friend class FSavedMove_Character_Pred;
protected:
	FORCEINLINE UPredMovement* GetPredCharacterMovement() const { return PredMovement; }

protected:
	/** Set by character movement to specify that this Character is currently Strolling. */
	UPROPERTY(BlueprintReadOnly, replicatedUsing=OnRep_IsStrolling, Category=Character)
	uint32 bIsStrolling:1;

	/** Set by character movement to specify that this Character is currently Walking. */
	UPROPERTY(BlueprintReadOnly, replicatedUsing=OnRep_IsWalking, Category=Character)
	uint32 bIsWalking:1;
	
	/** Set by character movement to specify that this Character is currently Sprinting. */
	UPROPERTY(BlueprintReadOnly, replicatedUsing=OnRep_IsSprinting, Category=Character)
	uint32 bIsSprinting:1;
	
public:
	APredCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	/** Request Gait Mode based on Player's Input */
	UFUNCTION(BlueprintCallable, Category=Character)
	virtual void SetGaitMode(EPredGaitMode NewGaitMode);

	/** @return GaitMode based on Player's Input */
	UFUNCTION(BlueprintPure, Category=Character)
	EPredGaitMode GetGaitMode() const;

	/** @return GaitMode based on current speed */
	UFUNCTION(BlueprintPure, Category=Character)
	EPredGaitMode GetGaitAtSpeed() const;

	UFUNCTION(BlueprintPure, Category=Character)
	static FString GetGaitModeAsString(EPredGaitMode GaitMode);
	
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
	uint32 bIsAimingDownSights:1;

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
	
protected:
	/** Set by character movement to specify that this Character is currently Proned. */
	UPROPERTY(BlueprintReadOnly, replicatedUsing=OnRep_IsProned, Category=Character)
	uint32 bIsProned:1;
	
public:
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
	virtual void OnModifierChanged(const FGameplayTag& ModifierType, uint8 ModifierLevel, uint8 PrevModifierLevel);
	virtual void OnModifierAdded(const FGameplayTag& ModifierType, uint8 ModifierLevel, uint8 PrevModifierLevel);
	virtual void OnModifierRemoved(const FGameplayTag& ModifierType, uint8 ModifierLevel, uint8 PrevModifierLevel);

	UFUNCTION(BlueprintImplementableEvent, Category=Character, meta=(DisplayName="On Modifier Added"))
	void K2_OnModifierAdded(const FGameplayTag& ModifierType, uint8 ModifierLevel, uint8 PrevModifierLevel);

	UFUNCTION(BlueprintImplementableEvent, Category=Character, meta=(DisplayName="On Modifier Changed"))
	void K2_OnModifierChanged(const FGameplayTag& ModifierType, uint8 ModifierLevel, uint8 PrevModifierLevel);

	UFUNCTION(BlueprintImplementableEvent, Category=Character, meta=(DisplayName="On Modifier Removed"))
	void K2_OnModifierRemoved(const FGameplayTag& ModifierType, uint8 ModifierLevel, uint8 PrevModifierLevel);

protected:
	/* Boost (Non-Generic) Implementation */
	
	UPROPERTY(ReplicatedUsing=OnRep_SimulatedBoost)
	uint8 SimulatedBoost;

	UFUNCTION()
	void OnRep_SimulatedBoost(uint8 PrevSimulatedBoost);

public:
	UFUNCTION(BlueprintCallable, Category=Character, meta=(FilterGameplayTag="Modifier.Type.Buff.Boost"))
	void Boost(FGameplayTag ModifierLevel);
	
	UFUNCTION(BlueprintCallable, Category=Character, meta=(FilterGameplayTag="Modifier.Type.Buff.Boost"))
	void RemoveBoost(FGameplayTag ModifierLevel);

	UFUNCTION(BlueprintCallable, Category=Character)
	void RemoveAllBoosts();
	
	UFUNCTION(BlueprintCallable, Category=Character)
	void RemoveAllBoostsOfLevel(FGameplayTag ModifierLevel);
	
	UFUNCTION(BlueprintPure, Category=Character)
	bool IsBoosted() const;
	
	UFUNCTION(BlueprintPure, Category=Character)
	bool WantsBoost() const;

	UFUNCTION(BlueprintCallable, Category=Character)
	FGameplayTag GetBoostLevel() const;

	UFUNCTION(BlueprintCallable, Category=Character)
	int32 GetNumBoosts() const;

	UFUNCTION(BlueprintCallable, Category=Character, meta=(FilterGameplayTag="Modifier.Type.Buff.Boost"))
	int32 GetNumBoostsByLevel(FGameplayTag ModifierLevel) const;
	
	/* ~Boost (Non-Generic) Implementation */

protected:
	/* Haste (Non-Generic) Implementation */
	
	UPROPERTY(ReplicatedUsing=OnRep_SimulatedHaste)
	uint8 SimulatedHaste;

	UFUNCTION()
	void OnRep_SimulatedHaste(uint8 PrevSimulatedHaste);

public:
	UFUNCTION(BlueprintCallable, Category=Character, meta=(FilterGameplayTag="Modifier.Type.Buff.Haste"))
	void Haste(FGameplayTag ModifierLevel);
	
	UFUNCTION(BlueprintCallable, Category=Character, meta=(FilterGameplayTag="Modifier.Type.Buff.Haste"))
	void RemoveHaste(FGameplayTag ModifierLevel);

	UFUNCTION(BlueprintCallable, Category=Character)
	void RemoveAllHastes();
	
	UFUNCTION(BlueprintCallable, Category=Character)
	void RemoveAllHastesOfLevel(FGameplayTag ModifierLevel);
	
	UFUNCTION(BlueprintPure, Category=Character)
	bool IsHaste() const;
	
	UFUNCTION(BlueprintPure, Category=Character)
	bool WantsHaste() const;

	UFUNCTION(BlueprintCallable, Category=Character)
	FGameplayTag GetHasteLevel() const;

	UFUNCTION(BlueprintCallable, Category=Character)
	int32 GetNumHastes() const;

	UFUNCTION(BlueprintCallable, Category=Character, meta=(FilterGameplayTag="Modifier.Type.Buff.Haste"))
	int32 GetNumHastesByLevel(FGameplayTag ModifierLevel) const;
	
	/* ~Haste (Non-Generic) Implementation */

protected:
	/* Slow (Non-Generic) Implementation */
	
	UPROPERTY(ReplicatedUsing=OnRep_SimulatedSlow)
	uint8 SimulatedSlow;

	UFUNCTION()
	void OnRep_SimulatedSlow(uint8 PrevSimulatedSlow);

public:
	UFUNCTION(BlueprintCallable, Category=Character, meta=(FilterGameplayTag="Modifier.Type.Debuff.Slow"))
	void Slow(FGameplayTag ModifierLevel);
	
	UFUNCTION(BlueprintCallable, Category=Character, meta=(FilterGameplayTag="Modifier.Type.Debuff.Slow"))
	void RemoveSlow(FGameplayTag ModifierLevel);

	UFUNCTION(BlueprintCallable, Category=Character)
	void RemoveAllSlows();
	
	UFUNCTION(BlueprintCallable, Category=Character)
	void RemoveAllSlowsOfLevel(FGameplayTag ModifierLevel);
	
	UFUNCTION(BlueprintPure, Category=Character)
	bool IsSlowed() const;
	
	UFUNCTION(BlueprintPure, Category=Character)
	bool WantsSlow() const;

	UFUNCTION(BlueprintCallable, Category=Character)
	FGameplayTag GetSlowLevel() const;

	UFUNCTION(BlueprintCallable, Category=Character)
	int32 GetNumSlows() const;

	UFUNCTION(BlueprintCallable, Category=Character, meta=(FilterGameplayTag="Modifier.Type.Debuff.Slow"))
	int32 GetNumSlowsByLevel(FGameplayTag ModifierLevel) const;
	
	/* ~Slow (Non-Generic) Implementation */
	
protected:
	/* SlowFall (Non-Generic) Implementation */

	UPROPERTY(ReplicatedUsing=OnRep_SimulatedSlowFall)
	uint8 SimulatedSlowFall;

	UFUNCTION()
	void OnRep_SimulatedSlowFall(uint8 PrevSimulatedSlowFall);

public:
	UFUNCTION(BlueprintCallable, Category=Character, meta=(FilterGameplayTag="Modifier.Type.Buff.SlowFall"))
	void SlowFall(FGameplayTag ModifierLevel);

	UFUNCTION(BlueprintCallable, Category=Character, meta=(FilterGameplayTag="Modifier.Type.Buff.SlowFall"))
	void RemoveSlowFall(FGameplayTag ModifierLevel);

	UFUNCTION(BlueprintCallable, Category=Character)
	void RemoveAllSlowFall();
	
	UFUNCTION(BlueprintCallable, Category=Character)
	void RemoveAllSlowFallOfLevel(FGameplayTag ModifierLevel);
	
	UFUNCTION(BlueprintPure, Category=Character)
	bool IsSlowFall() const;
	
	UFUNCTION(BlueprintPure, Category=Character)
	bool WantsSlowFall() const;

	UFUNCTION(BlueprintCallable, Category=Character)
	FGameplayTag GetSlowFallLevel() const;

	UFUNCTION(BlueprintCallable, Category=Character)
	int32 GetNumSlowFalls() const;

	UFUNCTION(BlueprintCallable, Category=Character, meta=(FilterGameplayTag="Modifier.Type.Buff.SlowFall"))
	int32 GetNumSlowFallsByLevel(FGameplayTag ModifierLevel) const;
	
	/* ~SlowFall (Non-Generic) Implementation */
	
protected:
	/* Snare (Non-Generic) Implementation -- Implement per-modifier type */
	
	UPROPERTY(ReplicatedUsing=OnRep_SimulatedSnare)
	uint8 SimulatedSnare;

	UFUNCTION()
	void OnRep_SimulatedSnare(uint8 PrevSimulatedSnare);

public:
	UFUNCTION(BlueprintCallable, Category=Character, meta=(FilterGameplayTag="Modifier.Type.Debuff.Snare"))
	void Snare(FGameplayTag ModifierLevel);

	UFUNCTION(BlueprintCallable, Category=Character, meta=(FilterGameplayTag="Modifier.Type.Debuff.Snare"))
	void RemoveSnare(FGameplayTag ModifierLevel);

	UFUNCTION(BlueprintCallable, Category=Character)
	void RemoveAllSnares();
	
	UFUNCTION(BlueprintCallable, Category=Character)
	void RemoveAllSnaresOfLevel(FGameplayTag ModifierLevel);
	
	UFUNCTION(BlueprintPure, Category=Character)
	bool IsSnared() const;
	
	UFUNCTION(BlueprintPure, Category=Character)
	bool WantsSnare() const;

	UFUNCTION(BlueprintCallable, Category=Character)
	FGameplayTag GetSnareLevel() const;

	UFUNCTION(BlueprintCallable, Category=Character)
	int32 GetNumSnares() const;

	UFUNCTION(BlueprintCallable, Category=Character, meta=(FilterGameplayTag="Modifier.Type.Debuff.Snare"))
	int32 GetNumSnaresByLevel(FGameplayTag ModifierLevel) const;
	
	/* ~Snare (Non-Generic) Implementation */
	
public:
	/**
	 * Consume the PendingMove if it exists, by sending it to the server and clearing it
	 * Useful for movement in GAS where we need the server to complete anything pending to resolve de-sync that occurs as a result of a delayed move
	 * @note Doesn't simply trash the PendingMove and there is only ever one, this function is poorly named and comes from Unreal
	 */
	UFUNCTION(BlueprintCallable, Category=Character)
	void FlushServerMoves();
};
