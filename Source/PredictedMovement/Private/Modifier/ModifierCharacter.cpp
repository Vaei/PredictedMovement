// Copyright (c) Jared Taylor


#include "Modifier/ModifierCharacter.h"

#include "Modifier/ModifierTags.h"
#include "Net/UnrealNetwork.h"
#include "Net/Core/PushModel/PushModel.h"
#include "PredictedMovement/Public/Modifier/ModifierMovement.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModifierCharacter)


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
		else if (ModifierType == FModifierTags::Modifier_Snare)
		{
			SimulatedSnare = ModifierMovement->GetSnareLevelIndex(ModifierLevel);
			MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, SimulatedSnare, this);
		}
		else if (ModifierType == FModifierTags::Modifier_SlowFall)
		{
			SimulatedSlowFall = ModifierMovement->GetSlowFallLevelIndex(ModifierLevel);
			MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, SimulatedSlowFall, this);
		}
	}
}

void AModifierCharacter::OnModifierAdded(const FGameplayTag& ModifierType, const FGameplayTag& ModifierLevel,
	const FGameplayTag& PrevModifierLevel)
{
	K2_OnModifierAdded(ModifierType, ModifierLevel, PrevModifierLevel);
}

void AModifierCharacter::OnModifierRemoved(const FGameplayTag& ModifierType, const FGameplayTag& ModifierLevel,
	const FGameplayTag& PrevModifierLevel)
{
	K2_OnModifierRemoved(ModifierType, ModifierLevel, PrevModifierLevel);
}

void AModifierCharacter::GrantClientAuthority(FGameplayTag ClientAuthSource, float OverrideDuration)
{
	if (ModifierMovement)
	{
		ModifierMovement->GrantClientAuthority(ClientAuthSource, OverrideDuration);
	}
}

/* Boost Implementation */

void AModifierCharacter::OnRep_SimulatedBoost(uint8 PrevLevel)
{
	if (ModifierMovement && SimulatedBoost != PrevLevel)
	{
		const FGameplayTag PrevBoostLevel = ModifierMovement->GetBoostLevel();
		ModifierMovement->BoostLevel = SimulatedBoost;
		NotifyModifierChanged<uint8>(FModifierTags::Modifier_Boost, ModifierMovement->GetBoostLevel(),
			PrevBoostLevel, ModifierMovement->BoostLevel, PrevLevel, NO_MODIFIER);

		ModifierMovement->bNetworkUpdateReceived = true;
	}
}

bool AModifierCharacter::Boost(FGameplayTag Level, EModifierNetType NetType)
{
	if (ModifierMovement && GetLocalRole() != ROLE_SimulatedProxy && Level.IsValid())
	{
		const uint8 LevelIndex = ModifierMovement->GetBoostLevelIndex(Level);
		if (LevelIndex == NO_MODIFIER)
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
		if (LevelIndex == NO_MODIFIER)
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

FGameplayTag AModifierCharacter::GetBoostLevel() const
{
	return ModifierMovement ? ModifierMovement->GetBoostLevel() : FGameplayTag::EmptyTag;
}

bool AModifierCharacter::IsBoostActive() const
{
	return ModifierMovement && ModifierMovement->IsBoostActive();
}

/* ~Boost Implementation */

/* Snare Implementation */

void AModifierCharacter::OnRep_SimulatedSnare(uint8 PrevLevel)
{
	if (ModifierMovement && SimulatedSnare != PrevLevel)
	{
		const FGameplayTag PrevSnareLevel = ModifierMovement->GetSnareLevel();
		ModifierMovement->SnareLevel = SimulatedSnare;
		NotifyModifierChanged<uint8>(FModifierTags::Modifier_Snare, ModifierMovement->GetSnareLevel(),
			PrevSnareLevel, ModifierMovement->SnareLevel, PrevLevel, NO_MODIFIER);

		ModifierMovement->bNetworkUpdateReceived = true;
	}
}

bool AModifierCharacter::Snare(FGameplayTag Level)
{
	if (ModifierMovement && HasAuthority() && Level.IsValid())
	{
		const uint8 LevelIndex = ModifierMovement->GetSnareLevelIndex(Level);
		if (LevelIndex == NO_MODIFIER)
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
		if (LevelIndex == NO_MODIFIER)
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

FGameplayTag AModifierCharacter::GetSnareLevel() const
{
	return ModifierMovement ? ModifierMovement->GetSnareLevel() : FGameplayTag::EmptyTag;
}

bool AModifierCharacter::IsSnareActive() const
{
	return ModifierMovement && ModifierMovement->IsSnareActive();
}

/* ~Snare Implementation */

/* SlowFall Implementation */

void AModifierCharacter::OnRep_SimulatedSlowFall(uint8 PrevLevel)
{
	if (ModifierMovement && SimulatedSlowFall != PrevLevel)
	{
		const FGameplayTag PrevSlowFallLevel = ModifierMovement->GetSlowFallLevel();
		ModifierMovement->SlowFallLevel = SimulatedSlowFall;
		NotifyModifierChanged<uint8>(FModifierTags::Modifier_SlowFall, ModifierMovement->GetSlowFallLevel(),
			PrevSlowFallLevel, ModifierMovement->SlowFallLevel, PrevLevel, NO_MODIFIER);

		ModifierMovement->bNetworkUpdateReceived = true;
	}
}

bool AModifierCharacter::SlowFall(FGameplayTag Level)
{
	if (ModifierMovement && GetLocalRole() != ROLE_SimulatedProxy && Level.IsValid())
	{
		const uint8 LevelIndex = ModifierMovement->GetSlowFallLevelIndex(Level);
		if (LevelIndex == NO_MODIFIER)
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
		if (LevelIndex == NO_MODIFIER)
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

FGameplayTag AModifierCharacter::GetSlowFallLevel() const
{
	return ModifierMovement ? ModifierMovement->GetSlowFallLevel() : FGameplayTag::EmptyTag;
}

bool AModifierCharacter::IsSlowFallActive() const
{
	return ModifierMovement && ModifierMovement->IsSlowFallActive();
}

/* ~SlowFall Implementation */
