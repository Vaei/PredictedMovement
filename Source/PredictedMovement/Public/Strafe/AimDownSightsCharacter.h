// Copyright (c) 2023 Jared Taylor. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Prone/ProneCharacter.h"
#include "AimDownSightsCharacter.generated.h"

class UAimDownSightsMovement;

/**
 * Strafe is a shell intended for changing to and from a strafing state, however the actual implementation of
 * "what strafe does" is highly dependent on a project, so override the functions and define the behaviour yourself.
 *
 * Generally AStrafeCharacter::OnStartStrafe might change bUseControllerRotationYaw = true and
 * bOrientRotationToMovement = false then and AStrafeCharacter::OnEndStrafe would change them back.
 *
 * Alternatively you could override any ACharacter and UCharacterMovementComponent functions that use
 * bUseControllerRotationYaw or bOrientRotationToMovement and check IsStrafing, but that implementation will be
 * more advanced and often unnecessary.
 */
UCLASS()
class PREDICTEDMOVEMENT_API AAimDownSightsCharacter : public AProneCharacter
{
	GENERATED_BODY()

private:
	/** Movement component used for movement logic in various movement modes (walking, falling, etc), containing relevant settings and functions to control movement. */
	TObjectPtr<UAimDownSightsMovement> AimDownSightsMovement;

	friend class FSavedMove_Character_AimDownSights;
protected:
	FORCEINLINE UAimDownSightsMovement* GetAimDownSightsCharacterMovement() const { return AimDownSightsMovement; }
	
public:
	/** Set by character movement to specify that this Character is currently Strafing. */
	UPROPERTY(BlueprintReadOnly, replicatedUsing=OnRep_IsAimingDownSights, Category=Character)
	uint32 bIsAimingDownSights:1;
	
public:
	AAimDownSightsCharacter(const FObjectInitializer& FObjectInitializer);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	/** Handle Crouching replicated from server */
	UFUNCTION()
	virtual void OnRep_IsAimingDownSights();

	/**
	 * Request the character to start Strafing. The request is processed on the next update of the CharacterMovementComponent.
	 * @see OnStartStrafe
	 * @see IsStrafing
	 * @see CharacterMovement->WantsToStrafe
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(HidePin="bClientSimulation"))
	virtual void AimDownSights(bool bClientSimulation = false);

	/**
	 * Request the character to stop Strafing. The request is processed on the next update of the CharacterMovementComponent.
	 * @see OnEndStrafe
	 * @see IsStrafing
	 * @see CharacterMovement->WantsToStrafe
	 */
	UFUNCTION(BlueprintCallable, Category=Character, meta=(HidePin="bClientSimulation"))
	virtual void StopAimDownSights(bool bClientSimulation = false);

	/** @return true if this character is currently able to Strafe (and is not currently Strafing) */
	UFUNCTION(BlueprintCallable, Category=Character)
	virtual bool CanAimDownSights() const;
	
	/** Called when Character stops Strafing. Called on non-owned Characters through bIsStrafing replication. */
	virtual void OnEndAimDownSights();

	/** Event when Character stops Strafing. */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName="OnEndAimDownSights", ScriptName="OnEndAimDownSights"))
	void K2_OnEndAimDownSights();

	/** Called when Character Strafes. Called on non-owned Characters through bIsStrafing replication. */
	virtual void OnStartAimDownSights();

	/** Event when Character Strafes. */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName="OnStartAimDownSights", ScriptName="OnStartAimDownSights"))
	void K2_OnStartAimDownSights();
};
