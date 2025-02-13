// Copyright (c) 2023 Jared Taylor. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SprintCharacter.generated.h"

class USprintMovement;
UCLASS()
class PREDICTEDMOVEMENT_API ASprintCharacter : public ACharacter
{
	GENERATED_BODY()

private:
	/** Movement component used for movement logic in various movement modes (walking, falling, etc), containing relevant settings and functions to control movement. */
	UPROPERTY(Category=Character, VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	TObjectPtr<USprintMovement> SprintMovement;

	friend class FSavedMove_Character_Sprint;
protected:
	FORCEINLINE USprintMovement* GetSprintCharacterMovement() const { return SprintMovement; }
	
protected:
	/** Set by character movement to specify that this Character is currently Sprinting. */
	UPROPERTY(BlueprintReadOnly, replicatedUsing=OnRep_IsSprinting, Category=Character)
	uint32 bIsSprinting:1;
	
public:
	ASprintCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	void SetIsSprinting(bool bNewSprinting);

	/** @return true if this character is currently Sprinting */
	UFUNCTION(BlueprintPure, Category=Character)
	virtual bool IsSprinting() const { return bIsSprinting; }

	/** @return True if the character is currently Sprinting at or above sprint speed */
	UFUNCTION(BlueprintPure, Category=Character)
	virtual bool IsSprintingAtSpeed() const;

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

	/** @return true if this character is currently able to Sprint (and is not currently Sprinting) */
	UFUNCTION(BlueprintPure, Category=Character)
	virtual bool CanSprint() const;
	
	/** Called when Character Sprints. Called on non-owned Characters through bIsSprinting replication. */
	virtual void OnStartSprint();

	/** Event when Character Sprints. */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName="On Start Sprint"))
	void K2_OnStartSprint();
	
	/** Called when Character stops Sprinting. Called on non-owned Characters through bIsSprinting replication. */
	virtual void OnEndSprint();

	/** Event when Character stops Sprinting. */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName="On End Sprint"))
	void K2_OnEndSprint();
};
