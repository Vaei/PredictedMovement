// Copyright (c) 2023 Jared Taylor. All Rights Reserved.


#include "Strafe/StrafeCharacter.h"

#include "Net/UnrealNetwork.h"
#include "Strafe/StrafeMovement.h"

AStrafeCharacter::AStrafeCharacter(const FObjectInitializer& FObjectInitializer)
	: Super(FObjectInitializer.SetDefaultSubobjectClass<UStrafeMovement>(CharacterMovementComponentName))
{
	StrafeMovement = Cast<UStrafeMovement>(GetCharacterMovement());
}

void AStrafeCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ThisClass, bIsStrafing, COND_SimulatedOnly);
}

void AStrafeCharacter::OnRep_IsStrafing()
{
	if (StrafeMovement)
	{
		if (bIsStrafing)
		{
			StrafeMovement->bWantsToStrafe = true;
			StrafeMovement->Strafe(true);
		}
		else
		{
			StrafeMovement->bWantsToStrafe = false;
			StrafeMovement->UnStrafe(true);
		}
		StrafeMovement->bNetworkUpdateReceived = true;
	}
}

void AStrafeCharacter::Strafe(bool bClientSimulation)
{
	if (StrafeMovement)
	{
		if (CanStrafe())
		{
			StrafeMovement->bWantsToStrafe = true;
		}
	}
}

void AStrafeCharacter::UnStrafe(bool bClientSimulation)
{
	if (StrafeMovement)
	{
		StrafeMovement->bWantsToStrafe = false;
	}
}

bool AStrafeCharacter::CanStrafe() const
{
	return !bIsStrafing && GetRootComponent() && !GetRootComponent()->IsSimulatingPhysics();
}

void AStrafeCharacter::OnEndStrafe()
{
	K2_OnEndStrafe();
}

void AStrafeCharacter::OnStartStrafe()
{
	K2_OnStartStrafe();
}