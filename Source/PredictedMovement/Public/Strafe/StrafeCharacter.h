// Copyright (c) 2023 Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "StrafeCharacter.generated.h"

class UStrafeMovement;

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
class PREDICTEDMOVEMENT_API AStrafeCharacter : public ACharacter
{
	GENERATED_BODY()

private:
	/** Movement component used for movement logic in various movement modes (walking, falling, etc), containing relevant settings and functions to control movement. */
	UPROPERTY(Category=Character, VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	TObjectPtr<UStrafeMovement> StrafeMovement;

	friend class FSavedMove_Character_Strafe;
protected:
	FORCEINLINE UStrafeMovement* GetStrafeCharacterMovement() const { return StrafeMovement; }
	
protected:
	/** Set by character movement to specify that this Character is currently Strafing. */
	UPROPERTY(BlueprintReadOnly, replicatedUsing=OnRep_IsStrafing, Category=Character)
	uint32 bIsStrafing:1;
	
public:
	AStrafeCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	virtual void SetIsStrafing(bool bNewStrafing);

	/** @return true if this character is currently Strafing */
	UFUNCTION(BlueprintPure, Category=Character)
	virtual bool IsStrafing() const { return bIsStrafing; }
	
	/** Handle Strafing replicated from server */
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
	UFUNCTION(BlueprintPure, Category=Character)
	virtual bool CanStrafe() const;
	
	/** Called when Character Strafes. Called on non-owned Characters through bIsStrafing replication. */
	virtual void OnStartStrafe();

	/** Event when Character Strafes. */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName="On Start Strafe"))
	void K2_OnStartStrafe();
	
	/** Called when Character stops Strafing. Called on non-owned Characters through bIsStrafing replication. */
	virtual void OnEndStrafe();

	/** Event when Character stops Strafing. */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName="On End Strafe"))
	void K2_OnEndStrafe();
};
