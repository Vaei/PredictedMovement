﻿// Copyright (c) 2023 Jared Taylor. All Rights Reserved.

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
	
public:
	/** Set by character movement to specify that this Character is currently Sprinting. */
	UPROPERTY(BlueprintReadOnly, replicatedUsing=OnRep_IsSprinting, Category=Character)
	uint32 bIsSprinting:1;
	
public:
	APredCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	/** Handle Crouching replicated from server */
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
	FStaminaChangeEvent OnStaminaChanged;

	UPROPERTY(BlueprintAssignable, Category=Character)
	FStaminaChangeEvent OnMaxStaminaChanged;

	UPROPERTY(BlueprintAssignable, Category=Character)
	FStaminaEvent OnStaminaDrained;

	UPROPERTY(BlueprintAssignable, Category=Character)
	FStaminaEvent OnStaminaDrainRecovered;

	UFUNCTION(BlueprintPure, Category=Character)
	float GetStamina() const;
	
	UFUNCTION(BlueprintPure, Category=Character)
	float GetMaxStamina() const;
	
	UFUNCTION(BlueprintPure, Category=Character)
	float GetStaminaPct() const;

public:
	/**
	 * Consume the PendingMove if it exists, by sending it to the server and clearing it
	 * Useful for movement in GAS where we need the server to complete anything pending to resolve de-sync that occurs as a result of a delayed move
	 * @note Doesn't simply trash the PendingMove and there is only ever one, this function is poorly named and comes from Unreal
	 */
	UFUNCTION(BlueprintCallable, Category=Character)
	void FlushServerMoves();
};
