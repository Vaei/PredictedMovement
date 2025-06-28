// Copyright (c) Jared Taylor


#include "PredictedCharacter.h"

#include "Net/UnrealNetwork.h"
#include "PredictedCharacterMovement.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Components/SkeletalMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PredCharacter)

DEFINE_LOG_CATEGORY_STATIC(LogPredCharacter, Log, All);

APredictedCharacter::APredictedCharacter(const FObjectInitializer& FObjectInitializer)
	: Super(FObjectInitializer.SetDefaultSubobjectClass<UPredictedCharacterMovement>(CharacterMovementComponentName))
{
	PredMovement = Cast<UPredictedCharacterMovement>(GetCharacterMovement());

	PronedEyeHeight = 16.f;
}

void APredictedCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Push Model
	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;
	SharedParams.Condition = COND_SimulatedOnly;

	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, bIsAimingDownSights, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, bIsProned, SharedParams);

	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, bIsStrolling, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, bIsWalking, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, bIsSprinting, SharedParams);
}

void APredictedCharacter::SetGaitMode(EPredGaitMode NewGaitMode)
{ 
	if (PredMovement)
	{
		switch (NewGaitMode)
		{
		case EPredGaitMode::Stroll:
			Stroll();
			break;
		case EPredGaitMode::Walk:
			Walk();
			break;
		case EPredGaitMode::Run:
			UnStroll();
			UnWalk();
			UnSprint();
			break;
		case EPredGaitMode::Sprint:
			Sprint();
			break;
		}
	}
}

EPredGaitMode APredictedCharacter::GetGaitMode() const
{
	return PredMovement ? PredMovement->GetGaitMode() : EPredGaitMode::Run;
}

EPredGaitMode APredictedCharacter::GetGaitSpeed() const
{
	return PredMovement ? PredMovement->GetGaitSpeed() : EPredGaitMode::Run;
}

FString APredictedCharacter::GetGaitModeString(EPredGaitMode GaitMode)
{
	switch (GaitMode)
	{
	case EPredGaitMode::Stroll: return TEXT("Stroll");
	case EPredGaitMode::Walk: return TEXT("Walk");
	case EPredGaitMode::Run: return TEXT("Run");
	case EPredGaitMode::Sprint: return TEXT("Sprint");
	default: return TEXT("None");
	}
}

EPredStance APredictedCharacter::GetStance() const
{
	if (bIsProned)
	{
		return EPredStance::Prone;
	}
	else if (bIsCrouched)
	{
		return EPredStance::Crouch;
	}
	return EPredStance::Stand;
}

/* STROLLING */

void APredictedCharacter::SetIsStrolling(bool bNewStrolling)
{
	if (bIsStrolling != bNewStrolling)
	{
		bIsStrolling = bNewStrolling;

		if (HasAuthority())
		{
			MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, bIsStrolling, this);  // Push-model
		}
	}
}

void APredictedCharacter::OnRep_IsStrolling()
{
	if (PredMovement)
	{
		if (bIsStrolling)
		{
			PredMovement->bWantsToStroll = true;
			PredMovement->Stroll(true);
		}
		else
		{
			PredMovement->bWantsToStroll = false;
			PredMovement->UnStroll(true);
		}
		PredMovement->bNetworkUpdateReceived = true;
	}
}

bool APredictedCharacter::CanStroll() const
{
	return !bIsStrolling && GetRootComponent() && !GetRootComponent()->IsSimulatingPhysics();
}

void APredictedCharacter::Stroll(bool bClientSimulation)
{
	if (PredMovement && CanStroll())
	{
		PredMovement->bWantsToStroll = true;
		
		if (!bClientSimulation)
		{
			if (bIsSprinting)
			{
				UnSprint();
			}
			if (bIsWalking)
			{
				UnWalk();
			}
		}
	}
}

void APredictedCharacter::UnStroll(bool bClientSimulation)
{
	if (PredMovement)
	{
		PredMovement->bWantsToStroll = false;
	}
}

void APredictedCharacter::OnStartStroll()
{
	K2_OnStartStroll();
}

void APredictedCharacter::OnEndStroll()
{
	K2_OnEndStroll();
}

/* WALKING */

void APredictedCharacter::SetIsWalking(bool bNewWalking)
{
	if (bIsWalking != bNewWalking)
	{
		bIsWalking = bNewWalking;

		if (HasAuthority())
		{
			MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, bIsWalking, this);  // Push-model
		}
	}
}

void APredictedCharacter::OnRep_IsWalking()
{
	if (PredMovement)
	{
		if (bIsWalking)
		{
			PredMovement->bWantsToWalk = true;
			PredMovement->Walk(true);
		}
		else
		{
			PredMovement->bWantsToWalk = false;
			PredMovement->UnWalk(true);
		}
		PredMovement->bNetworkUpdateReceived = true;
	}
}

bool APredictedCharacter::CanWalk() const
{
	return !bIsWalking && GetRootComponent() && !GetRootComponent()->IsSimulatingPhysics();
}

void APredictedCharacter::Walk(bool bClientSimulation)
{
	if (PredMovement && CanWalk())
	{
		PredMovement->bWantsToWalk = true;
		
		if (!bClientSimulation)
		{
			if (bIsStrolling)
			{
				UnStroll();
			}
			if (bIsSprinting)
			{
				UnSprint();
			}
		}
	}
}

void APredictedCharacter::UnWalk(bool bClientSimulation)
{
	if (PredMovement)
	{
		PredMovement->bWantsToWalk = false;
	}
}

void APredictedCharacter::OnStartWalk()
{
	K2_OnStartWalk();
}

void APredictedCharacter::OnEndWalk()
{
	K2_OnEndWalk();
}

/* SPRINTING */

void APredictedCharacter::SetIsSprinting(bool bNewSprinting)
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

bool APredictedCharacter::IsSprintingAtSpeed() const
{
	return PredMovement ? PredMovement->IsSprintingAtSpeed() : false;
}

bool APredictedCharacter::IsSprintWithinAllowableInputAngle() const
{
	return PredMovement && PredMovement->IsSprintWithinAllowableInputAngle();
}

bool APredictedCharacter::IsSprintingInEffect() const
{
	return IsSprintingAtSpeed() && IsSprintWithinAllowableInputAngle();
}

void APredictedCharacter::OnRep_IsSprinting()
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

bool APredictedCharacter::CanSprint() const
{
	return !bIsSprinting && GetRootComponent() && !GetRootComponent()->IsSimulatingPhysics();
}

void APredictedCharacter::Sprint(bool bClientSimulation)
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
			if (IsStrolling())
			{
				UnStroll();
			}
			if (IsWalking())
			{
				UnWalk();
			}
		}
	}
}

void APredictedCharacter::UnSprint(bool bClientSimulation)
{
	if (PredMovement)
	{
		PredMovement->bWantsToSprint = false;
	}
}

void APredictedCharacter::OnStartSprint()
{
	K2_OnStartSprint();
}

void APredictedCharacter::OnEndSprint()
{
	K2_OnEndSprint();
}

/* STAMINA */

void APredictedCharacter::OnStaminaChanged(float Stamina, float PrevStamina)
{
	K2_OnStaminaChanged(Stamina, PrevStamina);
	NotifyOnStaminaChanged.Broadcast(Stamina, PrevStamina);
}

void APredictedCharacter::OnMaxStaminaChanged(float MaxStamina, float PrevMaxStamina)
{
	K2_OnMaxStaminaChanged(MaxStamina, PrevMaxStamina);
	NotifyOnMaxStaminaChanged.Broadcast(MaxStamina, PrevMaxStamina);
}

void APredictedCharacter::OnStaminaDrained()
{
	K2_OnStaminaDrained();
	NotifyOnStaminaDrained.Broadcast();
}

void APredictedCharacter::OnStaminaDrainRecovered()
{
	K2_OnStaminaDrainRecovered();
	NotifyOnStaminaDrainRecovered.Broadcast();
}

float APredictedCharacter::GetStamina() const
{
	return PredMovement ? PredMovement->GetStamina() : 0.f;
}

float APredictedCharacter::GetMaxStamina() const
{
	return PredMovement ? PredMovement->GetMaxStamina() : 0.f;
}

float APredictedCharacter::GetStaminaPct() const
{
	return PredMovement ? PredMovement->GetStaminaPct() : 0.f;
}

bool APredictedCharacter::IsStaminaDrained() const
{
	return PredMovement ? PredMovement->IsStaminaDrained() : false;
}

void APredictedCharacter::SetIsAimingDownSights(bool bNewAimingDownSights)
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

void APredictedCharacter::OnRep_IsAimingDownSights()
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

bool APredictedCharacter::CanAimDownSights() const
{
	return !bIsAimingDownSights && GetRootComponent() && !GetRootComponent()->IsSimulatingPhysics();
}

void APredictedCharacter::AimDownSights(bool bClientSimulation)
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

void APredictedCharacter::UnAimDownSights(bool bClientSimulation)
{
	if (PredMovement)
	{
		PredMovement->bWantsToAimDownSights = false;
	}
}

void APredictedCharacter::OnStartAimDownSights()
{
	K2_OnStartAimDownSights();
}

void APredictedCharacter::OnEndAimDownSights()
{
	K2_OnEndAimDownSights();
}

void APredictedCharacter::RecalculateBaseEyeHeight()
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

void APredictedCharacter::SetIsProned(bool bNewProned)
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

void APredictedCharacter::OnRep_IsProned()
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

bool APredictedCharacter::CanProne() const
{
	return !bIsProned && GetRootComponent() && !GetRootComponent()->IsSimulatingPhysics();
}

void APredictedCharacter::Crouch(bool bClientSimulation)
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
		else if (!PredMovement->CanEverCrouch())
		{
			UE_LOG(LogPredCharacter, Log, TEXT("%s is trying to crouch, but crouching is disabled on this character! (check CharacterMovement NavAgentSettings)"), *GetName());
		}
#endif
	}
}

void APredictedCharacter::Prone(bool bClientSimulation)
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

void APredictedCharacter::UnProne(bool bClientSimulation)
{
	if (PredMovement)
	{
		PredMovement->bWantsToProne = false;
	}
}

void APredictedCharacter::OnStartProne(float HeightAdjust, float ScaledHeightAdjust)
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

void APredictedCharacter::OnEndProne(float HeightAdjust, float ScaledHeightAdjust)
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

void APredictedCharacter::FlushServerMoves()
{
	if (PredMovement)
	{
		PredMovement->FlushServerMoves();
	}
}
