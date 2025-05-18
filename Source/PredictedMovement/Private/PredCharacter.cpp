// Copyright (c) 2023 Jared Taylor


#include "PredCharacter.h"

#include "GameplayTagContainer.h"
#include "Net/UnrealNetwork.h"
#include "PredMovement.h"
#include "PredTags.h"
#include "ModifierTypes.h"
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
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, bIsProned, SharedParams);

	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, bIsStrolling, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, bIsWalking, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, bIsSprinting, SharedParams);

	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, SimulatedBoost, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, SimulatedHaste, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, SimulatedSlow, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, SimulatedSlowFall, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, SimulatedSnare, SharedParams);
}

void APredCharacter::SetGaitMode(EPredGaitMode NewGaitMode)
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

EPredGaitMode APredCharacter::GetGaitMode() const
{
	return PredMovement ? PredMovement->GetGaitMode() : EPredGaitMode::Run;
}

EPredGaitMode APredCharacter::GetGaitSpeed() const
{
	return PredMovement ? PredMovement->GetGaitSpeed() : EPredGaitMode::Run;
}

FString APredCharacter::GetGaitModeString(EPredGaitMode GaitMode)
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

EPredStance APredCharacter::GetStance() const
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

void APredCharacter::SetIsStrolling(bool bNewStrolling)
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

void APredCharacter::OnRep_IsStrolling()
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

bool APredCharacter::CanStroll() const
{
	return !bIsStrolling && GetRootComponent() && !GetRootComponent()->IsSimulatingPhysics();
}

void APredCharacter::Stroll(bool bClientSimulation)
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

void APredCharacter::UnStroll(bool bClientSimulation)
{
	if (PredMovement)
	{
		PredMovement->bWantsToStroll = false;
	}
}

void APredCharacter::OnStartStroll()
{
	K2_OnStartStroll();
}

void APredCharacter::OnEndStroll()
{
	K2_OnEndStroll();
}

/* WALKING */

void APredCharacter::SetIsWalking(bool bNewWalking)
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

void APredCharacter::OnRep_IsWalking()
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

bool APredCharacter::CanWalk() const
{
	return !bIsWalking && GetRootComponent() && !GetRootComponent()->IsSimulatingPhysics();
}

void APredCharacter::Walk(bool bClientSimulation)
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

void APredCharacter::UnWalk(bool bClientSimulation)
{
	if (PredMovement)
	{
		PredMovement->bWantsToWalk = false;
	}
}

void APredCharacter::OnStartWalk()
{
	K2_OnStartWalk();
}

void APredCharacter::OnEndWalk()
{
	K2_OnEndWalk();
}

/* SPRINTING */

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

/* STAMINA */

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
		else if (!PredMovement->CanEverCrouch())
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


void APredCharacter::OnModifierChanged(const FGameplayTag& ModifierType, uint8 ModifierLevel, uint8 PrevModifierLevel)
{
	// Events for when a modifier is added, removed or changed
	if (ModifierLevel > 0 && PrevModifierLevel == 0)
	{
		OnModifierAdded(ModifierType, ModifierLevel, PrevModifierLevel);
	}
	else if (ModifierLevel == 0 && PrevModifierLevel > 0)
	{
		OnModifierRemoved(ModifierType, ModifierLevel, PrevModifierLevel);
	}

	K2_OnModifierChanged(ModifierType, ModifierLevel, PrevModifierLevel);

	// Replicate to simulated proxies
	if (HasAuthority())
	{
		if (ModifierType == FPredTags::Modifier_Type_Boost)
		{
			SimulatedBoost = ModifierLevel;
			MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, SimulatedBoost, this);			// Push-model
		}
		else if (ModifierType == FPredTags::Modifier_Type_SlowFall)
		{
			SimulatedSlowFall = ModifierLevel;
			MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, SimulatedSlowFall, this);		// Push-model
		}
		else if (ModifierType == FPredTags::Modifier_Type_Snare)
		{
			SimulatedSnare = ModifierLevel;
			MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, SimulatedSnare, this);			// Push-model
		}
	}
}

void APredCharacter::OnModifierAdded(const FGameplayTag& ModifierType, uint8 ModifierLevel, uint8 PrevModifierLevel)
{
	if (!PredMovement)
	{
		return;
	}

	if (ModifierType == FPredTags::Modifier_Type_Boost)
	{
		PredMovement->OnStartBoost();
	}
	else if (ModifierType == FPredTags::Modifier_Type_SlowFall)
	{
		PredMovement->OnStartSlowFall();
	}
	else if (ModifierType == FPredTags::Modifier_Type_Snare)
	{
		PredMovement->OnStartSnare();
	}

	K2_OnModifierAdded(ModifierType, ModifierLevel, PrevModifierLevel);
}

void APredCharacter::OnModifierRemoved(const FGameplayTag& ModifierType, uint8 ModifierLevel, uint8 PrevModifierLevel)
{
	// @TIP: Set Loose Gameplay Tag Here (Not Replicated)
	// DO NOT ADD or REMOVE HERE! Net corrections can violate this!

	if (ModifierType == FPredTags::Modifier_Type_Boost)
	{
		PredMovement->OnEndBoost();
	}
	else if (ModifierType == FPredTags::Modifier_Type_SlowFall)
	{
		PredMovement->OnEndSlowFall();
	}
	else if (ModifierType == FPredTags::Modifier_Type_Snare)
	{
		PredMovement->OnEndSnare();
	}

	K2_OnModifierRemoved(ModifierType, ModifierLevel, PrevModifierLevel);
}

/* Boost (Non-Generic) Implementation */

void APredCharacter::OnRep_SimulatedBoost(uint8 PrevSimulatedBoost)
{
	if (PredMovement)
	{
		PredMovement->Boost.ModifierLevel = SimulatedBoost;
		PredMovement->Boost.RequestedModifierLevel = SimulatedBoost;

		if (SimulatedBoost > 0)
		{
			PredMovement->Boost.StartModifier(SimulatedBoost, true, true, PrevSimulatedBoost);
		}
		else
		{
			PredMovement->Boost.RemoveAllModifiers();
			PredMovement->Boost.EndModifier(true, PrevSimulatedBoost);
		}
		PredMovement->bNetworkUpdateReceived = true;
	}
}

void APredCharacter::Boost(FGameplayTag ModifierLevel)
{
	if (GetLocalRole() == ROLE_SimulatedProxy)
	{
		return;
	}
	
	if (PredMovement)
	{
		if (const uint8 Level = PredMovement->Boost.GetModifierLevelByte(ModifierLevel); Level != LEVEL_NONE)
		{
			PredMovement->Boost.AddModifier(Level);
		}
	}
}

void APredCharacter::RemoveBoost(FGameplayTag ModifierLevel)
{
	if (GetLocalRole() == ROLE_SimulatedProxy)
	{
		return;
	}
	
	if (PredMovement)
	{
		if (const uint8 Level = PredMovement->Boost.GetModifierLevelByte(ModifierLevel); Level != LEVEL_NONE)
		{
			PredMovement->Boost.RemoveModifier(Level);
		}
	}
}

void APredCharacter::RemoveAllBoosts()
{
	if (GetLocalRole() == ROLE_SimulatedProxy)
	{
		return;
	}
	
	if (PredMovement)
	{
		PredMovement->Boost.RemoveAllModifiers();
	}
}

void APredCharacter::RemoveAllBoostsOfLevel(FGameplayTag ModifierLevel)
{
	if (GetLocalRole() == ROLE_SimulatedProxy)
	{
		return;
	}
	
	if (PredMovement)
	{
		if (const uint8 Level = PredMovement->Boost.GetModifierLevelByte(ModifierLevel); Level != LEVEL_NONE)
		{
			PredMovement->Boost.RemoveAllModifiersByLevel(Level);
		}
	}
}

bool APredCharacter::IsBoosted() const
{
	return PredMovement && PredMovement->Boost.HasModifier();
}

bool APredCharacter::WantsBoost() const
{
	return PredMovement && PredMovement->Boost.WantsModifier();
}

FGameplayTag APredCharacter::GetBoostLevel() const
{
	return PredMovement ? PredMovement->Boost.GetModifierLevel() : FGameplayTag::EmptyTag;
}

int32 APredCharacter::GetNumBoosts() const
{
	return PredMovement ? PredMovement->Boost.GetNumModifiers() : 0;
}

int32 APredCharacter::GetNumBoostsByLevel(FGameplayTag ModifierLevel) const
{
	if (PredMovement)
	{
		const uint8 Level = PredMovement->Boost.GetModifierLevelByte(ModifierLevel);
		return Level != LEVEL_NONE ? PredMovement->Boost.GetNumModifiersByLevel(Level) : 0;
	}
	return 0;
}

/* ~Boost (Non-Generic) Implementation */

/* Haste (Non-Generic) Implementation */

void APredCharacter::OnRep_SimulatedHaste(uint8 PrevSimulatedHaste)
{
	if (PredMovement)
	{
		PredMovement->Haste.ModifierLevel = SimulatedHaste;
		PredMovement->Haste.RequestedModifierLevel = SimulatedHaste;

		if (SimulatedHaste > 0)
		{
			PredMovement->Haste.StartModifier(SimulatedHaste, true, true, PrevSimulatedHaste);
		}
		else
		{
			PredMovement->Haste.RemoveAllModifiers();
			PredMovement->Haste.EndModifier(true, PrevSimulatedHaste);
		}
		PredMovement->bNetworkUpdateReceived = true;
	}
}

void APredCharacter::Haste(FGameplayTag ModifierLevel)
{
	if (GetLocalRole() == ROLE_SimulatedProxy)
	{
		return;
	}
	
	if (PredMovement)
	{
		if (const uint8 Level = PredMovement->Haste.GetModifierLevelByte(ModifierLevel); Level != LEVEL_NONE)
		{
			PredMovement->Haste.AddModifier(Level);
		}
	}
}

void APredCharacter::RemoveHaste(FGameplayTag ModifierLevel)
{
	if (GetLocalRole() == ROLE_SimulatedProxy)
	{
		return;
	}
	
	if (PredMovement)
	{
		if (const uint8 Level = PredMovement->Haste.GetModifierLevelByte(ModifierLevel); Level != LEVEL_NONE)
		{
			PredMovement->Haste.RemoveModifier(Level);
		}
	}
}

void APredCharacter::RemoveAllHastes()
{
	if (GetLocalRole() == ROLE_SimulatedProxy)
	{
		return;
	}
	
	if (PredMovement)
	{
		PredMovement->Haste.RemoveAllModifiers();
	}
}

void APredCharacter::RemoveAllHastesOfLevel(FGameplayTag ModifierLevel)
{
	if (GetLocalRole() == ROLE_SimulatedProxy)
	{
		return;
	}
	
	if (PredMovement)
	{
		if (const uint8 Level = PredMovement->Haste.GetModifierLevelByte(ModifierLevel); Level != LEVEL_NONE)
		{
			PredMovement->Haste.RemoveAllModifiersByLevel(Level);
		}
	}
}

bool APredCharacter::IsHaste() const
{
	return PredMovement && PredMovement->Haste.HasModifier();
}

bool APredCharacter::WantsHaste() const
{
	return PredMovement && PredMovement->Haste.WantsModifier();
}

FGameplayTag APredCharacter::GetHasteLevel() const
{
	return PredMovement ? PredMovement->Haste.GetModifierLevel() : FGameplayTag::EmptyTag;
}

int32 APredCharacter::GetNumHastes() const
{
	return PredMovement ? PredMovement->Haste.GetNumModifiers() : 0;
}

int32 APredCharacter::GetNumHastesByLevel(FGameplayTag ModifierLevel) const
{
	if (PredMovement)
	{
		const uint8 Level = PredMovement->Haste.GetModifierLevelByte(ModifierLevel);
		return Level != LEVEL_NONE ? PredMovement->Haste.GetNumModifiersByLevel(Level) : 0;
	}
	return 0;
}

/* ~Haste (Non-Generic) Implementation */

/* Slow (Non-Generic) Implementation */

void APredCharacter::OnRep_SimulatedSlow(uint8 PrevSimulatedSlow)
{
	if (PredMovement)
	{
		PredMovement->Slow.ModifierLevel = SimulatedSlow;
		PredMovement->Slow.RequestedModifierLevel = SimulatedSlow;

		if (SimulatedSlow > 0)
		{
			PredMovement->Slow.StartModifier(SimulatedSlow, true, true, PrevSimulatedSlow);
		}
		else
		{
			PredMovement->Slow.RemoveAllModifiers();
			PredMovement->Slow.EndModifier(true, PrevSimulatedSlow);
		}
		PredMovement->bNetworkUpdateReceived = true;
	}
}

void APredCharacter::Slow(FGameplayTag ModifierLevel)
{
	if (GetLocalRole() == ROLE_SimulatedProxy)
	{
		return;
	}
	
	if (PredMovement)
	{
		if (const uint8 Level = PredMovement->Slow.GetModifierLevelByte(ModifierLevel); Level != LEVEL_NONE)
		{
			PredMovement->Slow.AddModifier(Level);
		}
	}
}

void APredCharacter::RemoveSlow(FGameplayTag ModifierLevel)
{
	if (GetLocalRole() == ROLE_SimulatedProxy)
	{
		return;
	}
	
	if (PredMovement)
	{
		if (const uint8 Level = PredMovement->Slow.GetModifierLevelByte(ModifierLevel); Level != LEVEL_NONE)
		{
			PredMovement->Slow.RemoveModifier(Level);
		}
	}
}

void APredCharacter::RemoveAllSlows()
{
	if (GetLocalRole() == ROLE_SimulatedProxy)
	{
		return;
	}
	
	if (PredMovement)
	{
		PredMovement->Slow.RemoveAllModifiers();
	}
}

void APredCharacter::RemoveAllSlowsOfLevel(FGameplayTag ModifierLevel)
{
	if (GetLocalRole() == ROLE_SimulatedProxy)
	{
		return;
	}
	
	if (PredMovement)
	{
		if (const uint8 Level = PredMovement->Slow.GetModifierLevelByte(ModifierLevel); Level != LEVEL_NONE)
		{
			PredMovement->Slow.RemoveAllModifiersByLevel(Level);
		}
	}
}

bool APredCharacter::IsSlowed() const
{
	return PredMovement && PredMovement->Slow.HasModifier();
}

bool APredCharacter::WantsSlow() const
{
	return PredMovement && PredMovement->Slow.WantsModifier();
}

FGameplayTag APredCharacter::GetSlowLevel() const
{
	return PredMovement ? PredMovement->Slow.GetModifierLevel() : FGameplayTag::EmptyTag;
}

int32 APredCharacter::GetNumSlows() const
{
	return PredMovement ? PredMovement->Slow.GetNumModifiers() : 0;
}

int32 APredCharacter::GetNumSlowsByLevel(FGameplayTag ModifierLevel) const
{
	if (PredMovement)
	{
		const uint8 Level = PredMovement->Slow.GetModifierLevelByte(ModifierLevel);
		return Level != LEVEL_NONE ? PredMovement->Slow.GetNumModifiersByLevel(Level) : 0;
	}
	return 0;
}

/* ~Slow (Non-Generic) Implementation */

/* SlowFall (Non-Generic) Implementation */

void APredCharacter::OnRep_SimulatedSlowFall(uint8 PrevSimulatedSlowFall)
{
	if (PredMovement)
	{
		PredMovement->SlowFall.ModifierLevel = SimulatedSlowFall;
		PredMovement->SlowFall.RequestedModifierLevel = SimulatedSlowFall;

		if (SimulatedSlowFall > 0)
		{
			PredMovement->SlowFall.StartModifier(SimulatedSlowFall, true, true, PrevSimulatedSlowFall);
		}
		else
		{
			PredMovement->SlowFall.RemoveAllModifiers();
			PredMovement->SlowFall.EndModifier(true, PrevSimulatedSlowFall);
		}
		PredMovement->bNetworkUpdateReceived = true;
	}
}

void APredCharacter::SlowFall(FGameplayTag ModifierLevel)
{
	if (GetLocalRole() == ROLE_SimulatedProxy)
	{
		return;
	}
	
	if (PredMovement)
	{
		if (const uint8 Level = PredMovement->SlowFall.GetModifierLevelByte(ModifierLevel); Level != LEVEL_NONE)
		{
			PredMovement->SlowFall.AddModifier(Level);
		}
	}
}

void APredCharacter::RemoveSlowFall(FGameplayTag ModifierLevel)
{
	if (GetLocalRole() == ROLE_SimulatedProxy)
	{
		return;
	}
	
	if (PredMovement)
	{
		if (const uint8 Level = PredMovement->SlowFall.GetModifierLevelByte(ModifierLevel); Level != LEVEL_NONE)
		{
			PredMovement->SlowFall.RemoveModifier(Level);
		}
	}
}

void APredCharacter::RemoveAllSlowFall()
{
	if (GetLocalRole() == ROLE_SimulatedProxy)
	{
		return;
	}
	
	if (PredMovement)
	{
		PredMovement->SlowFall.RemoveAllModifiers();
	}
}

void APredCharacter::RemoveAllSlowFallOfLevel(FGameplayTag ModifierLevel)
{
	if (GetLocalRole() == ROLE_SimulatedProxy)
	{
		return;
	}
	
	if (PredMovement)
	{
		if (const uint8 Level = PredMovement->SlowFall.GetModifierLevelByte(ModifierLevel); Level != LEVEL_NONE)
		{
			PredMovement->SlowFall.RemoveAllModifiersByLevel(Level);
		}
	}
}

bool APredCharacter::IsSlowFall() const
{
	return PredMovement && PredMovement->SlowFall.HasModifier();
}

bool APredCharacter::WantsSlowFall() const
{
	return PredMovement && PredMovement->SlowFall.WantsModifier();
}

FGameplayTag APredCharacter::GetSlowFallLevel() const
{
	return PredMovement ? PredMovement->SlowFall.GetModifierLevel() : FGameplayTag::EmptyTag;
}

int32 APredCharacter::GetNumSlowFalls() const
{
	return PredMovement ? PredMovement->SlowFall.GetNumModifiers() : 0;
}

int32 APredCharacter::GetNumSlowFallsByLevel(FGameplayTag ModifierLevel) const
{
	if (PredMovement)
	{
		const uint8 Level = PredMovement->SlowFall.GetModifierLevelByte(ModifierLevel);
		return Level != LEVEL_NONE ? PredMovement->SlowFall.GetNumModifiersByLevel(Level) : 0;
	}
	return 0;
}

/* ~SlowFall (Non-Generic) Implementation */

/* Snare (Non-Generic) Implementation */

void APredCharacter::OnRep_SimulatedSnare(uint8 PrevSimulatedSnare)
{
	if (PredMovement)
	{
		PredMovement->Snare.ModifierLevel = SimulatedSnare;
		PredMovement->Snare.RequestedModifierLevel = SimulatedSnare;

		if (SimulatedSnare > 0)
		{
			PredMovement->Snare.StartModifier(SimulatedSnare, true, true, PrevSimulatedSnare);
		}
		else
		{
			PredMovement->Snare.RemoveAllModifiers();
			PredMovement->Snare.EndModifier(true, PrevSimulatedSnare);
		}
		PredMovement->bNetworkUpdateReceived = true;
	}
}

void APredCharacter::Snare(FGameplayTag ModifierLevel)
{
	if (GetLocalRole() == ROLE_SimulatedProxy)
	{
		return;
	}
	
	if (PredMovement)
	{
		if (const uint8 Level = PredMovement->Snare.GetModifierLevelByte(ModifierLevel); Level != LEVEL_NONE)
		{
			PredMovement->Snare.AddModifier(Level);
		}
	}
}

void APredCharacter::RemoveSnare(FGameplayTag ModifierLevel)
{
	if (GetLocalRole() == ROLE_SimulatedProxy)
	{
		return;
	}
	
	if (PredMovement)
	{
		if (const uint8 Level = PredMovement->Snare.GetModifierLevelByte(ModifierLevel); Level != LEVEL_NONE)
		{
			PredMovement->Snare.RemoveModifier(Level);
		}
	}
}

void APredCharacter::RemoveAllSnares()
{
	if (GetLocalRole() == ROLE_SimulatedProxy)
	{
		return;
	}
	
	if (PredMovement)
	{
		PredMovement->Snare.RemoveAllModifiers();
	}
}

void APredCharacter::RemoveAllSnaresOfLevel(FGameplayTag ModifierLevel)
{
	if (GetLocalRole() == ROLE_SimulatedProxy)
	{
		return;
	}
	
	if (PredMovement)
	{
		if (const uint8 Level = PredMovement->Snare.GetModifierLevelByte(ModifierLevel); Level != LEVEL_NONE)
		{
			PredMovement->Snare.RemoveAllModifiersByLevel(Level);
		}
	}
}

bool APredCharacter::IsSnared() const
{
	return PredMovement && PredMovement->Snare.HasModifier();
}

bool APredCharacter::WantsSnare() const
{
	return PredMovement && PredMovement->Snare.WantsModifier();
}

FGameplayTag APredCharacter::GetSnareLevel() const
{
	return PredMovement ? PredMovement->Snare.GetModifierLevel() : FGameplayTag::EmptyTag;
}

int32 APredCharacter::GetNumSnares() const
{
	return PredMovement ? PredMovement->Snare.GetNumModifiers() : 0;
}

int32 APredCharacter::GetNumSnaresByLevel(FGameplayTag ModifierLevel) const
{
	if (PredMovement)
	{
		const uint8 Level = PredMovement->Snare.GetModifierLevelByte(ModifierLevel);
		return Level != LEVEL_NONE ? PredMovement->Snare.GetNumModifiersByLevel(Level) : 0;
	}
	return 0;
}

/* ~Snare (Non-Generic) Implementation */

void APredCharacter::FlushServerMoves()
{
	if (PredMovement)
	{
		PredMovement->FlushServerMoves();
	}
}
