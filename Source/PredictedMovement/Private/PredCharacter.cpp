// Copyright (c) 2023 Jared Taylor. All Rights Reserved.


#include "PredCharacter.h"

#include "Net/UnrealNetwork.h"
#include "PredMovement.h"
#include "Net/Core/PushModel/PushModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PredCharacter)

APredCharacter::APredCharacter(const FObjectInitializer& FObjectInitializer)
	: Super(FObjectInitializer.SetDefaultSubobjectClass<UPredMovement>(CharacterMovementComponentName))
{
	PredMovement = Cast<UPredMovement>(GetCharacterMovement());
}

void APredCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Push Model
	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;
	SharedParams.Condition = COND_SimulatedOnly;

	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, bIsAimingDownSights, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, bIsSprinting, SharedParams);
}

void APredCharacter::SetIsSprinting(bool bNewSprinting)
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

bool APredCharacter::IsSprintingAtSpeed() const
{
	return PredMovement ? PredMovement->IsSprintingAtSpeed() : false;
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

void APredCharacter::OnStaminaChanged(float Stamina, float PrevStamina)
{
	K2_OnStaminaChanged(Stamina, PrevStamina);
	NotifyOnStaminaChanged.Broadcast(Stamina, PrevStamina);
}

void APredCharacter::OnMaxStaminaChanged(float MaxStamina, float PrevMaxStamina)
{
	K2_OnMaxStaminaChanged(MaxStamina, PrevMaxStamina);
	NotifyOnMaxStaminaChanged.Broadcast(MaxStamina, PrevMaxStamina);
}

void APredCharacter::OnStaminaDrained()
{
	K2_OnStaminaDrained();
	NotifyOnStaminaDrained.Broadcast();
}

void APredCharacter::OnStaminaDrainRecovered()
{
	K2_OnStaminaDrainRecovered();
	NotifyOnStaminaDrainRecovered.Broadcast();
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

bool APredCharacter::IsStaminaDrained() const
{
	return PredMovement ? PredMovement->IsStaminaDrained() : false;
}

void APredCharacter::SetIsAimingDownSights(bool bNewAimingDownSights)
{
	if (bIsAimingDownSights != bNewAimingDownSights)
	{
		bIsAimingDownSights = bNewAimingDownSights;

		if (HasAuthority())
		{
			MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, bIsAimingDownSights, this);  // Push-model
		}
	}
}

void APredCharacter::OnRep_IsAimingDownSights()
{
	if (PredMovement)
	{
		if (bIsAimingDownSights)
		{
			PredMovement->bWantsToAimDownSights = true;
			PredMovement->AimDownSights(true);
		}
		else
		{
			PredMovement->bWantsToAimDownSights = false;
			PredMovement->UnAimDownSights(true);
		}
		PredMovement->bNetworkUpdateReceived = true;
	}
}

void APredCharacter::AimDownSights(bool bClientSimulation)
{
	if (PredMovement)
	{
		PredMovement->bWantsToAimDownSights = true;
	}
}

void APredCharacter::UnAimDownSights(bool bClientSimulation)
{
	if (PredMovement)
	{
		PredMovement->bWantsToAimDownSights = false;
	}
}

void APredCharacter::OnEndAimDownSights()
{
	K2_OnEndAimDownSights();
}

void APredCharacter::OnStartAimDownSights()
{
	K2_OnStartAimDownSights();
}

void APredCharacter::FlushServerMoves()
{
	if (PredMovement)
	{
		PredMovement->FlushServerMoves();
	}
}
