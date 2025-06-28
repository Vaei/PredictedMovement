// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "ModifierCharacter.generated.h"

struct FGameplayTag;
class UModifierMovement;

UCLASS()
class PREDICTEDMOVEMENT_API AModifierCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	/** Movement component used for movement logic in various movement modes (walking, falling, etc), containing relevant settings and functions to control movement. */
	UPROPERTY(Category=Character, VisibleAnywhere, BlueprintReadOnly, meta=(AllowPrivateAccess = "true"))
	TObjectPtr<UModifierMovement> ModifierMovement;

	friend class FSavedMove_Character_Modifier;
protected:
	FORCEINLINE UModifierMovement* GetModifierCharacterMovement() const { return ModifierMovement; }

public:
	AModifierCharacter(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;

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
	UFUNCTION(BlueprintCallable, Category=Character, meta=(GameplayTagFilter="Modifier.Type.Local.Boost"))
	void Boost(FGameplayTag ModifierLevel);
	
	UFUNCTION(BlueprintCallable, Category=Character, meta=(GameplayTagFilter="Modifier.Type.Local.Boost"))
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

	UFUNCTION(BlueprintCallable, Category=Character, meta=(GameplayTagFilter="Modifier.Type.Local.Boost"))
	int32 GetNumBoostsByLevel(FGameplayTag ModifierLevel) const;
	
	/* ~Boost (Non-Generic) Implementation */

protected:
	/* Slow Fall (Non-Generic) Implementation */

	UPROPERTY(ReplicatedUsing=OnRep_SimulatedSlowFall)
	uint8 SimulatedSlowFall;

	UFUNCTION()
	void OnRep_SimulatedSlowFall(uint8 PrevSimulatedSlowFall);

public:
	UFUNCTION(BlueprintCallable, Category=Character, meta=(GameplayTagFilter="Modifier.Type.Local.SlowFall"))
	void SlowFall(FGameplayTag ModifierLevel);

	UFUNCTION(BlueprintCallable, Category=Character, meta=(GameplayTagFilter="Modifier.Type.Local.SlowFall"))
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

	UFUNCTION(BlueprintCallable, Category=Character, meta=(GameplayTagFilter="Modifier.Type.Local.SlowFall"))
	int32 GetNumSlowFallsByLevel(FGameplayTag ModifierLevel) const;
	
	/* ~Slow Fall (Non-Generic) Implementation */
	
protected:
	/* Snare (Non-Generic) Implementation -- Implement per-modifier type */
	
	UPROPERTY(ReplicatedUsing=OnRep_SimulatedSnare)
	uint8 SimulatedSnare;

	UFUNCTION()
	void OnRep_SimulatedSnare(uint8 PrevSimulatedSnare);

public:
	UFUNCTION(BlueprintCallable, Category=Character, meta=(GameplayTagFilter="Modifier.Type.Debuff.Snare"))
	void Snare(FGameplayTag ModifierLevel);

	UFUNCTION(BlueprintCallable, Category=Character, meta=(GameplayTagFilter="Modifier.Type.Debuff.Snare"))
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

	UFUNCTION(BlueprintCallable, Category=Character, meta=(GameplayTagFilter="Modifier.Type.Debuff.Snare"))
	int32 GetNumSnaresByLevel(FGameplayTag ModifierLevel) const;
	
	/* ~Snare (Non-Generic) Implementation */
};
