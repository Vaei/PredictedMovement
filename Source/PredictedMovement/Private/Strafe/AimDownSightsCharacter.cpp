// Copyright (c) 2023 Jared Taylor. All Rights Reserved.


#include "Strafe/AimDownSightsCharacter.h"

#include "Net/UnrealNetwork.h"
#include "Strafe/AimDownSightsMovement.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AimDownSightsCharacter)

AAimDownSightsCharacter::AAimDownSightsCharacter(const FObjectInitializer& FObjectInitializer)
	: Super(FObjectInitializer.SetDefaultSubobjectClass<UAimDownSightsMovement>(CharacterMovementComponentName))
{
	AimDownSightsMovement = Cast<UAimDownSightsMovement>(GetCharacterMovement());
}

void AAimDownSightsCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ThisClass, bIsAimingDownSights, COND_SimulatedOnly);
}

void AAimDownSightsCharacter::OnRep_IsAimingDownSights()
{
	if (AimDownSightsMovement)
	{
		if (bIsAimingDownSights)
		{
			AimDownSightsMovement->bWantsToAimDownSights = true;
			AimDownSightsMovement->AimDownSights(true);
		}
		else
		{
			AimDownSightsMovement->bWantsToAimDownSights = false;
			AimDownSightsMovement->StopAimDownSights(true);
		}
		AimDownSightsMovement->bNetworkUpdateReceived = true;
	}
}

void AAimDownSightsCharacter::AimDownSights(bool bClientSimulation)
{
	if (AimDownSightsMovement)
	{
		if (CanAimDownSights())
		{
			AimDownSightsMovement->bWantsToAimDownSights = true;
		}
	}
}

void AAimDownSightsCharacter::StopAimDownSights(bool bClientSimulation)
{
	if (AimDownSightsMovement)
	{
		AimDownSightsMovement->bWantsToAimDownSights = false;
	}
}

bool AAimDownSightsCharacter::CanAimDownSights() const
{
	return !bIsAimingDownSights && GetRootComponent() && !GetRootComponent()->IsSimulatingPhysics();
}

void AAimDownSightsCharacter::OnEndAimDownSights()
{
	K2_OnEndAimDownSights();
}

void AAimDownSightsCharacter::OnStartAimDownSights()
{
	K2_OnStartAimDownSights();
}