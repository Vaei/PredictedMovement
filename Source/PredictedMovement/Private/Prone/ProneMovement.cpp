// Copyright (c) 2023 Jared Taylor. All Rights Reserved.


#include "Prone/ProneMovement.h"

#include "Components/CapsuleComponent.h"
#include "Prone/ProneCharacter.h"
#include "Engine/World.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProneMovement)

UProneMovement::UProneMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MaxAccelerationProned = 256.f;
	MaxWalkSpeedProned = 168.f;
	BrakingDecelerationProned = 512.f;
	GroundFrictionProned = 3.f;
	BrakingFrictionProned = 1.f;

	PronedRadius = 60.f;
	PronedHalfHeight = 60.f;

	ProneLockDuration = 1.f;

	bCanWalkOffLedgesWhenProned = false;
	bWantsToProne = false;
	bProneLocked = false;
}

bool UProneMovement::HasValidData() const
{
	return Super::HasValidData() && IsValid(ProneCharacterOwner);
}

void UProneMovement::PostLoad()
{
	Super::PostLoad();

	ProneCharacterOwner = Cast<AProneCharacter>(PawnOwner);
}

void UProneMovement::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	Super::SetUpdatedComponent(NewUpdatedComponent);

	ProneCharacterOwner = Cast<AProneCharacter>(PawnOwner);
}

float UProneMovement::GetMaxAcceleration() const
{
	if (IsProned() && IsMovingOnGround())
	{
		return MaxAccelerationProned;
	}
	return Super::GetMaxAcceleration();
}

float UProneMovement::GetMaxSpeed() const
{
	if (IsProned() && IsMovingOnGround())
	{
		return MaxWalkSpeedProned;
	}
	return Super::GetMaxSpeed();
}

float UProneMovement::GetMaxBrakingDeceleration() const
{
	if (IsProned() && IsMovingOnGround())
	{
		return BrakingDecelerationProned;
	}
	return Super::GetMaxBrakingDeceleration();
}

void UProneMovement::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	if (IsProned() && IsMovingOnGround())
	{
		Friction = GroundFrictionProned;
	}
	Super::CalcVelocity(DeltaTime, Friction, bFluid, BrakingDeceleration);
}

void UProneMovement::ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration)
{
	if (IsProned() && IsMovingOnGround())
	{
		Friction = (bUseSeparateBrakingFriction ? BrakingFrictionProned : GroundFrictionProned);
	}
	Super::ApplyVelocityBraking(DeltaTime, Friction, BrakingDeceleration);
}

bool UProneMovement::CanWalkOffLedges() const
{
	if (!bCanWalkOffLedgesWhenProned && IsProned())
	{
		return false;
	}
	
	return Super::CanWalkOffLedges();
}

bool UProneMovement::CanAttemptJump() const
{
	return Super::CanAttemptJump() && !bWantsToProne;
}

bool UProneMovement::IsProneLocked() const
{
	if (CharacterOwner && CharacterOwner->GetLocalRole() == ROLE_SimulatedProxy)
	{
		// Sim proxies don't prone lock
		return false;
	}
	return bProneLocked;
}

bool UProneMovement::IsProneLockOnTimer() const
{
	return GetRemainingProneLockCooldown() > 0.f;
}

float UProneMovement::GetRemainingProneLockCooldown() const
{
	const float CurrentTimestamp = GetTimestamp();
	const float RemainingCooldown = ProneLockDuration - (CurrentTimestamp - ProneLockTimestamp);
	return FMath::Clamp(RemainingCooldown, 0.f, ProneLockDuration);
}

void UProneMovement::SetProneLock(bool bLock)
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

float UProneMovement::GetTimestamp() const
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

bool UProneMovement::IsProned() const
{
	return ProneCharacterOwner && ProneCharacterOwner->IsProned();
}

void UProneMovement::Prone(bool bClientSimulation)
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
			ProneCharacterOwner->SetIsProned(true);
		}
		ProneCharacterOwner->OnStartProne( 0.f, 0.f );
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

		ProneCharacterOwner->SetIsProned(true);
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
	ProneCharacterOwner->OnStartProne( HalfHeightAdjust, ScaledHalfHeightAdjust );

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

void UProneMovement::UnProne(bool bClientSimulation)
{
	if (!HasValidData())
	{
		return;
	}

	if (IsProneLocked())
	{
		return;
	}

	AProneCharacter* DefaultCharacter = ProneCharacterOwner->GetClass()->GetDefaultObject<AProneCharacter>();

	// See if collision is already at desired size.
	if (CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() == DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() &&
		CharacterOwner->GetCapsuleComponent()->GetUnscaledCapsuleRadius() == DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius())
	{
		if (!bClientSimulation)
		{
			ProneCharacterOwner->SetIsProned(false);
		}
		ProneCharacterOwner->OnEndProne( 0.f, 0.f );
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

		ProneCharacterOwner->SetIsProned(false);
	}	
	else
	{
		bShrinkProxyCapsule = true;
	}

	// Now call SetCapsuleSize() to cause touch/untouch events and actually grow the capsule
	CharacterOwner->GetCapsuleComponent()->SetCapsuleSize(DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleRadius(), DefaultCharacter->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight(), true);

	const float MeshAdjust = ScaledHalfHeightAdjust;
	AdjustProxyCapsuleSize();
	ProneCharacterOwner->OnEndProne( HalfHeightAdjust, ScaledHalfHeightAdjust );

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

bool UProneMovement::CanProneInCurrentState() const
{
	return (IsFalling() || IsMovingOnGround()) && UpdatedComponent && !UpdatedComponent->IsSimulatingPhysics();
}

void UProneMovement::UpdateCharacterStateBeforeMovement(float DeltaSeconds)
{
	// Proxies get replicated crouch state.
	if (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
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
	}

	// Proxies get replicated Prone state.
	if (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
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
}

void UProneMovement::UpdateCharacterStateAfterMovement(float DeltaSeconds)
{
	Super::UpdateCharacterStateAfterMovement(DeltaSeconds);

	// Proxies get replicated Prone state.
	if (CharacterOwner->GetLocalRole() != ROLE_SimulatedProxy)
	{
		// UnProne if no longer allowed to be Proned
		if (IsProned() && !CanProneInCurrentState())
		{
			UnProne(false);
		}
	}
}

bool UProneMovement::ClientUpdatePositionAfterServerUpdate()
{
	const bool bRealProne = bWantsToProne;
	const bool bResult = Super::ClientUpdatePositionAfterServerUpdate();
	bWantsToProne = bRealProne;

	return bResult;
}

void FSavedMove_Character_Prone::Clear()
{
	Super::Clear();

	bWantsToProne = false;
	bProneLocked = false;
}

void FSavedMove_Character_Prone::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel,
	FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	bWantsToProne = Cast<AProneCharacter>(C)->GetProneCharacterMovement()->bWantsToProne;
	bProneLocked = Cast<AProneCharacter>(C)->GetProneCharacterMovement()->bProneLocked;
}

void FSavedMove_Character_Prone::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);

	Cast<AProneCharacter>(C)->GetProneCharacterMovement()->bProneLocked = bProneLocked;
}

uint8 FSavedMove_Character_Prone::GetCompressedFlags() const
{
	uint8 Result = Super::GetCompressedFlags();

	if (bWantsToProne)
	{
		Result |= FLAG_Custom_1;
	}

	return Result;
}

void UProneMovement::UpdateFromCompressedFlags(uint8 Flags)
{
	Super::UpdateFromCompressedFlags(Flags);

	bWantsToProne = ((Flags & FSavedMove_Character::FLAG_Custom_1) != 0);
}

FSavedMovePtr FNetworkPredictionData_Client_Character_Prone::AllocateNewMove()
{
	return MakeShared<FSavedMove_Character_Prone>();
}

FNetworkPredictionData_Client* UProneMovement::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UProneMovement* MutableThis = const_cast<UProneMovement*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Character_Prone(*this);
	}

	return ClientPredictionData;
}
