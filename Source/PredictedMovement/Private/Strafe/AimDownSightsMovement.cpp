// Copyright (c) 2023 Jared Taylor. All Rights Reserved.


#include "Strafe/AimDownSightsMovement.h"

#include "Strafe/AimDownSightsCharacter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AimDownSightsMovement)

UAimDownSightsMovement::UAimDownSightsMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MaxAccelerationAiming = 1024.f;
	MaxWalkSpeedAiming = 400.f;
	BrakingDecelerationAiming = 512.f;
	GroundFrictionAiming = 12.f;
	BrakingFrictionAiming = 4.f;

	bWantsToAimDownSights = false;
}

bool UAimDownSightsMovement::HasValidData() const
{
	return Super::HasValidData() && IsValid(AimDownSightsCharacterOwner);
}

void UAimDownSightsMovement::PostLoad()
{
	Super::PostLoad();

	AimDownSightsCharacterOwner = Cast<AAimDownSightsCharacter>(PawnOwner);
}

void UAimDownSightsMovement::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	Super::SetUpdatedComponent(NewUpdatedComponent);

	AimDownSightsCharacterOwner = Cast<AAimDownSightsCharacter>(PawnOwner);
}

float UAimDownSightsMovement::GetMaxAcceleration() const
{
	if (IsAimingDownSights() && IsMovingOnGround())
	{
		return MaxAccelerationAiming;
	}
	return Super::GetMaxAcceleration();
}

float UAimDownSightsMovement::GetMaxSpeed() const
{
	if (IsAimingDownSights())
	{
		return MaxWalkSpeedAiming;
	}
	return Super::GetMaxSpeed();
}

float UAimDownSightsMovement::GetMaxBrakingDeceleration() const
{
	if (IsAimingDownSights() && IsMovingOnGround())
	{
		return BrakingDecelerationAiming;
	}
	return Super::GetMaxBrakingDeceleration();
}

void UAimDownSightsMovement::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	if (IsAimingDownSights() && IsMovingOnGround())
	{
		Friction = GroundFrictionAiming;
	}
	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
}

void UAimDownSightsMovement::ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration)
{
	if (IsAimingDownSights() && IsMovingOnGround())
	{
		Friction = (bUseSeparateBrakingFriction ? BrakingFrictionAiming : GroundFrictionAiming);
	}
	Super::ApplyVelocityBraking(DeltaTime, Friction, BrakingDeceleration);
}

bool UAimDownSightsMovement::IsAimingDownSights() const
{
	return AimDownSightsCharacterOwner && AimDownSightsCharacterOwner->bIsAimingDownSights;
}

void UAimDownSightsMovement::AimDownSights(bool bClientSimulation)
{
	if (!HasValidData())
	{
		return;
	}

	if (!bClientSimulation && !CanAimDownSightsInCurrentState())
	{
		return;
	}

	if (!bClientSimulation)
	{
		AimDownSightsCharacterOwner->bIsAimingDownSights = true;
	}
	AimDownSightsCharacterOwner->OnStartAimDownSights();
}

void UAimDownSightsMovement::StopAimDownSights(bool bClientSimulation)
{
	if (!HasValidData())
	{
		return;
	}

	if (!bClientSimulation)
	{
		AimDownSightsCharacterOwner->bIsAimingDownSights = false;
	}
	AimDownSightsCharacterOwner->OnEndAimDownSights();
}

bool UAimDownSightsMovement::CanAimDownSightsInCurrentState() const
{
	return (IsFalling() || IsMovingOnGround()) && UpdatedComponent && !UpdatedComponent->IsSimulatingPhysics();
}

void UAimDownSightsMovement::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	// Proxies get replicated Strafe state.
	if (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		// Check for a change in Strafe state. Players toggle Strafe by changing bWantsToStrafe.
		const bool bIsStrafing = IsAimingDownSights();
		if (bIsStrafing && (!bWantsToAimDownSights || !CanAimDownSightsInCurrentState()))
		{
			StopAimDownSights(false);
		}
		else if (!bIsStrafing && bWantsToAimDownSights && CanAimDownSightsInCurrentState())
		{
			AimDownSights(false);
		}
	}

	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

void UAimDownSightsMovement::UpdateCharacterStateAfterMovement(float DeltaSeconds)
{
	// Proxies get replicated Strafe state.
	if (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		// UnStrafe if no longer allowed to be Strafing
		if (IsAimingDownSights() && !CanAimDownSightsInCurrentState())
		{
			StopAimDownSights(false);
		}
	}

	Super::UpdateCharacterStateAfterMovement(DeltaSeconds);
}

bool UAimDownSightsMovement::ClientUpdatePositionAfterServerUpdate()
{
	const bool bRealStrafe = bWantsToAimDownSights;
	const bool bResult = Super::ClientUpdatePositionAfterServerUpdate();
	bWantsToAimDownSights = bRealStrafe;

	return bResult;
}

void FSavedMove_Character_AimDownSights::Clear()
{
	Super::Clear();

	bWantsToAimDownSights = false;
}

void FSavedMove_Character_AimDownSights::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel,
	FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	bWantsToAimDownSights = Cast<AAimDownSightsCharacter>(C)->GetAimDownSightsCharacterMovement()->bWantsToAimDownSights;
}

uint8 FSavedMove_Character_AimDownSights::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();

	if (bWantsToAimDownSights)
	{
		Result |= FLAG_Reserved_1;
	}

	return Result;
}

void UAimDownSightsMovement::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	bWantsToAimDownSights = ((Flags & FSavedMove_Character::FLAG_Reserved_1) != 0);
}

FSavedMovePtr FNetworkPredictionData_Client_Character_AimDownSights::AllocateNewMove()
{
	return MakeShared<FSavedMove_Character_AimDownSights>();
}

FNetworkPredictionData_Client* UAimDownSightsMovement::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UAimDownSightsMovement* MutableThis = const_cast<UAimDownSightsMovement*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Character_AimDownSights(*this);
	}

	return ClientPredictionData;
}
