// Copyright (c) 2023 Jared Taylor. All Rights Reserved.


#include "Strafe/StrafeMovement.h"

#include "Strafe/StrafeCharacter.h"

UStrafeMovement::UStrafeMovement()
{
	MaxAccelerationStrafing = 1024.f;
	MaxWalkSpeedStrafing = 400.f;
	BrakingDecelerationStrafing = 512.f;
	GroundFrictionStrafing = 12.f;
}

bool UStrafeMovement::HasValidData() const
{
	return Super::HasValidData() && IsValid(StrafeCharacterOwner);
}

void UStrafeMovement::PostLoad()
{
	Super::PostLoad();

	StrafeCharacterOwner = Cast<AStrafeCharacter>(PawnOwner);
}

void UStrafeMovement::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	Super::SetUpdatedComponent(NewUpdatedComponent);

	StrafeCharacterOwner = Cast<AStrafeCharacter>(PawnOwner);
}

float UStrafeMovement::GetMaxAcceleration() const
{
	if (IsStrafing() && IsMovingOnGround())
	{
		return MaxAccelerationStrafing;
	}
	return Super::GetMaxAcceleration();
}

float UStrafeMovement::GetMaxSpeed() const
{
	if (IsStrafing() && IsMovingOnGround())
	{
		return MaxWalkSpeedStrafing;
	}
	return Super::GetMaxSpeed();
}

float UStrafeMovement::GetMaxBrakingDeceleration() const
{
	if (IsStrafing() && IsMovingOnGround())
	{
		return BrakingDecelerationStrafing;
	}
	return Super::GetMaxBrakingDeceleration();
}

bool UStrafeMovement::IsStrafing() const
{
	return StrafeCharacterOwner && StrafeCharacterOwner->bIsStrafing;
}

void UStrafeMovement::Strafe(bool bClientSimulation)
{
	if (!HasValidData())
	{
		return;
	}

	if (!bClientSimulation && !CanStrafeInCurrentState())
	{
		return;
	}

	if (!bClientSimulation)
	{
		StrafeCharacterOwner->bIsStrafing = true;
	}
	StrafeCharacterOwner->OnStartStrafe();
}

void UStrafeMovement::UnStrafe(bool bClientSimulation)
{
	if (!HasValidData())
	{
		return;
	}

	if (!bClientSimulation)
	{
		StrafeCharacterOwner->bIsStrafing = false;
	}
	StrafeCharacterOwner->OnEndStrafe();
}

bool UStrafeMovement::CanStrafeInCurrentState() const
{
	return (IsFalling() || IsMovingOnGround()) && UpdatedComponent && !UpdatedComponent->IsSimulatingPhysics();
}

void UStrafeMovement::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	// Proxies get replicated Strafe state.
	if (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		// Check for a change in Strafe state. Players toggle Strafe by changing bWantsToStrafe.
		const bool bIsStrafing = IsStrafing();
		if (bIsStrafing && (!bWantsToStrafe || !CanStrafeInCurrentState()))
		{
			UnStrafe(false);
		}
		else if (!bIsStrafing && bWantsToStrafe && CanStrafeInCurrentState())
		{
			Strafe(false);
		}
	}

	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

void UStrafeMovement::UpdateCharacterStateAfterMovement(float DeltaSeconds)
{
	// Proxies get replicated Strafe state.
	if (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		// UnStrafe if no longer allowed to be Strafing
		if (IsStrafing() && !CanStrafeInCurrentState())
		{
			UnStrafe(false);
		}
	}

	Super::UpdateCharacterStateAfterMovement(DeltaSeconds);
}

bool UStrafeMovement::ClientUpdatePositionAfterServerUpdate()
{
	const bool bRealStrafe = bWantsToStrafe;
	const bool bResult = Super::ClientUpdatePositionAfterServerUpdate();
	bWantsToStrafe = bRealStrafe;

	return bResult;
}

void FSavedMove_Character_Strafe::Clear()
{
	FSavedMove_Character::Clear();

	bWantsToStrafe = false;
}

void FSavedMove_Character_Strafe::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel,
	FNetworkPredictionData_Client_Character& ClientData)
{
	FSavedMove_Character::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	bWantsToStrafe = Cast<AStrafeCharacter>(C)->GetStrafeCharacterMovement()->bWantsToStrafe;
}

uint8 FSavedMove_Character_Strafe::GetCompressedFlags() const
{
	uint8 Result = FSavedMove_Character::GetCompressedFlags();

	if (bWantsToStrafe)
	{
		Result |= FLAG_Reserved_1;
	}

	return Result;
}

void UStrafeMovement::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	bWantsToStrafe = ((Flags & FSavedMove_Character::FLAG_Reserved_1) != 0);
}

FSavedMovePtr FNetworkPredictionData_Client_Character_Strafe::AllocateNewMove()
{
	return MakeShared<FSavedMove_Character_Strafe>();
}

FNetworkPredictionData_Client* UStrafeMovement::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UStrafeMovement* MutableThis = const_cast<UStrafeMovement*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Character_Strafe(*this);
	}

	return ClientPredictionData;
}
