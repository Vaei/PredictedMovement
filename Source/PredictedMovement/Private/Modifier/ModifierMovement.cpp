// Copyright (c) 2023 Jared Taylor


#include "Modifier/ModifierMovement.h"

#include "Modifier/ModifierCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Net/NetPing.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModifierMovement)

DEFINE_LOG_CATEGORY_STATIC(LogModifierMovement, Log, All);

namespace FModifierTags
{
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Buff_SlowFall,		"Modifier.Type.Buff.SlowFall");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Buff_SlowFall_25,	"Modifier.Type.Buff.SlowFall.25");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Buff_SlowFall_50,	"Modifier.Type.Buff.SlowFall.50");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Buff_SlowFall_75,	"Modifier.Type.Buff.SlowFall.75");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Buff_SlowFall_100, "Modifier.Type.Buff.SlowFall.100");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Buff_Boost,		"Modifier.Type.Buff.Boost");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Buff_Boost_25,		"Modifier.Type.Buff.Boost.25");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Buff_Boost_50,		"Modifier.Type.Buff.Boost.50");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Buff_Boost_75,		"Modifier.Type.Buff.Boost.75");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Debuff_Snare,		"Modifier.Type.Debuff.Snare");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Debuff_Snare_25,	"Modifier.Type.Debuff.Snare.25");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Debuff_Snare_50,	"Modifier.Type.Debuff.Snare.50");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Debuff_Snare_75,	"Modifier.Type.Debuff.Snare.75");
}

namespace FModifierCVars
{
#if !UE_BUILD_SHIPPING
	static bool bClientAuthDisabled = false;
	FAutoConsoleVariableRef CVarClientAuthDisabled(
		TEXT("p.ClientAuth.Disabled"),
		bClientAuthDisabled,
		TEXT("Override client authority to disabled.\n")
		TEXT("If true, disable client authority"),
		ECVF_Default);
#endif
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
	}
	ClientAuthStack = MoveComp->ClientAuthStack;
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

	// Init activation sources
	Boost.ActivationSource		= EModifierActivationSource::LocalPredicted;		// We apply this to ourselves
	SlowFall.ActivationSource	= EModifierActivationSource::LocalPredicted;		// We apply this to ourselves
	Snare.ActivationSource		= EModifierActivationSource::ServerInitiated;		// Others apply this to us

	// Init data types used for levels as gameplay tags instead of enums
	Boost.LevelType		= EModifierLevelType::FGameplayTag;
	SlowFall.LevelType	= EModifierLevelType::FGameplayTag;
	Snare.LevelType		= EModifierLevelType::FGameplayTag;

	// Init boost levels
	Boost.ModifierLevelTags.AddTagFast(FModifierTags::Modifier_Type_Buff_Boost_25);		// 1.25x Speed Boost
	Boost.ModifierLevelTags.AddTagFast(FModifierTags::Modifier_Type_Buff_Boost_50); 		// 1.50x Speed Boost
	Boost.ModifierLevelTags.AddTagFast(FModifierTags::Modifier_Type_Buff_Boost_75); 		// 1.75x Speed Boost

	BoostLevels.Add(FModifierTags::Modifier_Type_Buff_Boost_25, { 1.25f });
	BoostLevels.Add(FModifierTags::Modifier_Type_Buff_Boost_50, { 1.50f });
	BoostLevels.Add(FModifierTags::Modifier_Type_Buff_Boost_75, { 1.75f });

	// Init slow fall levels
	SlowFall.ModifierLevelTags.AddTagFast(FModifierTags::Modifier_Type_Buff_SlowFall_25);	// 0.25x Reduction from 0.75x Gravity
	SlowFall.ModifierLevelTags.AddTagFast(FModifierTags::Modifier_Type_Buff_SlowFall_50); 	// 0.50x Reduction from 0.50x Gravity
	SlowFall.ModifierLevelTags.AddTagFast(FModifierTags::Modifier_Type_Buff_SlowFall_75); 	// 0.75x Reduction from 0.25x Gravity
	SlowFall.ModifierLevelTags.AddTagFast(FModifierTags::Modifier_Type_Buff_SlowFall_100); 	// 1.00x Reduction from 0.00x Gravity

	SlowFallLevels.Add(FModifierTags::Modifier_Type_Buff_SlowFall_25, { 0.75f });
	SlowFallLevels.Add(FModifierTags::Modifier_Type_Buff_SlowFall_50, { 0.50f });
	SlowFallLevels.Add(FModifierTags::Modifier_Type_Buff_SlowFall_75, { 0.25f });
	SlowFallLevels.Add(FModifierTags::Modifier_Type_Buff_SlowFall_100, { 0.00f });
	
	// Init snare levels
	Snare.ModifierLevelTags.AddTagFast(FModifierTags::Modifier_Type_Debuff_Snare_25); 	// 0.25x Speed Snare
	Snare.ModifierLevelTags.AddTagFast(FModifierTags::Modifier_Type_Debuff_Snare_50); 	// 0.50x Speed Snare
	Snare.ModifierLevelTags.AddTagFast(FModifierTags::Modifier_Type_Debuff_Snare_75); 	// 0.75x Speed Snare
	
	SnareLevels.Add(FModifierTags::Modifier_Type_Debuff_Snare_25, { 0.75f });
	SnareLevels.Add(FModifierTags::Modifier_Type_Debuff_Snare_50, { 0.50f });
	SnareLevels.Add(FModifierTags::Modifier_Type_Debuff_Snare_75, { 0.25f });

	// Auth params for Snare
	FClientAuthParams& SnareParams = ClientAuthParams.Add(FModifierTags::Modifier_Type_Debuff_Snare);
	SnareParams.bEnableClientAuth = true;
	SnareParams.MaxClientAuthDistance = 150.f;  // For something like a knockback, a greater distance would be sensible
	SnareParams.RejectClientAuthDistance = 800.f;
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

bool UModifierMovement::CanBoostInCurrentState(FGameplayTag ModifierLevel) const
{
	return UpdatedComponent && !UpdatedComponent->IsSimulatingPhysics();
}

bool UModifierMovement::CanSlowFallInCurrentState(FGameplayTag ModifierLevel) const
{
	if (!UpdatedComponent || UpdatedComponent->IsSimulatingPhysics())
	{
		return false;
	}

	// This is simplistic for demonstration purposes, but if a mage casts a 'slow fall' spell,
	// they should be able to slow fall regardless of their current state, consider returning true here
	return IsFalling();
}

void UModifierMovement::OnStartSlowFall()
{
	if (const FFallingModifierParams* Params = GetSlowFallLevelParams())
	{
		if (Params->bRemoveVelocityZOnStart)
		{
			Velocity.Z = 0.f;
		}
	}
}

bool UModifierMovement::CanBeSnaredInCurrentState(FGameplayTag ModifierLevel) const
{
	return UpdatedComponent && !UpdatedComponent->IsSimulatingPhysics();
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

void FSavedMove_Character_Modifier::PostUpdate(ACharacter* C, EPostUpdateMode PostUpdateMode)
{
	// When considering whether to delay or combine moves, we need to compare the move at the start and the end
	if (UModifierMovement* Movement = CastChecked<AModifierCharacter>(C)->GetModifierCharacterMovement())
	{
		EndBoost = Movement->Boost;
		EndSlowFall = Movement->SlowFall;
		EndSnare = Movement->Snare;
	}

	if (PostUpdateMode == PostUpdate_Record)
	{
		// Don't combine moves if the modifiers changed over the course of the move
		// ^= is the same as != but it also evaluates the current level rather than only the requested level

		if (Boost ^= EndBoost)
		{
			bForceNoCombine = true;
		}

		if (SlowFall ^= EndSlowFall)
		{
			bForceNoCombine = true;
		}

		if (Snare ^= EndSnare)
		{
			bForceNoCombine = true;
		}
	}
	
	Super::PostUpdate(C, PostUpdateMode);
}

bool FSavedMove_Character_Modifier::IsImportantMove(const FSavedMovePtr& LastAckedMove) const
{
	const TSharedPtr<FSavedMove_Character_Modifier>& SavedMove = StaticCastSharedPtr<FSavedMove_Character_Modifier>(LastAckedMove);
	if (Boost != SavedMove->Boost || Boost != SavedMove->EndBoost)
	{
		return true;
	}

	if (SlowFall != SavedMove->SlowFall || SlowFall != SavedMove->EndSlowFall)
	{
		return true;
	}

	if (Snare != SavedMove->Snare || Snare != SavedMove->EndSnare)
	{
		return true;
	}

	return Super::IsImportantMove(LastAckedMove);
}

bool FSavedMove_Character_Modifier::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter,
	float MaxDelta) const
{
	// We combine moves for the purpose of reducing the number of moves sent to the server, especially when exceeding
	// 60 fps (by default, see ClientNetSendMoveDeltaTime).
	// By combining moves, we can send fewer moves, but still have the same outcome.
	
	// If we didn't handle move combining, and then we used OnStartSprint() to modify our Velocity directly, it would
	// de-sync if we exceed 60fps. This is where move combining kicks in and starts using Pending Moves instead.
	
	// When combining moves, the PendingMove is passed into the NewMove. Locally, before sending a Move to the Server,
	// the AutonomousProxy Client will already have processed the current PendingMove (it's only pending for being sent,
	// not processed).

	// Since combining will happen before processing a move, PendingMove might end up being processed twice; once last
	// frame, and once as part of the new combined move.

	const TSharedPtr<FSavedMove_Character_Modifier>& SavedMove = StaticCastSharedPtr<FSavedMove_Character_Modifier>(NewMove);

	// We can only combine moves if they will result in the same state as if both moves were processed individually,
	// because the AutonomousProxy Client processes them individually prior to sending them to the server.
	
	if (Boost != SavedMove->Boost)
	{
		return false;
	}

	if (SlowFall != SavedMove->SlowFall)
	{
		return false;
	}
	
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
	// To counter the PendingMove potentially being processed twice, we need to make sure to reset the state of the CMC
	// back to the "InitialPosition" (state) it had before the PendingMove got processed.
	
	Super::SetInitialPosition(C);

	if (const UModifierMovement* MoveComp = C ? Cast<UModifierMovement>(C->GetCharacterMovement()) : nullptr)
	{
		// Retrieve the value from our CMC to revert the saved move value back to this.

		Boost = MoveComp->Boost;
		SlowFall = MoveComp->SlowFall;
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
	Boost.UpdateCharacterStateBeforeMovement(CanBoostInCurrentState(FGameplayTag::EmptyTag));
	SlowFall.UpdateCharacterStateBeforeMovement(CanSlowFallInCurrentState(FGameplayTag::EmptyTag));
	Snare.UpdateCharacterStateBeforeMovement(CanBeSnaredInCurrentState(FGameplayTag::EmptyTag));
	
	Super::UpdateCharacterStateBeforeMovement(DeltaTime);
}

void UModifierMovement::UpdateCharacterStateAfterMovement(float DeltaTime)
{
	Boost.UpdateCharacterStateAfterMovement(CanBoostInCurrentState(FGameplayTag::EmptyTag));
	SlowFall.UpdateCharacterStateAfterMovement(CanSlowFallInCurrentState(FGameplayTag::EmptyTag));
	Snare.UpdateCharacterStateAfterMovement(CanBeSnaredInCurrentState(FGameplayTag::EmptyTag));
	
	Super::UpdateCharacterStateAfterMovement(DeltaTime);
}

void UModifierMovement::InitClientAuth(FGameplayTag ClientAuthSource, float OverrideDuration)
{
	if (const FClientAuthParams* Params = GetClientAuthParams(ClientAuthSource))
	{
		if (Params->bEnableClientAuth)
		{
			const float Duration = OverrideDuration > 0.f ? OverrideDuration : Params->ClientAuthTime;
			ClientAuthStack.Stack.Add(FClientAuthData(ClientAuthSource, Duration));

			// Limit the number of auth data entries
			// IMPORTANT: We do not allow serializing more than 8, if this changes, update the serialization code too
			if (ClientAuthStack.Stack.Num() > 8)
			{
				ClientAuthStack.Stack.RemoveAt(0);
			}
		}
	}
	else
	{
#if !UE_BUILD_SHIPPING
		FMessageLog("PIE").Error(FText::FromString(FString::Printf(TEXT("ClientAuthSource '%s' not found in ClientAuthParams"), *ClientAuthSource.ToString())));
#else
		UE_LOG(LogModifierMovement, Error, TEXT("ClientAuthSource '%s' not found"), *ClientAuthSource.ToString());
#endif
	}
}

bool UModifierMovement::ServerShouldGrantClientPositionAuthority(FVector& ClientLoc)
{
	// Already ignoring client movement error checks and correction
	if (bIgnoreClientMovementErrorChecksAndCorrection)
	{
		return false;
	}

	// Abort if client authority is not enabled
#if !UE_BUILD_SHIPPING
	if (FModifierCVars::bClientAuthDisabled)
	{
		return false;
	}
#endif

	// Get auth data
	FClientAuthData* ClientAuthData = GetClientAuthData();
	if (!ClientAuthData)
	{
		// No auth data, can't do anything
		return false;
	}

	// Get auth params
	FClientAuthParams* Params = GetClientAuthParams(ClientAuthData->Source);
	if (!Params)
	{
		// No auth params, can't do anything
		return false;
	}

	if (!Params->bEnableClientAuth)
	{
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
	if (LocDiff.SizeSquared() >= FMath::Square(Params->RejectClientAuthDistance))
	{
		OnClientAuthRejected(ClientLoc, ServerLoc, LocDiff);
		return false;
	}

	// If the client is not within the maximum allowable distance, accept the client position, but only partially
	if (LocDiff.Size() >= Params->MaxClientAuthDistance)
	{
		// Accept only a portion of the client's location
		ClientAuthData->Alpha = Params->MaxClientAuthDistance / LocDiff.Size();
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
	
	// operator<< Will not copy the current level. We only take the requested level!
	Boost << SavedBoost;
	SlowFall << SavedSlowFall;
	Snare << SavedSnare;
	
	return bResult;
}

bool UModifierMovement::CanDelaySendingMove(const FSavedMovePtr& NewMove)
{
	// Don't delay sending moves if the modifiers changed over the course of the move
	// ^= is the same as != but it also evaluates the current level rather than only the requested level
	
	const TSharedPtr<FSavedMove_Character_Modifier>& SavedMove = StaticCastSharedPtr<FSavedMove_Character_Modifier>(NewMove);

	if (SavedMove->Boost ^= SavedMove->EndBoost)
	{
		return false;
	}

	if (SavedMove->SlowFall ^= SavedMove->EndSlowFall)
	{
		return false;
	}

	if (SavedMove->Snare ^= SavedMove->EndSnare)
	{
		return false;
	}
	
	return Super::CanDelaySendingMove(NewMove);
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
#if !UE_BUILD_SHIPPING
	if (!FModifierCVars::bClientAuthDisabled)
#endif
	{
		// Update client authority time remaining
		ClientAuthStack.Update(DeltaTime);

		if (CurrentMoveData->Snare != Snare)
		{
			// Snare inits client authority here, however other states may want to do it elsewhere, this is not a requirement
			InitClientAuth(FModifierTags::Modifier_Type_Debuff_Snare);
		}

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
