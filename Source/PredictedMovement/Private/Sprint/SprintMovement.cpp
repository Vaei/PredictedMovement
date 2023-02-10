// Copyright (c) 2023 Jared Taylor. All Rights Reserved.


#include "Sprint/SprintMovement.h"

#include "Sprint/SprintCharacter.h"

USprintMovement::USprintMovement()
{
	MaxWalkSpeedSprinting = 600.f;
}

void USprintMovement::PostLoad()
{
	Super::PostLoad();

	MyCharacterOwner = Cast<ASprintCharacter>(PawnOwner);
}

void USprintMovement::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	Super::SetUpdatedComponent(NewUpdatedComponent);

	MyCharacterOwner = Cast<ASprintCharacter>(PawnOwner);
}

float USprintMovement::GetMaxSpeed() const
{
	if (IsSprinting() && IsMovingOnGround())
	{
		return MaxWalkSpeedSprinting;
	}
	return Super::GetMaxSpeed();
}

bool USprintMovement::IsSprinting() const
{
	return MyCharacterOwner && MyCharacterOwner->bIsSprinting;
}

void USprintMovement::Sprint(bool bClientSimulation)
{
	if (!HasValidData())
	{
		return;
	}

	if (!bClientSimulation && !CanSprintInCurrentState())
	{
		return;
	}

	if (!bClientSimulation)
	{
		MyCharacterOwner->bIsSprinting = true;
	}
	MyCharacterOwner->OnStartSprint();
}

void USprintMovement::UnSprint(bool bClientSimulation)
{
	if (!HasValidData())
	{
		return;
	}

	if (!bClientSimulation)
	{
		MyCharacterOwner->bIsSprinting = false;
	}
	MyCharacterOwner->OnEndSprint();
}

bool USprintMovement::CanSprintInCurrentState() const
{
	return (IsFalling() || IsMovingOnGround()) && UpdatedComponent && !UpdatedComponent->IsSimulatingPhysics();
}

void USprintMovement::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	// Proxies get replicated Sprint state.
	if (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		// Check for a change in Sprint state. Players toggle Sprint by changing bWantsToSprint.
		const bool bIsSprinting = IsSprinting();
		if (bIsSprinting && (!bWantsToSprint || !CanSprintInCurrentState()))
		{
			UnSprint(false);
		}
		else if (!bIsSprinting && bWantsToSprint && CanSprintInCurrentState())
		{
			Sprint(false);
		}
	}
}

void USprintMovement::UpdateCharacterStateAfterMovement(float DeltaSeconds)
{
	// Proxies get replicated Sprint state.
	if (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		// UnSprint if no longer allowed to be Sprinting
		if (IsSprinting() && !CanSprintInCurrentState())
		{
			UnSprint(false);
		}
	}
}

bool USprintMovement::ClientUpdatePositionAfterServerUpdate()
{
	const bool bRealSprint = bWantsToSprint;
	const bool bResult = Super::ClientUpdatePositionAfterServerUpdate();
	bWantsToSprint = bRealSprint;

	return bResult;
}

void FSavedMove_Character_Sprint::Clear()
{
	FSavedMove_Character::Clear();

	bWantsToSprint = false;
}

void FSavedMove_Character_Sprint::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel,
	FNetworkPredictionData_Client_Character& ClientData)
{
	FSavedMove_Character::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	bWantsToSprint = Cast<ASprintCharacter>(C)->GetSprintCharacterMovement()->bWantsToSprint;
}

uint8 FSavedMove_Character_Sprint::GetCompressedFlags() const
{
	uint8 Result = FSavedMove_Character::GetCompressedFlags();

	if (bWantsToSprint)
	{
		Result |= FLAG_Custom_0;
	}

	return Result;
}

FSavedMovePtr FNetworkPredictionData_Client_Character_Sprint::AllocateNewMove()
{
	return MakeShared<FSavedMove_Character_Sprint>();
}

void USprintMovement::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	bWantsToSprint = ((Flags & FSavedMove_Character::FLAG_Custom_0) != 0);
}

FNetworkPredictionData_Client* USprintMovement::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		USprintMovement* MutableThis = const_cast<USprintMovement*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Character_Sprint(*this);
	}

	return ClientPredictionData;
}
