// Copyright (c) 2023 Jared Taylor. All Rights Reserved.


#include "PredMovement.h"

#include "PredCharacter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PredMovement)

namespace PredMovementCVars
{
#if UE_ENABLE_DEBUG_DRAWING
	int32 DrawStaminaValues = 0;
	FAutoConsoleVariableRef CVarDrawStaminaValues(
		TEXT("p.DrawStaminaValues"),
		DrawStaminaValues,
		TEXT("Whether to draw stamina values to screen.\n")
		TEXT("0: Disable, 1: Enable, 2: Enable Local Client Only, 3: Enable Authority Only"),
		ECVF_Default);
#endif
}

void FPredMoveResponseDataContainer::ServerFillResponseData(const UCharacterMovementComponent& CharacterMovement,
	const FClientAdjustment& PendingAdjustment)
{
	Super::ServerFillResponseData(CharacterMovement, PendingAdjustment);

	// Server ➜ Client
	const UPredMovement* MoveComp = Cast<UPredMovement>(&CharacterMovement);
	bStaminaDrained = MoveComp->IsStaminaDrained();
	Stamina = MoveComp->GetStamina();
}

bool FPredMoveResponseDataContainer::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar,
	UPackageMap* PackageMap)
{
	if (!Super::Serialize(CharacterMovement, Ar, PackageMap))
	{
		return false;
	}

	// Server ➜ Client
	if (IsCorrection())
	{
		Ar << Stamina;
		Ar << bStaminaDrained;
	}

	return !Ar.IsError();
}

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
	Stamina = SavedMove.EndStamina;
}

bool FPredNetworkMoveData::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar,
	UPackageMap* PackageMap, ENetworkMoveType MoveType)
{
	Super::Serialize(CharacterMovement, Ar, PackageMap, MoveType);

	Ar.SerializeBits(&bWantsToSprint, 1);
	SerializeOptionalValue<float>(Ar.IsSaving(), Ar, Stamina, 0.f);
	
	return !Ar.IsError();
}

UPredMovement::UPredMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetNetworkMoveDataContainer(PredMoveDataContainer);
	SetMoveResponseDataContainer(PredMoveResponseDataContainer);

	// Sprinting
	bUseMaxAccelerationSprintingOnlyAtSpeed = true;
	MaxAccelerationSprinting = 1024.f;
	MaxWalkSpeedSprinting = 600.f;
	BrakingDecelerationSprinting = 512.f;
	GroundFrictionSprinting = 8.f;
	BrakingFrictionSprinting = 4.f;

	VelocityCheckMitigatorSprinting = 0.98f;

	bWantsToSprint = false;

	// Enable crouch
	NavAgentProps.bCanCrouch = true;

	// Stamina drained scalars
	MaxWalkSpeedScalarStaminaDrained = 0.25f;
	MaxAccelerationScalarStaminaDrained = 0.5f;
	MaxBrakingDecelerationScalarStaminaDrained = 0.5f;

	// Stamina
	SetMaxStamina(100.f);
	SprintStaminaDrainRate = 65.f;
	StaminaRegenRate = 60.f;
	StaminaDrainedRegenRate = 120.f;
	bStaminaRecoveryFromPct = true;
	StaminaRecoveryAmount = 20.f;
	StaminaRecoveryPct = 0.2f;
	StartSprintStaminaPct = 0.05f;  // 5% stamina to start sprinting
	
	NetworkStaminaCorrectionThreshold = 2.f;
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

void UPredMovement::BeginPlay()
{
	Super::BeginPlay();

	// Broadcast events to initialize UI, etc.
	OnMaxStaminaChanged(GetMaxStamina(), GetMaxStamina());
	OnStaminaChanged(GetStamina(), GetStamina());
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

float UPredMovement::GetMaxAccelerationMultiplier() const
{
	return IsStaminaDrained() ? MaxAccelerationScalarStaminaDrained : 1.f;
}

float UPredMovement::GetMaxSpeedMultiplier() const
{
	return IsStaminaDrained() ? MaxWalkSpeedScalarStaminaDrained : 1.f;
}

float UPredMovement::GetMaxBrakingDecelerationMultiplier() const
{
	return IsStaminaDrained() ? MaxBrakingDecelerationScalarStaminaDrained : 1.f;
}

float UPredMovement::GetMaxAcceleration() const
{
	if (IsSprinting() && (!bUseMaxAccelerationSprintingOnlyAtSpeed || IsSprintingAtSpeed()))
	{
		return MaxAccelerationSprinting * GetMaxAccelerationMultiplier();
	}
	if (IsAimingDownSights())
	{
		return MaxAccelerationAimingDownSights * GetMaxAccelerationMultiplier();
	}
	return Super::GetMaxAcceleration() * GetMaxAccelerationMultiplier();
}

float UPredMovement::GetMaxSpeed() const
{
	if (IsSprinting())
	{
		return MaxWalkSpeedSprinting * GetMaxSpeedMultiplier();
	}
	if (IsAimingDownSights())
	{
		return MaxWalkSpeedAimingDownSights * GetMaxSpeedMultiplier();
	}
	return Super::GetMaxSpeed() * GetMaxSpeedMultiplier();
}

float UPredMovement::GetMaxBrakingDeceleration() const
{
	if (IsSprinting() && IsSprintingAtSpeed())
	{
		return BrakingDecelerationSprinting * GetMaxBrakingDecelerationMultiplier();
	}
	if (IsAimingDownSights())
	{
		return BrakingDecelerationAimingDownSights * GetMaxBrakingDecelerationMultiplier();
	}
	return Super::GetMaxBrakingDeceleration() * GetMaxBrakingDecelerationMultiplier();
}

float UPredMovement::GetGroundFrictionMultiplier() const
{
	return 1.f;
}

float UPredMovement::GetGroundFriction(float DefaultGroundFriction) const
{
	if (IsSprinting())
	{
		return GroundFrictionSprinting * GetGroundFrictionMultiplier();
	}
	if (IsAimingDownSights())
	{
		return GroundFrictionAimingDownSights * GetGroundFrictionMultiplier();
	}
	return DefaultGroundFriction * GetGroundFrictionMultiplier();
}

float UPredMovement::GetBrakingFrictionMultiplier() const
{
	return 1.f;
}

float UPredMovement::GetBrakingFriction() const
{
	if (IsSprinting())
	{
		return BrakingFrictionSprinting * GetBrakingFrictionMultiplier();
	}
	if (IsAimingDownSights())
	{
		return BrakingFrictionAimingDownSights * GetBrakingFrictionMultiplier();
	}
	return BrakingFriction * GetBrakingFrictionMultiplier();
}

void UPredMovement::CalcStamina(float DeltaTime)
{
	// Do not update velocity when using root motion or when SimulatedProxy and not simulating root motion - SimulatedProxy are repped their Velocity
	if (!HasValidData() || HasAnimRootMotion() || DeltaTime < MIN_TICK_TIME || (CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy && !bWasSimulatingRootMotion))
	{
		return;
	}
	
	if (IsSprintingAtSpeed())
	{
		SetStamina(GetStamina() - SprintStaminaDrainRate * DeltaTime);
	}
	else
	{
		const float RegenRate = IsStaminaDrained() ? StaminaDrainedRegenRate : StaminaRegenRate;
		SetStamina(GetStamina() + RegenRate * DeltaTime);
	}
}

void UPredMovement::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	if (IsMovingOnGround())
	{
		Friction = GetGroundFriction(Friction);
	}
	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
}

void UPredMovement::ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration)
{
	if (IsMovingOnGround())
	{
		Friction = bUseSeparateBrakingFriction ? GetBrakingFriction() : GetGroundFriction(Friction);
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

	// Cannot sprint if stamina is drained
	if (IsStaminaDrained())
	{
		return false;
	}

	// Cannot sprint if stamina is at 0
	if (GetStaminaPct() <= 0.f)
	{
		return false;
	}

	// Cannot start to sprint if stamina is below threshold
	if (!IsSprinting() && GetStaminaPct() < StartSprintStaminaPct)
	{
		return false;
	}

	// Cannot sprint if in an invalid movement mode
	if (!IsFalling() && !IsMovingOnGround())
	{
		return false;
	}

	// Cannot sprint backwards or sideways (ish)
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


void UPredMovement::SetStamina(float NewStamina)
{
	const float PrevStamina = Stamina;
	Stamina = FMath::Clamp(NewStamina, 0.f, MaxStamina);
	if (CharacterOwner != nullptr)
	{
		if (!FMath::IsNearlyEqual(PrevStamina, Stamina))
		{
			OnStaminaChanged(PrevStamina, Stamina);
		}
	}
}

void UPredMovement::SetMaxStamina(float NewMaxStamina)
{
	const float PrevMaxStamina = MaxStamina;
	MaxStamina = FMath::Max(0.f, NewMaxStamina);
	if (CharacterOwner != nullptr)
	{
		if (!FMath::IsNearlyEqual(PrevMaxStamina, MaxStamina))
		{
			OnMaxStaminaChanged(PrevMaxStamina, MaxStamina);
		}
	}
}

void UPredMovement::SetStaminaDrained(bool bNewValue)
{
	const bool bWasStaminaDrained = bStaminaDrained;
	bStaminaDrained = bNewValue;
	if (CharacterOwner != nullptr)
	{
		if (bWasStaminaDrained != bStaminaDrained)
		{
			if (bStaminaDrained)
			{
				OnStaminaDrained();
			}
			else
			{
				OnStaminaDrainRecovered();
			}
		}
	}
}

void UPredMovement::OnStaminaChanged(float PrevValue, float NewValue)
{
	if (IsValid(PredCharacterOwner))
	{
		PredCharacterOwner->OnStaminaChanged.Broadcast(NewValue, PrevValue);
	}
	
	if (FMath::IsNearlyZero(Stamina))
	{
		Stamina = 0.f;
		if (!bStaminaDrained)
		{
			SetStaminaDrained(true);
		}
	}
	else if (bStaminaDrained && IsStaminaRecovered())
	{
		SetStaminaDrained(false);
	}
	else if (FMath::IsNearlyEqual(Stamina, MaxStamina))
	{
		Stamina = MaxStamina;
		if (bStaminaDrained)
		{
			SetStaminaDrained(false);
		}
	}
}

void UPredMovement::OnMaxStaminaChanged(float PrevValue, float NewValue)
{
	if (IsValid(PredCharacterOwner))
	{
		PredCharacterOwner->OnMaxStaminaChanged.Broadcast(NewValue, PrevValue);
	}

	// Ensure that Stamina is within the new MaxStamina
	SetStamina(GetStamina());
}

void UPredMovement::OnStaminaDrained()
{
	UnSprint();
	
	if (IsValid(PredCharacterOwner))
	{
		PredCharacterOwner->OnStaminaDrained.Broadcast();
	}
}

void UPredMovement::OnStaminaDrainRecovered()
{
	if (IsValid(PredCharacterOwner))
	{
		PredCharacterOwner->OnStaminaDrainRecovered.Broadcast();
	}
}

bool UPredMovement::IsAimingDownSights() const
{
	return PredCharacterOwner && PredCharacterOwner->IsAimingDownSights();
}

void UPredMovement::AimDownSights(bool bClientSimulation)
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
		PredCharacterOwner->SetIsAimingDownSights(true);
	}
	PredCharacterOwner->OnStartAimDownSights();
}

void UPredMovement::UnAimDownSights(bool bClientSimulation)
{
	if (!HasValidData())
	{
		return;
	}

	if (!bClientSimulation)
	{
		PredCharacterOwner->SetIsAimingDownSights(false);
	}
	PredCharacterOwner->OnEndAimDownSights();
}

bool UPredMovement::CanAimDownSightsInCurrentState() const
{
	return (IsFalling() || IsMovingOnGround()) && UpdatedComponent && !UpdatedComponent->IsSimulatingPhysics();
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
		
		// Check for a change in AimDownSights state. Players toggle AimDownSights by changing bWantsToAimDownSights.
		const bool bIsAimingDownSights = IsAimingDownSights();
		if (bIsAimingDownSights && (!bWantsToAimDownSights || !CanAimDownSightsInCurrentState()))
		{
			UnAimDownSights(false);
		}
		else if (!bIsAimingDownSights && bWantsToAimDownSights && CanAimDownSightsInCurrentState())
		{
			AimDownSights(false);
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
		
		// UnAimDownSights if no longer allowed to be AimingDownSights
		if (IsAimingDownSights() && !CanAimDownSightsInCurrentState())
		{
			UnAimDownSights(false);
		}
	}

	Super::UpdateCharacterStateAfterMovement(DeltaSeconds);
	
#if !UE_BUILD_SHIPPING
	// Draw Stamina values to Screen
	if (GEngine && PredMovementCVars::DrawStaminaValues > 0)
	{
		const uint32 DebugKey = (CharacterOwner->GetUniqueID() + 74290) % UINT32_MAX;
		if (CharacterOwner->HasAuthority() && (PredMovementCVars::DrawStaminaValues == 1 || PredMovementCVars::DrawStaminaValues == 3))
		{
			const uint64 AuthDebugKey = DebugKey + 1;
			GEngine->AddOnScreenDebugMessage(AuthDebugKey, 1.f, FColor::Orange, FString::Printf(TEXT("[Authority] Stamina %f    Drained %d"), GetStamina(), IsStaminaDrained()));
		}
		else if (CharacterOwner->IsLocallyControlled() && (PredMovementCVars::DrawStaminaValues == 1 || PredMovementCVars::DrawStaminaValues == 2))
		{
			const uint64 LocalDebugKey = DebugKey + 2;
			GEngine->AddOnScreenDebugMessage(LocalDebugKey, 1.f, FColor::Yellow, FString::Printf(TEXT("[Local] Stamina %f    Drained %d"), GetStamina(), IsStaminaDrained()));
		}
	}
#endif
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
	const bool bRealAimDownSights = bWantsToAimDownSights;
	const bool bResult = Super::ClientUpdatePositionAfterServerUpdate();
	bWantsToSprint = bRealSprint;
	bWantsToAimDownSights = bRealAimDownSights;

	return bResult;
}

void UPredMovement::OnClientCorrectionReceived(class FNetworkPredictionData_Client_Character& ClientData,
	float TimeStamp, FVector NewLocation, FVector NewVelocity, UPrimitiveComponent* NewBase, FName NewBaseBoneName,
	bool bHasBase, bool bBaseRelativePosition, uint8 ServerMovementMode
#if UE_5_03_OR_LATER
	, FVector ServerGravityDirection
#endif
	)
{
	// This occurs on AutonomousProxy, when the server sends the move response
	// This is where we receive the snare, and can override the server's location, assuming it has given us authority

	// Server >> SendClientAdjustment() ➜ ServerSendMoveResponse ➜ ServerFillResponseData() + MoveResponsePacked_ServerSend() >> Client
	// >> ClientMoveResponsePacked() ➜ ClientHandleMoveResponse() ➜ ClientAdjustPosition_Implementation() ➜ OnClientCorrectionReceived
	
	const FPredMoveResponseDataContainer& PredMoveResponse = static_cast<const FPredMoveResponseDataContainer&>(GetMoveResponseDataContainer());

	SetStamina(PredMoveResponse.Stamina);
	SetStaminaDrained(PredMoveResponse.bStaminaDrained);
	
	Super::OnClientCorrectionReceived(ClientData, TimeStamp, NewLocation, NewVelocity, NewBase, NewBaseBoneName,
		bHasBase, bBaseRelativePosition, ServerMovementMode, ServerGravityDirection);
}

bool UPredMovement::ServerCheckClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel,
	const FVector& ClientWorldLocation, const FVector& RelativeClientLocation, UPrimitiveComponent* ClientMovementBase,
	FName ClientBaseBoneName, uint8 ClientMovementMode)
{
	// ServerMovePacked_ServerReceive ➜ ServerMove_HandleMoveData ➜ ServerMove_PerformMovement
	// ➜ ServerMoveHandleClientError ➜ ServerCheckClientError
	
	if (Super::ServerCheckClientError(ClientTimeStamp, DeltaTime, Accel, ClientWorldLocation, RelativeClientLocation, ClientMovementBase, ClientBaseBoneName, ClientMovementMode))
	{
		return true;
	}
    
	/*
	 * This will trigger a client correction if the Stamina value in the Client differs
	 * NetworkStaminaCorrectionThreshold (2.f default) units from the one in the server
	 * De-syncs can happen if we set the Stamina directly in Gameplay code (ie: GAS)
	 */
	const FPredNetworkMoveData* CurrentMoveData = static_cast<const FPredNetworkMoveData*>(GetCurrentNetworkMoveData());
	if (!FMath::IsNearlyEqual(CurrentMoveData->Stamina, Stamina, NetworkStaminaCorrectionThreshold))
	{
		return true;
	}
    
	return false;
}

void UPredMovement::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	bWantsToAimDownSights = ((Flags & FSavedMove_Character::FLAG_Reserved_2) != 0);
}

uint8 FSavedMove_Character_Pred::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();

	if (bWantsToAimDownSights)
	{
		Result |= FLAG_Reserved_2;
	}

	return Result;
}

void FSavedMove_Character_Pred::Clear()
{
	Super::Clear();

	bWantsToAimDownSights = false;

	bWantsToSprint = false;
	
	bStaminaDrained = false;
	StartStamina = 0.f;
	EndStamina = 0.f;
}

void FSavedMove_Character_Pred::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel,
	FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	if (UPredMovement* MoveComp = C ? Cast<UPredMovement>(C->GetCharacterMovement()) : nullptr)
	{
		bWantsToSprint = MoveComp->bWantsToSprint;
		bWantsToAimDownSights = MoveComp->bWantsToAimDownSights;
	}
}

bool FSavedMove_Character_Pred::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter,
	float MaxDelta) const
{
	const TSharedPtr<FSavedMove_Character_Pred>& SavedMove = StaticCastSharedPtr<FSavedMove_Character_Pred>(NewMove);

	if (bStaminaDrained != SavedMove->bStaminaDrained)
	{
		return false;
	}

	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void FSavedMove_Character_Pred::CombineWith(const FSavedMove_Character* OldMove, ACharacter* C,
	APlayerController* PC, const FVector& OldStartLocation)
{
	Super::CombineWith(OldMove, C, PC, OldStartLocation);

	const FSavedMove_Character_Pred* SavedOldMove = static_cast<const FSavedMove_Character_Pred*>(OldMove);

	if (UPredMovement* MoveComp = C ? Cast<UPredMovement>(C->GetCharacterMovement()) : nullptr)
	{
		MoveComp->SetStamina(SavedOldMove->StartStamina);
		MoveComp->SetStaminaDrained(SavedOldMove->bStaminaDrained);
	}
}

void FSavedMove_Character_Pred::SetInitialPosition(ACharacter* C)
{
	Super::SetInitialPosition(C);

	if (const UPredMovement* MoveComp = C ? Cast<UPredMovement>(C->GetCharacterMovement()) : nullptr)
	{
		bStaminaDrained = MoveComp->IsStaminaDrained();
		StartStamina = MoveComp->GetStamina();
	}
}

void FSavedMove_Character_Pred::PostUpdate(ACharacter* C, EPostUpdateMode PostUpdateMode)
{
	// When considering whether to delay or combine moves, we need to compare the move at the start and the end
	if (UPredMovement* MoveComp = C ? Cast<UPredMovement>(C->GetCharacterMovement()) : nullptr)
	{
		EndStamina = MoveComp->GetStamina();
	
		if (PostUpdateMode == PostUpdate_Record)
		{
			// Don't combine moves if the modifiers changed over the course of the move
			if (bStaminaDrained != MoveComp->IsStaminaDrained())
			{
				bForceNoCombine = true;
			}
		}
	}

	Super::PostUpdate(C, PostUpdateMode);
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
