// Copyright (c) 2023 Jared Taylor. All Rights Reserved.


#include "Sprint/SprintMovement.h"

#include "Sprint/SprintCharacter.h"

USprintMovement::USprintMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MaxAccelerationSprinting = 1024.f;
	MaxWalkSpeedSprinting = 600.f;
	BrakingDecelerationSprinting = 512.f;
	GroundFrictionSprinting = 8.f;
}

bool USprintMovement::HasValidData() const
{
	return Super::HasValidData() && IsValid(SprintCharacterOwner);
}

void USprintMovement::PostLoad()
{
	Super::PostLoad();

	SprintCharacterOwner = Cast<ASprintCharacter>(PawnOwner);
}

void USprintMovement::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	Super::SetUpdatedComponent(NewUpdatedComponent);

	SprintCharacterOwner = Cast<ASprintCharacter>(PawnOwner);
}

bool USprintMovement::IsSprintingAtSpeed() const
{
	if (!IsSprinting())
	{
		return false;
	}
	const float MaxSpeed = IsCrouching() ? MaxWalkSpeedCrouched : MaxWalkSpeed;
	return Velocity.SizeSquared2D() >= (MaxSpeed * MaxSpeed * 0.98f);
}

float USprintMovement::GetMaxAcceleration() const
{
	if (IsSprinting() && IsMovingOnGround() && IsSprintingAtSpeed())
	{
		return MaxAccelerationSprinting;
	}
	return Super::GetMaxAcceleration();
}

float USprintMovement::GetMaxSpeed() const
{
	if (IsSprinting() && IsMovingOnGround() && IsSprintingAtSpeed())
	{
		return MaxWalkSpeedSprinting;
	}
	return Super::GetMaxSpeed();
}

float USprintMovement::GetMaxBrakingDeceleration() const
{
	if (IsSprinting() && IsMovingOnGround() && IsSprintingAtSpeed())
	{
		return BrakingDecelerationSprinting;
	}
	return Super::GetMaxBrakingDeceleration();
}

bool USprintMovement::IsSprinting() const
{
	return SprintCharacterOwner && SprintCharacterOwner->bIsSprinting;
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
		SprintCharacterOwner->bIsSprinting = true;
	}
	SprintCharacterOwner->OnStartSprint();
}

void USprintMovement::UnSprint(bool bClientSimulation)
{
	if (!HasValidData())
	{
		return;
	}

	if (!bClientSimulation)
	{
		SprintCharacterOwner->bIsSprinting = false;
	}
	SprintCharacterOwner->OnEndSprint();
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

	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
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

	Super::UpdateCharacterStateAfterMovement(DeltaSeconds);
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

void USprintMovement::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	bWantsToSprint = ((Flags & FSavedMove_Character::FLAG_Custom_0) != 0);
}

FSavedMovePtr FNetworkPredictionData_Client_Character_Sprint::AllocateNewMove()
{
	return MakeShared<FSavedMove_Character_Sprint>();
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
