// Copyright (c) Jared Taylor


#include "Modifier/ModifierCharacter.h"

#include "Modifier/ModifierTags.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "PredictedMovement/Public/Modifier/ModifierMovement.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModifierCharacter)


void AModifierCharacter::OnModifierChanged(const FGameplayTag& ModifierType, const FGameplayTag& ModifierLevel,
	const FGameplayTag& PrevModifierLevel)
{
	K2_OnModifierChanged(ModifierType, ModifierLevel, PrevModifierLevel);

	// Replicate to simulated proxies
	if (ModifierMovement && HasAuthority())
	{
		if (ModifierType == FModifierTags::Modifier_Boost)
		{
			SimulatedBoost = ModifierMovement->GetBoostLevelIndex(ModifierLevel);
			MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, SimulatedBoost, this);
		}
	}
}

void AModifierCharacter::OnModifierAdded(const FGameplayTag& ModifierType, const FGameplayTag& ModifierLevel,
	const FGameplayTag& PrevModifierLevel)
{
	if (ModifierMovement)
	{
		if (ModifierType == FModifierTags::Modifier_SlowFall)
		{
			ModifierMovement->OnStartSlowFall();
		}
	}
	K2_OnModifierAdded(ModifierType, ModifierLevel, PrevModifierLevel);
}

void AModifierCharacter::OnModifierRemoved(const FGameplayTag& ModifierType, const FGameplayTag& ModifierLevel,
	const FGameplayTag& PrevModifierLevel)
{
	K2_OnModifierRemoved(ModifierType, ModifierLevel, PrevModifierLevel);
}

AModifierCharacter::AModifierCharacter(const FObjectInitializer& FObjectInitializer)
	: Super(FObjectInitializer.SetDefaultSubobjectClass<UModifierMovement>(CharacterMovementComponentName))
{
	ModifierMovement = Cast<UModifierMovement>(GetCharacterMovement());
}

void AModifierCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Push Model
	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;
	SharedParams.Condition = COND_SimulatedOnly;
	
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, SimulatedBoost, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, SimulatedSnare, SharedParams);
	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, SimulatedSlowFall, SharedParams);
}

/* Boost Implementation */

void AModifierCharacter::OnRep_SimulatedBoost(uint8 PrevLevel)
{
	if (ModifierMovement && SimulatedBoost != PrevLevel)
	{
		const FGameplayTag PrevBoostLevel = ModifierMovement->GetBoostLevel();
		ModifierMovement->BoostLevel = SimulatedBoost;
		NotifyModifierChanged(FModifierTags::Modifier_Boost, ModifierMovement->GetBoostLevel(),
			PrevBoostLevel, ModifierMovement->BoostLevel, PrevLevel, UINT8_MAX);

		ModifierMovement->bNetworkUpdateReceived = true;
	}
}

bool AModifierCharacter::Boost(FGameplayTag Level, EModifierNetType NetType)
{
	if (ModifierMovement && GetLocalRole() != ROLE_SimulatedProxy && Level.IsValid())
	{
		const uint8 LevelIndex = ModifierMovement->GetBoostLevelIndex(Level);
		if (LevelIndex == UINT8_MAX)
		{
			return false;
		}
		
		switch (NetType)
		{
		case EModifierNetType::LocalPredicted:
			return ModifierMovement->BoostLocal.AddModifier(LevelIndex);
		case EModifierNetType::WithCorrection:
			return ModifierMovement->BoostCorrection.AddModifier(LevelIndex);
		case EModifierNetType::ServerInitiated:
			if (HasAuthority())
			{
				return ModifierMovement->BoostServer.AddModifier(LevelIndex);
			}
		default: return false;
		}
	}
	return false;
}

bool AModifierCharacter::UnBoost(FGameplayTag Level, EModifierNetType NetType, bool bRemoveAll)
{
	if (ModifierMovement && GetLocalRole() != ROLE_SimulatedProxy && Level.IsValid())
	{
		const uint8 LevelIndex = ModifierMovement->GetBoostLevelIndex(Level);
		if (LevelIndex == UINT8_MAX)
		{
			return false;
		}
		
		switch (NetType)
		{
		case EModifierNetType::LocalPredicted:
			return ModifierMovement->BoostLocal.RemoveModifier(LevelIndex, bRemoveAll);
		case EModifierNetType::WithCorrection:
			return ModifierMovement->BoostCorrection.RemoveModifier(LevelIndex, bRemoveAll);
		case EModifierNetType::ServerInitiated:
			if (HasAuthority())
			{
				return ModifierMovement->BoostServer.RemoveModifier(LevelIndex, bRemoveAll);
			}
		default: return false;
		}
	}
	return false;
}

bool AModifierCharacter::ResetBoost(EModifierNetType NetType)
{
	if (ModifierMovement && GetLocalRole() != ROLE_SimulatedProxy)
	{
		switch (NetType)
		{
		case EModifierNetType::LocalPredicted:
			return ModifierMovement->BoostLocal.ResetModifiers();
		case EModifierNetType::WithCorrection:
			return ModifierMovement->BoostCorrection.ResetModifiers();
		case EModifierNetType::ServerInitiated:
			if (HasAuthority())
			{
				return ModifierMovement->BoostServer.ResetModifiers();
			}
		default: return false;
		}
	}
	return false;
}

/* ~Boost Implementation */

/* Snare Implementation */

void AModifierCharacter::OnRep_SimulatedSnare(uint8 PrevLevel)
{
	if (ModifierMovement && SimulatedSnare != PrevLevel)
	{
		const FGameplayTag PrevSnareLevel = ModifierMovement->GetSnareLevel();
		ModifierMovement->SnareLevel = SimulatedSnare;
		NotifyModifierChanged(FModifierTags::Modifier_Snare, ModifierMovement->GetSnareLevel(),
			PrevSnareLevel, ModifierMovement->SnareLevel, PrevLevel, UINT8_MAX);

		ModifierMovement->bNetworkUpdateReceived = true;
	}
}

bool AModifierCharacter::Snare(FGameplayTag Level)
{
	if (ModifierMovement && HasAuthority() && Level.IsValid())
	{
		const uint8 LevelIndex = ModifierMovement->GetSnareLevelIndex(Level);
		if (LevelIndex == UINT8_MAX)
		{
			return false;
		}
		
		return ModifierMovement->SnareServer.AddModifier(LevelIndex);
	}
	return false;
}

bool AModifierCharacter::UnSnare(FGameplayTag Level, bool bRemoveAll)
{
	if (ModifierMovement && HasAuthority() && Level.IsValid())
	{
		const uint8 LevelIndex = ModifierMovement->GetSnareLevelIndex(Level);
		if (LevelIndex == UINT8_MAX)
		{
			return false;
		}

		return ModifierMovement->SnareServer.RemoveModifier(LevelIndex, bRemoveAll);
	}
	return false;
}

bool AModifierCharacter::ResetSnare()
{
	if (ModifierMovement && HasAuthority())
	{
		return ModifierMovement->SnareServer.ResetModifiers();
	}
	return false;
}

/* ~Snare Implementation */

/* SlowFall Implementation */

void AModifierCharacter::OnRep_SimulatedSlowFall(uint8 PrevLevel)
{
	if (ModifierMovement && SimulatedSlowFall != PrevLevel)
	{
		const FGameplayTag PrevSlowFallLevel = ModifierMovement->GetSlowFallLevel();
		ModifierMovement->SlowFallLevel = SimulatedSlowFall;
		NotifyModifierChanged(FModifierTags::Modifier_SlowFall, ModifierMovement->GetSlowFallLevel(),
			PrevSlowFallLevel, ModifierMovement->SlowFallLevel, PrevLevel, UINT8_MAX);

		ModifierMovement->bNetworkUpdateReceived = true;
	}
}

bool AModifierCharacter::SlowFall(FGameplayTag Level)
{
	if (ModifierMovement && GetLocalRole() != ROLE_SimulatedProxy && Level.IsValid())
	{
		const uint8 LevelIndex = ModifierMovement->GetSlowFallLevelIndex(Level);
		if (LevelIndex == UINT8_MAX)
		{
			return false;
		}
		
		return ModifierMovement->SlowFallLocal.AddModifier(LevelIndex);
	}
	return false;
}

bool AModifierCharacter::UnSlowFall(FGameplayTag Level, bool bRemoveAll)
{
	if (ModifierMovement && GetLocalRole() != ROLE_SimulatedProxy && Level.IsValid())
	{
		const uint8 LevelIndex = ModifierMovement->GetSlowFallLevelIndex(Level);
		if (LevelIndex == UINT8_MAX)
		{
			return false;
		}
		
		return ModifierMovement->SlowFallLocal.RemoveModifier(LevelIndex, bRemoveAll);
	}
	return false;
}

bool AModifierCharacter::ResetSlowFall()
{
	if (ModifierMovement && GetLocalRole() != ROLE_SimulatedProxy)
	{
		return ModifierMovement->SlowFallLocal.ResetModifiers();
	}
	return false;
}

/* ~SlowFall Implementation */
