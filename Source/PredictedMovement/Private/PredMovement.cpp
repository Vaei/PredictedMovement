// Copyright (c) 2023 Jared Taylor


#include "PredMovement.h"

#include "PredCharacter.h"
#include "PredTags.h"
#include "Components/CapsuleComponent.h"

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

UPredMovement::UPredMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetNetworkMoveDataContainer(PredMoveDataContainer);
	SetMoveResponseDataContainer(PredMoveResponseDataContainer);

	// Defaults
	GroundFriction = 12.f;  // More grounded, less sliding
	RotationRate.Yaw = 540.f;
	
	// Strolling
	MaxAccelerationStrolling = 512.f;
	MaxWalkSpeedStrolling = 96.f;
	BrakingDecelerationStrolling = 512.f;
	GroundFrictionStrolling = 12.f;
	BrakingFrictionStrolling = 4.f;

	bWantsToStroll = false;

	// Walking
	MaxAcceleration = 1300.f;
	MaxWalkSpeed = 240.f;
	VelocityCheckMitigatorWalking = 0.98f;
	
	// Running
	MaxAccelerationRunning = 1600.f;
	MaxWalkSpeedRunning = 600.f;
	BrakingDecelerationRunning = 512.f;
	GroundFrictionRunning = 12.f;
	BrakingFrictionRunning = 4.f;
	VelocityCheckMitigatorRunning = 0.98f;

	bWantsToWalk = false;
	
	// Sprinting
	bUseMaxAccelerationSprintingOnlyAtSpeed = true;
	MaxAccelerationSprinting = 2400.f;
	MaxWalkSpeedSprinting = 860.f;
	BrakingDecelerationSprinting = 512.f;
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

	// Init activation sources
	Boost.ActivationSource		= EModifierActivationSource::LocalPredicted;		// We apply this to ourselves
	Haste.ActivationSource		= EModifierActivationSource::LocalPredicted;		// We apply this to ourselves
	Slow.ActivationSource		= EModifierActivationSource::LocalPredicted;		// We apply this to ourselves
	SlowFall.ActivationSource	= EModifierActivationSource::LocalPredicted;		// We apply this to ourselves
	Snare.ActivationSource		= EModifierActivationSource::ServerInitiated;		// Others apply this to us

	// Init data types used for levels as gameplay tags instead of enums
	Boost.LevelType		= EModifierLevelType::FGameplayTag;
	Haste.LevelType		= EModifierLevelType::FGameplayTag;
	Slow.LevelType		= EModifierLevelType::FGameplayTag;
	SlowFall.LevelType	= EModifierLevelType::FGameplayTag;
	Snare.LevelType		= EModifierLevelType::FGameplayTag;

	// Init Boost Levels
	Boost.ModifierLevelTags.AddTagFast(FPredTags::Modifier_Type_Local_Boost_Test); 		// 1.50x Speed Boost
	BoostLevels.Add(FPredTags::Modifier_Type_Local_Boost_Test, { 1.50f });

	// Init Haste Levels
	Haste.ModifierLevelTags.AddTagFast(FPredTags::Modifier_Type_Local_Haste_Test); 		// 1.50x Sprint Haste
	HasteLevels.Add(FPredTags::Modifier_Type_Local_Haste_Test, { 1.50f });

	// Init Slow Levels
	Slow.ModifierLevelTags.AddTagFast(FPredTags::Modifier_Type_Local_Slow_Test); 		// 0.50x Speed Slow
	SlowLevels.Add(FPredTags::Modifier_Type_Local_Slow_Test, { 0.50f });
	
	// Init SlowFall Levels
	SlowFall.ModifierLevelTags.AddTagFast(FPredTags::Modifier_Type_Local_SlowFall_Test); // 1.00x Reduction for 0.00x Gravity

	FFallingModifierParams SlowFallParam = { 0.00f };
	SlowFallParam.bRemoveVelocityZOnStart = true;
	SlowFallParam.bOverrideAirControl = true;
	SlowFallParam.AirControlOverride = 1.f;  // Full air control
	SlowFallLevels.Add(FPredTags::Modifier_Type_Local_SlowFall_Test, SlowFallParam);
	
	// Init Snare Levels
	Snare.ModifierLevelTags.AddTagFast(FPredTags::Modifier_Type_Server_Snare_Test); 	// 0.75x Speed Snare
	SnareLevels.Add(FPredTags::Modifier_Type_Server_Snare_Test, { 0.25f });

	// Auth params for Snare
	static constexpr int32 DefaultPriority = 5;
	ClientAuthParams.FindOrAdd(FPredTags::ClientAuth_Snare, { DefaultPriority });
}

void FPredMoveResponseDataContainer::ServerFillResponseData(const UCharacterMovementComponent& CharacterMovement,
	const FClientAdjustment& PendingAdjustment)
{
	Super::ServerFillResponseData(CharacterMovement, PendingAdjustment);

	// Server ➜ Client
	const UPredMovement* MoveComp = Cast<UPredMovement>(&CharacterMovement);
	bStaminaDrained = MoveComp->IsStaminaDrained();
	Stamina = MoveComp->GetStamina();

	if (Snare != MoveComp->Snare)
	{
		Snare = MoveComp->Snare;
	}
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

		bool bOutSuccess;
		Snare.NetSerialize(Ar, PackageMap, bOutSuccess);
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
	Stamina = SavedMove.EndStamina;
	
	Boost = SavedMove.EndBoost;
	Haste = SavedMove.EndHaste;
	Slow = SavedMove.EndSlow;
	SlowFall = SavedMove.EndSlowFall;
	Snare = SavedMove.EndSnare;
}

bool FPredNetworkMoveData::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar,
	UPackageMap* PackageMap, ENetworkMoveType MoveType)
{
	Super::Serialize(CharacterMovement, Ar, PackageMap, MoveType);

	// Client ➜ Server
	SerializeOptionalValue<float>(Ar.IsSaving(), Ar, Stamina, 0.f);

	bool bOutSuccess;
	Boost.NetSerialize(Ar, PackageMap, bOutSuccess);
	Haste.NetSerialize(Ar, PackageMap, bOutSuccess);
	Slow.NetSerialize(Ar, PackageMap, bOutSuccess);
	SlowFall.NetSerialize(Ar, PackageMap, bOutSuccess);
	Snare.NetSerialize(Ar, PackageMap, bOutSuccess);
	
	return !Ar.IsError();
}

#if WITH_EDITOR
void UPredMovement::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
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

bool UPredMovement::HasValidData() const
{
	return Super::HasValidData() && IsValid(PredCharacterOwner);
}

void UPredMovement::PostLoad()
{
	Super::PostLoad();

	SetUpdatedCharacter();
}

void UPredMovement::OnRegister()
{
	Super::OnRegister();

#if WITH_EDITOR
	// Ensure MaxInputAngleSprint is computed from the Angle.
	SetMaxInputAngleSprint(MaxInputAngleSprint);
#endif
}

void UPredMovement::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	Super::SetUpdatedComponent(NewUpdatedComponent);
	SetUpdatedCharacter();
}

void UPredMovement::SetUpdatedCharacter()
{
	PredCharacterOwner = Cast<APredCharacter>(PawnOwner);

	Boost.Initialize(PredCharacterOwner, FPredTags::Modifier_Type_Local_Boost);
	Haste.Initialize(PredCharacterOwner, FPredTags::Modifier_Type_Local_Haste);
	Slow.Initialize(PredCharacterOwner, FPredTags::Modifier_Type_Local_Slow);
	SlowFall.Initialize(PredCharacterOwner, FPredTags::Modifier_Type_Local_SlowFall);
	Snare.Initialize(PredCharacterOwner, FPredTags::Modifier_Type_Server_Snare);
}

void UPredMovement::BeginPlay()
{
	Super::BeginPlay();

	// Broadcast events to initialize UI, etc.
	OnMaxStaminaChanged(GetMaxStamina(), GetMaxStamina());

	// Set stamina to max
	SetStamina(GetMaxStamina());
}

EPredGaitMode UPredMovement::GetGaitMode() const
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

EPredGaitMode UPredMovement::GetGaitSpeed() const
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

bool UPredMovement::IsStrolling() const
{
	return PredCharacterOwner && PredCharacterOwner->IsStrolling() && !IsSprintingInEffect();
}

bool UPredMovement::IsWalk() const
{
	return PredCharacterOwner && PredCharacterOwner->IsWalking() && !IsStrolling() && !IsSprintingInEffect();
}

bool UPredMovement::IsWalkingAtSpeed() const
{
	if (!IsWalk())
	{
		return false;
	}

	// When moving on ground we want to factor moving uphill or downhill so variations in terrain
	// aren't culled from the check. When falling, we don't want to factor fall velocity, only lateral
	const float Vel = IsMovingOnGround() ? Velocity.SizeSquared() : Velocity.SizeSquared2D();

	// When struggling to surpass walk speed, which can occur with heavy rotation and low acceleration, we
	// mitigate the check so there isn't a constant re-entry that can occur as an edge case
	return Vel >= FMath::Square(GetBasicMaxSpeed() * GetGaitSpeedFactor()) * VelocityCheckMitigatorWalking;
}

bool UPredMovement::IsRunning() const
{
	// We're running if we're not walking, sprinting, etc.
	return !PredCharacterOwner || (!IsStrolling() && !IsWalk() && !IsSprinting());
}

bool UPredMovement::IsRunningAtSpeed() const
{
	if (!IsRunning())
	{
		return false;
	}

	// When moving on ground we want to factor moving uphill or downhill so variations in terrain
	// aren't culled from the check. When falling, we don't want to factor fall velocity, only lateral
	const float Vel = IsMovingOnGround() ? Velocity.SizeSquared() : Velocity.SizeSquared2D();

	// When struggling to surpass walk speed, which can occur with heavy rotation and low acceleration, we
	// mitigate the check so there isn't a constant re-entry that can occur as an edge case
	return Vel >= FMath::Square(GetBasicMaxSpeed() * GetGaitSpeedFactor()) * VelocityCheckMitigatorRunning;
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

	// When struggling to surpass walk speed, which can occur with heavy rotation and low acceleration, we
	// mitigate the check so there isn't a constant re-entry that can occur as an edge case
	return Vel >= FMath::Square(GetBasicMaxSpeed() * GetGaitSpeedFactor()) * VelocityCheckMitigatorSprinting;
}

float UPredMovement::GetGaitSpeedFactor() const
{
	// Infinite recursion protection to avoid stack overflow -- we must exclude Haste from speed checks
	// e.g. IsSprintWithinAllowableInputAngle() ➜ IsSprintingAtSpeed() ➜ GetMaxSpeed() ➜ GetMaxSpeedScalar() ➜ IsSprintingInEffect() ➜ IsSprintWithinAllowableInputAngle()
	const float StaminaDrained = IsStaminaDrained() ? MaxWalkSpeedScalarStaminaDrained : 1.f;
	const float AimingDownSights = IsAimingDownSights() ? MaxWalkSpeedAimingDownSightsScalar : 1.f;
	const float BoostScalar = GetBoostSpeedScalar();
	const float SlowScalar = GetSlowSpeedScalar();
	const float SnareScalar = GetSnareSpeedScalar();
	return StaminaDrained * AimingDownSights * BoostScalar * SlowScalar * SnareScalar;
}

float UPredMovement::GetMaxAccelerationScalar() const
{
	const float StaminaDrained = IsStaminaDrained() ? MaxAccelerationScalarStaminaDrained : 1.f;
	const float AimingDownSights = IsAimingDownSights() ? MaxAccelerationAimingDownSightsScalar : 1.f;
	const float BoostScalar = GetBoostAccelScalar();
	const float SlowScalar = GetSlowAccelScalar();
	const float SnareScalar = GetSnareAccelScalar();
	const float HasteScalar = IsSprintingInEffect() ? GetHasteAccelScalar() : 1.f;
	return StaminaDrained * AimingDownSights * BoostScalar * SlowScalar * SnareScalar * HasteScalar;
}

float UPredMovement::GetMaxSpeedScalar() const
{
	const float StaminaDrained = IsStaminaDrained() ? MaxWalkSpeedScalarStaminaDrained : 1.f;
	const float AimingDownSights = IsAimingDownSights() ? MaxWalkSpeedAimingDownSightsScalar : 1.f;
	const float BoostScalar = GetBoostSpeedScalar();
	const float SlowScalar = GetSlowSpeedScalar();
	const float SnareScalar = GetSnareSpeedScalar();
	const float HasteScalar = IsSprintingInEffect() ? GetHasteSpeedScalar() : 1.f;
	return StaminaDrained * AimingDownSights * BoostScalar * SlowScalar * SnareScalar * HasteScalar;
}

float UPredMovement::GetMaxBrakingDecelerationScalar() const
{
	const float StaminaDrained = IsStaminaDrained() ? MaxBrakingDecelerationScalarStaminaDrained : 1.f;
	const float AimingDownSights = IsAimingDownSights() ? BrakingDecelerationAimingDownSightsScalar : 1.f;
	const float BoostScalar = GetBoostBrakingScalar();
	const float SlowScalar = GetSlowBrakingScalar();
	const float SnareScalar = GetSnareBrakingScalar();
	const float HasteScalar = IsSprintingInEffect() ? GetHasteBrakingScalar() : 1.f;
	return StaminaDrained * AimingDownSights * BoostScalar * SlowScalar * SnareScalar * HasteScalar;
}

float UPredMovement::GetGroundFrictionScalar() const
{
	const float AimingDownSights = IsAimingDownSights() ? GroundFrictionAimingDownSightsScalar : 1.f;
	const float BoostScalar = GetBoostGroundFrictionScalar();
	const float SlowScalar = GetSlowGroundFrictionScalar();
	const float SnareScalar = GetSnareGroundFrictionScalar();
	const float HasteScalar = IsSprintingInEffect() ? GetHasteGroundFrictionScalar() : 1.f;
	return AimingDownSights * BoostScalar * SlowScalar * SnareScalar * HasteScalar;
}

float UPredMovement::GetBrakingFrictionScalar() const
{
	const float AimingDownSights = IsAimingDownSights() ? BrakingFrictionAimingDownSightsScalar : 1.f;
	const float BoostScalar = GetBoostBrakingFrictionScalar();
	const float SlowScalar = GetSlowBrakingFrictionScalar();
	const float SnareScalar = GetSnareBrakingFrictionScalar();
	const float HasteScalar = IsSprintingInEffect() ? GetHasteBrakingFrictionScalar() : 1.f;
	return AimingDownSights * BoostScalar * SlowScalar * SnareScalar * HasteScalar;
}

float UPredMovement::GetGravityZScalar() const
{
	const float SlowFallScalar = GetSlowFallGravityZScalar();
	return SlowFallScalar;
}

float UPredMovement::GetRootMotionTranslationScalar() const
{
	// Allowing boost to affect root motion will increase attack range, dodge range, etc., it is disabled by default
	const float BoostScalar = ShouldBoostAffectRootMotion() ? GetBoostSpeedScalar() : 1.f;
	const float SlowScalar = ShouldSlowAffectRootMotion() ? GetSlowSpeedScalar() : 1.f;
	const float SnareScalar = ShouldSnareAffectRootMotion() ? GetSnareSpeedScalar() : 1.f;
	return BoostScalar * SlowScalar * SnareScalar;
}

float UPredMovement::GetMaxAcceleration() const
{ 
	const float Scalar = GetMaxAccelerationScalar();
	
	if (IsFlying())		{ return MaxAccelerationRunning * Scalar; }
	if (IsSwimming())	{ return MaxAccelerationRunning * Scalar; }
	if (IsProned())		{ return MaxAccelerationProned * Scalar; }
	if (IsCrouching())	{ return MaxAccelerationCrouched * Scalar; }

	if (IsSprintingInEffect())
	{
		return MaxAccelerationSprinting * Scalar;
	}

	if (!bUseMaxAccelerationSprintingOnlyAtSpeed && IsSprinting() && IsSprintWithinAllowableInputAngle())
	{
		return MaxAccelerationSprinting * Scalar;
	}

	const EPredGaitMode GaitMode = GetGaitMode();
	switch (GaitMode)
	{
	case EPredGaitMode::Stroll: return MaxAccelerationStrolling * Scalar;
	case EPredGaitMode::Walk: return MaxAcceleration * Scalar;
	case EPredGaitMode::Run:
	case EPredGaitMode::Sprint:
		return MaxAccelerationRunning * Scalar;
	}
	return 0.f;
}

float UPredMovement::GetBasicMaxSpeed() const
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

float UPredMovement::GetMaxSpeed() const
{
	return GetBasicMaxSpeed() * GetMaxSpeedScalar();
}

float UPredMovement::GetMaxBrakingDeceleration() const
{
	const float Scalar = GetMaxBrakingDecelerationScalar();
	
	if (IsFlying()) { return BrakingDecelerationFlying * Scalar; }
	if (IsFalling()) { return BrakingDecelerationFalling * Scalar; }
	if (IsSwimming()) { return BrakingDecelerationSwimming * Scalar; }
	if (IsProned()) { return BrakingDecelerationProned * Scalar; }
	if (IsCrouching()) { return BrakingDecelerationCrouched * Scalar; }

	const EPredGaitMode GaitMode = GetGaitMode();
	switch (GaitMode)
	{
		case EPredGaitMode::Stroll: return BrakingDecelerationStrolling * Scalar;
		case EPredGaitMode::Walk: return BrakingDecelerationWalking * Scalar;
		case EPredGaitMode::Run: return BrakingDecelerationRunning * Scalar;
		case EPredGaitMode::Sprint: return BrakingDecelerationSprinting * Scalar;
	}
	return 0.f;
}

float UPredMovement::GetGroundFriction(float DefaultGroundFriction) const
{
	const float Scalar = GetGroundFrictionScalar();

	if (IsProned()) { return GroundFrictionProned * Scalar; }
	if (IsCrouching()) { return GroundFrictionCrouched * Scalar; }

	const EPredGaitMode GaitMode = GetGaitMode();
	switch (GaitMode)
	{
		case EPredGaitMode::Stroll: return GroundFrictionStrolling * Scalar;
		case EPredGaitMode::Walk: return DefaultGroundFriction * Scalar;
		case EPredGaitMode::Run: return GroundFrictionRunning * Scalar;
		case EPredGaitMode::Sprint: return GroundFrictionSprinting * Scalar;
	}

	return DefaultGroundFriction;
}

float UPredMovement::GetBrakingFriction() const
{
	const float Scalar = GetBrakingFrictionScalar();

	if (IsProned()) { return BrakingFrictionProned * Scalar; }
	if (IsCrouching()) { return BrakingFrictionCrouched * Scalar; }

	const EPredGaitMode GaitMode = GetGaitMode();
	switch (GaitMode)
	{
		case EPredGaitMode::Stroll: return BrakingFrictionStrolling * Scalar;
		case EPredGaitMode::Walk: return BrakingFriction * Scalar;
		case EPredGaitMode::Run: return BrakingFrictionRunning * Scalar;
		case EPredGaitMode::Sprint: return BrakingFrictionSprinting * Scalar;
	}
	
	return BrakingFriction;
}

float UPredMovement::GetGravityZ() const
{
	return Super::GetGravityZ() * GetGravityZScalar();
}

FVector UPredMovement::GetAirControl(float DeltaTime, float TickAirControl, const FVector& FallAcceleration)
{
	// Slow fall air control
	if (const FFallingModifierParams* Params = GetSlowFallLevelParams())
	{
		TickAirControl = Params->GetAirControl(TickAirControl);
	}
	
	return Super::GetAirControl(DeltaTime, TickAirControl, FallAcceleration);
}

void UPredMovement::CalcStamina(float DeltaTime)
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

void UPredMovement::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	if (IsMovingOnGround())
	{
		Friction = GetGroundFriction(Friction);
	}
	
	CalcStamina(DeltaTime);
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

bool UPredMovement::CanWalkOffLedges() const
{
	if (!bCanWalkOffLedgesWhenProned && IsProned())
	{
		return false;
	}
	
	return Super::CanWalkOffLedges();
}

bool UPredMovement::CanAttemptJump() const
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

void UPredMovement::Stroll(bool bClientSimulation)
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

void UPredMovement::UnStroll(bool bClientSimulation)
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

bool UPredMovement::CanStrollInCurrentState() const
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

void UPredMovement::Walk(bool bClientSimulation)
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

void UPredMovement::UnWalk(bool bClientSimulation)
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

bool UPredMovement::CanWalkInCurrentState() const
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

void UPredMovement::SetMaxInputAngleSprint(float InMaxAngleSprint)
{
	MaxInputAngleSprint = FMath::Clamp(InMaxAngleSprint, 0.f, 180.0f);
	MaxInputNormalSprint = FMath::Cos(FMath::DegreesToRadians(MaxInputAngleSprint));
}

bool UPredMovement::IsSprinting() const
{
	return PredCharacterOwner && PredCharacterOwner->IsSprinting();
}

void UPredMovement::Sprint(bool bClientSimulation)
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

void UPredMovement::UnSprint(bool bClientSimulation)
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

bool UPredMovement::IsSprintWithinAllowableInputAngle() const
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

void UPredMovement::OnMaxStaminaChanged(float PrevValue, float NewValue)
{
	if (IsValid(PredCharacterOwner))
	{
		PredCharacterOwner->OnMaxStaminaChanged(NewValue, PrevValue);
	}

	// Ensure that Stamina is within the new MaxStamina
	SetStamina(GetStamina());
}

void UPredMovement::OnStaminaDrained()
{
	if (IsValid(PredCharacterOwner))
	{
		PredCharacterOwner->OnStaminaDrained();
	}
}

void UPredMovement::OnStaminaDrainRecovered()
{
	if (IsValid(PredCharacterOwner))
	{
		PredCharacterOwner->OnStaminaDrainRecovered();
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

bool UPredMovement::IsProneLocked() const
{
	if (CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
	{
		// Sim proxies don't prone lock
		return false;
	}
	return bProneLocked;
}

bool UPredMovement::IsProneLockOnTimer() const
{
	return GetRemainingProneLockCooldown() > 0.f;
}

float UPredMovement::GetRemainingProneLockCooldown() const
{
	const float CurrentTimestamp = GetTimestamp();
	const float RemainingCooldown = ProneLockDuration - (CurrentTimestamp - ProneLockTimestamp);
	return FMath::Clamp(RemainingCooldown, 0.f, ProneLockDuration);
}

void UPredMovement::SetProneLock(bool bLock)
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

float UPredMovement::GetTimestamp() const
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

bool UPredMovement::IsProned() const
{
	return PredCharacterOwner && PredCharacterOwner->IsProned();
}

void UPredMovement::Prone(bool bClientSimulation)
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

void UPredMovement::UnProne(bool bClientSimulation)
{
	if (!HasValidData())
	{
		return;
	}

	if (IsProneLocked())
	{
		return;
	}

	APredCharacter* DefaultCharacter = PredCharacterOwner->GetClass()->GetDefaultObject<APredCharacter>();

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

bool UPredMovement::CanProneInCurrentState() const
{
	return (IsFalling() || IsMovingOnGround()) && UpdatedComponent && !UpdatedComponent->IsSimulatingPhysics();
}

bool UPredMovement::CanCrouchInCurrentState() const
{
	return Super::CanCrouchInCurrentState() && (!IsSprinting() || bCanSprintDuringCrouch);
}

bool UPredMovement::CanBoostInCurrentState(FGameplayTag ModifierLevel) const
{
	return UpdatedComponent && !UpdatedComponent->IsSimulatingPhysics();
}

bool UPredMovement::CanHasteInCurrentState(FGameplayTag ModifierLevel) const
{
	return UpdatedComponent && !UpdatedComponent->IsSimulatingPhysics();
}

bool UPredMovement::CanSlowInCurrentState(FGameplayTag ModifierLevel) const
{
	return UpdatedComponent && !UpdatedComponent->IsSimulatingPhysics();
}

bool UPredMovement::CanSlowFallInCurrentState(FGameplayTag ModifierLevel) const
{
	return UpdatedComponent && !UpdatedComponent->IsSimulatingPhysics();
}

void UPredMovement::OnStartSlowFall()
{
	if (const FFallingModifierParams* Params = GetSlowFallLevelParams())
	{
		if (Params->bRemoveVelocityZOnStart)
		{
			Velocity.Z = 0.f;
		}
	}
}

bool UPredMovement::CanBeSnaredInCurrentState(FGameplayTag ModifierLevel) const
{
	return UpdatedComponent && !UpdatedComponent->IsSimulatingPhysics();
}

void UPredMovement::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	if (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		Boost.UpdateCharacterStateBeforeMovement(CanBoostInCurrentState(GetBoostLevel()));
		Haste.UpdateCharacterStateBeforeMovement(CanHasteInCurrentState(GetHasteLevel()));
		Slow.UpdateCharacterStateBeforeMovement(CanSlowInCurrentState(GetSlowLevel()));
		SlowFall.UpdateCharacterStateBeforeMovement(CanSlowFallInCurrentState(GetSlowFallLevel()));
		Snare.UpdateCharacterStateBeforeMovement(CanBeSnaredInCurrentState(GetSnareLevel()));
	
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

void UPredMovement::UpdateCharacterStateAfterMovement(float DeltaSeconds)
{
	if (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		Boost.UpdateCharacterStateAfterMovement(CanBoostInCurrentState(GetBoostLevel()));
		Haste.UpdateCharacterStateAfterMovement(CanHasteInCurrentState(GetHasteLevel()));
		Slow.UpdateCharacterStateAfterMovement(CanSlowInCurrentState(GetSlowLevel()));
		SlowFall.UpdateCharacterStateAfterMovement(CanSlowFallInCurrentState(GetSlowFallLevel()));
		Snare.UpdateCharacterStateAfterMovement(CanBeSnaredInCurrentState(GetSnareLevel()));
	
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

void UPredMovement::ServerMove_PerformMovement(const FCharacterNetworkMoveData& MoveData)
{
	// Server updates from the client's move data
	// Use this instead of UpdateFromCompressedFlags()

	// Client >> CallServerMovePacked ➜ ClientFillNetworkMoveData ➜ ServerMovePacked_ClientSend >> Server
	// >> ServerMovePacked_ServerReceive ➜ ServerMove_HandleMoveData ➜ ServerMove_PerformMovement
	
	const FPredNetworkMoveData& PredMoveData = static_cast<const FPredNetworkMoveData&>(MoveData);

	// << operator copies only the RequestedModifierLevel and Modifiers stack, i.e. requested state not current state

	// Local Predicted Modifiers Only
	Boost << PredMoveData.Boost;
	Haste << PredMoveData.Haste;
	Slow << PredMoveData.Slow;
	SlowFall << PredMoveData.SlowFall;
	
	Super::ServerMove_PerformMovement(MoveData);
}

void UPredMovement::ClientHandleMoveResponse(const FCharacterMoveResponseDataContainer& MoveResponse)
{
	// This occurs on AutonomousProxy, when the server sends the move response

	// Server >> SendClientAdjustment() ➜ ServerSendMoveResponse()
	// ➜ ServerFillResponseData() + MoveResponsePacked_ServerSend() >> Client
	// >> ClientMoveResponsePacked() ➜ ClientHandleMoveResponse()

	const FPredMoveResponseDataContainer& ModifierMoveResponse = static_cast<const FPredMoveResponseDataContainer&>(MoveResponse);

	// Apply Snare correction
	if (Snare != ModifierMoveResponse.Snare)
	{
		// << operator copies only the RequestedModifierLevel and Modifiers stack, i.e. requested state not current state
		Snare << ModifierMoveResponse.Snare;
	}

	Super::ClientHandleMoveResponse(MoveResponse);
}

bool UPredMovement::ClientUpdatePositionAfterServerUpdate()
{
	const FMovementModifier SavedBoost = Boost;
	const FMovementModifier SavedHaste = Haste;
	const FMovementModifier SavedSlow = Slow;
	const FMovementModifier SavedSlowFall = SlowFall;
	const FMovementModifier SavedSnare = Snare;
	const bool bRealStroll = bWantsToStroll;
	const bool bRealWalk = bWantsToWalk;
	const bool bRealSprint = bWantsToSprint;
	const bool bRealProne = bWantsToProne;
	const bool bRealAimDownSights = bWantsToAimDownSights;
	
	const bool bResult = Super::ClientUpdatePositionAfterServerUpdate();

	// operator<< will not copy the current level. We only take the requested level!
	Boost << SavedBoost;
	Haste << SavedHaste;
	Slow << SavedSlow;
	SlowFall << SavedSlowFall;
	Snare << SavedSnare;
	bWantsToStroll = bRealStroll;
	bWantsToWalk = bRealWalk;
	bWantsToSprint = bRealSprint;
	bWantsToProne = bRealProne;
	bWantsToAimDownSights = bRealAimDownSights;

	return bResult;
}

bool UPredMovement::CanDelaySendingMove(const FSavedMovePtr& NewMove)
{
	// Don't delay sending moves if the modifiers changed over the course of the move
	// ^= is the same as != but it also evaluates the current level rather than only the requested level
	
	const TSharedPtr<FSavedMove_Character_Pred>& SavedMove = StaticCastSharedPtr<FSavedMove_Character_Pred>(NewMove);

	if (SavedMove->Boost ^= SavedMove->EndBoost)
	{
		return false;
	}

	if (SavedMove->Haste ^= SavedMove->EndHaste)
	{
		return false;
	}

	if (SavedMove->Slow ^= SavedMove->EndSlow)
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

	// Server >> SendClientAdjustment() ➜ ServerSendMoveResponse() ➜ ServerFillResponseData() + MoveResponsePacked_ServerSend() >> Client
	// >> ClientMoveResponsePacked() ➜ ClientHandleMoveResponse() ➜ ClientAdjustPosition_Implementation() ➜ OnClientCorrectionReceived()
	
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

	// Trigger client correction if modifiers have different ModifierLevel or Modifiers Array
	// De-syncs can happen when we set the Modifier directly in Gameplay code (i.e. GAS)
	if (CurrentMoveData->Snare != Snare)
	{
		return true;
	}
    
	return false;
}

void UPredMovement::ServerMoveHandleClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel,
	const FVector& RelativeClientLocation, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName,
	uint8 ClientMovementMode)
{
	// This is the entry-point for determining how to handle client corrections; we can determine they are out of sync
	// and make any changes that suit our needs
	
	// Client >> TickComponent ➜ ControlledCharacterMove ➜ CallServerMove ➜ ReplicateMoveToServer >> Server
	// >> ServerMove ➜ ServerMoveHandleClientError

	const FPredNetworkMoveData* CurrentMoveData = static_cast<const FPredNetworkMoveData*>(GetCurrentNetworkMoveData());

	// Initialize client authority when the client is about to receive a correction that applies Snare
#if !UE_BUILD_SHIPPING
	if (!PredMovementCVars::bClientAuthDisabled)
#endif
	{
		// Update client authority time remaining
		ClientAuthStack.Update(DeltaTime);

		if (CurrentMoveData->Snare != Snare)
		{
			// Snare inits client authority here, however other states may want to do it elsewhere, this is not a requirement
			InitClientAuthority(FPredTags::ClientAuth_Snare);
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

void UPredMovement::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	bWantsToStroll = (Flags & FSavedMove_Character::FLAG_Custom_2) != 0;
	bWantsToWalk = (Flags & FSavedMove_Character::FLAG_Custom_3) != 0;
	bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
	bWantsToProne = (Flags & FSavedMove_Character::FLAG_Custom_1) != 0;
	bWantsToAimDownSights = (Flags & FSavedMove_Character::FLAG_Reserved_2) != 0;
}

uint8 FSavedMove_Character_Pred::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();

	if (bWantsToSprint)
	{
		Result |= FLAG_Custom_0;
	}

	if (bWantsToProne)
	{
		Result |= FLAG_Custom_1;
	}

	if (bWantsToStroll)
	{
		Result |= FLAG_Custom_2;
	}

	if (bWantsToWalk)
	{
		Result |= FLAG_Custom_3;
	}
	
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

	bWantsToProne = false;
	bProneLocked = false;

	bWantsToStroll = false;
	bWantsToWalk = false;
	bWantsToSprint = false;
	
	bStaminaDrained = false;
	StartStamina = 0.f;
	EndStamina = 0.f;

	Boost = {};
	Haste = {};
	Slow = {};
	SlowFall = {};
	Snare = {};
}

void FSavedMove_Character_Pred::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel,
	FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	if (const UPredMovement* MoveComp = C ? Cast<UPredMovement>(C->GetCharacterMovement()) : nullptr)
	{
		bWantsToProne = MoveComp->bWantsToProne;
		bProneLocked = MoveComp->bProneLocked;
		bWantsToStroll = MoveComp->bWantsToStroll;
		bWantsToWalk = MoveComp->bWantsToWalk;
		bWantsToSprint = MoveComp->bWantsToSprint;
		bWantsToAimDownSights = MoveComp->bWantsToAimDownSights;

		// Local Predicted Modifiers
		Boost = MoveComp->Boost;
		Haste = MoveComp->Haste;
		Slow = MoveComp->Slow;
		SlowFall = MoveComp->SlowFall;
	}
}

void FSavedMove_Character_Pred::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);
	
	if (UPredMovement* MoveComp = C ? Cast<UPredMovement>(C->GetCharacterMovement()) : nullptr)
	{
		MoveComp->bProneLocked = bProneLocked;
	}
}

bool FSavedMove_Character_Pred::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter,
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
	
	const TSharedPtr<FSavedMove_Character_Pred>& SavedMove = StaticCastSharedPtr<FSavedMove_Character_Pred>(NewMove);

	if (bStaminaDrained != SavedMove->bStaminaDrained)
	{
		return false;
	}
	
	// We can only combine moves if they will result in the same state as if both moves were processed individually,
	// because the AutonomousProxy Client processes them individually prior to sending them to the server.

	if (Boost != SavedMove->Boost)
	{
		return false;
	}

	if (Haste != SavedMove->Haste)
	{
		return false;
	}

	if (Slow != SavedMove->Slow)
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

void FSavedMove_Character_Pred::CombineWith(const FSavedMove_Character* OldMove, ACharacter* C,
	APlayerController* PC, const FVector& OldStartLocation)
{
	Super::CombineWith(OldMove, C, PC, OldStartLocation);

	const FSavedMove_Character_Pred* SavedOldMove = static_cast<const FSavedMove_Character_Pred*>(OldMove);

	if (UPredMovement* MoveComp = C ? Cast<UPredMovement>(C->GetCharacterMovement()) : nullptr)
	{
		MoveComp->SetStamina(SavedOldMove->StartStamina);
		MoveComp->SetStaminaDrained(SavedOldMove->bStaminaDrained);
		
		// Server Initiated Modifiers
		MoveComp->Snare << SavedOldMove->Snare;
	}
}

void FSavedMove_Character_Pred::SetInitialPosition(ACharacter* C)
{
	// To counter the PendingMove potentially being processed twice, we need to make sure to reset the state of the CMC
	// back to the "InitialPosition" (state) it had before the PendingMove got processed.
	
	Super::SetInitialPosition(C);

	if (const UPredMovement* MoveComp = C ? Cast<UPredMovement>(C->GetCharacterMovement()) : nullptr)
	{
		// Retrieve the value from our CMC to revert the saved move value back to this.
		bStaminaDrained = MoveComp->IsStaminaDrained();
		StartStamina = MoveComp->GetStamina();

		Boost = MoveComp->Boost;
		Haste = MoveComp->Haste;
		Slow = MoveComp->Slow;
		SlowFall = MoveComp->SlowFall;
		Snare = MoveComp->Snare;
	}
}

void FSavedMove_Character_Pred::PostUpdate(ACharacter* C, EPostUpdateMode PostUpdateMode)
{
	// When considering whether to delay or combine moves, we need to compare the move at the start and the end
	if (const UPredMovement* MoveComp = C ? Cast<UPredMovement>(C->GetCharacterMovement()) : nullptr)
	{
		EndStamina = MoveComp->GetStamina();

		EndBoost = MoveComp->Boost;
		EndHaste = MoveComp->Haste;
		EndSlow = MoveComp->Slow;
		EndSlowFall = MoveComp->SlowFall;
		EndSnare = MoveComp->Snare;
	
		if (PostUpdateMode == PostUpdate_Record)
		{
			// Don't combine moves if the properties changed over the course of the move
			if (bStaminaDrained != MoveComp->IsStaminaDrained())
			{
				bForceNoCombine = true;
			}

			// ^= is the same as != but it also evaluates the current level rather than only the requested level

			if (Boost ^= EndBoost)
			{
				bForceNoCombine = true;
			}

			if (Haste ^= EndHaste)
			{
				bForceNoCombine = true;
			}
			
			if (Slow ^= EndSlow)
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
	}

	Super::PostUpdate(C, PostUpdateMode);
}

bool FSavedMove_Character_Pred::IsImportantMove(const FSavedMovePtr& LastAckedMove) const
{
	const TSharedPtr<FSavedMove_Character_Pred>& SavedMove = StaticCastSharedPtr<FSavedMove_Character_Pred>(LastAckedMove);
	if (Boost != SavedMove->Boost || Boost != SavedMove->EndBoost)
	{
		return true;
	}

	if (Haste != SavedMove->Haste || Haste != SavedMove->EndHaste)
	{
		return true;
	}

	if (Slow != SavedMove->Slow || Slow != SavedMove->EndSlow)
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

FSavedMovePtr FNetworkPredictionData_Client_Character_Pred::AllocateNewMove()
{
	return MakeShared<FSavedMove_Character_Pred>();
}

FClientAuthData* UPredMovement::GetClientAuthData()
{
	ClientAuthStack.SortByPriority();
	return ClientAuthStack.GetFirst();
}

FClientAuthParams UPredMovement::GetClientAuthParams(const FClientAuthData* ClientAuthData)
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

void UPredMovement::InitClientAuthority(FGameplayTag ClientAuthSource, float OverrideDuration)
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
			ClientAuthStack.Stack.Add(FClientAuthData(ClientAuthSource, Duration, Params->Priority));

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

bool UPredMovement::ServerShouldGrantClientPositionAuthority(FVector& ClientLoc)
{
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
	FClientAuthData* ClientAuthData = GetClientAuthData();
	if (!ClientAuthData || !ClientAuthData->IsValid())
	{
		// No auth data, can't do anything
		return false;
	}

	// Get auth params
	const FClientAuthParams Params = GetClientAuthParams(ClientAuthData);

	// Disabled
	if (!Params.bEnableClientAuth)
	{
		return false;
	}

	// Validate auth data
#if !UE_BUILD_SHIPPING
	if (UNLIKELY(ClientAuthData->TimeRemaining <= 0.f))
	{
		// ServerMoveHandleClientError() should have removed the auth data already
		return ensure(false);
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
	if (LocDiff.SizeSquared() >= FMath::Square(Params.RejectClientAuthDistance))
	{
		OnClientAuthRejected(ClientLoc, ServerLoc, LocDiff);
		return false;
	}

	// If the client is not within the maximum allowable distance, accept the client position, but only partially
	if (LocDiff.Size() >= Params.MaxClientAuthDistance)
	{
		// Accept only a portion of the client's location
		ClientAuthData->Alpha = Params.MaxClientAuthDistance / LocDiff.Size();
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

void UPredMovement::TickCharacterPose(float DeltaTime)
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

FNetworkPredictionData_Client* UPredMovement::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UPredMovement* MutableThis = const_cast<UPredMovement*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Character_Pred(*this);
	}

	return ClientPredictionData;
}
