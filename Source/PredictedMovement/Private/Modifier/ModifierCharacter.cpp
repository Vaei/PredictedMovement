// Copyright (c) 2023 Jared Taylor. All Rights Reserved.

#include "Modifier/ModifierCharacter.h"

#include "Modifier/ModifierMovement.h"
#include "Net/UnrealNetwork.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModifierCharacter)

AModifierCharacter::AModifierCharacter(const FObjectInitializer& FObjectInitializer)
	: Super(FObjectInitializer.SetDefaultSubobjectClass<UModifierMovement>(CharacterMovementComponentName))
{
	ModifierMovement = Cast<UModifierMovement>(GetCharacterMovement());
}

void AModifierCharacter::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(AModifierCharacter, SimulatedBoost, COND_SimulatedOnly);
	DOREPLIFETIME_CONDITION(AModifierCharacter, SimulatedSnare, COND_SimulatedOnly);
}

void AModifierCharacter::OnModifierChanged(const FGameplayTag& ModifierType, uint8 ModifierLevel, uint8 PrevModifierLevel)
{
	// Events for when a modifier is added or removed
	if (ModifierLevel > 0 && PrevModifierLevel == 0)
	{
		OnModifierAdded(ModifierType, ModifierLevel, PrevModifierLevel);
	}
	else if (ModifierLevel == 0 && PrevModifierLevel > 0)
	{
		OnModifierRemoved(ModifierType, ModifierLevel, PrevModifierLevel);
	}

	// Replicate to simulated proxies
	if (HasAuthority())
	{
		 SimulatedSnare = ModifierLevel;
	}
}

void AModifierCharacter::OnModifierAdded(const FGameplayTag& ModifierType, uint8 ModifierLevel, uint8 PrevModifierLevel)
{
	// @TIP: Add Loose Gameplay Tag Here (Not Replicated)

	K2_OnModifierAdded(ModifierType, ModifierLevel, PrevModifierLevel);
}

void AModifierCharacter::OnModifierRemoved(const FGameplayTag& ModifierType, uint8 ModifierLevel, uint8 PrevModifierLevel)
{
	// @TIP: Remove Loose Gameplay Tag Here (Not Replicated)

	K2_OnModifierRemoved(ModifierType, ModifierLevel, PrevModifierLevel);
}

/* Boost */

void AModifierCharacter::OnRep_SimulatedBoost(uint8 PrevSimulatedBoost)
{
	if (ModifierMovement)
	{
		ModifierMovement->Boost.ModifierLevel = SimulatedBoost;
		ModifierMovement->bNetworkUpdateReceived = true;
		
		OnModifierChanged(FModifierTags::Modifier_Type_Buff_Boost, SimulatedBoost, PrevSimulatedBoost);
	}
}

void AModifierCharacter::Boost(EBoost BoostLevel)
{
	if (ModifierMovement)
	{
		ModifierMovement->Boost.AddModifier(static_cast<uint8>(BoostLevel));
	}
}

void AModifierCharacter::RemoveBoost(EBoost BoostLevel)
{
	if (ModifierMovement)
	{
		ModifierMovement->Boost.RemoveModifier(static_cast<uint8>(BoostLevel));
	}
}

bool AModifierCharacter::IsBoosted() const
{
	return ModifierMovement && ModifierMovement->Boost.HasModifier();
}

EBoost AModifierCharacter::GetBoostLevel() const
{
	return ModifierMovement ? ModifierMovement->Boost.GetModifierLevel<EBoost>() : EBoost::None;
}

int32 AModifierCharacter::GetNumBoosts() const
{
	return ModifierMovement ? ModifierMovement->Boost.GetNumModifiers() : 0;
}

int32 AModifierCharacter::GetNumBoostsByLevel(EBoost BoostLevel) const
{
	return ModifierMovement ? ModifierMovement->Boost.GetNumModifiersByLevel(static_cast<uint8>(BoostLevel)) : 0;
}


/* Snare */

void AModifierCharacter::OnRep_SimulatedSnare(uint8 PrevSimulatedSnare)
{
	if (ModifierMovement)
	{
		ModifierMovement->Snare.ModifierLevel = SimulatedSnare;
		ModifierMovement->bNetworkUpdateReceived = true;
		
		OnModifierChanged(FModifierTags::Modifier_Type_Debuff_Snare, SimulatedSnare, PrevSimulatedSnare);
	}
}

void AModifierCharacter::Snare(ESnare SnareLevel)
{
	if (ModifierMovement)
	{
		ModifierMovement->Snare.AddModifier(static_cast<uint8>(SnareLevel));
	}
}

void AModifierCharacter::RemoveSnare(ESnare SnareLevel)
{
	if (ModifierMovement)
	{
		ModifierMovement->Snare.RemoveModifier(static_cast<uint8>(SnareLevel));
	}
}

bool AModifierCharacter::IsSnared() const
{
	return ModifierMovement && ModifierMovement->Snare.HasModifier();
}

ESnare AModifierCharacter::GetSnareLevel() const
{
	return ModifierMovement ? ModifierMovement->Snare.GetModifierLevel<ESnare>() : ESnare::None;
}

int32 AModifierCharacter::GetNumSnares() const
{
	return ModifierMovement ? ModifierMovement->Snare.GetNumModifiers() : 0;
}

int32 AModifierCharacter::GetNumSnaresByLevel(ESnare SnareLevel) const
{
	return ModifierMovement ? ModifierMovement->Snare.GetNumModifiersByLevel(static_cast<uint8>(SnareLevel)) : 0;
}
