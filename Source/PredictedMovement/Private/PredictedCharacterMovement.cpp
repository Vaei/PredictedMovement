// Copyright (c) Jared Taylor


#include "PredictedCharacterMovement.h"

#include "PredictedCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"

#if !UE_BUILD_SHIPPING
#include "Engine/Engine.h"
#endif

#include "Modifier/ModifierTags.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PredictedCharacterMovement)

DEFINE_LOG_CATEGORY_STATIC(LogPredictedMovement, Log, All);

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

UPredictedCharacterMovement::UPredictedCharacterMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetNetworkMoveDataContainer(PredMoveDataContainer);
	SetMoveResponseDataContainer(PredMoveResponseDataContainer);

	// Defaults
	GroundFriction = 12.f;  // More grounded, less sliding
	RotationRate.Yaw = 500.f;
	BrakingFrictionFactor = 1.f;
	bUseSeparateBrakingFriction = true;
	PerchRadiusThreshold = 15.f;
	
	// Strolling
	MaxAccelerationStrolling = 512.f;
	MaxWalkSpeedStrolling = 120.f;
	BrakingDecelerationStrolling = 512.f;
	GroundFrictionStrolling = 12.f;
	BrakingFrictionStrolling = 4.f;

	bWantsToStroll = false;

	// Walking
	MaxAcceleration = 1300.f;
	MaxWalkSpeed = 260.f;
	BrakingDecelerationWalking = 512.f;
	VelocityCheckMitigatorWalking = 0.98f;
	
	// Running
	MaxAccelerationRunning = 1600.f;
	MaxWalkSpeedRunning = 500.f;
	BrakingDecelerationRunning = 1680.f;
	GroundFrictionRunning = 12.f;
	BrakingFrictionRunning = 4.f;
	VelocityCheckMitigatorRunning = 0.98f;

	bWantsToWalk = false;
	
	// Sprinting
	bUseMaxAccelerationSprintingOnlyAtSpeed = true;
	MaxAccelerationSprinting = 2400.f;
	MaxWalkSpeedSprinting = 860.f;
	BrakingDecelerationSprinting = 2048.f;
	GroundFrictionSprinting = 12.f;
	BrakingFrictionSprinting = 4.f;

	VelocityCheckMitigatorSprinting = 0.98f;
	bRestrictSprintInputAngle = true;
	SetMaxInputAngleSprint(50.f);

	bWantsToSprint = false;

	// Enable crouch
	NavAgentProps.bCanCrouch = true;

	// Stamina drained scalars
	MaxWalkSpeedScalarStaminaDrained = 0.25f;
	MaxAccelerationScalarStaminaDrained = 0.5f;
	MaxBrakingDecelerationScalarStaminaDrained = 0.5f;

	// Stamina
	BaseMaxStamina = 100.f;
	SetMaxStamina(BaseMaxStamina);
	SprintStaminaDrainRate = 34.f;
	StaminaRegenRate = 20.f;
	StaminaDrainedRegenRate = 10.f;
	bStaminaRecoveryFromPct = true;
	StaminaRecoveryAmount = 20.f;
	StaminaRecoveryPct = 0.2f;
	StartSprintStaminaPct = 0.05f;  // 5% stamina to start sprinting
	
	NetworkStaminaCorrectionThreshold = 2.f;

	// Aim Down Sights
	MaxWalkSpeedAimingDownSightsScalar = 0.333f;
	MaxAccelerationAimingDownSightsScalar = 0.666f;
	BrakingDecelerationAimingDownSightsScalar = 0.75f;
	GroundFrictionAimingDownSightsScalar = 1.f;
	BrakingFrictionAimingDownSightsScalar = 1.f;

	// Crouch
	SetCrouchedHalfHeight(54.f);
	MaxAccelerationCrouched = 384.f;
	BrakingDecelerationCrouched = 512.f;
	GroundFrictionCrouched = 12.f;
	BrakingFrictionCrouched = 3.f;
	
	// Prone
	MaxAccelerationProned = 256.f;
	MaxWalkSpeedProned = 168.f;
	BrakingDecelerationProned = 512.f;
	GroundFrictionProned = 3.f;
	BrakingFrictionProned = 1.f;

	PronedRadius = 40.f;
	PronedHalfHeight = 40.f;

	ProneLockDuration = 1.f;

	bCanWalkOffLedgesWhenProned = false;
	bWantsToProne = false;
	bProneLocked = false;

	// Init Modifier Levels
	Boost.Add(FModifierTags::Modifier_Boost, { 1.50f });		// 50% Speed Boost
	Haste.Add(FModifierTags::Modifier_Haste, { 1.50f });		// 50% Speed Haste (Sprinting)
	Slow.Add(FModifierTags::Modifier_Slow, { 0.50f });		// 50% Speed Slow
	Snare.Add(FModifierTags::Modifier_Snare, { 0.33f });		// 33% Speed Snare
	SlowFall.Add(FModifierTags::Modifier_SlowFall, { 0.1f, EModifierFallZ::Enabled });  // 90% Gravity Reduction

	// Auth params for Snare
	static constexpr int32 DefaultPriority = 5;
	ClientAuthParams.FindOrAdd(FModifierTags::ClientAuth_Snare, { DefaultPriority });
}

void FPredictedMoveResponseDataContainer::ServerFillResponseData(const UCharacterMovementComponent& CharacterMovement,
	const FClientAdjustment& PendingAdjustment)
{
	Super::ServerFillResponseData(CharacterMovement, PendingAdjustment);

	// Server ➜ Client

	// Server >> APlayerController::SendClientAdjustment() ➜ SendClientAdjustment ➜ ServerSendMoveResponse ➜
	// ServerFillResponseData ➜ MoveResponsePacked_ServerSend >> Client
	
	const UPredictedCharacterMovement* MoveComp = Cast<UPredictedCharacterMovement>(&CharacterMovement);

	// Stamina
	bStaminaDrained = MoveComp->IsStaminaDrained();
	Stamina = MoveComp->GetStamina();

	// Fill the response data with the current modifier state
	BoostCorrection.ServerFillResponseData(MoveComp->BoostCorrection.Modifiers);
	HasteCorrection.ServerFillResponseData(MoveComp->HasteCorrection.Modifiers);
	SlowCorrection.ServerFillResponseData(MoveComp->SlowCorrection.Modifiers);
	SnareServer.ServerFillResponseData(MoveComp->SnareServer.Modifiers);
	SlowFallCorrection.ServerFillResponseData(MoveComp->SlowFallCorrection.Modifiers);

	// Fill ClientAuthAlpha
	ClientAuthAlpha = MoveComp->ClientAuthAlpha;
	bHasClientAuthAlpha = ClientAuthAlpha > 0.f;
}

bool FPredictedMoveResponseDataContainer::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar,
	UPackageMap* PackageMap)
{
	if (!Super::Serialize(CharacterMovement, Ar, PackageMap))
	{
		return false;
	}

	// Server ➜ Client
	if (IsCorrection())
	{
		// Serialize Stamina
		Ar << Stamina;
		Ar << bStaminaDrained;

		// Serialize Modifiers
		Ar << BoostCorrection.Modifiers;
		Ar << HasteCorrection.Modifiers;
		Ar << SlowCorrection.Modifiers;
		Ar << SnareServer.Modifiers;
		Ar << SlowFallCorrection.Modifiers;

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

void FPredictedNetworkMoveData::ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove,
	ENetworkMoveType MoveType)
{
	// Client packs move data to send to the server
	// Use this instead of GetCompressedFlags()
	Super::ClientFillNetworkMoveData(ClientMove, MoveType);

	// Client ➜ Server
	
	// CallServerMovePacked ➜ ClientFillNetworkMoveData ➜ ServerMovePacked_ClientSend >> Server
	// >> ServerMovePacked_ServerReceive ➜ ServerMove_HandleMoveData ➜ ServerMove_PerformMovement
	// ➜ MoveAutonomous (UpdateFromCompressedFlags)
	
	const FPredictedSavedMove& SavedMove = static_cast<const FPredictedSavedMove&>(ClientMove);

	// Compressed flags
	CompressedMoveFlagsExtra = SavedMove.GetCompressedFlagsExtra();
	
	// Stamina
	Stamina = SavedMove.EndStamina;

	// Fill the Modifier data from the saved move
	BoostLocal.ClientFillNetworkMoveData(SavedMove.BoostLocal.WantsModifiers);
	BoostCorrection.ClientFillNetworkMoveData(SavedMove.BoostCorrection.WantsModifiers, SavedMove.BoostCorrection.Modifiers);
	HasteLocal.ClientFillNetworkMoveData(SavedMove.HasteLocal.WantsModifiers);
	HasteCorrection.ClientFillNetworkMoveData(SavedMove.HasteCorrection.WantsModifiers, SavedMove.HasteCorrection.Modifiers);
	SlowLocal.ClientFillNetworkMoveData(SavedMove.SlowLocal.WantsModifiers);
	SlowCorrection.ClientFillNetworkMoveData(SavedMove.SlowCorrection.WantsModifiers, SavedMove.SlowCorrection.Modifiers);
	SnareServer.ClientFillNetworkMoveData(SavedMove.SnareServer.Modifiers);
	SlowFallLocal.ClientFillNetworkMoveData(SavedMove.SlowFallLocal.WantsModifiers);
	SlowFallCorrection.ClientFillNetworkMoveData(SavedMove.SlowFallCorrection.WantsModifiers, SavedMove.SlowFallCorrection.Modifiers);
}

bool FPredictedNetworkMoveData::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar,
	UPackageMap* PackageMap, ENetworkMoveType MoveType)
{
	Super::Serialize(CharacterMovement, Ar, PackageMap, MoveType);

	// Client ➜ Server

	// Compressed flags
	SerializeOptionalValue<uint8>(Ar.IsSaving(), Ar, CompressedMoveFlagsExtra, 0);

	// Stamina
	SerializeOptionalValue<float>(Ar.IsSaving(), Ar, Stamina, 0.f);

	// Serialize Modifier data
	BoostLocal.Serialize(Ar, TEXT("BoostLocal"));
	BoostCorrection.Serialize(Ar, TEXT("BoostCorrection"));
	HasteLocal.Serialize(Ar, TEXT("HasteLocal"));
	HasteCorrection.Serialize(Ar, TEXT("HasteCorrection"));
	SlowLocal.Serialize(Ar, TEXT("SlowLocal"));
	SlowCorrection.Serialize(Ar, TEXT("SlowCorrection"));
	SnareServer.Serialize(Ar, TEXT("SnareServer"));
	SlowFallLocal.Serialize(Ar, TEXT("SlowFallLocal"));
	SlowFallCorrection.Serialize(Ar, TEXT("SlowFallCorrection"));

	return !Ar.IsError();
}

#if WITH_EDITOR
void UPredictedCharacterMovement::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FProperty* PropertyThatChanged = PropertyChangedEvent.MemberProperty;
	if (PropertyThatChanged && PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(ThisClass, MaxInputAngleSprint))
	{
		// Compute MaxInputAngleSprint from the Angle.
		SetMaxInputAngleSprint(MaxInputAngleSprint);
	}
	else if (PropertyThatChanged && PropertyThatChanged->GetFName() == GET_MEMBER_NAME_CHECKED(ThisClass, BaseMaxStamina))
	{
		// Update max stamina
		SetMaxStamina(BaseMaxStamina);
	}
}
#endif

bool UPredictedCharacterMovement::HasValidData() const
{
	return Super::HasValidData() && IsValid(PredCharacterOwner);
}

void UPredictedCharacterMovement::PostLoad()
{
	Super::PostLoad();

	SetUpdatedCharacter();
}

void UPredictedCharacterMovement::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITOR
	// Ensure MaxInputAngleSprint is computed from the Angle.
	SetMaxInputAngleSprint(MaxInputAngleSprint);
#endif
}

void UPredictedCharacterMovement::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	Super::SetUpdatedComponent(NewUpdatedComponent);
	
	SetUpdatedCharacter();
}

void UPredictedCharacterMovement::SetUpdatedCharacter()
{
	PredCharacterOwner = Cast<APredictedCharacter>(PawnOwner);
}

void UPredictedCharacterMovement::BeginPlay()
{
	Super::BeginPlay();

	// Broadcast events to initialize UI, etc.
	OnMaxStaminaChanged(GetMaxStamina(), GetMaxStamina());

	// Set stamina to max
	SetStamina(GetMaxStamina());
}

EPredGaitMode UPredictedCharacterMovement::GetGaitMode() const
{
	if (IsSprinting())
	{
		return EPredGaitMode::Sprint;
	}
	if (IsWalk())
	{
		return EPredGaitMode::Walk;
	}
	if (IsStrolling())
	{
		return EPredGaitMode::Stroll;
	}
	return EPredGaitMode::Run;
}

EPredGaitMode UPredictedCharacterMovement::GetGaitSpeed() const
{
	if (IsSprintingInEffect())
	{
		return EPredGaitMode::Sprint;
	}
	if (IsRunningAtSpeed())
	{
		return EPredGaitMode::Run;
	}
	if (IsWalkingAtSpeed())
	{
		return EPredGaitMode::Walk;
	}
	return EPredGaitMode::Stroll;
}

bool UPredictedCharacterMovement::IsGaitAtSpeed(float Mitigator) const
{
	// When moving on ground we want to factor moving uphill or downhill so variations in terrain
	// aren't culled from the check. When falling, we don't want to factor fall velocity, only lateral
	const float Vel = IsMovingOnGround() ? Velocity.SizeSquared() : Velocity.SizeSquared2D();

	// When struggling to surpass walk speed, which can occur with heavy rotation and low acceleration, we
	// mitigate the check so there isn't a constant re-entry that can occur as an edge case
	return Vel >= FMath::Square(GetBaseMaxSpeed() * GetGaitSpeedFactor()) * Mitigator;
}

bool UPredictedCharacterMovement::IsStrolling() const
{
	return PredCharacterOwner && PredCharacterOwner->IsStrolling() && !IsSprintingInEffect();
}

bool UPredictedCharacterMovement::IsWalk() const
{
	return PredCharacterOwner && PredCharacterOwner->IsWalking() && !IsStrolling() && !IsSprintingInEffect();
}

bool UPredictedCharacterMovement::IsRunning() const
{
	// We're running if we're not walking, sprinting, etc.
	return !PredCharacterOwner || (!IsStrolling() && !IsWalk() && !IsSprinting());
}

float UPredictedCharacterMovement::GetGaitSpeedFactor() const
{
	/*
	 * Infinite recursion protection to avoid stack overflow -- we must exclude Haste from speed checks
	 * e.g. IsSprintWithinAllowableInputAngle() ➜ IsSprintingAtSpeed() ➜ GetMaxSpeed() ➜ GetMaxSpeedScalar() ➜ IsSprintingInEffect() ➜ IsSprintWithinAllowableInputAngle()
	 */
	
	const float StaminaDrained = IsStaminaDrained() ? MaxWalkSpeedScalarStaminaDrained : 1.f;
	const float AimingDownSights = IsAimingDownSights() ? MaxWalkSpeedAimingDownSightsScalar : 1.f;
	const float BoostScalar = GetBoostSpeedScalar();
	const float SlowScalar = GetSlowSpeedScalar();
	const float SnareScalar = GetSnareSpeedScalar();
	return StaminaDrained * AimingDownSights * BoostScalar * SlowScalar * SnareScalar;
}

float UPredictedCharacterMovement::GetMaxAccelerationScalar() const
{
	const float StaminaDrained = IsStaminaDrained() ? MaxAccelerationScalarStaminaDrained : 1.f;
	const float AimingDownSights = IsAimingDownSights() ? MaxAccelerationAimingDownSightsScalar : 1.f;
	const float BoostScalar = GetBoostAccelScalar();
	const float SlowScalar = GetSlowAccelScalar();
	const float SnareScalar = GetSnareAccelScalar();
	const float HasteScalar = IsSprintingInEffect() ? GetHasteAccelScalar() : 1.f;
	return StaminaDrained * AimingDownSights * BoostScalar * SlowScalar * SnareScalar * HasteScalar;
}

float UPredictedCharacterMovement::GetMaxSpeedScalar() const
{
	const float StaminaDrained = IsStaminaDrained() ? MaxWalkSpeedScalarStaminaDrained : 1.f;
	const float AimingDownSights = IsAimingDownSights() ? MaxWalkSpeedAimingDownSightsScalar : 1.f;
	const float BoostScalar = GetBoostSpeedScalar();
	const float SlowScalar = GetSlowSpeedScalar();
	const float SnareScalar = GetSnareSpeedScalar();
	const float HasteScalar = IsSprintingInEffect() ? GetHasteSpeedScalar() : 1.f;
	return StaminaDrained * AimingDownSights * BoostScalar * SlowScalar * SnareScalar * HasteScalar;
}

float UPredictedCharacterMovement::GetMaxBrakingDecelerationScalar() const
{
	const float StaminaDrained = IsStaminaDrained() ? MaxBrakingDecelerationScalarStaminaDrained : 1.f;
	const float AimingDownSights = IsAimingDownSights() ? BrakingDecelerationAimingDownSightsScalar : 1.f;
	const float BoostScalar = GetBoostBrakingScalar();
	const float SlowScalar = GetSlowBrakingScalar();
	const float SnareScalar = GetSnareBrakingScalar();
	const float HasteScalar = IsSprintingInEffect() ? GetHasteBrakingScalar() : 1.f;
	return StaminaDrained * AimingDownSights * BoostScalar * SlowScalar * SnareScalar * HasteScalar;
}

float UPredictedCharacterMovement::GetGroundFrictionScalar() const
{
	const float AimingDownSights = IsAimingDownSights() ? GroundFrictionAimingDownSightsScalar : 1.f;
	const float BoostScalar = GetBoostGroundFrictionScalar();
	const float SlowScalar = GetSlowGroundFrictionScalar();
	const float SnareScalar = GetSnareGroundFrictionScalar();
	const float HasteScalar = IsSprintingInEffect() ? GetHasteGroundFrictionScalar() : 1.f;
	return AimingDownSights * BoostScalar * SlowScalar * SnareScalar * HasteScalar;
}

float UPredictedCharacterMovement::GetBrakingFrictionScalar() const
{
	const float AimingDownSights = IsAimingDownSights() ? BrakingFrictionAimingDownSightsScalar : 1.f;
	const float BoostScalar = GetBoostBrakingFrictionScalar();
	const float SlowScalar = GetSlowBrakingFrictionScalar();
	const float SnareScalar = GetSnareBrakingFrictionScalar();
	const float HasteScalar = IsSprintingInEffect() ? GetHasteBrakingFrictionScalar() : 1.f;
	return AimingDownSights * BoostScalar * SlowScalar * SnareScalar * HasteScalar;
}

float UPredictedCharacterMovement::GetGravityZScalar() const
{
	const float SlowFallScalar = GetSlowFallGravityZScalar();
	return SlowFallScalar;
}

float UPredictedCharacterMovement::GetRootMotionTranslationScalar() const
{
	// Allowing boost to affect root motion will increase attack range, dodge range, etc., it is disabled by default
	const float BoostScalar = BoostAffectsRootMotion() ? GetBoostSpeedScalar() : 1.f;
	const float SlowScalar = SlowAffectsRootMotion() ? GetSlowSpeedScalar() : 1.f;
	const float SnareScalar = SnareAffectsRootMotion() ? GetSnareSpeedScalar() : 1.f;
	return BoostScalar * SlowScalar * SnareScalar;
}

float UPredictedCharacterMovement::GetBaseMaxAcceleration() const
{
	if (IsFlying())		{ return MaxAccelerationRunning; }
	if (IsSwimming())	{ return MaxAccelerationRunning; }
	if (IsProned())		{ return MaxAccelerationProned; }
	if (IsCrouching())	{ return MaxAccelerationCrouched; }

	if (IsSprintingInEffect())
	{
		return MaxAccelerationSprinting;
	}

	if (!bUseMaxAccelerationSprintingOnlyAtSpeed && IsSprinting() && IsSprintWithinAllowableInputAngle())
	{
		return MaxAccelerationSprinting;
	}

	const EPredGaitMode GaitMode = GetGaitMode();
	switch (GaitMode)
	{
	case EPredGaitMode::Stroll: return MaxAccelerationStrolling;
	case EPredGaitMode::Walk: return MaxAcceleration;
	case EPredGaitMode::Run:
	case EPredGaitMode::Sprint:
		return MaxAccelerationRunning;
	}
	return 0.f;
}

float UPredictedCharacterMovement::GetBaseMaxSpeed() const
{
	if (IsFlying())		{ return MaxFlySpeed; }
	if (IsSwimming())	{ return MaxSwimSpeed; }
	if (IsProned())		{ return MaxWalkSpeedProned; }
	if (IsCrouching())	{ return MaxWalkSpeedCrouched; }
	if (MovementMode == MOVE_Custom) { return MaxCustomMovementSpeed; }

	const EPredGaitMode GaitMode = GetGaitMode();
	switch (GaitMode)
	{
	case EPredGaitMode::Stroll: return MaxWalkSpeedStrolling;
	case EPredGaitMode::Walk: return MaxWalkSpeed;
	case EPredGaitMode::Run: return MaxWalkSpeedRunning;
	case EPredGaitMode::Sprint: return MaxWalkSpeedSprinting;
	}
	return 0.f;
}

float UPredictedCharacterMovement::GetBaseMaxBrakingDeceleration() const
{
	if (IsFlying()) { return BrakingDecelerationFlying; }
	if (IsFalling()) { return BrakingDecelerationFalling; }
	if (IsSwimming()) { return BrakingDecelerationSwimming; }
	if (IsProned()) { return BrakingDecelerationProned; }
	if (IsCrouching()) { return BrakingDecelerationCrouched; }

	const EPredGaitMode GaitMode = GetGaitMode();
	switch (GaitMode)
	{
	case EPredGaitMode::Stroll: return BrakingDecelerationStrolling;
	case EPredGaitMode::Walk: return BrakingDecelerationWalking;
	case EPredGaitMode::Run: return BrakingDecelerationRunning;
	case EPredGaitMode::Sprint: return BrakingDecelerationSprinting;
	}
	return 0.f;
}

float UPredictedCharacterMovement::GetBaseGroundFriction(float DefaultGroundFriction) const
{
	// This function is already gated by IsMovingOnGround() when called
	
	if (IsProned()) { return GroundFrictionProned; }
	if (IsCrouching()) { return GroundFrictionCrouched; }

	const EPredGaitMode GaitMode = GetGaitMode();
	switch (GaitMode)
	{
	case EPredGaitMode::Stroll: return GroundFrictionStrolling;
	case EPredGaitMode::Walk: return DefaultGroundFriction;
	case EPredGaitMode::Run: return GroundFrictionRunning;
	case EPredGaitMode::Sprint: return GroundFrictionSprinting;
	}

	return DefaultGroundFriction;
}

float UPredictedCharacterMovement::GetBaseBrakingFriction() const
{
	// This function is already gated by IsMovingOnGround() when called
	
	if (IsProned()) { return BrakingFrictionProned; }
	if (IsCrouching()) { return BrakingFrictionCrouched; }

	const EPredGaitMode GaitMode = GetGaitMode();
	switch (GaitMode)
	{
	case EPredGaitMode::Stroll: return BrakingFrictionStrolling;
	case EPredGaitMode::Walk: return BrakingFriction;
	case EPredGaitMode::Run: return BrakingFrictionRunning;
	case EPredGaitMode::Sprint: return BrakingFrictionSprinting;
	}
	
	return BrakingFriction;
}

float UPredictedCharacterMovement::GetGravityZ() const
{
	return Super::GetGravityZ() * GetGravityZScalar();
}

FVector UPredictedCharacterMovement::GetAirControl(float DeltaTime, float TickAirControl, const FVector& FallAcceleration)
{
	// Slow fall air control
	if (const FFallingModifierParams* Params = GetSlowFallParams())
	{
		TickAirControl = Params->GetAirControl(TickAirControl);
	}
	
	return Super::GetAirControl(DeltaTime, TickAirControl, FallAcceleration);
}

void UPredictedCharacterMovement::CalcStamina(float DeltaTime)
{
	// Do not update velocity when using root motion or when SimulatedProxy and not simulating root motion - SimulatedProxy are repped their Velocity
	if (!HasValidData() || HasAnimRootMotion() || DeltaTime < MIN_TICK_TIME || (CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy && !bWasSimulatingRootMotion))
	{
		return;
	}
	
	if (IsSprintingInEffect())
	{
		SetStamina(GetStamina() - SprintStaminaDrainRate * DeltaTime);
	}
	else
	{
		const float RegenRate = IsStaminaDrained() ? StaminaDrainedRegenRate : StaminaRegenRate;
		SetStamina(GetStamina() + RegenRate * DeltaTime);
	}
}

void UPredictedCharacterMovement::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	if (IsMovingOnGround())
	{
		Friction = GetGroundFriction(Friction);
	}
	
	CalcStamina(DeltaTime);
	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
}

void UPredictedCharacterMovement::ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration)
{
	if (IsMovingOnGround())
	{
		Friction = bUseSeparateBrakingFriction ? GetBrakingFriction() : GetGroundFriction(Friction);
	}
	Super::ApplyVelocityBraking(DeltaTime, Friction, BrakingDeceleration);
}

bool UPredictedCharacterMovement::CanWalkOffLedges() const
{
	if (!bCanWalkOffLedgesWhenProned && IsProned())
	{
		return false;
	}
	
	return Super::CanWalkOffLedges();
}

bool UPredictedCharacterMovement::CanAttemptJump() const
{
	// Cannot jump if not allowed
	if (!IsJumpAllowed())
	{
		return false;
	}

	// Can only jump from valid movement modes
	if (!IsMovingOnGround() && !IsFalling())
	{
		return false;
	}

	// Not during crouch
	if (!bCanJumpDuringCrouch && bWantsToCrouch)
	{
		return false;
	}

	// Not during prone
	if (!bCanJumpDuringProne && bWantsToProne)
	{
		return false;
	}

	return true;
}

void UPredictedCharacterMovement::Stroll(bool bClientSimulation)
{
	if (!HasValidData())
	{
		return;
	}
	
	if (!bClientSimulation)
	{
		if (!CanStrollInCurrentState())
		{
			return;
		}

		if (IsSprinting())
		{
			UnSprint();
		}

		if (IsWalk())
		{
			UnWalk();
		}

		PredCharacterOwner->SetIsStrolling(true);
	}
	PredCharacterOwner->OnStartStroll();
}

void UPredictedCharacterMovement::UnStroll(bool bClientSimulation)
{
	if (!HasValidData())
	{
		return;
	}

	if (!bClientSimulation)
	{
		PredCharacterOwner->SetIsStrolling(false);
	}
	PredCharacterOwner->OnEndStroll();
}

bool UPredictedCharacterMovement::CanStrollInCurrentState() const
{
	if (!UpdatedComponent || UpdatedComponent->IsSimulatingPhysics())
	{
		return false;
	}

	if (!IsFalling() && !IsMovingOnGround())
	{
		// Can only enter Stroll in either MOVE_Falling or MOVE_Strolling or MOVE_NavStrolling
		return false;
	}

	return true;
}

void UPredictedCharacterMovement::Walk(bool bClientSimulation)
{
	if (!HasValidData())
	{
		return;
	}
	
	if (!bClientSimulation)
	{
		if (!CanWalkInCurrentState())
		{
			return;
		}

		if (IsSprinting())
		{
			UnSprint();
		}

		if (IsWalk())
		{
			UnWalk();
		}

		PredCharacterOwner->SetIsWalking(true);
	}
	PredCharacterOwner->OnStartWalk();
}

void UPredictedCharacterMovement::UnWalk(bool bClientSimulation)
{
	if (!HasValidData())
	{
		return;
	}

	if (!bClientSimulation)
	{
		PredCharacterOwner->SetIsWalking(false);
	}
	PredCharacterOwner->OnEndWalk();
}

bool UPredictedCharacterMovement::CanWalkInCurrentState() const
{
	if (!UpdatedComponent || UpdatedComponent->IsSimulatingPhysics())
	{
		return false;
	}

	if (!IsFalling() && !IsMovingOnGround())
	{
		// Can only enter walk in either MOVE_Falling or MOVE_Walking or MOVE_NavWalking
		return false;
	}

	return true;
}

void UPredictedCharacterMovement::SetMaxInputAngleSprint(float InMaxAngleSprint)
{
	MaxInputAngleSprint = FMath::Clamp(InMaxAngleSprint, 0.f, 180.0f);
	MaxInputNormalSprint = FMath::Cos(FMath::DegreesToRadians(MaxInputAngleSprint));
}

bool UPredictedCharacterMovement::IsSprinting() const
{
	return PredCharacterOwner && PredCharacterOwner->IsSprinting();
}

void UPredictedCharacterMovement::Sprint(bool bClientSimulation)
{
	if (!HasValidData())
	{
		return;
	}
	
	if (!bClientSimulation)
	{
		if (!CanSprintInCurrentState())
		{
			return;
		}

		if (IsProned() && !bCanSprintDuringProne)
		{
			UnProne();
		}

		if (IsCrouching() && !bCanSprintDuringCrouch)
		{
			UnCrouch();
		}

		if (IsAimingDownSights() && !bCanSprintDuringAimDownSights)
		{
			UnAimDownSights();
		}

		if (IsStrolling())
		{
			UnStroll();
		}

		if (IsWalk())
		{
			UnWalk();
		}

		PredCharacterOwner->SetIsSprinting(true);
	}
	PredCharacterOwner->OnStartSprint();
}

void UPredictedCharacterMovement::UnSprint(bool bClientSimulation)
{
	if (!HasValidData())
	{
		return;
	}

	if (!bClientSimulation)
	{
		PredCharacterOwner->SetIsSprinting(false);
	}
	PredCharacterOwner->OnEndSprint();
}

bool UPredictedCharacterMovement::CanSprintInCurrentState() const
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

	if (IsCrouching() && !bCanSprintDuringCrouch)
	{
		return false;
	}

	if (IsProned() && !bCanSprintDuringProne)
	{
		return false;
	}

	return true;
}

bool UPredictedCharacterMovement::IsSprintWithinAllowableInputAngle() const
{
	if (!UpdatedComponent)
	{
		return false;
	}
	
	if (!bRestrictSprintInputAngle || MaxInputAngleSprint <= 0.f)
	{
		return true;
	}
	
	// This check ensures that we are not sprinting backward or sideways, while allowing leeway 
	// This angle allows sprinting when holding forward, forward left, forward right
	// but not left or right or backward
	const float Dot = GetCurrentAcceleration().GetSafeNormal2D() | UpdatedComponent->GetForwardVector();
	return Dot >= MaxInputNormalSprint;
}

void UPredictedCharacterMovement::SetStamina(float NewStamina)
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

void UPredictedCharacterMovement::SetMaxStamina(float NewMaxStamina)
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

void UPredictedCharacterMovement::SetStaminaDrained(bool bNewValue)
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

void UPredictedCharacterMovement::OnStaminaChanged(float PrevValue, float NewValue)
{
	if (IsValid(PredCharacterOwner))
	{
		PredCharacterOwner->OnStaminaChanged(NewValue, PrevValue);
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

void UPredictedCharacterMovement::OnMaxStaminaChanged(float PrevValue, float NewValue)
{
	if (IsValid(PredCharacterOwner))
	{
		PredCharacterOwner->OnMaxStaminaChanged(NewValue, PrevValue);
	}

	// Ensure that Stamina is within the new MaxStamina
	SetStamina(GetStamina());
}

void UPredictedCharacterMovement::OnStaminaDrained()
{
	if (IsValid(PredCharacterOwner))
	{
		PredCharacterOwner->OnStaminaDrained();
	}
}

void UPredictedCharacterMovement::OnStaminaDrainRecovered()
{
	if (IsValid(PredCharacterOwner))
	{
		PredCharacterOwner->OnStaminaDrainRecovered();
	}
}

bool UPredictedCharacterMovement::IsAimingDownSights() const
{
	return PredCharacterOwner && PredCharacterOwner->IsAimingDownSights();
}

void UPredictedCharacterMovement::AimDownSights(bool bClientSimulation)
{
	if (!HasValidData())
	{
		return;
	}

	if (!bClientSimulation)
	{
		if (!CanAimDownSightsInCurrentState())
		{
			return;
		}

		if (IsSprinting() && !bCanSprintDuringAimDownSights)
		{
			UnSprint();
		}

		PredCharacterOwner->SetIsAimingDownSights(true);
	}
	PredCharacterOwner->OnStartAimDownSights();
}

void UPredictedCharacterMovement::UnAimDownSights(bool bClientSimulation)
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

bool UPredictedCharacterMovement::CanAimDownSightsInCurrentState() const
{
	return (IsFalling() || IsMovingOnGround()) && UpdatedComponent && !UpdatedComponent->IsSimulatingPhysics();
}

bool UPredictedCharacterMovement::IsProneLocked() const
{
	if (CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
	{
		// Sim proxies don't prone lock
		return false;
	}
	return bProneLocked;
}

bool UPredictedCharacterMovement::IsProneLockOnTimer() const
{
	return GetRemainingProneLockCooldown() > 0.f;
}

float UPredictedCharacterMovement::GetRemainingProneLockCooldown() const
{
	const float CurrentTimestamp = GetTimestamp();
	const float RemainingCooldown = ProneLockDuration - (CurrentTimestamp - ProneLockTimestamp);
	return FMath::Clamp(RemainingCooldown, 0.f, ProneLockDuration);
}

void UPredictedCharacterMovement::SetProneLock(bool bLock)
{
	if (bLock)
	{
		bProneLocked = true;
		ProneLockTimestamp = GetTimestamp();
	}
	else
	{
		bProneLocked = false;
	}
}

float UPredictedCharacterMovement::GetTimestamp() const
{
	if (CharacterOwner->GetLocalRole() == ROLE_Authority)
	{
		if (CharacterOwner->IsLocallyControlled())
		{
			// Server owned character
			return GetWorld()->GetTimeSeconds();
		}
		else
		{
			// Server remote character
			const FNetworkPredictionData_Server_Character* ServerData = GetPredictionData_Server_Character();
			return ServerData->CurrentClientTimeStamp;
		}
	}
	else
	{
		// Client owned character
		const FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
		return ClientData->CurrentTimeStamp;
	}
}

bool UPredictedCharacterMovement::IsProned() const
{
	return PredCharacterOwner && PredCharacterOwner->IsProned();
}

void UPredictedCharacterMovement::Prone(bool bClientSimulation)
{
	if (!HasValidData())
	{
		return;
	}

	if (!bClientSimulation && !CanProneInCurrentState())
	{
		return;
	}

	// See if collision is already at desired size.
	if (CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() == PronedHalfHeight &&
		CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleRadius() == PronedRadius)
	{
		if (!bClientSimulation)
		{
			PredCharacterOwner->SetIsProned(true);
		}
		PredCharacterOwner->OnStartProne( 0.f, 0.f );
		SetProneLock(true);
		return;
	}

	if (bClientSimulation && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
	{
		// restore collision size before prone
		const ACharacter* DefaultCharacter = CharacterOwner->GetClass()->GetDefaultObject<ACharacter>();
		CharacterOwner->GetCapsuleComponent()->SetCapsuleSize(DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight());
		bShrinkProxyCapsule = true;
	}

	// Change collision size to prone dimensions
	const float ComponentScale = CharacterOwner->GetCapsuleComponent()->GetShapeScale();
	const float OldUnscaledHalfHeight = CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	const float OldUnscaledRadius = CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleRadius();

	// Height is not allowed to be smaller than radius.
	const float ClampedPronedHalfHeight = FMath::Max3(0.f, PronedRadius, PronedHalfHeight);
	CharacterOwner->GetCapsuleComponent()->SetCapsuleSize(PronedRadius, ClampedPronedHalfHeight);
	float HalfHeightAdjust = (OldUnscaledHalfHeight - ClampedPronedHalfHeight);
	float ScaledHalfHeightAdjust = HalfHeightAdjust * ComponentScale;

	if( !bClientSimulation )
	{
		// Proned to a larger height? (this is rare)
		if (ClampedPronedHalfHeight > OldUnscaledHalfHeight)
		{
			FCollisionQueryParams CapsuleParams(SCENE_QUERY_STAT(ProneTrace), false, CharacterOwner);
			FCollisionResponseParams ResponseParam;
			InitCollisionParams(CapsuleParams, ResponseParam);
			const bool bEncroached = GetWorld()->OverlapBlockingTestByChannel(UpdatedComponent->GetComponentLocation() - FVector(0.f,0.f,ScaledHalfHeightAdjust), FQuat::Identity,
				UpdatedComponent->GetCollisionObjectType(), GetPawnCapsuleCollisionShape(SHRINK_None), CapsuleParams, ResponseParam);

			// If encroached, cancel
			if( bEncroached )
			{
				CharacterOwner->GetCapsuleComponent()->SetCapsuleSize(OldUnscaledRadius, OldUnscaledHalfHeight);
				return;
			}
		}

		if (bCrouchMaintainsBaseLocation)
		{
			// Intentionally not using MoveUpdatedComponent, where a horizontal plane constraint would prevent the base of the capsule from staying at the same spot.
			UpdatedComponent->MoveComponent(FVector(0.f, 0.f, -ScaledHalfHeightAdjust), UpdatedComponent->GetComponentQuat(), true, nullptr, EMoveComponentFlags::MOVECOMP_NoFlags, ETeleportType::TeleportPhysics);
		}

		PredCharacterOwner->SetIsProned(true);
	}

	// Our capsule is growing during prone, test for encroaching from radius
	FCollisionQueryParams CapsuleParams(SCENE_QUERY_STAT(ProneTrace), false, CharacterOwner);
	FCollisionResponseParams ResponseParam;
	InitCollisionParams(CapsuleParams, ResponseParam);
	FHitResult Hit;
	const FVector Start = UpdatedComponent->GetComponentLocation() - FVector(0.f,0.f,ScaledHalfHeightAdjust);
	const FVector End = UpdatedComponent->GetComponentLocation() - FVector(0.f,0.f,ScaledHalfHeightAdjust * 1.01f);
	if (GetWorld()->SweepSingleByChannel(Hit, Start, End, FQuat::Identity, UpdatedComponent->GetCollisionObjectType(), FCollisionShape::MakeCapsule(PronedRadius, PronedHalfHeight), CapsuleParams, ResponseParam))
	{
		if (Hit.bStartPenetrating)
		{
			HandleImpact(Hit);
			SlideAlongSurface(FVector::DownVector, 1.f, Hit.Normal, Hit, true);

			if (Hit.bStartPenetrating)
			{
				OnCharacterStuckInGeometry(&Hit);
			}
		}
	}
	
	bForceNextFloorCheck = true;

	SetProneLock(true);

	// OnStartProne takes the change from the Default size, not the current one (though they are usually the same).
	const float MeshAdjust = ScaledHalfHeightAdjust;
	const ACharacter* DefaultCharacter = CharacterOwner->GetClass()->GetDefaultObject<ACharacter>();
	HalfHeightAdjust = (DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - ClampedPronedHalfHeight);
	ScaledHalfHeightAdjust = HalfHeightAdjust * ComponentScale;

	AdjustProxyCapsuleSize();
	PredCharacterOwner->OnStartProne( HalfHeightAdjust, ScaledHalfHeightAdjust );

	// Don't smooth this change in mesh position
	if ((bClientSimulation && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy) || (IsNetMode(NM_ListenServer) && CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy))
	{
		FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
		if (ClientData)
		{
			ClientData->MeshTranslationOffset -= FVector(0.f, 0.f, MeshAdjust);
			ClientData->OriginalMeshTranslationOffset = ClientData->MeshTranslationOffset;
		}
	}
}

void UPredictedCharacterMovement::UnProne(bool bClientSimulation)
{
	if (!HasValidData())
	{
		return;
	}

	if (IsProneLocked())
	{
		return;
	}

	APredictedCharacter* DefaultCharacter = PredCharacterOwner->GetClass()->GetDefaultObject<APredictedCharacter>();

	// See if collision is already at desired size.
	if (CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() == DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() &&
		CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleRadius() == DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius())
	{
		if (!bClientSimulation)
		{
			PredCharacterOwner->SetIsProned(false);
		}
		PredCharacterOwner->OnEndProne( 0.f, 0.f );
		return;
	}

	const float CurrentPronedHalfHeight = CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	const float ComponentScale = CharacterOwner->GetCapsuleComponent()->GetShapeScale();
	const float OldUnscaledHalfHeight = CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
	const float HalfHeightAdjust = DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() - OldUnscaledHalfHeight;
	const float ScaledHalfHeightAdjust = HalfHeightAdjust * ComponentScale;
	const FVector PawnLocation = UpdatedComponent->GetComponentLocation();

	// Grow to unproned size.
	check(CharacterOwner->GetCapsuleComponent());

	if( !bClientSimulation )
	{
		// Try to stay in place and see if the larger capsule fits. We use a slightly taller capsule to avoid penetration.
		const UWorld* MyWorld = GetWorld();
		constexpr float SweepInflation = UE_KINDA_SMALL_NUMBER * 10.f;
		FCollisionQueryParams CapsuleParams(SCENE_QUERY_STAT(ProneTrace), false, CharacterOwner);
		FCollisionResponseParams ResponseParam;
		InitCollisionParams(CapsuleParams, ResponseParam);

		// Compensate for the difference between current capsule size and standing size
		const FCollisionShape StandingCapsuleShape = GetPawnCapsuleCollisionShape(SHRINK_HeightCustom, -SweepInflation - ScaledHalfHeightAdjust); // Shrink by negative amount, so actually grow it.
		const ECollisionChannel CollisionChannel = UpdatedComponent->GetCollisionObjectType();
		bool bEncroached = true;

		if (!bCrouchMaintainsBaseLocation)
		{
			// Expand in place
			bEncroached = MyWorld->OverlapBlockingTestByChannel(PawnLocation, FQuat::Identity, CollisionChannel, StandingCapsuleShape, CapsuleParams, ResponseParam);
		
			if (bEncroached)
			{
				// Try adjusting capsule position to see if we can avoid encroachment.
				if (ScaledHalfHeightAdjust > 0.f)
				{
					// Shrink to a short capsule, sweep down to base to find where that would hit something, and then try to stand up from there.
					float PawnRadius, PawnHalfHeight;
					CharacterOwner->GetCapsuleComponent()->GetScaledCapsuleSize(PawnRadius, PawnHalfHeight);
					const float ShrinkHalfHeight = PawnHalfHeight - PawnRadius;
					const float TraceDist = PawnHalfHeight - ShrinkHalfHeight;
					const FVector Down = FVector(0.f, 0.f, -TraceDist);

					FHitResult Hit(1.f);
					const FCollisionShape ShortCapsuleShape = GetPawnCapsuleCollisionShape(SHRINK_HeightCustom, ShrinkHalfHeight);
					MyWorld->SweepSingleByChannel(Hit, PawnLocation, PawnLocation + Down, FQuat::Identity, CollisionChannel, ShortCapsuleShape, CapsuleParams);
					if (Hit.bStartPenetrating)
					{
						bEncroached = true;
					}
					else
					{
						// Compute where the base of the sweep ended up, and see if we can stand there
						const float DistanceToBase = (Hit.Time * TraceDist) + ShortCapsuleShape.Capsule.HalfHeight;
						const FVector NewLoc = FVector(PawnLocation.X, PawnLocation.Y, PawnLocation.Z - DistanceToBase + StandingCapsuleShape.Capsule.HalfHeight + SweepInflation + MIN_FLOOR_DIST / 2.f);
						bEncroached = MyWorld->OverlapBlockingTestByChannel(NewLoc, FQuat::Identity, CollisionChannel, StandingCapsuleShape, CapsuleParams, ResponseParam);
						if (!bEncroached)
						{
							// Intentionally not using MoveUpdatedComponent, where a horizontal plane constraint would prevent the base of the capsule from staying at the same spot.
							UpdatedComponent->MoveComponent(NewLoc - PawnLocation, UpdatedComponent->GetComponentQuat(), false, nullptr, EMoveComponentFlags::MOVECOMP_NoFlags, ETeleportType::TeleportPhysics);
						}
					}
				}
			}
		}
		else
		{
			// Expand while keeping base location the same.
			FVector StandingLocation = PawnLocation + FVector(0.f, 0.f, StandingCapsuleShape.GetCapsuleHalfHeight() - CurrentPronedHalfHeight);
			bEncroached = MyWorld->OverlapBlockingTestByChannel(StandingLocation, FQuat::Identity, CollisionChannel, StandingCapsuleShape, CapsuleParams, ResponseParam);

			if (bEncroached)
			{
				if (IsMovingOnGround())
				{
					// Something might be just barely overhead, try moving down closer to the floor to avoid it.
					constexpr float MinFloorDist = UE_KINDA_SMALL_NUMBER * 10.f;
					if (CurrentFloor.bBlockingHit && CurrentFloor.FloorDist > MinFloorDist)
					{
						StandingLocation.Z -= CurrentFloor.FloorDist - MinFloorDist;
						bEncroached = MyWorld->OverlapBlockingTestByChannel(StandingLocation, FQuat::Identity, CollisionChannel, StandingCapsuleShape, CapsuleParams, ResponseParam);
					}
				}				
			}

			if (!bEncroached)
			{
				// Commit the change in location.
				UpdatedComponent->MoveComponent(StandingLocation - PawnLocation, UpdatedComponent->GetComponentQuat(), false, nullptr, EMoveComponentFlags::MOVECOMP_NoFlags, ETeleportType::TeleportPhysics);
				bForceNextFloorCheck = true;
			}
		}

		// If still encroached then abort.
		if (bEncroached)
		{
			return;
		}

		PredCharacterOwner->SetIsProned(false);
	}	
	else
	{
		bShrinkProxyCapsule = true;
	}

	// Now call SetCapsuleSize() to cause touch/untouch events and actually grow the capsule
	CharacterOwner->GetCapsuleComponent()->SetCapsuleSize(DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), true);

	const float MeshAdjust = ScaledHalfHeightAdjust;
	AdjustProxyCapsuleSize();
	PredCharacterOwner->OnEndProne( HalfHeightAdjust, ScaledHalfHeightAdjust );

	// Don't smooth this change in mesh position
	if ((bClientSimulation && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy) || (IsNetMode(NM_ListenServer) && CharacterOwner->GetRemoteRole() == ROLE_AutonomousProxy))
	{
		FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
		if (ClientData)
		{
			ClientData->MeshTranslationOffset += FVector(0.f, 0.f, MeshAdjust);
			ClientData->OriginalMeshTranslationOffset = ClientData->MeshTranslationOffset;
		}
	}
}

bool UPredictedCharacterMovement::CanProneInCurrentState() const
{
	return (IsFalling() || IsMovingOnGround()) && UpdatedComponent && !UpdatedComponent->IsSimulatingPhysics();
}

bool UPredictedCharacterMovement::CanCrouchInCurrentState() const
{
	return Super::CanCrouchInCurrentState() && (!IsSprinting() || bCanSprintDuringCrouch);
}

bool UPredictedCharacterMovement::CanBoostInCurrentState() const
{
	return UpdatedComponent && !UpdatedComponent->IsSimulatingPhysics() && (IsFalling() || IsMovingOnGround());
}

bool UPredictedCharacterMovement::CanHasteInCurrentState() const
{
	return UpdatedComponent && !UpdatedComponent->IsSimulatingPhysics() && (IsFalling() || IsMovingOnGround());
}

bool UPredictedCharacterMovement::CanSlowInCurrentState() const
{
	return UpdatedComponent && !UpdatedComponent->IsSimulatingPhysics() && (IsFalling() || IsMovingOnGround());
}

bool UPredictedCharacterMovement::CanSnareInCurrentState() const
{
	return UpdatedComponent && !UpdatedComponent->IsSimulatingPhysics() && (IsFalling() || IsMovingOnGround());
}

bool UPredictedCharacterMovement::CanSlowFallInCurrentState() const
{
	return UpdatedComponent && !UpdatedComponent->IsSimulatingPhysics() && (IsFalling() || IsMovingOnGround());
}

bool UPredictedCharacterMovement::RemoveVelocityZOnSlowFallStart() const
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

void UPredictedCharacterMovement::ProcessModifierMovementState()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UModifierMovement::ProcessModifierMovementState);
	
	// Proxies get replicated Modifier state.
	if (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		// Check for a change in Modifier state. Players toggle Modifier by changing WantsModifier.

		{	// Boost
			const FGameplayTag PrevBoostLevel = GetBoostLevel();
			const uint8 PrevBoostLevelValue = BoostLevel;
			const TArray<FMovementModifier*> Boosts = { &BoostLocal, &BoostCorrection };
			if (FModifierStatics::ProcessModifiers(BoostLevel, BoostLevelMethod, BoostLevels,
				bLimitMaxBoosts, MaxBoosts, NO_MODIFIER, Boosts,
				[this] { return CanBoostInCurrentState(); }))
			{
				PredCharacterOwner->NotifyModifierChanged<uint8>(FModifierTags::Modifier_Boost,
					GetBoostLevel(), PrevBoostLevel, BoostLevel,
					PrevBoostLevelValue, NO_MODIFIER);
			}
		}

		{	// Haste
			const FGameplayTag PrevHasteLevel = GetHasteLevel();
			const uint8 PrevHasteLevelValue = HasteLevel;
			const TArray<FMovementModifier*> Hastes = { &HasteLocal, &HasteCorrection };
			if (FModifierStatics::ProcessModifiers(HasteLevel, HasteLevelMethod, HasteLevels,
				bLimitMaxHastes, MaxHastes, NO_MODIFIER, Hastes,
				[this] { return CanHasteInCurrentState(); }))
			{
				PredCharacterOwner->NotifyModifierChanged<uint8>(FModifierTags::Modifier_Haste,
					GetHasteLevel(), PrevHasteLevel, HasteLevel,
					PrevHasteLevelValue, NO_MODIFIER);
			}
		}

		{	// Slow
			const FGameplayTag PrevSlowLevel = GetSlowLevel();
			const uint8 PrevSlowLevelValue = SlowLevel;
			const TArray<FMovementModifier*> Slows = { &SlowLocal, &SlowCorrection };
			if (FModifierStatics::ProcessModifiers(SlowLevel, SlowLevelMethod, SlowLevels,
				bLimitMaxSlows, MaxSlows, NO_MODIFIER, Slows,
				[this] { return CanSlowInCurrentState(); }))
			{
				PredCharacterOwner->NotifyModifierChanged<uint8>(FModifierTags::Modifier_Slow,
					GetSlowLevel(), PrevSlowLevel, SlowLevel,
					PrevSlowLevelValue, NO_MODIFIER);
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
				PredCharacterOwner->NotifyModifierChanged<uint8>(FModifierTags::Modifier_Snare,
					GetSnareLevel(), PrevSnareLevel, SnareLevel,
					PrevSnareLevelValue, NO_MODIFIER);
			}
		}

		{	// SlowFall
			const FGameplayTag PrevSlowFallLevel = GetSlowFallLevel();
			const uint8 PrevSlowFallLevelValue = SlowFallLevel;
			const TArray<FMovementModifier*> SlowFalls = { &SlowFallLocal, &SlowFallCorrection };
			if (FModifierStatics::ProcessModifiers(SlowFallLevel, SlowFallLevelMethod, SlowFallLevels,
				bLimitMaxSlowFalls, MaxSlowFalls, NO_MODIFIER, SlowFalls,
				[this] { return CanSlowFallInCurrentState(); }))
			{
				PredCharacterOwner->NotifyModifierChanged<uint8>(FModifierTags::Modifier_SlowFall,
					GetSlowFallLevel(), PrevSlowFallLevel, SlowFallLevel,
					PrevSlowFallLevelValue, NO_MODIFIER);
			}
		}
	}
}

void UPredictedCharacterMovement::UpdateModifierMovementState()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UModifierMovement::UpdateModifierMovementState);
	
	if (!HasValidData())
	{
		return;
	}

	// Initialize Modifier levels if empty
	if (BoostLevels.Num() == 0)	{ for (const auto& Level : Boost) { BoostLevels.Add(Level.Key); } }
	if (HasteLevels.Num() == 0)	{ for (const auto& Level : Haste) { HasteLevels.Add(Level.Key); } }
	if (SlowLevels.Num() == 0)	{ for (const auto& Level : Slow) { SlowLevels.Add(Level.Key); } }
	if (SnareLevels.Num() == 0)	{ for (const auto& Level : Snare) { SnareLevels.Add(Level.Key); } }
	if (SlowFallLevels.Num() == 0) { for (const auto& Level : SlowFall) { SlowFallLevels.Add(Level.Key); } }

	// Update the modifiers
	ProcessModifierMovementState();
}

void UPredictedCharacterMovement::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	if (!HasValidData())
	{
		return;
	}

	// Detect when slow fall starts
	const bool bWasSlowFalling = IsSlowFallActive();

	// Update movement modifiers
	UpdateModifierMovementState();
	
	if (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		// Optionally clear Z velocity if slow fall just started
		if (!bWasSlowFalling && IsSlowFallActive() && RemoveVelocityZOnSlowFallStart())
		{
			Velocity.Z = 0.f;
		}
		
		/* We can't sprint if we're prone, we must clear input in the character */
		
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
		
		// Check for a change in Walk state. Players toggle Walk by changing bWantsToWalk.
		const bool bIsWalking = IsWalk();
		if (bIsWalking && (!bWantsToWalk || !CanWalkInCurrentState()))
		{
			UnWalk(false);
		}
		else if (!bIsWalking && bWantsToWalk && CanWalkInCurrentState())
		{
			Walk(false);
		}

		// Check for a change in Stroll state. Players toggle Stroll by changing bWantsToStroll.
		const bool bIsStrolling = IsStrolling();
		if (bIsStrolling && (!bWantsToStroll || !CanStrollInCurrentState()))
		{
			UnStroll(false);
		}
		else if (!bIsStrolling && bWantsToStroll && CanStrollInCurrentState())
		{
			Stroll(false);
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
		
		// Check for a change in crouch state. Players toggle crouch by changing bWantsToCrouch.
		const bool bIsCrouching = IsCrouching();
		if (bIsCrouching && (!bWantsToCrouch || !CanCrouchInCurrentState()))
		{
			UnCrouch(false);
		}
		else if (!bIsCrouching && bWantsToCrouch && CanCrouchInCurrentState())
		{
			if (IsProned())
			{
				bWantsToProne = false;
				UnProne(false);
			}
			if (!IsProned())
			{
				// Potential prone lock
				Crouch(false);
			}
		}

		// Check if prone lock timer has expired
		if (bProneLocked && !IsProneLockOnTimer())
		{
			SetProneLock(false);
		}
		
		// Check for a change in Prone state. Players toggle Prone by changing bWantsToProne.
		const bool bIsProned = IsProned();
		if (bIsProned && (!bWantsToProne || !CanProneInCurrentState()))
		{
			UnProne(false);
		}
		else if (!bIsProned && bWantsToProne && CanProneInCurrentState())
		{
			if (IsCrouching())
			{
				bWantsToCrouch = false;
				UnCrouch(false);
			}
			Prone(false);
		}
	}

	// We have repeated Super to handle crouch while also accommodating prone, so don't call it here
	// Super::UpdateCharacterStateBeforeMovement(DeltaSeconds);
}

void UPredictedCharacterMovement::UpdateCharacterStateAfterMovement(float DeltaSeconds)
{
	UpdateModifierMovementState();
	
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
		
		// UnProne if no longer allowed to be Proned
		if (IsProned() && !CanProneInCurrentState())
		{
			UnProne(false);
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

FClientAuthData* UPredictedCharacterMovement::ProcessClientAuthData()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPredictedCharacterMovement::ProcessClientAuthData);
	
	ClientAuthStack.SortByPriority();
	return ClientAuthStack.GetFirst();
}

FClientAuthParams UPredictedCharacterMovement::GetClientAuthParams(const FClientAuthData* ClientAuthData)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPredictedCharacterMovement::GetClientAuthParams);
	
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

void UPredictedCharacterMovement::GrantClientAuthority(FGameplayTag ClientAuthSource, float OverrideDuration)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPredictedCharacterMovement::GrantClientAuthority);
	
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
		UE_LOG(LogPredictedMovement, Error, TEXT("ClientAuthSource '%s' not found"), *ClientAuthSource.ToString());
#endif
	}
}

bool UPredictedCharacterMovement::ServerShouldGrantClientPositionAuthority(FVector& ClientLoc, FClientAuthData*& AuthData)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(UPredictedCharacterMovement::ServerShouldGrantClientPositionAuthority);
	
	AuthData = nullptr;
	
	// Already ignoring client movement error checks and correction
	if (bIgnoreClientMovementErrorChecksAndCorrection)
	{
		return false;
	}

	// Abort if client authority is not enabled
#if !UE_BUILD_SHIPPING
	if (PredMovementCVars::bClientAuthDisabled)
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

void UPredictedCharacterMovement::ServerMove_PerformMovement(const FCharacterNetworkMoveData& MoveData)
{
	// Server updates from the client's move data
	// Use this instead of UpdateFromCompressedFlags()

	// Client >> CallServerMovePacked ➜ ClientFillNetworkMoveData ➜ ServerMovePacked_ClientSend >> Server
	// >> ServerMovePacked_ServerReceive ➜ ServerMove_HandleMoveData ➜ ServerMove_PerformMovement
	
	const FPredictedNetworkMoveData& PredMoveData = static_cast<const FPredictedNetworkMoveData&>(MoveData);

	BoostLocal.ServerMove_PerformMovement(PredMoveData.BoostLocal.WantsModifiers);
	BoostCorrection.ServerMove_PerformMovement(PredMoveData.BoostCorrection.WantsModifiers);
	HasteLocal.ServerMove_PerformMovement(PredMoveData.HasteLocal.WantsModifiers);
	HasteCorrection.ServerMove_PerformMovement(PredMoveData.HasteCorrection.WantsModifiers);
	SlowLocal.ServerMove_PerformMovement(PredMoveData.SlowLocal.WantsModifiers);
	SlowCorrection.ServerMove_PerformMovement(PredMoveData.SlowCorrection.WantsModifiers);
	SlowFallLocal.ServerMove_PerformMovement(PredMoveData.SlowFallLocal.WantsModifiers);
	SlowFallCorrection.ServerMove_PerformMovement(PredMoveData.SlowFallCorrection.WantsModifiers);

	Super::ServerMove_PerformMovement(MoveData);
}

bool UPredictedCharacterMovement::ServerCheckClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel,
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
	const FPredictedNetworkMoveData* CurrentMoveData = static_cast<const FPredictedNetworkMoveData*>(GetCurrentNetworkMoveData());
    
	/*
	 * This will trigger a client correction if the Stamina value in the Client differs
	 * NetworkStaminaCorrectionThreshold (2.f default) units from the one in the server
	 * De-syncs can happen if we set the Stamina directly in Gameplay code (ie: GAS)
	 */
	if (!FMath::IsNearlyEqual(CurrentMoveData->Stamina, Stamina, NetworkStaminaCorrectionThreshold))
	{
		return true;
	}

	if (BoostCorrection.ServerCheckClientError(CurrentMoveData->BoostCorrection.Modifiers))	{ return true; }
	if (HasteCorrection.ServerCheckClientError(CurrentMoveData->HasteCorrection.Modifiers))	{ return true; }
	if (SlowCorrection.ServerCheckClientError(CurrentMoveData->SlowCorrection.Modifiers))	{ return true; }
	if (SnareServer.ServerCheckClientError(CurrentMoveData->SnareServer.Modifiers)) { return true; }
	if (SlowFallCorrection.ServerCheckClientError(CurrentMoveData->SlowFallCorrection.Modifiers)) { return true; }

	return false;
}

void UPredictedCharacterMovement::ServerMoveHandleClientError(float ClientTimeStamp, float DeltaTime,
	const FVector& Accel, const FVector& RelativeClientLocation, UPrimitiveComponent* ClientMovementBase,
	FName ClientBaseBoneName, uint8 ClientMovementMode)
{
	// This is the entry-point for determining how to handle client corrections; we can determine they are out of sync
	// and make any changes that suit our needs
	
	// Client >> TickComponent ➜ ControlledCharacterMove ➜ CallServerMovePacked ➜ ReplicateMoveToServer >> Server
	// >> ServerMove_PerformMovement ➜ ServerMoveHandleClientError

	// Process and grant client authority
#if !UE_BUILD_SHIPPING
	if (!PredMovementCVars::bClientAuthDisabled)
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

void UPredictedCharacterMovement::ClientAdjustPosition_Implementation(float TimeStamp, FVector NewLoc, FVector NewVel,
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

	const FPredictedMoveResponseDataContainer& MoveResponse = static_cast<const FPredictedMoveResponseDataContainer&>(GetMoveResponseDataContainer());
	ClientAuthAlpha = MoveResponse.bHasClientAuthAlpha ? MoveResponse.ClientAuthAlpha : 0.f;

	// Preserve client location relative to the partial client authority we have
	const FVector AuthLocation = FMath::Lerp<FVector>(UpdatedComponent->GetComponentLocation(), ClientLoc, ClientAuthAlpha);
	UpdatedComponent->SetWorldLocation(AuthLocation, false);
}

void UPredictedCharacterMovement::OnClientCorrectionReceived(class FNetworkPredictionData_Client_Character& ClientData,
	float TimeStamp, FVector NewLocation, FVector NewVelocity, UPrimitiveComponent* NewBase, FName NewBaseBoneName,
	bool bHasBase, bool bBaseRelativePosition, uint8 ServerMovementMode
#if UE_5_03_OR_LATER
	, FVector ServerGravityDirection
#endif
	)
{
	// This occurs on AutonomousProxy, when the server sends the move response
	// This is where we receive the snare, and can override the server's location, assuming it has given us authority

	// Server >> SendClientAdjustment() ➜ ServerSendMoveResponse() ➜ ServerFillResponseData() + MoveResponsePacked_ServerSend() >> Client
	// >> ClientMoveResponsePacked() ➜ ClientHandleMoveResponse() ➜ ClientAdjustPosition_Implementation() ➜ OnClientCorrectionReceived()
	
	const FPredictedMoveResponseDataContainer& MoveResponse = static_cast<const FPredictedMoveResponseDataContainer&>(GetMoveResponseDataContainer());

	// Stamina
	SetStamina(MoveResponse.Stamina);
	SetStaminaDrained(MoveResponse.bStaminaDrained);

	// Modifiers
	BoostCorrection.OnClientCorrectionReceived(MoveResponse.BoostCorrection.Modifiers);
	HasteCorrection.OnClientCorrectionReceived(MoveResponse.HasteCorrection.Modifiers);
	SlowCorrection.OnClientCorrectionReceived(MoveResponse.SlowCorrection.Modifiers);
	SnareServer.OnClientCorrectionReceived(MoveResponse.SnareServer.Modifiers);
	SlowFallCorrection.OnClientCorrectionReceived(MoveResponse.SlowFallCorrection.Modifiers);
	
	Super::OnClientCorrectionReceived(ClientData, TimeStamp, NewLocation, NewVelocity, NewBase, NewBaseBoneName,
		bHasBase, bBaseRelativePosition, ServerMovementMode, ServerGravityDirection);
}

bool UPredictedCharacterMovement::ClientUpdatePositionAfterServerUpdate()
{
	const bool bRealStroll = bWantsToStroll;
	const bool bRealWalk = bWantsToWalk;
	const bool bRealSprint = bWantsToSprint;
	const bool bRealProne = bWantsToProne;
	const bool bRealAimDownSights = bWantsToAimDownSights;

	// Modifiers
	const TModifierStack RealBoostLocal = BoostLocal.WantsModifiers;
	const TModifierStack RealBoostCorrection = BoostCorrection.WantsModifiers;
	const TModifierStack RealHasteLocal = HasteLocal.WantsModifiers;
	const TModifierStack RealHasteCorrection = HasteCorrection.WantsModifiers;
	const TModifierStack RealSlowLocal = SlowLocal.WantsModifiers;
	const TModifierStack RealSlowCorrection = SlowCorrection.WantsModifiers;
	const TModifierStack RealSlowFallLocal = SlowFallLocal.WantsModifiers;
	const TModifierStack RealSlowFallCorrection = SlowFallCorrection.WantsModifiers;

	// Client location authority
	const FVector ClientLoc = UpdatedComponent->GetComponentLocation();
	
	const bool bResult = Super::ClientUpdatePositionAfterServerUpdate();
	
	bWantsToStroll = bRealStroll;
	bWantsToWalk = bRealWalk;
	bWantsToSprint = bRealSprint;
	bWantsToProne = bRealProne;
	bWantsToAimDownSights = bRealAimDownSights;

	// Modifiers
	BoostLocal.WantsModifiers = RealBoostLocal;
	BoostCorrection.WantsModifiers = RealBoostCorrection;
	HasteLocal.WantsModifiers = RealHasteLocal;
	HasteCorrection.WantsModifiers = RealHasteCorrection;
	SlowLocal.WantsModifiers = RealSlowLocal;
	SlowCorrection.WantsModifiers = RealSlowCorrection;
	SlowFallLocal.WantsModifiers = RealSlowFallLocal;
	SlowFallCorrection.WantsModifiers = RealSlowFallCorrection;

	// Preserve client location relative to the partial client authority we have
	const FVector AuthLocation = FMath::Lerp<FVector>(UpdatedComponent->GetComponentLocation(), ClientLoc, ClientAuthAlpha);
	UpdatedComponent->SetWorldLocation(AuthLocation, false);

	return bResult;
}

void UPredictedCharacterMovement::MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags,
	const FVector& NewAccel)
{
	if (!HasValidData())
	{
		return;
	}
	
	if (const FPredictedNetworkMoveData* MoveData = static_cast<FPredictedNetworkMoveData*>(GetCurrentNetworkMoveData()))
	{
		// Extra set of compression flags
		UpdateFromCompressedFlagsExtra(MoveData->CompressedMoveFlagsExtra);
	}
	
	Super::MoveAutonomous(ClientTimeStamp, DeltaTime, CompressedFlags, NewAccel);
}

void UPredictedCharacterMovement::UpdateFromCompressedFlagsExtra(uint8 Flags)
{
	bWantsToStroll = (Flags & FPredictedSavedMove::FLAGEX_Stroll) != 0;
	bWantsToWalk = (Flags & FPredictedSavedMove::FLAGEX_Walk) != 0;
	bWantsToSprint = (Flags & FPredictedSavedMove::FLAGEX_Sprint) != 0;
	bWantsToProne = (Flags & FPredictedSavedMove::FLAGEX_Prone) != 0;
	bWantsToAimDownSights = (Flags & FPredictedSavedMove::FLAGEX_ADS) != 0;
}

uint8 FPredictedSavedMove::GetCompressedFlagsExtra() const
{
	uint8 Result = 0;

	if (bWantsToStroll)
	{
		Result |= FLAGEX_Stroll;
	}

	if (bWantsToWalk)
	{
		Result |= FLAGEX_Walk;
	}
	
	if (bWantsToSprint)
	{
		Result |= FLAGEX_Sprint;
	}

	if (bWantsToProne)
	{
		Result |= FLAGEX_Prone;
	}

	if (bWantsToAimDownSights)
	{
		Result |= FLAGEX_ADS;
	}

	return Result;
}

void FPredictedSavedMove::Clear()
{
	Super::Clear();

	bWantsToAimDownSights = false;

	bWantsToProne = false;
	bProneLocked = false;

	bWantsToStroll = false;
	bWantsToWalk = false;
	bWantsToSprint = false;
	
	bStaminaDrained = false;
	StartStamina = 0.f;
	EndStamina = 0.f;

	// Modifiers
	BoostLocal.Clear();
	BoostCorrection.Clear();
	HasteLocal.Clear();
	HasteCorrection.Clear();
	SlowLocal.Clear();
	SlowCorrection.Clear();
	SnareServer.Clear();
	SlowFallLocal.Clear();
	SlowFallCorrection.Clear();

	BoostLevel = NO_MODIFIER;
	HasteLevel = NO_MODIFIER;
	SlowLevel = NO_MODIFIER;
	SnareLevel = NO_MODIFIER;
	SlowFallLevel = NO_MODIFIER;
}

void FPredictedSavedMove::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel,
	FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	if (const UPredictedCharacterMovement* MoveComp = C ? Cast<UPredictedCharacterMovement>(C->GetCharacterMovement()) : nullptr)
	{
		bWantsToProne = MoveComp->bWantsToProne;
		bProneLocked = MoveComp->bProneLocked;
		bWantsToStroll = MoveComp->bWantsToStroll;
		bWantsToWalk = MoveComp->bWantsToWalk;
		bWantsToSprint = MoveComp->bWantsToSprint;
		bWantsToAimDownSights = MoveComp->bWantsToAimDownSights;

		// Modifiers
		BoostLocal.SetMoveFor(MoveComp->BoostLocal.WantsModifiers);
		BoostCorrection.SetMoveFor(MoveComp->BoostCorrection.WantsModifiers);
		HasteLocal.SetMoveFor(MoveComp->HasteLocal.WantsModifiers);
		HasteCorrection.SetMoveFor(MoveComp->HasteCorrection.WantsModifiers);
		SlowLocal.SetMoveFor(MoveComp->SlowLocal.WantsModifiers);
		SlowCorrection.SetMoveFor(MoveComp->SlowCorrection.WantsModifiers);
		SlowFallLocal.SetMoveFor(MoveComp->SlowFallLocal.WantsModifiers);
		SlowFallCorrection.SetMoveFor(MoveComp->SlowFallCorrection.WantsModifiers);
	}
}

void FPredictedSavedMove::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);
	
	if (UPredictedCharacterMovement* MoveComp = C ? Cast<UPredictedCharacterMovement>(C->GetCharacterMovement()) : nullptr)
	{
		MoveComp->bProneLocked = bProneLocked;
	}
}

bool FPredictedSavedMove::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter,
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
	
	const TSharedPtr<FPredictedSavedMove>& SavedMove = StaticCastSharedPtr<FPredictedSavedMove>(NewMove);

	if (bStaminaDrained != SavedMove->bStaminaDrained)
	{
		return false;
	}
	
	// We can only combine moves if they will result in the same state as if both moves were processed individually,
	// because the AutonomousProxy Client processes them individually prior to sending them to the server.

	if (!BoostLocal.CanCombineWith(SavedMove->BoostLocal.WantsModifiers)) { return false; }
	if (!BoostCorrection.CanCombineWith(SavedMove->BoostCorrection.WantsModifiers)) { return false; }
	if (!HasteLocal.CanCombineWith(SavedMove->HasteLocal.WantsModifiers)) { return false; }
	if (!HasteCorrection.CanCombineWith(SavedMove->HasteCorrection.WantsModifiers)) { return false; }
	if (!SlowLocal.CanCombineWith(SavedMove->SlowLocal.WantsModifiers)) { return false; }
	if (!SlowCorrection.CanCombineWith(SavedMove->SlowCorrection.WantsModifiers)) { return false; }
	if (!SlowFallLocal.CanCombineWith(SavedMove->SlowFallLocal.WantsModifiers)) { return false; }
	if (!SlowFallCorrection.CanCombineWith(SavedMove->SlowFallCorrection.WantsModifiers)) { return false; }

	// Without these, the change/start/stop events will trigger twice causing de-sync, so we don't combine moves if the level changes
	if (BoostLevel != SavedMove->BoostLevel) { return false; }
	if (HasteLevel != SavedMove->HasteLevel) { return false; }
	if (SlowLevel != SavedMove->SlowLevel) { return false; }
	if (SnareLevel != SavedMove->SnareLevel) { return false; }
	if (SlowFallLevel != SavedMove->SlowFallLevel) { return false; }
	
	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void FPredictedSavedMove::SetInitialPosition(ACharacter* C)
{
	// To counter the PendingMove potentially being processed twice, we need to make sure to reset the state of the CMC
	// back to the "InitialPosition" (state) it had before the PendingMove got processed.
	
	Super::SetInitialPosition(C);

	if (const UPredictedCharacterMovement* MoveComp = C ? Cast<UPredictedCharacterMovement>(C->GetCharacterMovement()) : nullptr)
	{
		// Retrieve the value from our CMC to revert the saved move value back to this.
		bStaminaDrained = MoveComp->IsStaminaDrained();
		StartStamina = MoveComp->GetStamina();

		// Modifiers
		BoostLocal.SetInitialPosition(MoveComp->BoostLocal.WantsModifiers);
		BoostCorrection.SetInitialPosition(MoveComp->BoostCorrection.WantsModifiers);
		HasteLocal.SetInitialPosition(MoveComp->HasteLocal.WantsModifiers);
		HasteCorrection.SetInitialPosition(MoveComp->HasteCorrection.WantsModifiers);
		SlowLocal.SetInitialPosition(MoveComp->SlowLocal.WantsModifiers);
		SlowCorrection.SetInitialPosition(MoveComp->SlowCorrection.WantsModifiers);
		SlowFallLocal.SetInitialPosition(MoveComp->SlowFallLocal.WantsModifiers);
		SlowFallCorrection.SetInitialPosition(MoveComp->SlowFallCorrection.WantsModifiers);

		BoostLevel = MoveComp->BoostLevel;
		HasteLevel = MoveComp->HasteLevel;
		SlowLevel = MoveComp->SlowLevel;
		SnareLevel = MoveComp->SnareLevel;
		SlowFallLevel = MoveComp->SlowFallLevel;
	}
}

void FPredictedSavedMove::CombineWith(const FSavedMove_Character* OldMove, ACharacter* C,
	APlayerController* PC, const FVector& OldStartLocation)
{
	Super::CombineWith(OldMove, C, PC, OldStartLocation);

	const FPredictedSavedMove* SavedOldMove = static_cast<const FPredictedSavedMove*>(OldMove);

	if (UPredictedCharacterMovement* MoveComp = C ? Cast<UPredictedCharacterMovement>(C->GetCharacterMovement()) : nullptr)
	{
		MoveComp->SetStamina(SavedOldMove->StartStamina);
		MoveComp->SetStaminaDrained(SavedOldMove->bStaminaDrained);

		// Modifiers
		MoveComp->BoostLocal.CombineWith(SavedOldMove->BoostLocal.WantsModifiers);
		MoveComp->BoostCorrection.CombineWith(SavedOldMove->BoostCorrection.WantsModifiers);
		MoveComp->HasteLocal.CombineWith(SavedOldMove->HasteLocal.WantsModifiers);
		MoveComp->HasteCorrection.CombineWith(SavedOldMove->HasteCorrection.WantsModifiers);
		MoveComp->SlowLocal.CombineWith(SavedOldMove->SlowLocal.WantsModifiers);
		MoveComp->SlowCorrection.CombineWith(SavedOldMove->SlowCorrection.WantsModifiers);
		MoveComp->SlowFallLocal.CombineWith(SavedOldMove->SlowFallLocal.WantsModifiers);
		MoveComp->SlowFallCorrection.CombineWith(SavedOldMove->SlowFallCorrection.WantsModifiers);

		MoveComp->BoostLevel = SavedOldMove->BoostLevel;
		MoveComp->HasteLevel = SavedOldMove->HasteLevel;
		MoveComp->SlowLevel = SavedOldMove->SlowLevel;
		MoveComp->SnareLevel = SavedOldMove->SnareLevel;
		MoveComp->SlowFallLevel = SavedOldMove->SlowFallLevel;
	}
}

void FPredictedSavedMove::PostUpdate(ACharacter* C, EPostUpdateMode PostUpdateMode)
{
	// When considering whether to delay or combine moves, we need to compare the move at the start and the end
	if (const UPredictedCharacterMovement* MoveComp = C ? Cast<UPredictedCharacterMovement>(C->GetCharacterMovement()) : nullptr)
	{
		EndStamina = MoveComp->GetStamina();

		// Modifiers
		BoostCorrection.PostUpdate(MoveComp->BoostCorrection.Modifiers);
		HasteCorrection.PostUpdate(MoveComp->HasteCorrection.Modifiers);
		SlowCorrection.PostUpdate(MoveComp->SlowCorrection.Modifiers);
		SnareServer.PostUpdate(MoveComp->SnareServer.Modifiers);
		SlowFallCorrection.PostUpdate(MoveComp->SlowFallCorrection.Modifiers);

		if (PostUpdateMode == PostUpdate_Record)
		{
			// Don't combine moves if the properties changed over the course of the move
			if (bStaminaDrained != MoveComp->IsStaminaDrained())
			{
				bForceNoCombine = true;
			}
		}
	}

	Super::PostUpdate(C, PostUpdateMode);
}

bool FPredictedSavedMove::IsImportantMove(const FSavedMovePtr& LastAckedMove) const
{
	// Important moves get sent again if not acked by the server
	
	const TSharedPtr<FPredictedSavedMove>& SavedMove = StaticCastSharedPtr<FPredictedSavedMove>(LastAckedMove);

	if (BoostLocal.IsImportantMove(SavedMove->BoostLocal.WantsModifiers)) { return true; }
	if (BoostCorrection.IsImportantMove(SavedMove->BoostCorrection.WantsModifiers))	{ return true; }
	if (HasteLocal.IsImportantMove(SavedMove->HasteLocal.WantsModifiers)) { return true; }
	if (HasteCorrection.IsImportantMove(SavedMove->HasteCorrection.WantsModifiers)) { return true; }
	if (SlowLocal.IsImportantMove(SavedMove->SlowLocal.WantsModifiers)) { return true; }
	if (SlowCorrection.IsImportantMove(SavedMove->SlowCorrection.WantsModifiers)) { return true; }
	if (SlowFallLocal.IsImportantMove(SavedMove->SlowFallLocal.WantsModifiers)) { return true; }
	if (SlowFallCorrection.IsImportantMove(SavedMove->SlowFallCorrection.WantsModifiers)) { return true; }
	
	return Super::IsImportantMove(LastAckedMove);
}

FSavedMovePtr FPredictedNetworkPredictionData_Client::AllocateNewMove()
{
	return MakeShared<FPredictedSavedMove>();
}

void UPredictedCharacterMovement::TickCharacterPose(float DeltaTime)
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

FNetworkPredictionData_Client* UPredictedCharacterMovement::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UPredictedCharacterMovement* MutableThis = const_cast<UPredictedCharacterMovement*>(this);
		MutableThis->ClientPredictionData = new FPredictedNetworkPredictionData_Client(*this);
	}

	return ClientPredictionData;
}
