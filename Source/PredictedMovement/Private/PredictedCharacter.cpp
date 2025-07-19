// Copyright (c) Jared Taylor


#include "PredictedCharacter.h"

#include "Net/UnrealNetwork.h"
#include "PredictedCharacterMovement.h"
#include "Net/Core/PushModel/PushModel.h"
#include "Components/SkeletalMeshComponent.h"
#include "Modifier/ModifierTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PredictedCharacter)

DEFINE_LOG_CATEGORY_STATIC(LogPredCharacter, Log, All);

APredictedCharacter::APredictedCharacter(const FObjectInitializer& FObjectInitializer)
	: Super(FObjectInitializer.SetDefaultSubobjectClass<UPredictedCharacterMovement>(CharacterMovementComponentName))
{
	PredictedMovement = Cast<UPredictedCharacterMovement>(GetCharacterMovement());

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

	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, SimulatedBoost, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, SimulatedHaste, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, SimulatedSlow, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, SimulatedSnare, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, SimulatedSlowFall, SharedParams);
}

void APredictedCharacter::OnModifierChanged(const FGameplayTag& ModifierType, const FGameplayTag& ModifierLevel,
	const FGameplayTag& PrevModifierLevel)
{
	K2_OnModifierChanged(ModifierType, ModifierLevel, PrevModifierLevel);

	// Replicate to simulated proxies
	if (PredictedMovement && HasAuthority())
	{
		if (ModifierType == FModifierTags::Modifier_Boost)
		{
			SimulatedBoost = PredictedMovement->GetBoostLevelIndex(ModifierLevel);
			MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, SimulatedBoost, this);
		}
		else if (ModifierType == FModifierTags::Modifier_Haste)
		{
			SimulatedHaste = PredictedMovement->GetHasteLevelIndex(ModifierLevel);
			MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, SimulatedHaste, this);
		}
		else if (ModifierType == FModifierTags::Modifier_Slow)
		{
			SimulatedSlow = PredictedMovement->GetSlowLevelIndex(ModifierLevel);
			MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, SimulatedSlow, this);
		}
		else if (ModifierType == FModifierTags::Modifier_Snare)
		{
			SimulatedSnare = PredictedMovement->GetSnareLevelIndex(ModifierLevel);
			MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, SimulatedSnare, this);
		}
		else if (ModifierType == FModifierTags::Modifier_SlowFall)
		{
			SimulatedSlowFall = PredictedMovement->GetSlowFallLevelIndex(ModifierLevel);
			MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, SimulatedSlowFall, this);
		}
	}
}

void APredictedCharacter::OnModifierAdded(const FGameplayTag& ModifierType, const FGameplayTag& ModifierLevel,
	const FGameplayTag& PrevModifierLevel)
{
	K2_OnModifierAdded(ModifierType, ModifierLevel, PrevModifierLevel);
}

void APredictedCharacter::OnModifierRemoved(const FGameplayTag& ModifierType, const FGameplayTag& ModifierLevel,
	const FGameplayTag& PrevModifierLevel)
{
	K2_OnModifierRemoved(ModifierType, ModifierLevel, PrevModifierLevel);
}

void APredictedCharacter::GrantClientAuthority(FGameplayTag ClientAuthSource, float OverrideDuration)
{
	if (PredictedMovement)
	{
		PredictedMovement->GrantClientAuthority(ClientAuthSource, OverrideDuration);
	}
}

void APredictedCharacter::FlushServerMoves()
{
	if (PredictedMovement)
	{
		PredictedMovement->FlushServerMoves();
	}
}
void APredictedCharacter::SetGaitMode(EPredGaitMode NewGaitMode)
{ 
	if (PredictedMovement)
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
			Run();
			break;
		case EPredGaitMode::Sprint:
			Sprint();
			break;
		}
	}
}

EPredGaitMode APredictedCharacter::GetGaitMode() const
{
	return PredictedMovement ? PredictedMovement->GetGaitMode() : EPredGaitMode::Run;
}

EPredGaitMode APredictedCharacter::GetGaitSpeed() const
{
	return PredictedMovement ? PredictedMovement->GetGaitSpeed() : EPredGaitMode::Run;
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
	if (PredictedMovement)
	{
		if (bIsStrolling)
		{
			PredictedMovement->bWantsToStroll = true;
			PredictedMovement->Stroll(true);
		}
		else
		{
			PredictedMovement->bWantsToStroll = false;
			PredictedMovement->UnStroll(true);
		}
		PredictedMovement->bNetworkUpdateReceived = true;
	}
}

bool APredictedCharacter::CanStroll() const
{
	return !bIsStrolling && GetRootComponent() && !GetRootComponent()->IsSimulatingPhysics();
}

void APredictedCharacter::Stroll(bool bClientSimulation)
{
	if (PredictedMovement && CanStroll())
	{
		PredictedMovement->bWantsToStroll = true;
		
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
	if (PredictedMovement)
	{
		PredictedMovement->bWantsToStroll = false;
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
	if (PredictedMovement)
	{
		if (bIsWalking)
		{
			PredictedMovement->bWantsToWalk = true;
			PredictedMovement->Walk(true);
		}
		else
		{
			PredictedMovement->bWantsToWalk = false;
			PredictedMovement->UnWalk(true);
		}
		PredictedMovement->bNetworkUpdateReceived = true;
	}
}

bool APredictedCharacter::CanWalk() const
{
	return !bIsWalking && GetRootComponent() && !GetRootComponent()->IsSimulatingPhysics();
}

void APredictedCharacter::Walk(bool bClientSimulation)
{
	if (PredictedMovement && CanWalk())
	{
		PredictedMovement->bWantsToWalk = true;
		
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
	if (PredictedMovement)
	{
		PredictedMovement->bWantsToWalk = false;
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

void APredictedCharacter::Run(bool bClientSimulation)
{
	UnStroll();
	UnWalk();
	UnSprint();
}

bool APredictedCharacter::IsRunningAtSpeed() const
{
	return PredictedMovement ? PredictedMovement->IsRunningAtSpeed() : false;
}

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
	return PredictedMovement ? PredictedMovement->IsSprintingAtSpeed() : false;
}

bool APredictedCharacter::IsSprintWithinAllowableInputAngle() const
{
	return PredictedMovement && PredictedMovement->IsSprintWithinAllowableInputAngle();
}

bool APredictedCharacter::IsSprintingInEffect() const
{
	return IsSprintingAtSpeed() && IsSprintWithinAllowableInputAngle();
}

void APredictedCharacter::OnRep_IsSprinting()
{
	if (PredictedMovement)
	{
		if (bIsSprinting)
		{
			PredictedMovement->bWantsToSprint = true;
			PredictedMovement->Sprint(true);
		}
		else
		{
			PredictedMovement->bWantsToSprint = false;
			PredictedMovement->UnSprint(true);
		}
		PredictedMovement->bNetworkUpdateReceived = true;
	}
}

bool APredictedCharacter::CanSprint() const
{
	return !bIsSprinting && GetRootComponent() && !GetRootComponent()->IsSimulatingPhysics();
}

void APredictedCharacter::Sprint(bool bClientSimulation)
{
	if (PredictedMovement && CanSprint())
	{
		PredictedMovement->bWantsToSprint = true;
		
		if (!bClientSimulation)
		{
			// If we can't sprint during certain states, then allow sprint to cancel those states
			if (bIsCrouched && !PredictedMovement->bCanSprintDuringCrouch)
			{
				UnCrouch();
			}
			if (IsProned() && !PredictedMovement->bCanSprintDuringProne)
			{
				UnProne();
			}
			if (IsAimingDownSights() && !PredictedMovement->bCanSprintDuringAimDownSights)
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
	if (PredictedMovement)
	{
		PredictedMovement->bWantsToSprint = false;
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
	return PredictedMovement ? PredictedMovement->GetStamina() : 0.f;
}

float APredictedCharacter::GetMaxStamina() const
{
	return PredictedMovement ? PredictedMovement->GetMaxStamina() : 0.f;
}

float APredictedCharacter::GetStaminaPct() const
{
	return PredictedMovement ? PredictedMovement->GetStaminaPct() : 0.f;
}

bool APredictedCharacter::IsStaminaDrained() const
{
	return PredictedMovement ? PredictedMovement->IsStaminaDrained() : false;
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
	if (PredictedMovement)
	{
		if (bIsAimingDownSights)
		{
			PredictedMovement->bWantsToAimDownSights = true;
			PredictedMovement->AimDownSights(true);
		}
		else
		{
			PredictedMovement->bWantsToAimDownSights = false;
			PredictedMovement->UnAimDownSights(true);
		}
		PredictedMovement->bNetworkUpdateReceived = true;
	}
}

bool APredictedCharacter::CanAimDownSights() const
{
	return !bIsAimingDownSights && GetRootComponent() && !GetRootComponent()->IsSimulatingPhysics();
}

void APredictedCharacter::AimDownSights(bool bClientSimulation)
{
	if (PredictedMovement && CanAimDownSights())
	{
		PredictedMovement->bWantsToAimDownSights = true;

		// If we can't sprint during ADS, then allow ADS to cancel sprint
		if (!bClientSimulation && IsSprinting() && !PredictedMovement->bCanSprintDuringAimDownSights)
		{
			UnSprint();
		}
	}
}

void APredictedCharacter::UnAimDownSights(bool bClientSimulation)
{
	if (PredictedMovement)
	{
		PredictedMovement->bWantsToAimDownSights = false;
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

float APredictedCharacter::GetStandingBaseEyeHeight() const
{
	return GetDefault<ACharacter>(GetClass())->BaseEyeHeight;
}

float APredictedCharacter::GetBaseEyeHeight() const
{
	switch (GetStance())
	{
	case EPredStance::Crouch: return CrouchedEyeHeight;
	case EPredStance::Prone: return PronedEyeHeight;
	default: return GetStandingBaseEyeHeight();
	}
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
	if (PredictedMovement)
	{
		if (bIsProned)
		{
			PredictedMovement->bWantsToProne = true;
			PredictedMovement->Prone(true);
		}
		else
		{
			PredictedMovement->bWantsToProne = false;
			PredictedMovement->UnProne(true);
		}
		PredictedMovement->bNetworkUpdateReceived = true;
	}
}

bool APredictedCharacter::CanProne() const
{
	return !bIsProned && GetRootComponent() && !GetRootComponent()->IsSimulatingPhysics();
}

void APredictedCharacter::Crouch(bool bClientSimulation)
{
	if (PredictedMovement)
	{
		if (CanCrouch())
		{
			PredictedMovement->bWantsToCrouch = true;
			
			// If we can't sprint during crouch, then allow crouch to cancel sprint
			if (!bClientSimulation && IsSprinting() && !PredictedMovement->bCanSprintDuringCrouch)
			{
				UnSprint();
			}
		}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		else if (!PredictedMovement->CanEverCrouch())
		{
			UE_LOG(LogPredCharacter, Log, TEXT("%s is trying to crouch, but crouching is disabled on this character! (check CharacterMovement NavAgentSettings)"), *GetName());
		}
#endif
	}
}

void APredictedCharacter::Prone(bool bClientSimulation)
{
	if (PredictedMovement && CanProne())
	{
		PredictedMovement->bWantsToProne = true;
		
		// If we can't sprint during prone, then allow prone to cancel sprint
		if (!bClientSimulation && IsSprinting() && !PredictedMovement->bCanSprintDuringProne)
		{
			UnSprint();
		}
	}
}

void APredictedCharacter::UnProne(bool bClientSimulation)
{
	if (PredictedMovement)
	{
		PredictedMovement->bWantsToProne = false;
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

/* Boost Implementation */

void APredictedCharacter::OnRep_SimulatedBoost(uint8 PrevLevel)
{
	if (PredictedMovement && SimulatedBoost != PrevLevel)
	{
		const FGameplayTag PrevBoostLevel = PredictedMovement->GetBoostLevel();
		PredictedMovement->BoostLevel = SimulatedBoost;
		NotifyModifierChanged<uint8>(<uint8>FModifierTags::Modifier_Boost, PredictedMovement->GetBoostLevel(),
			PrevBoostLevel, PredictedMovement->BoostLevel, PrevLevel, NO_MODIFIER);

		PredictedMovement->bNetworkUpdateReceived = true;
	}
}

bool APredictedCharacter::Boost(FGameplayTag Level, EModifierNetTypeLocal NetType)
{
	if (PredictedMovement && GetLocalRole() != ROLE_SimulatedProxy && Level.IsValid())
	{
		const uint8 LevelIndex = PredictedMovement->GetBoostLevelIndex(Level);
		if (LevelIndex == NO_MODIFIER)
		{
			return false;
		}
		
		switch (NetType)
		{
		case EModifierNetTypeLocal::LocalPredicted:
			return PredictedMovement->BoostLocal.AddModifier(LevelIndex);
		case EModifierNetTypeLocal::WithCorrection:
			return PredictedMovement->BoostCorrection.AddModifier(LevelIndex);
		default: return false;
		}
	}
	return false;
}

bool APredictedCharacter::UnBoost(FGameplayTag Level, EModifierNetTypeLocal NetType, bool bRemoveAll)
{
	if (PredictedMovement && GetLocalRole() != ROLE_SimulatedProxy && Level.IsValid())
	{
		const uint8 LevelIndex = PredictedMovement->GetBoostLevelIndex(Level);
		if (LevelIndex == NO_MODIFIER)
		{
			return false;
		}
		
		switch (NetType)
		{
		case EModifierNetTypeLocal::LocalPredicted:
			return PredictedMovement->BoostLocal.RemoveModifier(LevelIndex, bRemoveAll);
		case EModifierNetTypeLocal::WithCorrection:
			return PredictedMovement->BoostCorrection.RemoveModifier(LevelIndex, bRemoveAll);
		default: return false;
		}
	}
	return false;
}

bool APredictedCharacter::ResetBoost(EModifierNetTypeLocal NetType)
{
	if (PredictedMovement && GetLocalRole() != ROLE_SimulatedProxy)
	{
		switch (NetType)
		{
		case EModifierNetTypeLocal::LocalPredicted:
			return PredictedMovement->BoostLocal.ResetModifiers();
		case EModifierNetTypeLocal::WithCorrection:
			return PredictedMovement->BoostCorrection.ResetModifiers();
		default: return false;
		}
	}
	return false;
}

FGameplayTag APredictedCharacter::GetBoostLevel() const
{
	return PredictedMovement ? PredictedMovement->GetBoostLevel() : FGameplayTag::EmptyTag;
}

bool APredictedCharacter::IsBoostActive() const
{
	return PredictedMovement && PredictedMovement->IsBoostActive();
}

/* ~Boost Implementation */

/* Haste Implementation */

void APredictedCharacter::OnRep_SimulatedHaste(uint8 PrevLevel)
{
	if (PredictedMovement && SimulatedHaste != PrevLevel)
	{
		const FGameplayTag PrevHasteLevel = PredictedMovement->GetHasteLevel();
		PredictedMovement->HasteLevel = SimulatedHaste;
		NotifyModifierChanged<uint8>(<uint8>FModifierTags::Modifier_Haste, PredictedMovement->GetHasteLevel(),
			PrevHasteLevel, PredictedMovement->HasteLevel, PrevLevel, NO_MODIFIER);

		PredictedMovement->bNetworkUpdateReceived = true;
	}
}

bool APredictedCharacter::Haste(FGameplayTag Level, EModifierNetTypeLocal NetType)
{
	if (PredictedMovement && GetLocalRole() != ROLE_SimulatedProxy && Level.IsValid())
	{
		const uint8 LevelIndex = PredictedMovement->GetHasteLevelIndex(Level);
		if (LevelIndex == NO_MODIFIER)
		{
			return false;
		}
		
		switch (NetType)
		{
		case EModifierNetTypeLocal::LocalPredicted:
			return PredictedMovement->HasteLocal.AddModifier(LevelIndex);
		case EModifierNetTypeLocal::WithCorrection:
			return PredictedMovement->HasteCorrection.AddModifier(LevelIndex);
		default: return false;
		}
	}
	return false;
}

bool APredictedCharacter::UnHaste(FGameplayTag Level, EModifierNetTypeLocal NetType, bool bRemoveAll)
{
	if (PredictedMovement && GetLocalRole() != ROLE_SimulatedProxy && Level.IsValid())
	{
		const uint8 LevelIndex = PredictedMovement->GetHasteLevelIndex(Level);
		if (LevelIndex == NO_MODIFIER)
		{
			return false;
		}
		
		switch (NetType)
		{
		case EModifierNetTypeLocal::LocalPredicted:
			return PredictedMovement->HasteLocal.RemoveModifier(LevelIndex, bRemoveAll);
		case EModifierNetTypeLocal::WithCorrection:
			return PredictedMovement->HasteCorrection.RemoveModifier(LevelIndex, bRemoveAll);
		default: return false;
		}
	}
	return false;
}

bool APredictedCharacter::ResetHaste(EModifierNetTypeLocal NetType)
{
	if (PredictedMovement && GetLocalRole() != ROLE_SimulatedProxy)
	{
		switch (NetType)
		{
		case EModifierNetTypeLocal::LocalPredicted:
			return PredictedMovement->HasteLocal.ResetModifiers();
		case EModifierNetTypeLocal::WithCorrection:
			return PredictedMovement->HasteCorrection.ResetModifiers();
		default: return false;
		}
	}
	return false;
}

FGameplayTag APredictedCharacter::GetHasteLevel() const
{
	return PredictedMovement ? PredictedMovement->GetHasteLevel() : FGameplayTag::EmptyTag;
}

bool APredictedCharacter::IsHasteActive() const
{
	return PredictedMovement && PredictedMovement->IsHasteActive();
}

/* ~Haste Implementation */

/* Slow Implementation */

void APredictedCharacter::OnRep_SimulatedSlow(uint8 PrevLevel)
{
	if (PredictedMovement && SimulatedSlow != PrevLevel)
	{
		const FGameplayTag PrevSlowLevel = PredictedMovement->GetSlowLevel();
		PredictedMovement->SlowLevel = SimulatedSlow;
		NotifyModifierChanged<uint8>(<uint8>FModifierTags::Modifier_Slow, PredictedMovement->GetSlowLevel(),
			PrevSlowLevel, PredictedMovement->SlowLevel, PrevLevel, NO_MODIFIER);

		PredictedMovement->bNetworkUpdateReceived = true;
	}
}

bool APredictedCharacter::Slow(FGameplayTag Level, EModifierNetTypeLocal NetType)
{
	if (PredictedMovement && GetLocalRole() != ROLE_SimulatedProxy && Level.IsValid())
	{
		const uint8 LevelIndex = PredictedMovement->GetSlowLevelIndex(Level);
		if (LevelIndex == NO_MODIFIER)
		{
			return false;
		}
		
		switch (NetType)
		{
		case EModifierNetTypeLocal::LocalPredicted:
			return PredictedMovement->SlowLocal.AddModifier(LevelIndex);
		case EModifierNetTypeLocal::WithCorrection:
			return PredictedMovement->SlowCorrection.AddModifier(LevelIndex);
		default: return false;
		}
	}
	return false;
}

bool APredictedCharacter::UnSlow(FGameplayTag Level, EModifierNetTypeLocal NetType, bool bRemoveAll)
{
	if (PredictedMovement && GetLocalRole() != ROLE_SimulatedProxy && Level.IsValid())
	{
		const uint8 LevelIndex = PredictedMovement->GetSlowLevelIndex(Level);
		if (LevelIndex == NO_MODIFIER)
		{
			return false;
		}
		
		switch (NetType)
		{
		case EModifierNetTypeLocal::LocalPredicted:
			return PredictedMovement->SlowLocal.RemoveModifier(LevelIndex, bRemoveAll);
		case EModifierNetTypeLocal::WithCorrection:
			return PredictedMovement->SlowCorrection.RemoveModifier(LevelIndex, bRemoveAll);
		default: return false;
		}
	}
	return false;
}

bool APredictedCharacter::ResetSlow(EModifierNetTypeLocal NetType)
{
	if (PredictedMovement && GetLocalRole() != ROLE_SimulatedProxy)
	{
		switch (NetType)
		{
		case EModifierNetTypeLocal::LocalPredicted:
			return PredictedMovement->SlowLocal.ResetModifiers();
		case EModifierNetTypeLocal::WithCorrection:
			return PredictedMovement->SlowCorrection.ResetModifiers();
		default: return false;
		}
	}
	return false;
}

FGameplayTag APredictedCharacter::GetSlowLevel() const
{
	return PredictedMovement ? PredictedMovement->GetSlowLevel() : FGameplayTag::EmptyTag;
}

bool APredictedCharacter::IsSlowActive() const
{
	return PredictedMovement && PredictedMovement->IsSlowActive();
}

/* ~Slow Implementation */

/* Snare Implementation */

void APredictedCharacter::OnRep_SimulatedSnare(uint8 PrevLevel)
{
	if (PredictedMovement && SimulatedSnare != PrevLevel)
	{
		const FGameplayTag PrevSnareLevel = PredictedMovement->GetSnareLevel();
		PredictedMovement->SnareLevel = SimulatedSnare;
		NotifyModifierChanged<uint8>(<uint8>FModifierTags::Modifier_Snare, PredictedMovement->GetSnareLevel(),
			PrevSnareLevel, PredictedMovement->SnareLevel, PrevLevel, NO_MODIFIER);

		PredictedMovement->bNetworkUpdateReceived = true;
	}
}

bool APredictedCharacter::Snare(FGameplayTag Level)
{
	if (PredictedMovement && HasAuthority() && Level.IsValid())
	{
		const uint8 LevelIndex = PredictedMovement->GetSnareLevelIndex(Level);
		if (LevelIndex == NO_MODIFIER)
		{
			return false;
		}
		
		return PredictedMovement->SnareServer.AddModifier(LevelIndex);
	}
	return false;
}

bool APredictedCharacter::UnSnare(FGameplayTag Level, bool bRemoveAll)
{
	if (PredictedMovement && HasAuthority() && Level.IsValid())
	{
		const uint8 LevelIndex = PredictedMovement->GetSnareLevelIndex(Level);
		if (LevelIndex == NO_MODIFIER)
		{
			return false;
		}

		return PredictedMovement->SnareServer.RemoveModifier(LevelIndex, bRemoveAll);
	}
	return false;
}

bool APredictedCharacter::ResetSnare()
{
	if (PredictedMovement && HasAuthority())
	{
		return PredictedMovement->SnareServer.ResetModifiers();
	}
	return false;
}

FGameplayTag APredictedCharacter::GetSnareLevel() const
{
	return PredictedMovement ? PredictedMovement->GetSnareLevel() : FGameplayTag::EmptyTag;
}

bool APredictedCharacter::IsSnareActive() const
{
	return PredictedMovement && PredictedMovement->IsSnareActive();
}

/* ~Snare Implementation */

/* SlowFall Implementation */

void APredictedCharacter::OnRep_SimulatedSlowFall(uint8 PrevLevel)
{
	if (PredictedMovement && SimulatedSlowFall != PrevLevel)
	{
		const FGameplayTag PrevSlowFallLevel = PredictedMovement->GetSlowFallLevel();
		PredictedMovement->SlowFallLevel = SimulatedSlowFall;
		NotifyModifierChanged<uint8>(<uint8>FModifierTags::Modifier_SlowFall, PredictedMovement->GetSlowFallLevel(),
			PrevSlowFallLevel, PredictedMovement->SlowFallLevel, PrevLevel, NO_MODIFIER);

		PredictedMovement->bNetworkUpdateReceived = true;
	}
}

bool APredictedCharacter::SlowFall(FGameplayTag Level, EModifierNetTypeLocal NetType)
{
	if (PredictedMovement && GetLocalRole() != ROLE_SimulatedProxy && Level.IsValid())
	{
		const uint8 LevelIndex = PredictedMovement->GetSlowFallLevelIndex(Level);
		if (LevelIndex == NO_MODIFIER)
		{
			return false;
		}
		
		switch (NetType)
		{
		case EModifierNetTypeLocal::LocalPredicted:
			return PredictedMovement->SlowFallLocal.AddModifier(LevelIndex);
		case EModifierNetTypeLocal::WithCorrection:
			return PredictedMovement->SlowFallCorrection.AddModifier(LevelIndex);
		default: return false;
		}
	}
	return false;
}

bool APredictedCharacter::UnSlowFall(FGameplayTag Level, EModifierNetTypeLocal NetType, bool bRemoveAll)
{
	if (PredictedMovement && GetLocalRole() != ROLE_SimulatedProxy && Level.IsValid())
	{
		const uint8 LevelIndex = PredictedMovement->GetSlowFallLevelIndex(Level);
		if (LevelIndex == NO_MODIFIER)
		{
			return false;
		}
		
		switch (NetType)
		{
		case EModifierNetTypeLocal::LocalPredicted:
			return PredictedMovement->SlowFallLocal.RemoveModifier(LevelIndex, bRemoveAll);
		case EModifierNetTypeLocal::WithCorrection:
			return PredictedMovement->SlowFallCorrection.RemoveModifier(LevelIndex, bRemoveAll);
		default: return false;
		}
	}
	return false;
}

bool APredictedCharacter::ResetSlowFall(EModifierNetTypeLocal NetType)
{
	if (PredictedMovement && GetLocalRole() != ROLE_SimulatedProxy)
	{
		switch (NetType)
		{
		case EModifierNetTypeLocal::LocalPredicted:
			return PredictedMovement->SlowFallLocal.ResetModifiers();
		case EModifierNetTypeLocal::WithCorrection:
			return PredictedMovement->SlowFallCorrection.ResetModifiers();
		default: return false;
		}
	}
	return false;
}

FGameplayTag APredictedCharacter::GetSlowFallLevel() const
{
	return PredictedMovement ? PredictedMovement->GetSlowFallLevel() : FGameplayTag::EmptyTag;
}

bool APredictedCharacter::IsSlowFallActive() const
{
	return PredictedMovement && PredictedMovement->IsSlowFallActive();
}

/* ~SlowFall Implementation */
