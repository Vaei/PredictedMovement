// Copyright (c) 2023 Jared Taylor. All Rights Reserved.


#include "Modifier/ModifierMovement.h"

#include "Modifier/ModifierCharacter.h"
#include "Net/NetPing.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModifierMovement)

namespace FModifierTags
{
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Buff_SlowFall, "Modifier.Type.Buff.SlowFall");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Buff_SlowFall_25, "Modifier.Type.Buff.SlowFall.25");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Buff_SlowFall_50, "Modifier.Type.Buff.SlowFall.50");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Buff_SlowFall_75, "Modifier.Type.Buff.SlowFall.75");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Buff_SlowFall_100, "Modifier.Type.Buff.SlowFall.100");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Buff_Boost, "Modifier.Type.Buff.Boost");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Buff_Boost_25, "Modifier.Type.Buff.Boost.25");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Buff_Boost_50, "Modifier.Type.Buff.Boost.50");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Buff_Boost_75, "Modifier.Type.Buff.Boost.75");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Debuff_Snare, "Modifier.Type.Debuff.Snare");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Debuff_Snare_25, "Modifier.Type.Debuff.Snare.25");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Debuff_Snare_50, "Modifier.Type.Debuff.Snare.50");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Debuff_Snare_75, "Modifier.Type.Debuff.Snare.75");
}

namespace FModifierCVars
{
	static int32 ClientAuthEnableOverride = 0;
	FAutoConsoleVariableRef CVarClientAuthOverride(
		TEXT("p.ClientAuth.Enable.Override"),
		ClientAuthEnableOverride,
		TEXT("Override client authority enabled.\n")
		TEXT("0: Ignore Override, 1: Override to Enable, 2: Override to Disable"),
		ECVF_Default);
	
	static float ClientAuthTimeOverride = 0.f;
	FAutoConsoleVariableRef CVarClientAuthTimeOverride(
		TEXT("p.ClientAuth.Time.Override"),
		ClientAuthTimeOverride,
		TEXT("Override client authority time.\n")
		TEXT("Set 0 to disable the override and use character movement defaults"),
		ECVF_Default);
}

void FModifierMoveResponseDataContainer::ServerFillResponseData(
	const UCharacterMovementComponent& CharacterMovement, const FClientAdjustment& PendingAdjustment)
{
	Super::ServerFillResponseData(CharacterMovement, PendingAdjustment);

	// Server ➜ Client
	const UModifierMovement* MoveComp = Cast<UModifierMovement>(&CharacterMovement);
	if (Snare != MoveComp->Snare)
	{
		Snare = MoveComp->Snare;
		ClientAuthStack = MoveComp->ClientAuthStack;
	}
}

bool FModifierMoveResponseDataContainer::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar,
	UPackageMap* PackageMap)
{
	if (!Super::Serialize(CharacterMovement, Ar, PackageMap))
	{
		return false;
	}

	// Server ➜ Client
	if (IsCorrection())
	{
		bool bOutSuccess;
		Snare.NetSerialize(Ar, PackageMap, bOutSuccess);
		ClientAuthStack.NetSerialize(Ar, PackageMap, bOutSuccess);
	}

	return !Ar.IsError();
}

void FModifierNetworkMoveData::ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType)
{
    Super::ClientFillNetworkMoveData(ClientMove, MoveType);

	// Client ➜ Server
	
	// CallServerMovePacked ➜ ClientFillNetworkMoveData ➜ ServerMovePacked_ClientSend >> Server
	// >> ServerMovePacked_ServerReceive ➜ ServerMove_HandleMoveData ➜ ServerMove_PerformMovement
	// ➜ MoveAutonomous (UpdateFromCompressedFlags)
	const FSavedMove_Character_Modifier& SavedMove = static_cast<const FSavedMove_Character_Modifier&>(ClientMove);
	Boost = SavedMove.Boost;
	SlowFall = SavedMove.SlowFall;
	Snare = SavedMove.Snare;
}

bool FModifierNetworkMoveData::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType)
{
    Super::Serialize(CharacterMovement, Ar, PackageMap, MoveType);

	// Client ➜ Server
	bool bOutSuccess;
	Boost.NetSerialize(Ar, PackageMap, bOutSuccess);
	SlowFall.NetSerialize(Ar, PackageMap, bOutSuccess);
	Snare.NetSerialize(Ar, PackageMap, bOutSuccess);

    return !Ar.IsError();
}

UModifierMovement::UModifierMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetMoveResponseDataContainer(ModifierMoveResponseDataContainer);
    SetNetworkMoveDataContainer(ModifierMoveDataContainer);

	// Init data types
	Boost.LevelType = EModifierLevelType::FGameplayTag;
	SlowFall.LevelType = EModifierLevelType::FGameplayTag;
	Snare.LevelType = EModifierLevelType::FGameplayTag;

	// Init boost levels
	Boost.ModifierLevelTags.AddTagFast(FModifierTags::Modifier_Type_Buff_Boost_25);
	Boost.ModifierLevelTags.AddTagFast(FModifierTags::Modifier_Type_Buff_Boost_50);
	Boost.ModifierLevelTags.AddTagFast(FModifierTags::Modifier_Type_Buff_Boost_75);

	BoostLevels.Add(FModifierTags::Modifier_Type_Buff_Boost_25, { 1.25f });
	BoostLevels.Add(FModifierTags::Modifier_Type_Buff_Boost_50, { 1.50f });
	BoostLevels.Add(FModifierTags::Modifier_Type_Buff_Boost_75, { 1.75f });

	// Init slow fall levels
	SlowFall.ModifierLevelTags.AddTagFast(FModifierTags::Modifier_Type_Buff_SlowFall_25);
	SlowFall.ModifierLevelTags.AddTagFast(FModifierTags::Modifier_Type_Buff_SlowFall_50);
	SlowFall.ModifierLevelTags.AddTagFast(FModifierTags::Modifier_Type_Buff_SlowFall_75);
	SlowFall.ModifierLevelTags.AddTagFast(FModifierTags::Modifier_Type_Buff_SlowFall_100);

	SlowFallLevels.Add(FModifierTags::Modifier_Type_Buff_SlowFall_25, { 0.75f });
	SlowFallLevels.Add(FModifierTags::Modifier_Type_Buff_SlowFall_50, { 0.50f });
	SlowFallLevels.Add(FModifierTags::Modifier_Type_Buff_SlowFall_75, { 0.25f });
	SlowFallLevels.Add(FModifierTags::Modifier_Type_Buff_SlowFall_100, { 0.00f });
	
	// Init snare levels
	Snare.ModifierLevelTags.AddTagFast(FModifierTags::Modifier_Type_Debuff_Snare_25);
	Snare.ModifierLevelTags.AddTagFast(FModifierTags::Modifier_Type_Debuff_Snare_50);
	Snare.ModifierLevelTags.AddTagFast(FModifierTags::Modifier_Type_Debuff_Snare_75);
	
	SnareLevels.Add(FModifierTags::Modifier_Type_Debuff_Snare_25, { 0.75f });
	SnareLevels.Add(FModifierTags::Modifier_Type_Debuff_Snare_50, { 0.50f });
	SnareLevels.Add(FModifierTags::Modifier_Type_Debuff_Snare_75, { 0.25f });
}

void UModifierMovement::PostLoad()
{
	Super::PostLoad();
	SetUpdatedCharacter();
}

void UModifierMovement::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	Super::SetUpdatedComponent(NewUpdatedComponent);
	SetUpdatedCharacter();
}

void UModifierMovement::SetUpdatedCharacter()
{
	ModifierCharacterOwner = Cast<AModifierCharacter>(PawnOwner);
	Boost.Initialize(ModifierCharacterOwner, FModifierTags::Modifier_Type_Buff_Boost);
	SlowFall.Initialize(ModifierCharacterOwner, FModifierTags::Modifier_Type_Buff_SlowFall);
	Snare.Initialize(ModifierCharacterOwner, FModifierTags::Modifier_Type_Debuff_Snare);
}

bool UModifierMovement::CanBoostInCurrentState() const
{
	if (!UpdatedComponent || UpdatedComponent->IsSimulatingPhysics())
	{
		return false;
	}

	return true;
}

bool UModifierMovement::CanSlowFallInCurrentState() const
{
	if (!UpdatedComponent || UpdatedComponent->IsSimulatingPhysics())
	{
		return false;
	}

	return IsFalling();
}

void UModifierMovement::OnStartSlowFall()
{
	if (const FFallingModifierParams* Params = GetSlowFallLevelParams())
	{
		if (Params->bRemoveVelocityZOnStart)
		{
			Velocity.X = 0.f;
			Velocity.Y = 0.f;
		}
	}
}

bool UModifierMovement::CanBeSnaredInCurrentState() const
{
	if (!UpdatedComponent || UpdatedComponent->IsSimulatingPhysics())
	{
		return false;
	}

	return true;
}

void FSavedMove_Character_Modifier::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel,
	class FNetworkPredictionData_Client_Character& ClientData)
{
	FSavedMove_Character::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	if (UModifierMovement* Movement = CastChecked<AModifierCharacter>(C)->GetModifierCharacterMovement())
	{
		Boost = Movement->Boost;
		SlowFall = Movement->SlowFall;
	}
}

bool FSavedMove_Character_Modifier::IsImportantMove(const FSavedMovePtr& LastAckedMove) const
{
	const TSharedPtr<FSavedMove_Character_Modifier>& SavedMove = StaticCastSharedPtr<FSavedMove_Character_Modifier>(LastAckedMove);
	if (Boost != SavedMove->Boost)
	{
		return true;
	}

	if (SlowFall != SavedMove->SlowFall)
	{
		return true;
	}

	if (Snare != SavedMove->Snare)
	{
		return true;
	}

	return FSavedMove_Character::IsImportantMove(LastAckedMove);
}

bool FSavedMove_Character_Modifier::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter,
	float MaxDelta) const
{
	const TSharedPtr<FSavedMove_Character_Modifier>& SavedMove = StaticCastSharedPtr<FSavedMove_Character_Modifier>(NewMove);
	
	if (Snare != SavedMove->Snare)
	{
		return false;
	}

	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void FSavedMove_Character_Modifier::CombineWith(const FSavedMove_Character* OldMove, ACharacter* C,
	APlayerController* PC, const FVector& OldStartLocation)
{
	// Client only
	// ControlledCharacterMove ➜ ReplicateMoveToServer ➜ CombineWith()

	Super::CombineWith(OldMove, C, PC, OldStartLocation);

	const FSavedMove_Character_Modifier* SavedOldMove = static_cast<const FSavedMove_Character_Modifier*>(OldMove);

	if (UModifierMovement* MoveComp = C ? Cast<UModifierMovement>(C->GetCharacterMovement()) : nullptr)
	{
		MoveComp->Snare << SavedOldMove->Snare;
	}
}

void FSavedMove_Character_Modifier::Clear()
{
	Super::Clear();

	Boost = {};
	SlowFall = {};
	Snare = {};
}

void FSavedMove_Character_Modifier::SetInitialPosition(ACharacter* C)
{
	Super::SetInitialPosition(C);

	if (const UModifierMovement* MoveComp = C ? Cast<UModifierMovement>(C->GetCharacterMovement()) : nullptr)
	{
		Snare = MoveComp->Snare;
	}
}

float UModifierMovement::GetRootMotionTranslationScalar() const
{
	// Allowing boost to affect root motion will increase attack range, dodge range, etc., it is disabled by default
	return (bSnareAffectsRootMotion ? GetSnareSpeedScalar() : 1.f) * (bBoostAffectsRootMotion ? GetBoostSpeedScalar() : 1.f);
}

float UModifierMovement::GetMaxSpeed() const
{
	return Super::GetMaxSpeed() * GetMaxSpeedScalar();
}

float UModifierMovement::GetMaxAcceleration() const
{
	return Super::GetMaxAcceleration() * GetMaxAccelScalar();
}

float UModifierMovement::GetMaxBrakingDeceleration() const
{
	return Super::GetMaxBrakingDeceleration() * GetMaxBrakingScalar();
}

float UModifierMovement::GetGroundFriction() const
{
	return GroundFriction * GetMaxGroundFrictionScalar();
}

float UModifierMovement::GetBrakingFriction() const
{
	return BrakingFriction * GetBrakingFrictionScalar();
}

void UModifierMovement::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	if (IsMovingOnGround())
	{
		Friction = GetGroundFriction();
	}
	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
}

void UModifierMovement::ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration)
{
	if (IsMovingOnGround())
	{
		Friction = bUseSeparateBrakingFriction ? GetBrakingFriction() : GetGroundFriction();
	}
	Super::ApplyVelocityBraking(DeltaTime, Friction, BrakingDeceleration);
}

float UModifierMovement::GetGravityZ() const
{
	// Slow fall gravity
	float GravityScalar = 1.f;
	if (const FFallingModifierParams* Params = GetSlowFallLevelParams())
	{
		GravityScalar = Params->GetGravityScalar(Velocity);
	}
	
	return Super::GetGravityZ() * GravityScalar;
}

FVector UModifierMovement::GetAirControl(float DeltaTime, float TickAirControl, const FVector& FallAcceleration)
{
	// Slow fall air control
	if (const FFallingModifierParams* Params = GetSlowFallLevelParams())
	{
		TickAirControl = Params->GetAirControl(TickAirControl);
	}
	
	return Super::GetAirControl(DeltaTime, TickAirControl, FallAcceleration);
}

void UModifierMovement::UpdateCharacterStateBeforeMovement(float DeltaTime)
{
	if (!ModifierCharacterOwner)
	{
		return;
	}

	if (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		if (IsBoosted() && !CanBoostInCurrentState())
		{
			ModifierCharacterOwner->RemoveAllBoosts();
		}
		Boost.PreUpdateModifierLevel();

		if (IsSlowFall() && !CanSlowFallInCurrentState())
		{
			ModifierCharacterOwner->RemoveAllSlowFall();
		}
		SlowFall.PreUpdateModifierLevel();

		if (IsSnared() && !CanBeSnaredInCurrentState())
		{
			ModifierCharacterOwner->RemoveAllSnares();
		}
		Snare.PreUpdateModifierLevel();
	}
	
	Super::UpdateCharacterStateBeforeMovement(DeltaTime);
}

void UModifierMovement::UpdateCharacterStateAfterMovement(float DeltaTime)
{
	if (ModifierCharacterOwner && CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		if (IsBoosted() && !CanBoostInCurrentState())
		{
			ModifierCharacterOwner->RemoveAllBoosts();
			Boost.PostUpdateModifierLevel();
		}

		if (IsSlowFall() && !CanSlowFallInCurrentState())
		{
			ModifierCharacterOwner->RemoveAllSlowFall();
			SlowFall.PostUpdateModifierLevel();
		}

		if (IsSnared() && !CanBeSnaredInCurrentState())
		{
			ModifierCharacterOwner->RemoveAllSnares();
			Snare.PostUpdateModifierLevel();
		}
	}
	
	Super::UpdateCharacterStateAfterMovement(DeltaTime);
}

float UModifierMovement::GetClientAuthTime() const
{
	if (FModifierCVars::ClientAuthTimeOverride > 0.f)
	{
		return FModifierCVars::ClientAuthTimeOverride;
	}
	return ClientAuthTime;
}

bool UModifierMovement::IsClientAuthEnabled() const
{
	if (FModifierCVars::ClientAuthEnableOverride > 0)
	{
		return FModifierCVars::ClientAuthEnableOverride == 1;
	}
	return bEnableClientAuth;
}

void UModifierMovement::InitClientAuth(FGameplayTag ClientAuthSource, float OverrideDuration)
{
	const float Duration = OverrideDuration > 0.f ? OverrideDuration : GetClientAuthTime();
	ClientAuthStack.Stack.Add(FClientAuthData(ClientAuthSource, Duration));
}

bool UModifierMovement::ServerShouldGrantClientPositionAuthority(FVector& ClientLoc)
{
	// Already ignoring client movement error checks and correction
	if (bIgnoreClientMovementErrorChecksAndCorrection)
	{
		return false;
	}

	// Abort if client authority is not enabled
	if (!IsClientAuthEnabled())
	{
		return false;
	}

	// Get auth data
	FClientAuthData* ClientAuthData = GetClientAuthData();
	if (!ClientAuthData)
	{
		// No auth data, can't do anything
		return false;
	}

	// Validate auth data
#if !UE_BUILD_SHIPPING
	if (UNLIKELY(ClientAuthData->TimeRemaining <= 0.f))
	{
		// ServerMoveHandleClientError() should have removed the auth data already
		ensure(false);
		return false;
	}
#endif
	
	// Reset alpha, we're going to calculate it now
	ClientAuthData->Alpha = 0.f;

	// How far the client is from the server
	const FVector ServerLoc = UpdatedComponent->GetComponentLocation();
	FVector LocDiff = ServerLoc - ClientLoc;

	// No change or almost no change occurred
	if (LocDiff.IsNearlyZero())
	{
		// Grant full authority
		ClientAuthData->Alpha = 1.f;
		return true;
	}

	// If the client is too far away from the server, reject the client position entirely, potential cheater
	const float RejectDistance = GetRejectClientAuthDistance();
	if (LocDiff.SizeSquared() >= FMath::Square(RejectDistance))
	{
		OnClientAuthRejected(ClientLoc, ServerLoc, LocDiff);
		return false;
	}

	// If the client is not within the maximum allowable distance, accept the client position, but only partially
	if (LocDiff.Size() >= GetMaxClientAuthDistance())
	{
		// Accept only a portion of the client's location
		ClientAuthData->Alpha = GetMaxClientAuthDistance() / LocDiff.Size();
		ClientLoc = FMath::Lerp<FVector>(ServerLoc, ClientLoc, ClientAuthData->Alpha);
		LocDiff = ServerLoc - ClientLoc;
	}
	else
	{
		// Accept full client location
		ClientAuthData->Alpha = 1.f;
	}

	return true;
}

bool UModifierMovement::ClientUpdatePositionAfterServerUpdate()
{
	const FMovementModifier SavedBoost = Boost;
	const FMovementModifier SavedSlowFall = SlowFall;
	const FMovementModifier SavedSnare = Snare;
	bool bResult = Super::ClientUpdatePositionAfterServerUpdate();
	Boost = SavedBoost;
	SlowFall = SavedSlowFall;
	Snare = SavedSnare;
	
	return bResult;
}

void UModifierMovement::ServerMove_PerformMovement(const FCharacterNetworkMoveData& MoveData)
{
	// Server updates from the client's move data
	// Equivalent to UpdateFromCompressedFlags() when using Move Containers instead

	// Client >> CallServerMovePacked ➜ ClientFillNetworkMoveData ➜ ServerMovePacked_ClientSend >> Server
	// >> ServerMovePacked_ServerReceive ➜ ServerMove_HandleMoveData ➜ ServerMove_PerformMovement
	
	const FModifierNetworkMoveData& ModifierMoveData = static_cast<const FModifierNetworkMoveData&>(MoveData);

	// << operator copies only the RequestedModifierLevel and Modifiers stack, i.e. requested state not current state
	Boost << ModifierMoveData.Boost;
	SlowFall << ModifierMoveData.SlowFall;

	Super::ServerMove_PerformMovement(MoveData);
}

void UModifierMovement::ClientHandleMoveResponse(const FCharacterMoveResponseDataContainer& MoveResponse)
{
	// This occurs on AutonomousProxy, when the server sends the move response
	
	// This is where we receive the snare, and can override the server's location, assuming it has given us authority

	// Server >> SendClientAdjustment() ➜ ServerSendMoveResponse
	// ➜ ServerFillResponseData() + MoveResponsePacked_ServerSend() >> Client
	// >> ClientMoveResponsePacked() ➜ ClientHandleMoveResponse()

	const FModifierMoveResponseDataContainer& ModifierMoveResponse = static_cast<const FModifierMoveResponseDataContainer&>(MoveResponse);

	// Handle Snare & Client Authority
	ClientAuthStack = ModifierMoveResponse.ClientAuthStack;
	if (FClientAuthData* ClientAuthData = GetClientAuthData())
	{
		if (Snare != ModifierMoveResponse.Snare)
		{
			// Apply Snare correction
			Snare << ModifierMoveResponse.Snare;

			if (ClientAuthData)
			{
				if (FMath::IsNearlyZero(ClientAuthData->Alpha))
				{
					ClientAuthData->Alpha = 0.f;
				}
				else
				{
					FlushServerMoves();
				}
			}
		}
	
		if (ClientAuthData->Alpha > 0.f)
		{
			const bool bAdjustClientData = !MoveResponse.IsGoodMove();
			FNetworkPredictionData_Client_Character* ClientData = bAdjustClientData ? GetPredictionData_Client_Character() : nullptr;

			// Do not allow us to apply a partial move based on our own authoritative location that is in the past,
			// after server sends it back to us
			if (bAdjustClientData)
			{
				check(ClientData);
				if (ClientData->LastAckedMove.IsValid())
				{
					FCharacterMoveResponseDataContainer& MutableResponse = const_cast<FCharacterMoveResponseDataContainer&>(MoveResponse);
					ClientData->LastAckedMove->SavedLocation = MutableResponse.ClientAdjustment.NewLoc;
				}
			}

			// Cache the old location because the server will apply our own past authoritative location when calling Super!
			FVector OldLocation = UpdatedComponent->GetComponentLocation();
		
			Super::ClientHandleMoveResponse(MoveResponse);
		
			FVector NewLocation = UpdatedComponent->GetComponentLocation();

			// Clamp the location to the server's authoritative location based on the authority alpha
			const FVector ClampedLocation = FMath::Lerp<FVector>(OldLocation, NewLocation, 1.f - ClientAuthData->Alpha);
			UpdatedComponent->SetWorldLocation(ClampedLocation, false);

			// Block the next ClientUpdatePositionAfterServerUpdate() from replaying moves, this will overwrite our authority
			// @TODO Do we need to make sure it applies non-location moves?
			if (bAdjustClientData)
			{
				ClientData->bUpdatePosition = false;
			}
		}
		else
		{
			Super::ClientHandleMoveResponse(MoveResponse);
		}
	}
	else
	{
		// Apply Snare correction
		Snare << ModifierMoveResponse.Snare;
		Super::ClientHandleMoveResponse(MoveResponse);
	}
}

bool UModifierMovement::ServerCheckClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel, const FVector& ClientWorldLocation, const FVector& RelativeClientLocation, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode)
{
	// Client >> TickComponent ➜ ControlledCharacterMove ➜ CallServerMove ➜ ReplicateMoveToServer >> Server
	// >> ServerMove ➜ ServerMoveHandleClientError ➜ ServerCheckClientError
	
    if (Super::ServerCheckClientError(ClientTimeStamp, DeltaTime, Accel, ClientWorldLocation, RelativeClientLocation, ClientMovementBase, ClientBaseBoneName, ClientMovementMode))
    {
        return true;
    }

	// Trigger client correction if modifiers have different ModifierLevel or Modifiers Array
	// De-syncs can happen when we set the Modifier directly in Gameplay code (i.e. GAS)
	const FModifierNetworkMoveData* CurrentMoveData = static_cast<const FModifierNetworkMoveData*>(GetCurrentNetworkMoveData());

	if (CurrentMoveData->Snare != Snare)
	{
		return true;
	}
	
    return false;
}

void UModifierMovement::ServerMoveHandleClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel,
	const FVector& RelativeClientLocation, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName,
	uint8 ClientMovementMode)
{
	// This is the entry-point for determining how to handle client corrections; we can determine they are out of sync
	// and make any changes that suit our needs
	
	// Client >> TickComponent ➜ ControlledCharacterMove ➜ CallServerMove ➜ ReplicateMoveToServer >> Server
	// >> ServerMove ➜ ServerMoveHandleClientError

	const FModifierNetworkMoveData* CurrentMoveData = static_cast<const FModifierNetworkMoveData*>(GetCurrentNetworkMoveData());

	// Initialize client authority when the client is about to receive a correction that applies Snare
	if (IsClientAuthEnabled())
	{
		if (CurrentMoveData->Snare != Snare)
		{
			// Snare inits client authority here, however other states may want to do it elsewhere, this is not a requirement
			InitClientAuth(FModifierTags::Modifier_Type_Debuff_Snare);
		}

		// Update client authority time remaining
		ClientAuthStack.Update(DeltaTime);

		// Apply these thresholds to control client authority
		FVector ClientLoc = FRepMovement::RebaseOntoZeroOrigin(RelativeClientLocation, this);
		if (ServerShouldGrantClientPositionAuthority(ClientLoc))
		{
			// Apply client authoritative position directly -- Subsequent moves will resolve overlapping conditions
			UpdatedComponent->SetWorldLocation(ClientLoc, false);
		}
	}

	// The move prepared here will finally be sent in the next ReplicateMoveToServer()
	
	Super::ServerMoveHandleClientError(ClientTimeStamp, DeltaTime, Accel, RelativeClientLocation, ClientMovementBase,
		ClientBaseBoneName, ClientMovementMode);
}

FNetworkPredictionData_Client* UModifierMovement::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UModifierMovement* MutableThis = const_cast<UModifierMovement*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Character_Modifier(*this);
	}

	return ClientPredictionData;
}

FSavedMovePtr FNetworkPredictionData_Client_Character_Modifier::AllocateNewMove()
{
	return MakeShared<FSavedMove_Character_Modifier>();
}

void UModifierMovement::TickCharacterPose(float DeltaTime)
{
	/*
	 * ACharacter::GetAnimRootMotionTranslationScale() is non-virtual, so we have to duplicate the entire function.
	 * All we do here is scale CharacterOwner->GetAnimRootMotionTranslationScale() by GetRootMotionTranslationScalar()
	 * 
	 * This allows our snares to affect root motion.
	 */
	
	if (DeltaTime < MIN_TICK_TIME)
	{
		return;
	}

	check(CharacterOwner && CharacterOwner->GetMesh());
	USkeletalMeshComponent* CharacterMesh = CharacterOwner->GetMesh();

	// bAutonomousTickPose is set, we control TickPose from the Character's Movement and Networking updates, and bypass the Component's update.
	// (Or Simulating Root Motion for remote clients)
	CharacterMesh->bIsAutonomousTickPose = true;

	if (CharacterMesh->ShouldTickPose())
	{
		// Keep track of if we're playing root motion, just in case the root motion montage ends this frame.
		const bool bWasPlayingRootMotion = CharacterOwner->IsPlayingRootMotion();

		CharacterMesh->TickPose(DeltaTime, true);

		// Grab root motion now that we have ticked the pose
		if (CharacterOwner->IsPlayingRootMotion() || bWasPlayingRootMotion)
		{
			FRootMotionMovementParams RootMotion = CharacterMesh->ConsumeRootMotion();
			if (RootMotion.bHasRootMotion)
			{
				RootMotion.ScaleRootMotionTranslation(CharacterOwner->GetAnimRootMotionTranslationScale() * GetRootMotionTranslationScalar());
				RootMotionParams.Accumulate(RootMotion);
			}

#if !(UE_BUILD_SHIPPING)
			// Debugging
			{
				FAnimMontageInstance* RootMotionMontageInstance = CharacterOwner->GetRootMotionAnimMontageInstance();
				UE_LOG(LogRootMotion, Log, TEXT("UCharacterMovementComponent::TickCharacterPose Role: %s, RootMotionMontage: %s, MontagePos: %f, DeltaTime: %f, ExtractedRootMotion: %s, AccumulatedRootMotion: %s")
					, *UEnum::GetValueAsString(TEXT("Engine.ENetRole"), CharacterOwner->GetLocalRole())
					, *GetNameSafe(RootMotionMontageInstance ? RootMotionMontageInstance->Montage : NULL)
					, RootMotionMontageInstance ? RootMotionMontageInstance->GetPosition() : -1.f
					, DeltaTime
					, *RootMotion.GetRootMotionTransform().GetTranslation().ToCompactString()
					, *RootMotionParams.GetRootMotionTransform().GetTranslation().ToCompactString()
					);
			}
#endif // !(UE_BUILD_SHIPPING)
		}
	}

	CharacterMesh->bIsAutonomousTickPose = false;
}
