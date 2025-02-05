// Copyright (c) 2023 Jared Taylor. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ProneCharacter.generated.h"

class UProneMovement;
UCLASS()
class PREDICTEDMOVEMENT_API AProneCharacter : public ACharacter
{
	GENERATED_BODY()

private:
	/** Movement component used for movement logic in various movement modes (walking, falling, etc), containing relevant settings and functions to control movement. */
	UPROPERTY(Category=Character, VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	TObjectPtr<UProneMovement> ProneMovement;

	friend class FSavedMove_Character_Prone;
protected:
	FORCEINLINE UProneMovement* GetProneCharacterMovement() const { return ProneMovement; }

public:
	/** Default proned eye height */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Camera)
	float PronedEyeHeight;
	
protected:
	/** Set by character movement to specify that this Character is currently Proned. */
	UPROPERTY(BlueprintReadOnly, replicatedUsing=OnRep_IsProned, Category=Character)
	uint32 bIsProned:1;
	
public:
	AProneCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

public:
	virtual void RecalculateBaseEyeHeight() override;
	
	virtual void SetIsProned(bool bNewProned);

	/** @return true if this character is currently Proned */
	UFUNCTION(BlueprintPure, Category=Character)
	virtual bool IsProned() const { return bIsProned; }

	/** Handle Prone replicated from server */
	UFUNCTION()
	virtual void OnRep_IsProned();

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

	/** @return true if this character is currently able to Prone (and is not currently Proned) */
	UFUNCTION(BlueprintPure, Category=Character)
	virtual bool CanProne() const;
	
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
};

