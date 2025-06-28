// Copyright (c) Jared Taylor


#include "Sprint/SprintCharacter.h"

#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
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

	// Legacy
	// DOREPLIFETIME_CONDITION(ThisClass, bIsSprinting, COND_SimulatedOnly);

	// Push Model
	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;
	SharedParams.Condition = COND_SimulatedOnly;

	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, bIsSprinting, SharedParams);
}

void ASprintCharacter::SetIsSprinting(bool bNewSprinting)
{
	if (bIsSprinting != bNewSprinting)
	{
		bIsSprinting = bNewSprinting;

		if (HasAuthority())
		{
			MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, bIsSprinting, this);  // Push-model
		}
	}
}

bool ASprintCharacter::IsSprintingAtSpeed() const
{
	return SprintMovement && SprintMovement->IsSprintingAtSpeed();
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
	if (SprintMovement && CanSprint())
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

bool ASprintCharacter::CanSprint() const
{
	return !bIsSprinting && GetRootComponent() && !GetRootComponent()->IsSimulatingPhysics();
}

void ASprintCharacter::OnStartSprint()
{
	K2_OnStartSprint();
}

void ASprintCharacter::OnEndSprint()
{
	K2_OnEndSprint();
}