// Copyright (c) Jared Taylor


#include "Modifier/ModifierMovement.h"

#include "Modifier/ModifierCharacter.h"
#include "Modifier/ModifierTags.h"
#include "Components/SkeletalMeshComponent.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModifierMovement)

DEFINE_LOG_CATEGORY_STATIC(LogModifierMovement, Log, All);

namespace ModifierMovementCVars
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

UModifierMovement::UModifierMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetNetworkMoveDataContainer(ModifierMoveDataContainer);
	SetMoveResponseDataContainer(ModifierMoveResponseDataContainer);

	// Init Modifier Levels
	Boost.Add(FModifierTags::Modifier_Boost, { 1.50f });  // 50% Speed Boost
	Snare.Add(FModifierTags::Modifier_Snare, { 0.50f });  // 50% Speed Snare
	SlowFall.Add(FModifierTags::Modifier_SlowFall, { 0.1f });  // 90% Gravity Reduction

	// Auth params for Snare
	static constexpr int32 DefaultPriority = 5;
	ClientAuthParams.FindOrAdd(FModifierTags::ClientAuth_Snare, { DefaultPriority });
}

void FModifierMoveResponseDataContainer::ServerFillResponseData(const UCharacterMovementComponent& CharacterMovement,
	const FClientAdjustment& PendingAdjustment)
{
	Super::ServerFillResponseData(CharacterMovement, PendingAdjustment);

	// Server ➜ Client

	// Server >> APlayerController::SendClientAdjustment() ➜ SendClientAdjustment ➜ ServerSendMoveResponse ➜
	// ServerFillResponseData ➜ MoveResponsePacked_ServerSend >> Client 
	
	const UModifierMovement* MoveComp = Cast<UModifierMovement>(&CharacterMovement);

	// Fill the response data with the current modifier state
	BoostCorrection.ServerFillResponseData(MoveComp->BoostCorrection.Modifiers);
	BoostServer.ServerFillResponseData(MoveComp->BoostServer.Modifiers);
	SnareServer.ServerFillResponseData(MoveComp->SnareServer.Modifiers);

	// Fill ClientAuthAlpha
	ClientAuthAlpha = MoveComp->ClientAuthAlpha;
	bHasClientAuthAlpha = ClientAuthAlpha > 0.f;
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
		// Serialize Modifiers
		Ar << BoostCorrection.Modifiers;
		Ar << BoostServer.Modifiers;
		Ar << SnareServer.Modifiers;

		// Serialize ClientAuthAlpha
		Ar.SerializeBits(&bHasClientAuthAlpha, 1);
		if (bHasClientAuthAlpha)
		{
			Ar << ClientAuthAlpha;
		}
		else if (!Ar.IsSaving())
		{
			ClientAuthAlpha = 0.f;
		}
	}

	return !Ar.IsError();
}

void FModifierNetworkMoveData::ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove,
	ENetworkMoveType MoveType)
{
	// Client packs move data to send to the server
	// Use this instead of GetCompressedFlags()
	Super::ClientFillNetworkMoveData(ClientMove, MoveType);

	// Client ➜ Server
	
	// Client >> CallServerMovePacked ➜ ClientFillNetworkMoveData ➜ ServerMovePacked_ClientSend >> Server
	// >> ServerMovePacked_ServerReceive ➜ ServerMove_HandleMoveData ➜ ServerMove_PerformMovement
	// ➜ MoveAutonomous (UpdateFromCompressedFlags)
	
	const FSavedMove_Character_Modifier& SavedMove = static_cast<const FSavedMove_Character_Modifier&>(ClientMove);

	// Fill the Modifier data from the saved move
	BoostLocal.ClientFillNetworkMoveData(SavedMove.BoostLocal.WantsModifiers);
	BoostCorrection.ClientFillNetworkMoveData(SavedMove.BoostCorrection.WantsModifiers, SavedMove.BoostCorrection.Modifiers);
	BoostServer.ClientFillNetworkMoveData(SavedMove.BoostServer.Modifiers);
	SnareServer.ClientFillNetworkMoveData(SavedMove.SnareServer.Modifiers);
	SlowFallLocal.ClientFillNetworkMoveData(SavedMove.SlowFallLocal.WantsModifiers);
}

bool FModifierNetworkMoveData::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar,
	UPackageMap* PackageMap, ENetworkMoveType MoveType)
{  // Client ➜ Server
	Super::Serialize(CharacterMovement, Ar, PackageMap, MoveType);

	// Serialize Modifier data
	BoostLocal.Serialize(Ar, TEXT("BoostLocal"));
	BoostCorrection.Serialize(Ar, TEXT("BoostCorrection"));
	BoostServer.Serialize(Ar, TEXT("BoostServer"));
	SnareServer.Serialize(Ar, TEXT("SnareServer"));
	SlowFallLocal.Serialize(Ar, TEXT("SlowFallLocal"));

	return !Ar.IsError();
}

bool UModifierMovement::HasValidData() const
{
	return Super::HasValidData() && IsValid(ModifierCharacterOwner);
}

void UModifierMovement::PostLoad()
{
	Super::PostLoad();

	ModifierCharacterOwner = Cast<AModifierCharacter>(PawnOwner);
}

void UModifierMovement::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	Super::SetUpdatedComponent(NewUpdatedComponent);

	ModifierCharacterOwner = Cast<AModifierCharacter>(PawnOwner);
}

float UModifierMovement::GetMaxAcceleration() const
{
	return Super::GetMaxAcceleration() * GetBoostAccelScalar() * GetSnareAccelScalar();
}

float UModifierMovement::GetMaxSpeed() const
{
	return Super::GetMaxSpeed() * GetBoostSpeedScalar() * GetSnareSpeedScalar();
}

float UModifierMovement::GetMaxBrakingDeceleration() const
{
	return Super::GetMaxBrakingDeceleration() * GetBoostBrakingScalar() * GetSnareBrakingScalar();
}

float UModifierMovement::GetGroundFriction(float DefaultGroundFriction) const
{
	return GroundFriction * GetBoostGroundFrictionScalar() * GetSnareGroundFrictionScalar();
}

float UModifierMovement::GetBrakingFriction() const
{
	return BrakingFriction * GetBoostBrakingFrictionScalar() * GetSnareBrakingFrictionScalar();
}

float UModifierMovement::GetRootMotionTranslationScalar() const
{
	const float BoostScalar = BoostAffectsRootMotion() ? GetBoostSpeedScalar() : 1.f;
	const float SnareScalar = SnareAffectsRootMotion() ? GetSnareSpeedScalar() : 1.f;
	return BoostScalar * SnareScalar;
}

float UModifierMovement::GetGravityZ() const
{
	return Super::GetGravityZ() * GetSlowFallGravityZScalar();
}

FVector UModifierMovement::GetAirControl(float DeltaTime, float TickAirControl, const FVector& FallAcceleration)
{
	if (const FFallingModifierParams* SlowFallParams = GetSlowFallParams())
	{
		TickAirControl = SlowFallParams->GetAirControl(TickAirControl);
	}
	
	return Super::GetAirControl(DeltaTime, TickAirControl, FallAcceleration);
}

void UModifierMovement::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	if (IsMovingOnGround())
	{
		Friction = GetGroundFriction(Friction);
	}
	
	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
}

void UModifierMovement::ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration)
{
	if (IsMovingOnGround())
	{
		Friction = bUseSeparateBrakingFriction ? GetBrakingFriction() : GetGroundFriction(Friction);
	}
	Super::ApplyVelocityBraking(DeltaTime, Friction, BrakingDeceleration);
}

bool UModifierMovement::CanBoostInCurrentState() const
{
	return UpdatedComponent && !UpdatedComponent->IsSimulatingPhysics() && (IsFalling() || IsMovingOnGround());
}

bool UModifierMovement::CanSnareInCurrentState() const
{
	return UpdatedComponent && !UpdatedComponent->IsSimulatingPhysics() && (IsFalling() || IsMovingOnGround());
}

bool UModifierMovement::CanSlowFallInCurrentState() const
{
	return UpdatedComponent && !UpdatedComponent->IsSimulatingPhysics() && (IsFalling() || IsMovingOnGround());
}

bool UModifierMovement::RemoveVelocityZOnSlowFallStart() const
{
	if (IsMovingOnGround())
	{
		return false;
	}
	
	// Optionally clear Z velocity if slow fall is active
	const EModifierFallZ RemoveVelocityZ = GetSlowFallParams() ?
		GetSlowFallParams()->RemoveVelocityZOnStart : EModifierFallZ::Disabled;
		
	switch (RemoveVelocityZ)
	{
	case EModifierFallZ::Disabled:
		return false;
	case EModifierFallZ::Enabled:
		return true;
	case EModifierFallZ::Falling:
		return Velocity.Z < 0.f;
	case EModifierFallZ::Rising:
		return Velocity.Z > 0.f;
	}

	return false;
}

void UModifierMovement::ProcessModifierMovementState()
{
	// Proxies get replicated Modifier state.
	if (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		// Check for a change in Modifier state. Players toggle Modifier by changing WantsModifier.

		{	// Boost
			const FGameplayTag PrevBoostLevel = GetBoostLevel();
			const uint8 PrevBoostLevelValue = BoostLevel;
			const TArray<FMovementModifier*> Boosts = { &BoostLocal, &BoostCorrection, &BoostServer };
			if (FModifierStatics::ProcessModifiers(BoostLevel, BoostLevelMethod, BoostLevels,
				bLimitMaxBoosts, MaxBoosts, NO_MODIFIER, Boosts,
				[this] { return CanBoostInCurrentState(); }))
			{
				ModifierCharacterOwner->NotifyModifierChanged(FModifierTags::Modifier_Boost,
					GetBoostLevel(), PrevBoostLevel, BoostLevel,
					PrevBoostLevelValue, NO_MODIFIER);
			}
		}

		{	// Snare
			const FGameplayTag PrevSnareLevel = GetSnareLevel();
			const uint8 PrevSnareLevelValue = SnareLevel;
			const TArray<FMovementModifier*> Snares = { &SnareServer };
			if (FModifierStatics::ProcessModifiers(SnareLevel, SnareLevelMethod, SnareLevels,
				bLimitMaxSnares, MaxSnares, NO_MODIFIER, Snares,
				[this] { return CanSnareInCurrentState(); }))
			{
				ModifierCharacterOwner->NotifyModifierChanged(FModifierTags::Modifier_Snare,
					GetSnareLevel(), PrevSnareLevel, SnareLevel,
					PrevSnareLevelValue, NO_MODIFIER);
			}
		}

		{	// SlowFall
			const FGameplayTag PrevSlowFallLevel = GetSlowFallLevel();
			const uint8 PrevSlowFallLevelValue = SlowFallLevel;
			const TArray<FMovementModifier*> SlowFalls = { &SlowFallLocal };
			if (FModifierStatics::ProcessModifiers(SlowFallLevel, SlowFallLevelMethod, SlowFallLevels,
				bLimitMaxSlowFalls, MaxSlowFalls, NO_MODIFIER, SlowFalls,
				[this] { return CanSlowFallInCurrentState(); }))
			{
				ModifierCharacterOwner->NotifyModifierChanged(FModifierTags::Modifier_SlowFall,
					GetSlowFallLevel(), PrevSlowFallLevel, SlowFallLevel,
					PrevSlowFallLevelValue, NO_MODIFIER);
			}
		}
	}
}

void UModifierMovement::UpdateModifierMovementState()
{
	if (!HasValidData())
	{
		return;
	}

	// Initialize Modifier levels if empty
	if (BoostLevels.Num() == 0)	{ for (const auto& Level : Boost) { BoostLevels.Add(Level.Key); } }
	if (SnareLevels.Num() == 0)	{ for (const auto& Level : Snare) { SnareLevels.Add(Level.Key); } }
	if (SlowFallLevels.Num() == 0) { for (const auto& Level : SlowFall) { SlowFallLevels.Add(Level.Key); } }

	// Update the modifiers
	ProcessModifierMovementState();
}

void UModifierMovement::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	if (!HasValidData())
	{
		return;
	}
	
	const bool bWasSlowFalling = IsSlowFallActive();
	UpdateModifierMovementState();

	if (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		// Optionally clear Z velocity if slow fall is active
		if (!bWasSlowFalling && IsSlowFallActive() && RemoveVelocityZOnSlowFallStart())
		{
			Velocity.Z = 0.f;
		}
	}
	
	Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

void UModifierMovement::UpdateCharacterStateAfterMovement(float DeltaSeconds)
{
	UpdateModifierMovementState();

	Super::UpdateCharacterStateAfterMovement(DeltaSeconds);
}

FClientAuthData* UModifierMovement::ProcessClientAuthData()
{
	ClientAuthStack.SortByPriority();
	return ClientAuthStack.GetFirst();
}

FClientAuthParams UModifierMovement::GetClientAuthParams(const FClientAuthData* ClientAuthData)
{
	if (!ClientAuthData)
	{
		return {};
	}
	
	FClientAuthParams Params = { false, 0.f, 0.f, 0.f, ClientAuthData->Priority };

	// Get all active client auth data that matches the priority
	TArray<FClientAuthData> Priority = ClientAuthStack.FilterPriority(ClientAuthData->Priority);

	// Combine the parameters
	int32 Num = 0;
	for (const FClientAuthData& Data : Priority)
	{
		if (const FClientAuthParams* DataParams = GetClientAuthParamsForSource(Data.Source))
		{
			Params.ClientAuthTime += DataParams->ClientAuthTime;
			Params.MaxClientAuthDistance += DataParams->MaxClientAuthDistance;
			Params.RejectClientAuthDistance += DataParams->RejectClientAuthDistance;
			Num++;
		}
	}

	// Average the parameters
	Params.bEnableClientAuth = Num > 0;
	if (Num > 1)
	{
		Params.ClientAuthTime /= Num;
		Params.MaxClientAuthDistance /= Num;
		Params.RejectClientAuthDistance /= Num;
	}

	return Params;
}

void UModifierMovement::GrantClientAuthority(FGameplayTag ClientAuthSource, float OverrideDuration)
{
	if (!CharacterOwner || !CharacterOwner->HasAuthority())
	{
		return;
	}
	
	if (const FClientAuthParams* Params = GetClientAuthParamsForSource(ClientAuthSource))
	{
		if (Params->bEnableClientAuth)
		{
			const float Duration = OverrideDuration > 0.f ? OverrideDuration : Params->ClientAuthTime;
			ClientAuthStack.Stack.Add(FClientAuthData(ClientAuthSource, Duration, Params->Priority, ++ClientAuthIdCounter));

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
#if WITH_EDITOR
		FMessageLog("PIE").Error(FText::FromString(FString::Printf(TEXT("ClientAuthSource '%s' not found in ClientAuthParams"), *ClientAuthSource.ToString())));
#else
		UE_LOG(LogModifierMovement, Error, TEXT("ClientAuthSource '%s' not found"), *ClientAuthSource.ToString());
#endif
	}
}

bool UModifierMovement::ServerShouldGrantClientPositionAuthority(FVector& ClientLoc, FClientAuthData*& AuthData)
{
	AuthData = nullptr;
	
	// Already ignoring client movement error checks and correction
	if (bIgnoreClientMovementErrorChecksAndCorrection)
	{
		return false;
	}

	// Abort if client authority is not enabled
#if !UE_BUILD_SHIPPING
	if (ModifierMovementCVars::bClientAuthDisabled)
	{
		return false;
	}
#endif

	// Get auth data
	AuthData = ProcessClientAuthData();
	if (!AuthData || !AuthData->IsValid())
	{
		// No auth data, can't do anything
		return false;
	}

	// Get auth params
	const FClientAuthParams Params = GetClientAuthParams(AuthData);

	// Disabled
	if (!Params.bEnableClientAuth)
	{
		return false;
	}

	// Validate auth data
#if !UE_BUILD_SHIPPING
	if (UNLIKELY(AuthData->TimeRemaining <= 0.f))
	{
		// ServerMoveHandleClientError() should have removed the auth data already
		return ensure(false);
	}
#endif
	
	// Reset alpha, we're going to calculate it now
	AuthData->Alpha = 0.f;

	// How far the client is from the server
	const FVector ServerLoc = UpdatedComponent->GetComponentLocation();
	FVector LocDiff = ServerLoc - ClientLoc;

	// No change or almost no change occurred
	if (LocDiff.IsNearlyZero())
	{
		// Grant full authority
		AuthData->Alpha = 1.f;
		return true;
	}

	// If the client is too far away from the server, reject the client position entirely, potential cheater
	if (LocDiff.SizeSquared() >= FMath::Square(Params.RejectClientAuthDistance))
	{
		OnClientAuthRejected(ClientLoc, ServerLoc, LocDiff);
		return false;
	}

	// If the client is not within the maximum allowable distance, accept the client position, but only partially
	if (LocDiff.Size() >= Params.MaxClientAuthDistance)
	{
		// Accept only a portion of the client's location
		AuthData->Alpha = Params.MaxClientAuthDistance / LocDiff.Size();
		ClientLoc = FMath::Lerp<FVector>(ServerLoc, ClientLoc, AuthData->Alpha);
		LocDiff = ServerLoc - ClientLoc;
	}
	else
	{
		// Accept full client location
		AuthData->Alpha = 1.f;
	}

	return true;
}

void UModifierMovement::ServerMove_PerformMovement(const FCharacterNetworkMoveData& MoveData)
{
	// Server updates from the client's move data
	// Use this instead of UpdateFromCompressedFlags()

	// Client >> CallServerMovePacked ➜ ClientFillNetworkMoveData ➜ ServerMovePacked_ClientSend >> Server
	// >> ServerMovePacked_ServerReceive ➜ ServerMove_HandleMoveData ➜ ServerMove_PerformMovement
	
	const FModifierNetworkMoveData& ModifierMoveData = static_cast<const FModifierNetworkMoveData&>(MoveData);

	BoostLocal.ServerMove_PerformMovement(ModifierMoveData.BoostLocal.WantsModifiers);
	BoostCorrection.ServerMove_PerformMovement(ModifierMoveData.BoostCorrection.WantsModifiers);

	SlowFallLocal.ServerMove_PerformMovement(ModifierMoveData.SlowFallLocal.WantsModifiers);

	Super::ServerMove_PerformMovement(MoveData);
}

bool UModifierMovement::ServerCheckClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel,
	const FVector& ClientWorldLocation, const FVector& RelativeClientLocation, UPrimitiveComponent* ClientMovementBase,
	FName ClientBaseBoneName, uint8 ClientMovementMode)
{
	// ServerMovePacked_ServerReceive ➜ ServerMove_HandleMoveData ➜ ServerMove_PerformMovement
	// ➜ ServerMoveHandleClientError ➜ ServerCheckClientError
	
	if (Super::ServerCheckClientError(ClientTimeStamp, DeltaTime, Accel, ClientWorldLocation, RelativeClientLocation, ClientMovementBase, ClientBaseBoneName, ClientMovementMode))
	{
		return true;
	}
    
	// Trigger a client correction if the value in the Client differs
	const FModifierNetworkMoveData* CurrentMoveData = static_cast<const FModifierNetworkMoveData*>(GetCurrentNetworkMoveData());

	if (BoostCorrection.ServerCheckClientError(CurrentMoveData->BoostCorrection.Modifiers))	{ return true; }
	if (BoostServer.ServerCheckClientError(CurrentMoveData->BoostServer.Modifiers)) { return true; }
	if (SnareServer.ServerCheckClientError(CurrentMoveData->SnareServer.Modifiers)) { return true; }

	return false;
}

void UModifierMovement::ServerMoveHandleClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel,
	const FVector& RelativeClientLocation, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName,
	uint8 ClientMovementMode)
{
	// This is the entry-point for determining how to handle client corrections; we can determine they are out of sync
	// and make any changes that suit our needs
	
	// Client >> TickComponent ➜ ControlledCharacterMove ➜ CallServerMovePacked ➜ ReplicateMoveToServer >> Server
	// >> ServerMove_PerformMovement ➜ ServerMoveHandleClientError

	// Process and grant client authority
#if !UE_BUILD_SHIPPING
	if (!ModifierMovementCVars::bClientAuthDisabled)
#endif
	{
		// Update client authority time remaining
		ClientAuthStack.Update(DeltaTime);

		// Test for client authority
		FVector ClientLoc = FRepMovement::RebaseOntoZeroOrigin(RelativeClientLocation, this);
		FClientAuthData* AuthData = nullptr;
		if (ServerShouldGrantClientPositionAuthority(ClientLoc, AuthData))
		{
			// Apply client authoritative position directly -- Subsequent moves will resolve overlapping conditions
			UpdatedComponent->SetWorldLocation(ClientLoc, false);
		}

		// Cached to be sent to the client later with FMoveResponseDataContainer
		ClientAuthAlpha = AuthData ? AuthData->Alpha : 0.f;
	}

	// The move prepared here will finally be sent in the next ReplicateMoveToServer()

	Super::ServerMoveHandleClientError(ClientTimeStamp, DeltaTime, Accel, RelativeClientLocation, ClientMovementBase,
		ClientBaseBoneName, ClientMovementMode);
}

void UModifierMovement::ClientAdjustPosition_Implementation(float TimeStamp, FVector NewLoc, FVector NewVel,
	UPrimitiveComponent* NewBase, FName NewBaseBoneName, bool bHasBase, bool bBaseRelativePosition,
	uint8 ServerMovementMode, TOptional<FRotator> OptionalRotation)
{
	if (!HasValidData() || !IsActive())
	{
		return;
	}

	const FVector ClientLoc = UpdatedComponent->GetComponentLocation();
	
	Super::ClientAdjustPosition_Implementation(TimeStamp, NewLoc, NewVel, NewBase, NewBaseBoneName, bHasBase,
		bBaseRelativePosition, ServerMovementMode,OptionalRotation);

	const FModifierMoveResponseDataContainer& MoveResponse = static_cast<const FModifierMoveResponseDataContainer&>(GetMoveResponseDataContainer());
	ClientAuthAlpha = MoveResponse.bHasClientAuthAlpha ? MoveResponse.ClientAuthAlpha : 0.f;

	// Preserve client location relative to the partial client authority we have
	const FVector AuthLocation = FMath::Lerp<FVector>(UpdatedComponent->GetComponentLocation(), ClientLoc, ClientAuthAlpha);
	UpdatedComponent->SetWorldLocation(AuthLocation, false);
}

void UModifierMovement::OnClientCorrectionReceived(class FNetworkPredictionData_Client_Character& ClientData,
	float TimeStamp, FVector NewLocation, FVector NewVelocity, UPrimitiveComponent* NewBase, FName NewBaseBoneName,
	bool bHasBase, bool bBaseRelativePosition, uint8 ServerMovementMode
#if UE_5_03_OR_LATER
	, FVector ServerGravityDirection
#endif
	)
{
	// Occurs on AutonomousProxy, when the server sends the move response
	// This is where we receive the snare, and can override the server's location, assuming it has given us authority

	// Server >> SendClientAdjustment() ➜ ServerSendMoveResponse() ➜ ServerFillResponseData() + MoveResponsePacked_ServerSend() >> Client
	// >> ClientMoveResponsePacked() ➜ ClientHandleMoveResponse() ➜ ClientAdjustPosition_Implementation() ➜ OnClientCorrectionReceived()
	
	const FModifierMoveResponseDataContainer& MoveResponse = static_cast<const FModifierMoveResponseDataContainer&>(GetMoveResponseDataContainer());

	BoostCorrection.OnClientCorrectionReceived(MoveResponse.BoostCorrection.Modifiers);
	BoostServer.OnClientCorrectionReceived(MoveResponse.BoostServer.Modifiers);
	SnareServer.OnClientCorrectionReceived(MoveResponse.SnareServer.Modifiers);

	Super::OnClientCorrectionReceived(ClientData, TimeStamp, UpdatedComponent->GetComponentLocation(), NewVelocity, NewBase, NewBaseBoneName,
		bHasBase, bBaseRelativePosition, ServerMovementMode, ServerGravityDirection);
}

bool UModifierMovement::ClientUpdatePositionAfterServerUpdate()
{
	const TModifierStack RealBoostLocal = BoostLocal.WantsModifiers;
	const TModifierStack RealBoostCorrection = BoostCorrection.WantsModifiers;
	const TModifierStack RealSlowFallLocal = SlowFallLocal.WantsModifiers;

	const FVector ClientLoc = UpdatedComponent->GetComponentLocation();
	
	const bool bResult = Super::ClientUpdatePositionAfterServerUpdate();
	
	BoostLocal.WantsModifiers = RealBoostLocal;
	BoostCorrection.WantsModifiers = RealBoostCorrection;
	SlowFallLocal.WantsModifiers = RealSlowFallLocal;

	// Preserve client location relative to the partial client authority we have
	const FVector AuthLocation = FMath::Lerp<FVector>(UpdatedComponent->GetComponentLocation(), ClientLoc, ClientAuthAlpha);
	UpdatedComponent->SetWorldLocation(AuthLocation, false);

	return bResult;
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
				const FAnimMontageInstance* RootMotionMontageInstance = CharacterOwner->GetRootMotionAnimMontageInstance();
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

void FSavedMove_Character_Modifier::Clear()
{
	Super::Clear();

	BoostLocal.Clear();
	BoostCorrection.Clear();
	BoostServer.Clear();
	SnareServer.Clear();
	SlowFallLocal.Clear();

	BoostLevel = NO_MODIFIER;
	SnareLevel = NO_MODIFIER;
	SlowFallLevel = NO_MODIFIER;
}

void FSavedMove_Character_Modifier::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel,
	FNetworkPredictionData_Client_Character& ClientData)
{
	// Client ➜ Server (ReplicateMoveToServer)
	
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	if (const UModifierMovement* MoveComp = Cast<AModifierCharacter>(C)->GetModifierCharacterMovement())
	{
		BoostLocal.SetMoveFor(MoveComp->BoostLocal.WantsModifiers);
		BoostCorrection.SetMoveFor(MoveComp->BoostCorrection.WantsModifiers);
		SlowFallLocal.SetMoveFor(MoveComp->SlowFallLocal.WantsModifiers);
	}
}

bool FSavedMove_Character_Modifier::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter,
	float MaxDelta) const
{
	// We combine moves for the purpose of reducing the number of moves sent to the server, especially when exceeding
	// 60 fps (by default, see ClientNetSendMoveDeltaTime).
	// By combining moves, we can send fewer moves, but still have the same outcome.
	
	// If we didn't handle move combining, and then we used OnStartModifier() to modify our Velocity directly, it would
	// de-sync if we exceed 60fps. This is where move combining kicks in and starts using Pending Moves instead.
	
	// When combining moves, the PendingMove is passed into the NewMove. Locally, before sending a Move to the Server,
	// the AutonomousProxy Client will already have processed the current PendingMove (it's only pending for being sent,
	// not processed).

	// Since combining will happen before processing a move, PendingMove might end up being processed twice; once last
	// frame, and once as part of the new combined move.
	
	const TSharedPtr<FSavedMove_Character_Modifier> SavedMove = StaticCastSharedPtr<FSavedMove_Character_Modifier>(NewMove);

	// We can only combine moves if they will result in the same state as if both moves were processed individually,
	// because the AutonomousProxy Client processes them individually prior to sending them to the server.

	if (!BoostLocal.CanCombineWith(SavedMove->BoostLocal.WantsModifiers)) { return false; }
	if (!BoostCorrection.CanCombineWith(SavedMove->BoostCorrection.WantsModifiers)) { return false; }

	if (!SlowFallLocal.CanCombineWith(SavedMove->SlowFallLocal.WantsModifiers)) { return false; }

	// Without these, the change/start/stop events will trigger twice causing de-sync, so we don't combine moves if the level changes
	if (BoostLevel != SavedMove->BoostLevel) { return false; }
	if (SnareLevel != SavedMove->SnareLevel) { return false; }
	if (SlowFallLevel != SavedMove->SlowFallLevel) { return false; }
	
	return FSavedMove_Character::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void FSavedMove_Character_Modifier::SetInitialPosition(ACharacter* C)
{
	// To counter the PendingMove potentially being processed twice, we need to make sure to reset the state of the CMC
	// back to the "InitialPosition" (state) it had before the PendingMove got processed.
	
	Super::SetInitialPosition(C);

	// Retrieve the value from our CMC to revert the saved move value back to this.
	if (const UModifierMovement* MoveComp = Cast<AModifierCharacter>(C)->GetModifierCharacterMovement())
	{
		BoostLocal.SetInitialPosition(MoveComp->BoostLocal.WantsModifiers);
		BoostCorrection.SetInitialPosition(MoveComp->BoostCorrection.WantsModifiers);
		SlowFallLocal.SetInitialPosition(MoveComp->SlowFallLocal.WantsModifiers);

		BoostLevel = MoveComp->BoostLevel;
		SnareLevel = MoveComp->SnareLevel;
		SlowFallLevel = MoveComp->SlowFallLevel;
	}
}

void FSavedMove_Character_Modifier::CombineWith(const FSavedMove_Character* OldMove, ACharacter* C,
	APlayerController* PC, const FVector& OldStartLocation)
{
	Super::CombineWith(OldMove, C, PC, OldStartLocation);

	const FSavedMove_Character_Modifier* SavedOldMove = static_cast<const FSavedMove_Character_Modifier*>(OldMove);

	if (UModifierMovement* MoveComp = C ? Cast<UModifierMovement>(C->GetCharacterMovement()) : nullptr)
	{
		MoveComp->BoostLocal.CombineWith(SavedOldMove->BoostLocal.WantsModifiers);
		MoveComp->BoostCorrection.CombineWith(SavedOldMove->BoostCorrection.WantsModifiers);
		MoveComp->SlowFallLocal.CombineWith(SavedOldMove->SlowFallLocal.WantsModifiers);

		MoveComp->BoostLevel = SavedOldMove->BoostLevel;
		MoveComp->SnareLevel = SavedOldMove->SnareLevel;
		MoveComp->SlowFallLevel = SavedOldMove->SlowFallLevel;
	}
}

void FSavedMove_Character_Modifier::PostUpdate(ACharacter* C, EPostUpdateMode PostUpdateMode)
{
	// When considering whether to delay or combine moves, we need to compare the move at the start and the end
	if (const UModifierMovement* MoveComp = C ? Cast<UModifierMovement>(C->GetCharacterMovement()) : nullptr)
	{
		BoostCorrection.PostUpdate(MoveComp->BoostCorrection.Modifiers);
		BoostServer.PostUpdate(MoveComp->BoostServer.Modifiers);
		SnareServer.PostUpdate(MoveComp->SnareServer.Modifiers);

		// if (PostUpdateMode == PostUpdate_Record)
	}

	Super::PostUpdate(C, PostUpdateMode);
}

bool FSavedMove_Character_Modifier::IsImportantMove(const FSavedMovePtr& LastAckedMove) const
{
	// Important moves get sent again if not acked by the server
	
	const TSharedPtr<FSavedMove_Character_Modifier>& SavedMove = StaticCastSharedPtr<FSavedMove_Character_Modifier>(LastAckedMove);

	if (BoostLocal.IsImportantMove(SavedMove->BoostLocal.WantsModifiers)) { return true; }
	if (BoostCorrection.IsImportantMove(SavedMove->BoostCorrection.WantsModifiers))	{ return true; }
	if (SlowFallLocal.IsImportantMove(SavedMove->SlowFallLocal.WantsModifiers)) { return true; }
	
	return Super::IsImportantMove(LastAckedMove);
}

FSavedMovePtr FNetworkPredictionData_Client_Character_Modifier::AllocateNewMove()
{
	return MakeShared<FSavedMove_Character_Modifier>();
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
