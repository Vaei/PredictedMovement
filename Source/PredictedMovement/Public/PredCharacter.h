// Copyright (c) 2023 Jared Taylor. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PredCharacter.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FStaminaChangeEvent, float, Stamina, float, PrevStamina);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FStaminaEvent);

class UPredMovement;
UCLASS()
class PREDICTEDMOVEMENT_API APredCharacter : public ACharacter
{
	GENERATED_BODY()

private:
	/** Movement component used for movement logic in various movement modes (walking, falling, etc), containing relevant settings and functions to control movement. */
	UPROPERTY(Category=Character, VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	TObjectPtr<UPredMovement> PredMovement;

	friend class FSavedMove_Character_Pred;
protected:
	FORCEINLINE UPredMovement* GetPredCharacterMovement() const { return PredMovement; }
	
protected:
	/** Set by character movement to specify that this Character is currently Sprinting. */
	UPROPERTY(BlueprintReadOnly, replicatedUsing=OnRep_IsSprinting, Category=Character)
	uint32 bIsSprinting:1;
	
public:
	APredCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	void SetIsSprinting(bool bNewSprinting);

	UFUNCTION(BlueprintPure, Category=Character)
	bool IsSprinting() const { return bIsSprinting; }

	UFUNCTION(BlueprintPure, Category=Character)
	bool IsSprintingAtSpeed() const;
	
	/** Handle Sprinting replicated from server */
	UFUNCTION()
	virtual void OnRep_IsSprinting();

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

	/** Called when Character stops Sprinting. Called on non-owned Characters through bIsSprinting replication. */
	virtual void OnEndSprint();

	/** Event when Character stops Sprinting. */
	UFUNCTION(BlueprintImplementableEvent, Category=Character, meta=(DisplayName="OnEndSprint", ScriptName="OnEndSprint"))
	void K2_OnEndSprint();

	/** Called when Character Sprints. Called on non-owned Characters through bIsSprinting replication. */
	virtual void OnStartSprint();

	/** Event when Character Sprints. */
	UFUNCTION(BlueprintImplementableEvent, Category=Character, meta=(DisplayName="OnStartSprint", ScriptName="OnStartSprint"))
	void K2_OnStartSprint();

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

	/** @return true if this character is currently able to AimDownSights (and is not currently AimingDownSights) */
	UFUNCTION(BlueprintCallable, Category=Character)
	virtual bool CanAimDownSights() const;
	
	/** Called when Character stops AimingDownSights. Called on non-owned Characters through bIsAimingDownSights replication. */
	virtual void OnEndAimDownSights();

	/** Event when Character stops AimingDownSights. */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName="OnEndAimDownSights", ScriptName="OnEndAimDownSights"))
	void K2_OnEndAimDownSights();

	/** Called when Character AimDownSightses. Called on non-owned Characters through bIsAimingDownSights replication. */
	virtual void OnStartAimDownSights();

	/** Event when Character AimDownSightses. */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName="OnStartAimDownSights", ScriptName="OnStartAimDownSights"))
	void K2_OnStartAimDownSights();

public:
	/**
	 * Consume the PendingMove if it exists, by sending it to the server and clearing it
	 * Useful for movement in GAS where we need the server to complete anything pending to resolve de-sync that occurs as a result of a delayed move
	 * @note Doesn't simply trash the PendingMove and there is only ever one, this function is poorly named and comes from Unreal
	 */
	UFUNCTION(BlueprintCallable, Category=Character)
	void FlushServerMoves();
};
