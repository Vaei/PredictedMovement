// Copyright (c) 2023 Jared Taylor. All Rights Reserved.


#include "Stamina/StaminaMovement.h"

#include "GameFramework/Character.h"

UStaminaMovement::UStaminaMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetMoveResponseDataContainer(StaminaMoveResponseDataContainer);
}

void FStaminaMoveResponseDataContainer::ServerFillResponseData(
	const UCharacterMovementComponent& CharacterMovement, const FClientAdjustment& PendingAdjustment)
{
	Super::ServerFillResponseData(CharacterMovement, PendingAdjustment);

	const UStaminaMovement* MoveComp = Cast<UStaminaMovement>(&CharacterMovement);

	bStaminaDrained = MoveComp->IsStaminaDrained();
	Stamina = MoveComp->GetStamina();
}

bool FStaminaMoveResponseDataContainer::Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar,
	UPackageMap* PackageMap)
{
	if (!Super::Serialize(CharacterMovement, Ar, PackageMap))
	{
		return false;
	}

	if (IsCorrection())
	{
		Ar << Stamina;
		Ar << bStaminaDrained;
	}

	return !Ar.IsError();
}

bool FSavedMove_Character_Stamina::CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter,
	float MaxDelta) const
{
	const TSharedPtr<FSavedMove_Character_Stamina>& SavedMove = StaticCastSharedPtr<FSavedMove_Character_Stamina>(NewMove);

	if (bStaminaDrained != SavedMove->bStaminaDrained)
	{
		return false;
	}

	return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
}

void FSavedMove_Character_Stamina::CombineWith(const FSavedMove_Character* OldMove, ACharacter* C,
	APlayerController* PC, const FVector& OldStartLocation)
{
	Super::CombineWith(OldMove, C, PC, OldStartLocation);

	const FSavedMove_Character_Stamina* IsekaiOldMove = static_cast<const FSavedMove_Character_Stamina*>(OldMove);

	if (UStaminaMovement* MoveComp = C ? Cast<UStaminaMovement>(C->GetCharacterMovement()) : nullptr)
	{
		MoveComp->SetStamina(IsekaiOldMove->Stamina);
		MoveComp->SetStaminaDrained(IsekaiOldMove->bStaminaDrained);
	}
}

void FSavedMove_Character_Stamina::Clear()
{
	Super::Clear();

	bStaminaDrained = false;
	Stamina = 0.f;
}

void FSavedMove_Character_Stamina::SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel,
	FNetworkPredictionData_Client_Character& ClientData)
{
	Super::SetMoveFor(C, InDeltaTime, NewAccel, ClientData);

	if (const UStaminaMovement* MoveComp = C ? Cast<UStaminaMovement>(C->GetCharacterMovement()) : nullptr)
	{
		bStaminaDrained = MoveComp->IsStaminaDrained();
		Stamina = MoveComp->GetStamina();
	}
}

void FSavedMove_Character_Stamina::PrepMoveFor(ACharacter* C)
{
	Super::PrepMoveFor(C);

	if (UStaminaMovement* MoveComp = C ? Cast<UStaminaMovement>(C->GetCharacterMovement()) : nullptr)
	{
		MoveComp->SetStaminaDrained(bStaminaDrained);
		MoveComp->SetStamina(Stamina);
	}
}

bool FSavedMove_Character_Stamina::IsImportantMove(const FSavedMovePtr& LastAckedMove) const
{
	const TSharedPtr<FSavedMove_Character_Stamina>& SavedAckedMove = StaticCastSharedPtr<FSavedMove_Character_Stamina>(LastAckedMove);

	if (bStaminaDrained != SavedAckedMove->bStaminaDrained)
	{
		return true;
	}

	return Super::IsImportantMove(LastAckedMove);
}

void FSavedMove_Character_Stamina::SetInitialPosition(ACharacter* C)
{
	Super::SetInitialPosition(C);

	if (const UStaminaMovement* MoveComp = C ? Cast<UStaminaMovement>(C->GetCharacterMovement()) : nullptr)
	{
		bStaminaDrained = MoveComp->IsStaminaDrained();
		Stamina = MoveComp->GetStamina();
	}
}

void UStaminaMovement::ClientHandleMoveResponse(const FCharacterMoveResponseDataContainer& MoveResponse)
{
	if (MoveResponse.IsCorrection())
	{
		if (!MoveResponse.bRootMotionSourceCorrection && !MoveResponse.bRootMotionMontageCorrection)
		{
			bool ValidMoveResponse = true;
			if (!HasValidData() || !IsActive())
			{
				ValidMoveResponse = false;
			}

			const FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
			check(ClientData);

			const FStaminaMoveResponseDataContainer& StaminaMoveResponse = static_cast<const FStaminaMoveResponseDataContainer&>(MoveResponse);

			// Make sure the base actor exists on this client.
			const bool bUnresolvedBase = StaminaMoveResponse.bHasBase && (StaminaMoveResponse.ClientAdjustment.NewBase == nullptr);
			if (bUnresolvedBase)
			{
				if (StaminaMoveResponse.ClientAdjustment.bBaseRelativePosition)
				{
					ValidMoveResponse = false;
				}
			}

			// Make sure the SavedMove still exists
			const int32 MoveIndex = ClientData->GetSavedMoveIndex(StaminaMoveResponse.ClientAdjustment.TimeStamp);
			if (MoveIndex == INDEX_NONE)
			{
				ValidMoveResponse = false;
			}

			// This is a bit weird, but the actual function the adjust the client position doesn't get the MoveResponse, but single variables
			// So we can't really override that one as we need the full container.
			// The Adjust Client Function does however filter actually correcting things based on the two conditions above, so we have to mimic them
			if (ValidMoveResponse)
			{
#if !UE_BUILD_SHIPPING
				const IConsoleVariable* VarNetShowCorrections = IConsoleManager::Get().FindConsoleVariable(*FString("p.NetShowCorrections"));
				if (VarNetShowCorrections != nullptr && VarNetShowCorrections->GetInt() != 0)
				{
					UE_LOG(LogTemp, Error, TEXT("[%s] Client Received Correction."), *FString(__FUNCTION__));
				}
#endif // !UE_BUILD_SHIPPPING

				SetStamina(StaminaMoveResponse.Stamina);
				SetStaminaDrained(StaminaMoveResponse.bStaminaDrained);
			}
		}
	}
	
	Super::ClientHandleMoveResponse(MoveResponse);
}

FNetworkPredictionData_Client* UStaminaMovement::GetPredictionData_Client() const
{
	if (ClientPredictionData == nullptr)
	{
		UStaminaMovement* MutableThis = const_cast<UStaminaMovement*>(this);
		MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_Character_Stamina(*this);
	}

	return ClientPredictionData;
}

FSavedMovePtr FNetworkPredictionData_Client_Character_Stamina::AllocateNewMove()
{
	return MakeShared<FSavedMove_Character_Stamina>();
}