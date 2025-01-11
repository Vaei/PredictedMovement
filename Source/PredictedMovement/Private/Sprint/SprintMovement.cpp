// Copyright (c) 2023 Jared Taylor. All Rights Reserved.


#include "Sprint/SprintMovement.h"

#include "Sprint/SprintCharacter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SprintMovement)

void FSprintNetworkMoveData::ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove,
	ENetworkMoveType MoveType)
{
	// Client packs move data to send to the server
	// Use this instead of GetCompressedFlags()
	Super::ClientFillNetworkMoveData(ClientMove, MoveType);

	// Client ➜ Server
	
	// CallServerMovePacked ➜ ClientFillNetworkMoveData ➜ ServerMovePacked_ClientSend >> Server
	// >> ServerMovePacked_ServerReceive ➜ ServerMove_HandleMoveData ➜ ServerMove_PerformMovement
	// ➜ MoveAutonomous (UpdateFromCompressedFlags)
	
	const FSavedMove_Character_Sprint& SavedMove = static_cast<const FSavedMove_Character_Sprint&>(ClientMove);
	bWantsToSprint = SavedMove.bWantsToSprint;
}

bool FSprintNetworkMoveData::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar,
	UPackageMap* PackageMap, ENetworkMoveType MoveType)
{
	Super::Serialize(CharacterMovement, Ar, PackageMap, MoveType);

	Ar.SerializeBits(&bWantsToSprint, 1);
	
	return !Ar.IsError();
}

USprintMovement::USprintMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetNetworkMoveDataContainer(SprintMoveDataContainer);
	
	bUseMaxAccelerationSprintingOnlyAtSpeed = true;
	MaxAccelerationSprinting = 1024.f;
	MaxWalkSpeedSprinting = 600.f;
	BrakingDecelerationSprinting = 512.f;
	GroundFrictionSprinting = 8.f;
	BrakingFrictionSprinting = 4.f;

	VelocityCheckMitigatorSprinting = 0.98f;

	bWantsToSprint = false;
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

	// When moving on ground we want to factor moving uphill or downhill so variations in terrain
	// aren't culled from the check. When falling, we don't want to factor fall velocity, only lateral
	const float Vel = IsMovingOnGround() ? Velocity.SizeSquared() : Velocity.SizeSquared2D();
	const float WalkSpeed = IsCrouching() ? MaxWalkSpeedCrouched : MaxWalkSpeed;

	// When struggling to surpass walk speed, which can occur with heavy rotation and low acceleration, we
	// mitigate the check so there isn't a constant re-entry that can occur as an edge case
	return Vel >= (WalkSpeed * WalkSpeed * VelocityCheckMitigatorSprinting);
}

float USprintMovement::GetMaxAcceleration() const
{
	if (IsSprinting() && (!bUseMaxAccelerationSprintingOnlyAtSpeed || IsSprintingAtSpeed()))
	{
		return MaxAccelerationSprinting;
	}
	return Super::GetMaxAcceleration();
}

float USprintMovement::GetMaxSpeed() const
{
	if (IsSprinting())
	{
		return MaxWalkSpeedSprinting;
	}
	return Super::GetMaxSpeed();
}

float USprintMovement::GetMaxBrakingDeceleration() const
{
	if (IsSprinting() && IsSprintingAtSpeed())
	{
		return BrakingDecelerationSprinting;
	}
	return Super::GetMaxBrakingDeceleration();
}

void USprintMovement::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	if (IsSprinting() && IsMovingOnGround())
	{
		Friction = GroundFrictionSprinting;
	}
	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
}

void USprintMovement::ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration)
{
	if (IsSprinting() && IsMovingOnGround())
	{
		Friction = (bUseSeparateBrakingFriction ? BrakingFrictionSprinting : GroundFrictionSprinting);
	}
	Super::ApplyVelocityBraking(DeltaTime, Friction, BrakingDeceleration);
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
	if (!UpdatedComponent || UpdatedComponent->IsSimulatingPhysics())
	{
		return false;
	}

	if (!IsFalling() && !IsMovingOnGround())
	{
		return false;
	}
	
	if (!IsSprintWithinAllowableInputAngle())
	{
		return false;
	}

	return true;
}

bool USprintMovement::IsSprintWithinAllowableInputAngle() const
{
	// This check ensures that we are not sprinting backward or sideways, while allowing leeway 
	// This angle allows sprinting when holding forward, forward left, forward right
	// but not left or right or backward)
	static constexpr float MaxSprintInputDegrees = 50.f;
	static constexpr float MaxSprintInputNormal = 0.64278732;  // cos(rad(MaxSprintInputDegrees))

	if constexpr (MaxSprintInputDegrees > 0.f)
	{
		const float Dot = (GetCurrentAcceleration().GetSafeNormal2D() | UpdatedComponent->GetForwardVector());
		if (Dot < MaxSprintInputNormal)
		{
			return false;
		}
	}
	return true;
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

void USprintMovement::ServerMove_PerformMovement(const FCharacterNetworkMoveData& MoveData)
{
	// Server updates from the client's move data
	// Use this instead of UpdateFromCompressedFlags()

	// Client >> CallServerMovePacked ➜ ClientFillNetworkMoveData ➜ ServerMovePacked_ClientSend >> Server
	// >> ServerMovePacked_ServerReceive ➜ ServerMove_HandleMoveData ➜ ServerMove_PerformMovement
	
	const FSprintNetworkMoveData& ModifierMoveData = static_cast<const FSprintNetworkMoveData&>(MoveData);

	bWantsToSprint = ModifierMoveData.bWantsToSprint;

	Super::ServerMove_PerformMovement(MoveData);
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
	Super::Clear();

	bWantsToSprint = false;
}

void FSavedMove_Character_Sprint::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel,
	FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	bWantsToSprint = Cast<ASprintCharacter>(C)->GetSprintCharacterMovement()->bWantsToSprint;
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
