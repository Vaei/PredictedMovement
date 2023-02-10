// Copyright (c) 2023 Jared Taylor. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Prone/ProneCharacter.h"
#include "StrafeCharacter.generated.h"

class UStrafeMovement;
UCLASS()
class PREDICTEDMOVEMENT_API AStrafeCharacter : public AProneCharacter
{
	GENERATED_BODY()

private:
	/** Movement component used for movement logic in various movement modes (walking, falling, etc), containing relevant settings and functions to control movement. */
	UPROPERTY(Category=Character, VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	TObjectPtr<UStrafeMovement> StrafeMovement;

	friend class FSavedMove_Character_Strafe;
protected:
	FORCEINLINE UStrafeMovement* GetStrafeCharacterMovement() const { return StrafeMovement; }
	
public:
	/** Set by character movement to specify that this Character is currently Strafing. */
	UPROPERTY(BlueprintReadOnly, replicatedUsing=OnRep_IsStrafing, Category=Character)
	uint32 bIsStrafing:1;
	
public:
	AStrafeCharacter(const FObjectInitializer& OI);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	/** Handle Crouching replicated from server */
	UFUNCTION()
	virtual void OnRep_IsStrafing();

	/**
	 * Request the character to start Strafing. The request is processed on the next update of the CharacterMovementComponent.
	 * @see OnStartStrafe
	 * @see IsStrafing
	 * @see CharacterMovement->WantsToStrafe
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(HidePin="bClientSimulation"))
	virtual void Strafe(bool bClientSimulation = false);

	/**
	 * Request the character to stop Strafing. The request is processed on the next update of the CharacterMovementComponent.
	 * @see OnEndStrafe
	 * @see IsStrafing
	 * @see CharacterMovement->WantsToStrafe
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(HidePin="bClientSimulation"))
	virtual void UnStrafe(bool bClientSimulation = false);

	/** @return true if this character is currently able to Strafe (and is not currently Strafing) */
	UFUNCTION(BlueprintCallable, Category=Character)
	virtual bool CanStrafe() const;
	
	/** Called when Character stops Strafing. Called on non-owned Characters through bIsStrafing replication. */
	virtual void OnEndStrafe();

	/** Event when Character stops Strafing. */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName="OnEndStrafe", ScriptName="OnEndStrafe"))
	void K2_OnEndStrafe();

	/** Called when Character Strafees. Called on non-owned Characters through bIsStrafing replication. */
	virtual void OnStartStrafe();

	/** Event when Character Strafees. */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName="OnStartStrafe", ScriptName="OnStartStrafe"))
	void K2_OnStartStrafe();
};
