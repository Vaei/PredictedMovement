// Copyright (c) 2023 Jared Taylor. All Rights Reserved.


#include "Strafe/StrafeCharacter.h"

#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Strafe/StrafeMovement.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StrafeCharacter)

AStrafeCharacter::AStrafeCharacter(const FObjectInitializer& FObjectInitializer)
	: Super(FObjectInitializer.SetDefaultSubobjectClass<UStrafeMovement>(CharacterMovementComponentName))
{
	StrafeMovement = Cast<UStrafeMovement>(GetCharacterMovement());
}

void AStrafeCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Legacy
	// DOREPLIFETIME_CONDITION(ThisClass, bIsStrafing, COND_SimulatedOnly);
	
	// Push Model
	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;
	SharedParams.Condition = COND_SimulatedOnly;

	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, bIsStrafing, SharedParams);
}

void AStrafeCharacter::SetIsStrafing(bool bNewStrafing)
{
	if (bIsStrafing != bNewStrafing)
	{
		bIsStrafing = bNewStrafing;

		if (HasAuthority())
		{
			MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, bIsStrafing, this);  // Push-model
		}
	}
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
	if (StrafeMovement && CanStrafe())
	{
		StrafeMovement->bWantsToStrafe = true;
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

void AStrafeCharacter::OnStartStrafe()
{
	K2_OnStartStrafe();
}

void AStrafeCharacter::OnEndStrafe()
{
	K2_OnEndStrafe();
}