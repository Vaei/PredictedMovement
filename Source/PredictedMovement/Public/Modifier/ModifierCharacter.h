﻿// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ModifierTypes.h"
#include "GameFramework/Character.h"
#include "ModifierCharacter.generated.h"

class UModifierMovement;
UCLASS()
class PREDICTEDMOVEMENT_API AModifierCharacter : public ACharacter
{
	GENERATED_BODY()

protected:
	/** Movement component used for movement logic in various movement modes (walking, falling, etc), containing relevant settings and functions to control movement. */
	UPROPERTY(Category=Character, VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	TObjectPtr<UModifierMovement> ModifierMovement;

	friend class FSavedMove_Character_Modifier;
protected:
	FORCEINLINE UModifierMovement* GetModifierCharacterMovement() const { return ModifierMovement; }

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
	AModifierCharacter(const FObjectInitializer& FObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	/* Boost Implementation */
	
	/** Set by character movement to specify that this Character is currently Modified. */
	UPROPERTY(ReplicatedUsing=OnRep_SimulatedBoost)
	uint8 SimulatedBoost = 0;

	/** Handle Crouching replicated from server */
	UFUNCTION()
	virtual void OnRep_SimulatedBoost(uint8 PrevLevel);

	/**
	 * Request the character to start Boost. The request is processed on the next update of the CharacterMovementComponent.
	 * @param Level The level of the Boost to remove.
	 * @param NetType How the Boost is applied, either locally predicted, with correction, or server initiated.
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(GameplayTagFilter="Modifier.Boost"))
	virtual bool Boost(FGameplayTag Level, EModifierNetType NetType);

	/**
	 * Request the character to stop Boost. The request is processed on the next update of the CharacterMovementComponent.
	 * @param Level The level of the Boost to remove.
	 * @param NetType How the Boost is applied, either locally predicted, with correction, or server initiated.
	 * @param bRemoveAll If true, removes all Boosts of the specified level, otherwise only removes the first one found.
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(GameplayTagFilter="Modifier.Boost"))
	virtual bool UnBoost(FGameplayTag Level, EModifierNetType NetType, bool bRemoveAll=false);

	UFUNCTION(BlueprintCallable, Category=Character)
	virtual bool ResetBoost(EModifierNetType NetType);

	/* ~Boost Implementation */

public:
	/* Snare Implementation */
	
	/** Set by character movement to specify that this Character is currently Modified. */
	UPROPERTY(ReplicatedUsing=OnRep_SimulatedSnare)
	uint8 SimulatedSnare = 0;

	/** Handle Crouching replicated from server */
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
	virtual bool UnSnare(FGameplayTag Level);

	UFUNCTION(BlueprintCallable, Category=Character)
	virtual bool ResetSnare();

	/* ~Snare Implementation */
};
