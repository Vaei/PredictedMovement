// Copyright (c) 2023 Jared Taylor. All Rights Reserved.


#include "PredCharacter.h"

#include "Net/UnrealNetwork.h"
#include "PredMovement.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PredCharacter)

APredCharacter::APredCharacter(const FObjectInitializer& FObjectInitializer)
	: Super(FObjectInitializer.SetDefaultSubobjectClass<UPredMovement>(CharacterMovementComponentName))
{
	PredMovement = Cast<UPredMovement>(GetCharacterMovement());
}

void APredCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ThisClass, bIsSprinting, COND_SimulatedOnly);
}

void APredCharacter::OnRep_IsSprinting()
{
	if (PredMovement)
	{
		if (bIsSprinting)
		{
			PredMovement->bWantsToSprint = true;
			PredMovement->Sprint(true);
		}
		else
		{
			PredMovement->bWantsToSprint = false;
			PredMovement->UnSprint(true);
		}
		PredMovement->bNetworkUpdateReceived = true;
	}
}

void APredCharacter::Sprint(bool bClientSimulation)
{
	if (PredMovement)
	{
		PredMovement->bWantsToSprint = true;
	}
}

void APredCharacter::UnSprint(bool bClientSimulation)
{
	if (PredMovement)
	{
		PredMovement->bWantsToSprint = false;
	}
}

void APredCharacter::OnEndSprint()
{
	K2_OnEndSprint();
}

void APredCharacter::OnStartSprint()
{
	K2_OnStartSprint();
}

float APredCharacter::GetStamina() const
{
	return PredMovement ? PredMovement->GetStamina() : 0.f;
}

float APredCharacter::GetMaxStamina() const
{
	return PredMovement ? PredMovement->GetMaxStamina() : 0.f;
}

float APredCharacter::GetStaminaPct() const
{
	return PredMovement ? PredMovement->GetStaminaPct() : 0.f;
}

void APredCharacter::FlushServerMoves()
{
	if (PredMovement)
	{
		PredMovement->FlushServerMoves();
	}
}
