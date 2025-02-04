// Copyright (c) 2023 Jared Taylor. All Rights Reserved.


#include "Prone/ProneCharacter.h"

#include "Net/UnrealNetwork.h"
#include "Prone/ProneMovement.h"
#include "Components/SkeletalMeshComponent.h"
#include "Net/Core/PushModel/PushModel.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ProneCharacter)

AProneCharacter::AProneCharacter(const FObjectInitializer& FObjectInitializer)
	: Super(FObjectInitializer.SetDefaultSubobjectClass<UProneMovement>(CharacterMovementComponentName))
{
	ProneMovement = Cast<UProneMovement>(GetCharacterMovement());

	PronedEyeHeight = 30.f;
}

void AProneCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// Legacy
	// DOREPLIFETIME_CONDITION(ThisClass, bIsProned, COND_SimulatedOnly);

	// Push Model
	FDoRepLifetimeParams SharedParams;
	SharedParams.bIsPushBased = true;
	SharedParams.Condition = COND_SimulatedOnly;

	DOREPLIFETIME_WITH_PARAMS_FAST(ThisClass, bIsProned, SharedParams);
}

void AProneCharacter::RecalculateBaseEyeHeight()
{
	if (bIsProned)
	{
		BaseEyeHeight = PronedEyeHeight;
	}
	else
	{
		Super::RecalculateBaseEyeHeight();
	}
}

void AProneCharacter::SetIsProned(bool bNewProned)
{
	if (bIsProned != bNewProned)
	{
		bIsProned = bNewProned;

		if (HasAuthority())
		{
			MARK_PROPERTY_DIRTY_FROM_NAME(ThisClass, bIsProned, this);  // Push-model
		}
	}
}

void AProneCharacter::OnRep_IsProned()
{
	if (ProneMovement)
	{
		if (bIsProned)
		{
			ProneMovement->bWantsToProne = true;
			ProneMovement->Prone(true);
		}
		else
		{
			ProneMovement->bWantsToProne = false;
			ProneMovement->UnProne(true);
		}
		ProneMovement->bNetworkUpdateReceived = true;
	}
}

void AProneCharacter::Prone(bool bClientSimulation)
{
	if (ProneMovement)
	{
		if (CanProne())
		{
			ProneMovement->bWantsToProne = true;
		}
	}
}

void AProneCharacter::UnProne(bool bClientSimulation)
{
	if (ProneMovement)
	{
		ProneMovement->bWantsToProne = false;
	}
}

bool AProneCharacter::CanProne() const
{
	return !bIsProned && GetRootComponent() && !GetRootComponent()->IsSimulatingPhysics();
}

void AProneCharacter::OnEndProne(float HeightAdjust, float ScaledHeightAdjust)
{
	RecalculateBaseEyeHeight();

	if (!bIsCrouched)
	{
		const ACharacter* DefaultChar = GetDefault<ACharacter>(GetClass());
		if (GetMesh() && DefaultChar->GetMesh())
		{
			FVector& MeshRelativeLocation = GetMesh()->GetRelativeLocation_DirectMutable();
			MeshRelativeLocation.Z = DefaultChar->GetMesh()->GetRelativeLocation().Z;
			BaseTranslationOffset.Z = MeshRelativeLocation.Z;
		}
		else
		{
			BaseTranslationOffset.Z = DefaultChar->GetBaseTranslationOffset().Z;
		}
	}
	K2_OnEndProne(HeightAdjust, ScaledHeightAdjust);
}

void AProneCharacter::OnStartProne(float HeightAdjust, float ScaledHeightAdjust)
{
	RecalculateBaseEyeHeight();

	const ACharacter* DefaultChar = GetDefault<ACharacter>(GetClass());
	if (GetMesh() && DefaultChar->GetMesh())
	{
		FVector& MeshRelativeLocation = GetMesh()->GetRelativeLocation_DirectMutable();
		MeshRelativeLocation.Z = DefaultChar->GetMesh()->GetRelativeLocation().Z + HeightAdjust;
		BaseTranslationOffset.Z = MeshRelativeLocation.Z;
	}
	else
	{
		BaseTranslationOffset.Z = DefaultChar->GetBaseTranslationOffset().Z + HeightAdjust;
	}

	K2_OnStartProne(HeightAdjust, ScaledHeightAdjust);
}