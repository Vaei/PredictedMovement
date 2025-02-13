// Copyright (c) 2023 Jared Taylor. All Rights Reserved.


#include "PredCharacter.h"

#include "Net/UnrealNetwork.h"
#include "PredMovement.h"
#include "Net/Core/PushModel/PushModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PredCharacter)

DEFINE_LOG_CATEGORY_STATIC(LogPredCharacter, Log, All);

APredCharacter::APredCharacter(const FObjectInitializer& FObjectInitializer)
	: Super(FObjectInitializer.SetDefaultSubobjectClass<UPredMovement>(CharacterMovementComponentName))
{
	PredMovement = Cast<UPredMovement>(GetCharacterMovement());

	PronedEyeHeight = 16.f;
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
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, bIsProned, SharedParams);
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

bool APredCharacter::IsSprintWithinAllowableInputAngle() const
{
	return PredMovement && PredMovement->IsSprintWithinAllowableInputAngle();
}

bool APredCharacter::IsSprintingInEffect() const
{
	return IsSprintingAtSpeed() && IsSprintWithinAllowableInputAngle();
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

bool APredCharacter::CanSprint() const
{
	return !bIsSprinting && GetRootComponent() && !GetRootComponent()->IsSimulatingPhysics();
}

void APredCharacter::Sprint(bool bClientSimulation)
{
	if (PredMovement && CanSprint())
	{
		PredMovement->bWantsToSprint = true;
		
		if (!bClientSimulation)
		{
			// If we can't sprint during certain states, then allow sprint to cancel those states
			if (bIsCrouched && !PredMovement->bCanSprintDuringCrouch)
			{
				UnCrouch();
			}
			if (IsProned() && !PredMovement->bCanSprintDuringProne)
			{
				UnProne();
			}
			if (IsAimingDownSights() && !PredMovement->bCanSprintDuringAimDownSights)
			{
				UnAimDownSights();
			}
		}
	}
}

void APredCharacter::UnSprint(bool bClientSimulation)
{
	if (PredMovement)
	{
		PredMovement->bWantsToSprint = false;
	}
}

void APredCharacter::OnStartSprint()
{
	K2_OnStartSprint();
}

void APredCharacter::OnEndSprint()
{
	K2_OnEndSprint();
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

bool APredCharacter::CanAimDownSights() const
{
	return !bIsAimingDownSights && GetRootComponent() && !GetRootComponent()->IsSimulatingPhysics();
}

void APredCharacter::AimDownSights(bool bClientSimulation)
{
	if (PredMovement && CanAimDownSights())
	{
		PredMovement->bWantsToAimDownSights = true;

		// If we can't sprint during ADS, then allow ADS to cancel sprint
		if (!bClientSimulation && IsSprinting() && !PredMovement->bCanSprintDuringAimDownSights)
		{
			UnSprint();
		}
	}
}

void APredCharacter::UnAimDownSights(bool bClientSimulation)
{
	if (PredMovement)
	{
		PredMovement->bWantsToAimDownSights = false;
	}
}

void APredCharacter::OnStartAimDownSights()
{
	K2_OnStartAimDownSights();
}

void APredCharacter::OnEndAimDownSights()
{
	K2_OnEndAimDownSights();
}

void APredCharacter::RecalculateBaseEyeHeight()
{
	if (bIsProned)
	{
		BaseEyeHeight = PronedEyeHeight;
	}
	else
	{
		Super::RecalculateBaseEyeHeight();
	}
}

void APredCharacter::SetIsProned(bool bNewProned)
{
	if (bIsProned != bNewProned)
	{
		bIsProned = bNewProned;

		if (HasAuthority())
		{
			MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, bIsProned, this);  // Push-model
		}
	}
}

void APredCharacter::OnRep_IsProned()
{
	if (PredMovement)
	{
		if (bIsProned)
		{
			PredMovement->bWantsToProne = true;
			PredMovement->Prone(true);
		}
		else
		{
			PredMovement->bWantsToProne = false;
			PredMovement->UnProne(true);
		}
		PredMovement->bNetworkUpdateReceived = true;
	}
}

bool APredCharacter::CanProne() const
{
	return !bIsProned && GetRootComponent() && !GetRootComponent()->IsSimulatingPhysics();
}

void APredCharacter::Crouch(bool bClientSimulation)
{
	if (PredMovement)
	{
		if (CanCrouch())
		{
			PredMovement->bWantsToCrouch = true;
			
			// If we can't sprint during crouch, then allow crouch to cancel sprint
			if (!bClientSimulation && IsSprinting() && !PredMovement->bCanSprintDuringCrouch)
			{
				UnSprint();
			}
		}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		else
		{
			UE_LOG(LogPredCharacter, Log, TEXT("%s is trying to crouch, but crouching is disabled on this character! (check CharacterMovement NavAgentSettings)"), *GetName());
		}
#endif
	}
}

void APredCharacter::Prone(bool bClientSimulation)
{
	if (PredMovement && CanProne())
	{
		PredMovement->bWantsToProne = true;
		
		// If we can't sprint during prone, then allow prone to cancel sprint
		if (!bClientSimulation && IsSprinting() && !PredMovement->bCanSprintDuringProne)
		{
			UnSprint();
		}
	}
}

void APredCharacter::UnProne(bool bClientSimulation)
{
	if (PredMovement)
	{
		PredMovement->bWantsToProne = false;
	}
}

void APredCharacter::OnStartProne(float HeightAdjust, float ScaledHeightAdjust)
{
	RecalculateBaseEyeHeight();

	const ACharacter* DefaultChar = GetDefault<ACharacter>(GetClass());
	if (GetMesh() && DefaultChar->GetMesh())
	{
		FVector& MeshRelativeLocation = GetMesh()->GetRelativeLocation_DirectMutable();
		MeshRelativeLocation.Z = DefaultChar->GetMesh()->GetRelativeLocation().Z + HeightAdjust;
		BaseTranslationOffset.Z = MeshRelativeLocation.Z;
	}
	else
	{
		BaseTranslationOffset.Z = DefaultChar->GetBaseTranslationOffset().Z + HeightAdjust;
	}

	K2_OnStartProne(HeightAdjust, ScaledHeightAdjust);
}

void APredCharacter::OnEndProne(float HeightAdjust, float ScaledHeightAdjust)
{
	RecalculateBaseEyeHeight();

	if (!bIsCrouched)
	{
		const ACharacter* DefaultChar = GetDefault<ACharacter>(GetClass());
		if (GetMesh() && DefaultChar->GetMesh())
		{
			FVector& MeshRelativeLocation = GetMesh()->GetRelativeLocation_DirectMutable();
			MeshRelativeLocation.Z = DefaultChar->GetMesh()->GetRelativeLocation().Z;
			BaseTranslationOffset.Z = MeshRelativeLocation.Z;
		}
		else
		{
			BaseTranslationOffset.Z = DefaultChar->GetBaseTranslationOffset().Z;
		}
	}
	K2_OnEndProne(HeightAdjust, ScaledHeightAdjust);
}

void APredCharacter::FlushServerMoves()
{
	if (PredMovement)
	{
		PredMovement->FlushServerMoves();
	}
}
