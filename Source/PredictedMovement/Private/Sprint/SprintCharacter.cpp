// Copyright (c) 2023 Jared Taylor. All Rights Reserved.


#include "Sprint/SprintCharacter.h"

#include "Net/UnrealNetwork.h"
#include "Sprint/SprintMovement.h"

ASprintCharacter::ASprintCharacter(const FObjectInitializer& OI)
	: Super(OI.SetDefaultSubobjectClass<USprintMovement>(CharacterMovementComponentName))
{
	SprintMovement = Cast<USprintMovement>(GetCharacterMovement());
}

void ASprintCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ThisClass, bIsSprinting, COND_SimulatedOnly);
}

void ASprintCharacter::OnRep_IsSprinting()
{
	if (SprintMovement)
	{
		if (bIsSprinting)
		{
			SprintMovement->bWantsToSprint = true;
			SprintMovement->Sprint(true);
		}
		else
		{
			SprintMovement->bWantsToSprint = false;
			SprintMovement->UnSprint(true);
		}
		SprintMovement->bNetworkUpdateReceived = true;
	}
}

void ASprintCharacter::Sprint(bool bClientSimulation)
{
	if (SprintMovement)
	{
		if (CanSprint())
		{
			SprintMovement->bWantsToSprint = true;
		}
	}
}

void ASprintCharacter::UnSprint(bool bClientSimulation)
{
	if (SprintMovement)
	{
		SprintMovement->bWantsToSprint = false;
	}
}

bool ASprintCharacter::CanSprint() const
{
	if (bIsSprinting || !GetRootComponent() && GetRootComponent()->IsSimulatingPhysics())
	{
		return false;
	}
		
	// This check ensures that we are not sprinting backward or sideways, while allowing leeway 
	// This angle allows sprinting when holding forward, forward left, forward right
	// but not left or right or backward)
	static constexpr float MaxSprintInputDegrees = 50.f;
	static constexpr float MaxSprintInputNormal = 0.64278732;  // cos(rad(MaxSprintInputDegrees))

	if constexpr (MaxSprintInputDegrees > 0.f)
	{
		if (SprintMovement)
		{
			const float Dot = (SprintMovement->GetCurrentAcceleration().GetSafeNormal2D() | GetActorForwardVector());
			if (Dot < MaxSprintInputNormal)
			{
				return false;
			}
		}
	}

	return true;
}

void ASprintCharacter::OnEndSprint()
{
	K2_OnEndSprint();
}

void ASprintCharacter::OnStartSprint()
{
	K2_OnStartSprint();
}