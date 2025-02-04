// Copyright (c) 2023 Jared Taylor. All Rights Reserved.


#include "PredMovement.h"

#include "PredCharacter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PredMovement)

void FPredNetworkMoveData::ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove,
	ENetworkMoveType MoveType)
{
	// Client packs move data to send to the server
	// Use this instead of GetCompressedFlags()
	Super::ClientFillNetworkMoveData(ClientMove, MoveType);

	// Client ➜ Server
	
	// CallServerMovePacked ➜ ClientFillNetworkMoveData ➜ ServerMovePacked_ClientSend >> Server
	// >> ServerMovePacked_ServerReceive ➜ ServerMove_HandleMoveData ➜ ServerMove_PerformMovement
	// ➜ MoveAutonomous (UpdateFromCompressedFlags)
	
	const FSavedMove_Character_Pred& SavedMove = static_cast<const FSavedMove_Character_Pred&>(ClientMove);
	bWantsToSprint = SavedMove.bWantsToSprint;
}

bool FPredNetworkMoveData::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar,
	UPackageMap* PackageMap, ENetworkMoveType MoveType)
{
	Super::Serialize(CharacterMovement, Ar, PackageMap, MoveType);

	Ar.SerializeBits(&bWantsToSprint, 1);
	
	return !Ar.IsError();
}

UPredMovement::UPredMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetNetworkMoveDataContainer(PredMoveDataContainer);
	
	bUseMaxAccelerationSprintingOnlyAtSpeed = true;
	MaxAccelerationSprinting = 1024.f;
	MaxWalkSpeedSprinting = 600.f;
	BrakingDecelerationSprinting = 512.f;
	GroundFrictionSprinting = 8.f;
	BrakingFrictionSprinting = 4.f;

	VelocityCheckMitigatorSprinting = 0.98f;

	bWantsToSprint = false;
}

bool UPredMovement::HasValidData() const
{
	return Super::HasValidData() && IsValid(PredCharacterOwner);
}

void UPredMovement::PostLoad()
{
	Super::PostLoad();

	PredCharacterOwner = Cast<APredCharacter>(PawnOwner);
}

void UPredMovement::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	Super::SetUpdatedComponent(NewUpdatedComponent);

	PredCharacterOwner = Cast<APredCharacter>(PawnOwner);
}

bool UPredMovement::IsSprintingAtSpeed() const
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

float UPredMovement::GetMaxAcceleration() const
{
	if (IsSprinting() && (!bUseMaxAccelerationSprintingOnlyAtSpeed || IsSprintingAtSpeed()))
	{
		return MaxAccelerationSprinting;
	}
	return Super::GetMaxAcceleration();
}

float UPredMovement::GetMaxSpeed() const
{
	if (IsSprinting())
	{
		return MaxWalkSpeedSprinting;
	}
	return Super::GetMaxSpeed();
}

float UPredMovement::GetMaxBrakingDeceleration() const
{
	if (IsSprinting() && IsSprintingAtSpeed())
	{
		return BrakingDecelerationSprinting;
	}
	return Super::GetMaxBrakingDeceleration();
}

void UPredMovement::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	if (IsSprinting() && IsMovingOnGround())
	{
		Friction = GroundFrictionSprinting;
	}
	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
}

void UPredMovement::ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration)
{
	if (IsSprinting() && IsMovingOnGround())
	{
		Friction = (bUseSeparateBrakingFriction ? BrakingFrictionSprinting : GroundFrictionSprinting);
	}
	Super::ApplyVelocityBraking(DeltaTime, Friction, BrakingDeceleration);
}

bool UPredMovement::IsSprinting() const
{
	return PredCharacterOwner && PredCharacterOwner->bIsSprinting;
}

void UPredMovement::Sprint(bool bClientSimulation)
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
		PredCharacterOwner->bIsSprinting = true;
	}
	PredCharacterOwner->OnStartSprint();
}

void UPredMovement::UnSprint(bool bClientSimulation)
{
	if (!HasValidData())
	{
		return;
	}

	if (!bClientSimulation)
	{
		PredCharacterOwner->bIsSprinting = false;
	}
	PredCharacterOwner->OnEndSprint();
}

bool UPredMovement::CanSprintInCurrentState() const
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

bool UPredMovement::IsSprintWithinAllowableInputAngle() const
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

void UPredMovement::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
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

void UPredMovement::UpdateCharacterStateAfterMovement(float DeltaSeconds)
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

void UPredMovement::ServerMove_PerformMovement(const FCharacterNetworkMoveData& MoveData)
{
	// Server updates from the client's move data
	// Use this instead of UpdateFromCompressedFlags()

	// Client >> CallServerMovePacked ➜ ClientFillNetworkMoveData ➜ ServerMovePacked_ClientSend >> Server
	// >> ServerMovePacked_ServerReceive ➜ ServerMove_HandleMoveData ➜ ServerMove_PerformMovement
	
	const FPredNetworkMoveData& ModifierMoveData = static_cast<const FPredNetworkMoveData&>(MoveData);

	bWantsToSprint = ModifierMoveData.bWantsToSprint;

	Super::ServerMove_PerformMovement(MoveData);
}

bool UPredMovement::ClientUpdatePositionAfterServerUpdate()
{
	const bool bRealSprint = bWantsToSprint;
	const bool bResult = Super::ClientUpdatePositionAfterServerUpdate();
	bWantsToSprint = bRealSprint;

	return bResult;
}

void FSavedMove_Character_Pred::Clear()
{
	Super::Clear();

	bWantsToSprint = false;
}

void FSavedMove_Character_Pred::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel,
	FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	bWantsToSprint = Cast<APredCharacter>(C)->GetSprintCharacterMovement()->bWantsToSprint;
}

FSavedMovePtr FNetworkPredictionData_Client_Character_Pred::AllocateNewMove()
{
	return MakeShared<FSavedMove_Character_Pred>();
}

FNetworkPredictionData_Client* UPredMovement::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UPredMovement* MutableThis = const_cast<UPredMovement*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Character_Pred(*this);
	}

	return ClientPredictionData;
}
