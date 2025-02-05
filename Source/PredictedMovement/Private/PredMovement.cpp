// Copyright (c) 2023 Jared Taylor. All Rights Reserved.


#include "PredMovement.h"

#include "PredCharacter.h"
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
	Stamina = SavedMove.EndStamina;
}

bool FPredNetworkMoveData::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar,
	UPackageMap* PackageMap, ENetworkMoveType MoveType)
{
	Super::Serialize(CharacterMovement, Ar, PackageMap, MoveType);

	SerializeOptionalValue<float>(Ar.IsSaving(), Ar, Stamina, 0.f);
	
	return !Ar.IsError();
}

UPredMovement::UPredMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetNetworkMoveDataContainer(PredMoveDataContainer);
	SetMoveResponseDataContainer(PredMoveResponseDataContainer);

	// Defaults
	GroundFriction = 12.f;  // More grounded, less sliding
	
	// Sprinting
	bUseMaxAccelerationSprintingOnlyAtSpeed = true;
	MaxAccelerationSprinting = 1024.f;
	MaxWalkSpeedSprinting = 600.f;
	BrakingDecelerationSprinting = 512.f;
	GroundFrictionSprinting = 12.f;
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

	// Aim Down Sights
	MaxWalkSpeedAimingDownSightsMultiplier = 0.333f;
	MaxAccelerationAimingDownSightsMultiplier = 0.666f;
	BrakingDecelerationAimingDownSightsMultiplier = 0.75f;
	GroundFrictionAimingDownSightsMultiplier = 1.f;
	BrakingFrictionAimingDownSightsMultiplier = 1.f;

	// Crouch
	SetCrouchedHalfHeight(54.f);
	
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

	// Set stamina to max
	SetStamina(GetMaxStamina());
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
	return (IsStaminaDrained() ? MaxAccelerationScalarStaminaDrained : 1.f) *
		(IsAimingDownSights() ? MaxAccelerationAimingDownSightsMultiplier : 1.f);
}

float UPredMovement::GetMaxSpeedMultiplier() const
{
	return (IsStaminaDrained() ? MaxWalkSpeedScalarStaminaDrained : 1.f) *
		(IsAimingDownSights() ? MaxWalkSpeedAimingDownSightsMultiplier : 1.f);
}

float UPredMovement::GetMaxBrakingDecelerationMultiplier() const
{
	return (IsStaminaDrained() ? MaxBrakingDecelerationScalarStaminaDrained : 1.f) *
		(IsAimingDownSights() ? BrakingDecelerationAimingDownSightsMultiplier : 1.f);
}

float UPredMovement::GetMaxAcceleration() const
{
	if (IsSprinting() && (!bUseMaxAccelerationSprintingOnlyAtSpeed || IsSprintingAtSpeed()))
	{
		return MaxAccelerationSprinting * GetMaxAccelerationMultiplier();
	}
	if (IsProned())
	{
		return MaxAccelerationProned * GetMaxAccelerationMultiplier();
	}
	return Super::GetMaxAcceleration() * GetMaxAccelerationMultiplier();
}

float UPredMovement::GetMaxSpeed() const
{
	const float Multiplier = GetMaxSpeedMultiplier();
	if (IsSprinting())
	{
		return MaxWalkSpeedSprinting * Multiplier;
	}
	if (IsProned())
	{
		return MaxWalkSpeedProned * Multiplier;
	}
	return Super::GetMaxSpeed() * Multiplier;
}

float UPredMovement::GetMaxBrakingDeceleration() const
{
	const float Multiplier = GetMaxBrakingDecelerationMultiplier();
	if (IsSprinting() && IsSprintingAtSpeed())
	{
		return BrakingDecelerationSprinting * Multiplier;
	}
	if (IsProned())
	{
		return BrakingDecelerationProned * Multiplier;
	}
	return Super::GetMaxBrakingDeceleration() * Multiplier;
}

float UPredMovement::GetGroundFrictionMultiplier() const
{
	return (IsAimingDownSights() ? GroundFrictionAimingDownSightsMultiplier : 1.f);
}

float UPredMovement::GetGroundFriction(float DefaultGroundFriction) const
{
	const float Multiplier = GetGroundFrictionMultiplier();
	if (IsSprinting())
	{
		return GroundFrictionSprinting * Multiplier;
	}
	if (IsProned())
	{
		return GroundFrictionProned * Multiplier;
	}
	return DefaultGroundFriction * Multiplier;
}

float UPredMovement::GetBrakingFrictionMultiplier() const
{
	return (IsAimingDownSights() ? BrakingFrictionAimingDownSightsMultiplier : 1.f);
}

float UPredMovement::GetBrakingFriction() const
{
	if (IsSprinting())
	{
		return BrakingFrictionSprinting * GetBrakingFrictionMultiplier();
	}
	if (IsProned())
	{
		return BrakingFrictionProned * GetBrakingFrictionMultiplier();
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

	// Cannot sprint backwards or sideways (ish)
	if (!IsSprintWithinAllowableInputAngle())
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

void UPredMovement::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	// Proxies get replicated Sprint state.
	if (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
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

	Super::ServerMove_PerformMovement(MoveData);
}

bool UPredMovement::ClientUpdatePositionAfterServerUpdate()
{
	const bool bRealSprint = bWantsToSprint;
	const bool bRealProne = bWantsToProne;
	const bool bRealAimDownSights = bWantsToAimDownSights;
	const bool bResult = Super::ClientUpdatePositionAfterServerUpdate();
	bWantsToSprint = bRealSprint;
	bWantsToProne = bRealProne;
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
		bWantsToProne = MoveComp->bWantsToProne;
		bProneLocked = MoveComp->bProneLocked;
		bWantsToSprint = MoveComp->bWantsToSprint;
		bWantsToAimDownSights = MoveComp->bWantsToAimDownSights;
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
			// Don't combine moves if the properties changed over the course of the move
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
