// Copyright (c) 2023 Jared Taylor. All Rights Reserved.


#include "Sprint/SprintCharacter.h"

#include "Net/UnrealNetwork.h"
#include "Sprint/SprintMovement.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SprintCharacter)

ASprintCharacter::ASprintCharacter(const FObjectInitializer& FObjectInitializer)
	: Super(FObjectInitializer.SetDefaultSubobjectClass<USprintMovement>(CharacterMovementComponentName))
{
	SprintMovement = Cast<USprintMovement>(GetCharacterMovement());
}

void ASprintCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION(ThisClass, bIsSprinting, COND_SimulatedOnly);
}

bool ASprintCharacter::IsSprintWithinAllowableInputAngle() const
{
	return SprintMovement && SprintMovement->IsSprintWithinAllowableInputAngle();
}

bool ASprintCharacter::IsSprintingInEffect() const
{
	return IsSprintingAtSpeed() && IsSprintWithinAllowableInputAngle();
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
		SprintMovement->bWantsToSprint = true;
	}
}

void ASprintCharacter::UnSprint(bool bClientSimulation)
{
	if (SprintMovement)
	{
		SprintMovement->bWantsToSprint = false;
	}
}

void ASprintCharacter::OnEndSprint()
{
	K2_OnEndSprint();
}

void ASprintCharacter::OnStartSprint()
{
	K2_OnStartSprint();
}