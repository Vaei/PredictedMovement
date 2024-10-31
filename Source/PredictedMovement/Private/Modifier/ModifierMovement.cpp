﻿// Copyright (c) 2023 Jared Taylor. All Rights Reserved.


#include "Modifier/ModifierMovement.h"

#include "Modifier/ModifierCharacter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModifierMovement)

void FModifierMoveResponseDataContainer::ServerFillResponseData(
	const UCharacterMovementComponent& CharacterMovement, const FClientAdjustment& PendingAdjustment)
{
	Super::ServerFillResponseData(CharacterMovement, PendingAdjustment);

	// Server ➜ Client
	const UModifierMovement* MoveComp = Cast<UModifierMovement>(&CharacterMovement);
	Snare = MoveComp->Snare;
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
	}

	return !Ar.IsError();
}

FModifierNetworkMoveDataContainer::FModifierNetworkMoveDataContainer()
{
    NewMoveData = &MoveData[0];
    PendingMoveData = &MoveData[1];
    OldMoveData = &MoveData[2];
}

void FModifierNetworkMoveData::ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType)
{
    Super::ClientFillNetworkMoveData(ClientMove, MoveType);

	// Client ➜ Server
	const FSavedMove_Character_Modifier& SavedMove = static_cast<const FSavedMove_Character_Modifier&>(ClientMove);
	Snare = SavedMove.Snare;
}

bool FModifierNetworkMoveData::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType)
{
    Super::Serialize(CharacterMovement, Ar, PackageMap, MoveType);

	// Client ➜ Server
	bool bOutSuccess;
	Snare.NetSerialize(Ar, PackageMap, bOutSuccess);
	
    return !Ar.IsError();
}

UModifierMovement::UModifierMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetMoveResponseDataContainer(ModifierMoveResponseDataContainer);
    SetNetworkMoveDataContainer(ModifierMoveDataContainer);
}

void UModifierMovement::PostLoad()
{
	Super::PostLoad();

	AModifierCharacter* ModifierCharacter = Cast<AModifierCharacter>(PawnOwner);
	Snare.Initialize(ModifierCharacter);
}

void UModifierMovement::SetUpdatedComponent(USceneComponent* NewUpdatedComponent)
{
	Super::SetUpdatedComponent(NewUpdatedComponent);

	AModifierCharacter* ModifierCharacter = Cast<AModifierCharacter>(PawnOwner);
	Snare.Initialize(ModifierCharacter);
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
	Super::CombineWith(OldMove, C, PC, OldStartLocation);

	const FSavedMove_Character_Modifier* SavedOldMove = static_cast<const FSavedMove_Character_Modifier*>(OldMove);

	if (UModifierMovement* MoveComp = C ? Cast<UModifierMovement>(C->GetCharacterMovement()) : nullptr)
	{
		MoveComp->Snare = SavedOldMove->Snare;
	}
}

void FSavedMove_Character_Modifier::Clear()
{
	Super::Clear();
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

float UModifierMovement::GetMaxSpeedScalar() const
{
	switch (Snare.GetModifierLevel<ESnare>())
	{
	case ESnare::Snare_25: return SnaredScalar_25;
	case ESnare::Snare_50: return SnaredScalar_50;
	case ESnare::Snare_75: return SnaredScalar_75;
	default: return 1.f;
	}
}

float UModifierMovement::GetRootMotionTranslationScalar() const
{
	return bSnareAffectsRootMotion ? GetMaxSpeedScalar() : 1.f;
}

float UModifierMovement::GetMaxSpeed() const
{
	return Super::GetMaxSpeed() * GetMaxSpeedScalar();
}

void UModifierMovement::TickCharacterPose(float DeltaTime)
{
	/*
	 * Epic made ACharacter::GetAnimRootMotionTranslationScale() non-virtual which was silly,
	 * so we have to duplicate the entire function. All we do here is scale
	 * CharacterOwner->GetAnimRootMotionTranslationScale() by GetRootMotionTranslationScalar()
	 * 
	 * This means our snare affects root motion as well.
	 */
	
	if (DeltaTime < UCharacterMovementComponent::MIN_TICK_TIME)
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

void UModifierMovement::OnClientCorrectionReceived(FNetworkPredictionData_Client_Character& ClientData, float TimeStamp,
	FVector NewLocation, FVector NewVelocity, UPrimitiveComponent* NewBase, FName NewBaseBoneName, bool bHasBase,
	bool bBaseRelativePosition, uint8 ServerMovementMode
#if UE_5_03_OR_LATER
	, FVector ServerGravityDirection)
#else
	)
#endif
{
	// ClientHandleMoveResponse() ➜ ClientAdjustPosition_Implementation() ➜ OnClientCorrectionReceived()
	const FModifierMoveResponseDataContainer& ModifierMoveResponse = static_cast<const FModifierMoveResponseDataContainer&>(GetMoveResponseDataContainer());

	if (Snare != ModifierMoveResponse.Snare)
	{
		// Snare has changed, apply correction
		Snare = ModifierMoveResponse.Snare;
		Snare.OnModifiersChanged();
	}

	Super::OnClientCorrectionReceived(ClientData, TimeStamp, NewLocation, NewVelocity, NewBase, NewBaseBoneName,
	bHasBase, bBaseRelativePosition, ServerMovementMode
#if UE_5_03_OR_LATER
	, ServerGravityDirection);
#else
	);
#endif
}

bool UModifierMovement::ServerCheckClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel, const FVector& ClientWorldLocation, const FVector& RelativeClientLocation, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode)
{
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