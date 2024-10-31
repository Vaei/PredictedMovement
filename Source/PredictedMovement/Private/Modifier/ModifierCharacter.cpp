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

	DOREPLIFETIME_CONDITION(AModifierCharacter, SimulatedSnare, COND_SimulatedOnly);
}

void AModifierCharacter::OnModifierChanged(const FGameplayTag& ModifierType, uint8 ModifierLevel, uint8 PrevModifierLevel)
{
	// Events for when a modifier is added or removed
	if (ModifierLevel > 0 && PrevModifierLevel == 0)
	{
		OnModifierAdded(ModifierType, ModifierLevel);
	}
	else if (ModifierLevel == 0 && PrevModifierLevel > 0)
	{
		OnModifierRemoved(ModifierType, PrevModifierLevel);
	}

	// Replicate to simulated proxies
	if (HasAuthority())
	{
		 SimulatedSnare = ModifierLevel;
	}
}

void AModifierCharacter::OnModifierAdded(const FGameplayTag& ModifierType, uint8 ModifierLevel)
{
	// @TIP: Add Loose Gameplay Tag Here (Not Replicated)

	K2_OnModifierAdded(ModifierType, ModifierLevel, 0);
}

void AModifierCharacter::OnModifierRemoved(const FGameplayTag& ModifierType, uint8 ModifierLevel)
{
	// @TIP: Remove Loose Gameplay Tag Here (Not Replicated)

	K2_OnModifierRemoved(ModifierType, ModifierLevel, 0);
}

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
