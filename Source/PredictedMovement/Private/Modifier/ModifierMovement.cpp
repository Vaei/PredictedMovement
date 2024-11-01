// Copyright (c) 2023 Jared Taylor. All Rights Reserved.


#include "Modifier/ModifierMovement.h"

#include "Modifier/ModifierCharacter.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ModifierMovement)

namespace FModifierTags
{
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Buff_Boost, "Modifier.Type.Buff.Boost");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Buff_Boost_25, "Modifier.Type.Buff.Boost.25");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Buff_Boost_50, "Modifier.Type.Buff.Boost.50");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Buff_Boost_75, "Modifier.Type.Buff.Boost.75");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Debuff_Snare, "Modifier.Type.Debuff.Snare");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Debuff_Snare_25, "Modifier.Type.Debuff.Snare.25");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Debuff_Snare_50, "Modifier.Type.Debuff.Snare.50");
	UE_DEFINE_GAMEPLAY_TAG(Modifier_Type_Debuff_Snare_75, "Modifier.Type.Debuff.Snare.75");
}

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

	// Init boost levels
	BoostLevels.AddTagFast(FModifierTags::Modifier_Type_Buff_Boost_25.GetTag());
	BoostLevels.AddTagFast(FModifierTags::Modifier_Type_Buff_Boost_50.GetTag());
	BoostLevels.AddTagFast(FModifierTags::Modifier_Type_Buff_Boost_75.GetTag());

	BoostScalars.Add(FModifierTags::Modifier_Type_Buff_Boost_25, 1.25f);
	BoostScalars.Add(FModifierTags::Modifier_Type_Buff_Boost_50, 1.50f);
	BoostScalars.Add(FModifierTags::Modifier_Type_Buff_Boost_75, 1.75f);

	// Init snare levels
	SnareLevels.AddTagFast(FModifierTags::Modifier_Type_Debuff_Snare_25.GetTag());
	SnareLevels.AddTagFast(FModifierTags::Modifier_Type_Debuff_Snare_50.GetTag());
	SnareLevels.AddTagFast(FModifierTags::Modifier_Type_Debuff_Snare_75.GetTag());
	
	SnareScalars.Add(FModifierTags::Modifier_Type_Debuff_Snare_25, 0.75f);
	SnareScalars.Add(FModifierTags::Modifier_Type_Debuff_Snare_50, 0.50f);
	SnareScalars.Add(FModifierTags::Modifier_Type_Debuff_Snare_75, 0.25f);
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
	AModifierCharacter* ModifierCharacter = Cast<AModifierCharacter>(PawnOwner);
	Boost.Initialize(ModifierCharacter, FModifierTags::Modifier_Type_Buff_Boost, BoostLevels);
	Snare.Initialize(ModifierCharacter, FModifierTags::Modifier_Type_Debuff_Snare, SnareLevels);
}

void FSavedMove_Character_Modifier::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel,
	class FNetworkPredictionData_Client_Character& ClientData)
{
	FSavedMove_Character::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	if (UModifierMovement* Movement = CastChecked<AModifierCharacter>(C)->GetModifierCharacterMovement())
	{
		Boost = Movement->Boost;
	}
}

void FSavedMove_Character_Modifier::PrepMoveFor(ACharacter* C)
{
	FSavedMove_Character::PrepMoveFor(C);

	if (UModifierMovement* Movement = CastChecked<AModifierCharacter>(C)->GetModifierCharacterMovement())
	{
		Movement->Boost = Boost;
	}
}

bool FSavedMove_Character_Modifier::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter,
	float MaxDelta) const
{
	const TSharedPtr<FSavedMove_Character_Modifier>& SavedMove = StaticCastSharedPtr<FSavedMove_Character_Modifier>(NewMove);

	if (Boost != SavedMove->Boost) { return false; }
	if (Snare != SavedMove->Snare) { return false; }

	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void FSavedMove_Character_Modifier::CombineWith(const FSavedMove_Character* OldMove, ACharacter* C,
	APlayerController* PC, const FVector& OldStartLocation)
{
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

float UModifierMovement::GetBoostScalar() const
{
	if (const float* Level = BoostScalars.Find(GetBoostLevel()))
	{
		return *Level;
	}
	return 1.f;
}

float UModifierMovement::GetSnareScalar() const
{
	if (const float* Level = SnareScalars.Find(GetSnareLevel()))
	{
		return *Level;
	}
	return 1.f;
}

float UModifierMovement::GetMaxSpeedScalar() const
{
	return GetBoostScalar() * GetSnareScalar();
}

float UModifierMovement::GetRootMotionTranslationScalar() const
{
	// Allowing boost to affect root motion will increase attack range, dodge range, etc., it is disabled by default
	return (bSnareAffectsRootMotion ? GetSnareScalar() : 1.f) * (bBoostAffectsRootMotion ? GetBoostScalar() : 1.f);
}

float UModifierMovement::GetMaxSpeed() const
{
	return Super::GetMaxSpeed() * GetMaxSpeedScalar();
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

	// If snare changed, apply correction
	Snare << ModifierMoveResponse.Snare;
	
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
	if (CurrentMoveData->Snare != Snare) {	return true; }
	
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

void UModifierMovement::TickCharacterPose(float DeltaTime)
{
	/*
	 * Epic made ACharacter::GetAnimRootMotionTranslationScale() non-virtual which was silly,
	 * so we have to duplicate the entire function. All we do here is scale
	 * CharacterOwner->GetAnimRootMotionTranslationScale() by GetRootMotionTranslationScalar()
	 * 
	 * This means our snare affects root motion as well.
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
