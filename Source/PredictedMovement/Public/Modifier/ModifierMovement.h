// Copyright (c) 2023 Jared Taylor. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "System/PredictedMovementVersioning.h"
#include "ModifierTypes.h"
#include "ModifierMovement.generated.h"

namespace FModifierTags
{
	PREDICTEDMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Modifier_Type_Buff_Boost);
	PREDICTEDMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Modifier_Type_Buff_Boost_25);
	PREDICTEDMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Modifier_Type_Buff_Boost_50);
	PREDICTEDMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Modifier_Type_Buff_Boost_75);
	PREDICTEDMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Modifier_Type_Debuff_Snare);
	PREDICTEDMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Modifier_Type_Debuff_Snare_25);
	PREDICTEDMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Modifier_Type_Debuff_Snare_50);
	PREDICTEDMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Modifier_Type_Debuff_Snare_75);
}

struct PREDICTEDMOVEMENT_API FModifierMoveResponseDataContainer : FCharacterMoveResponseDataContainer
{  // Server ➜ Client
	using Super = FCharacterMoveResponseDataContainer;

	FModifierData Snare;
	
	virtual void ServerFillResponseData(const UCharacterMovementComponent& CharacterMovement, const FClientAdjustment& PendingAdjustment) override;
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap) override;
};

struct PREDICTEDMOVEMENT_API FModifierNetworkMoveData : public FCharacterNetworkMoveData
{  // Client ➜ Server
public:
    typedef FCharacterNetworkMoveData Super;
 
    FModifierNetworkMoveData()
    {
    	
    }

	FModifierData Snare;
 
    virtual void ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType) override;
    virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType) override;
};
 
struct PREDICTEDMOVEMENT_API FModifierNetworkMoveDataContainer : public FCharacterNetworkMoveDataContainer
{  // Client ➜ Server
public:
    typedef FCharacterNetworkMoveDataContainer Super;
 
    FModifierNetworkMoveDataContainer();
 
private:
    FModifierNetworkMoveData MoveData[3];
};

/**
 * Supports stackable modifiers that can affect movement
 * 
 * Snare is used as an example of a debuff modifier, you can duplicate
 * the Snare implementation for your own modifiers
 */
UCLASS()
class PREDICTEDMOVEMENT_API UModifierMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UModifierMovement(const FObjectInitializer& ObjectInitializer);

	virtual void PostLoad() override;
	virtual void SetUpdatedComponent(USceneComponent* NewUpdatedComponent) override;
	virtual void SetUpdatedCharacter();

public:
	UPROPERTY(Category="Character Movement: Walking", EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTagContainer BoostLevels;

	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="x"))
	TMap<FGameplayTag, float> BoostScalars;

	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="x"))
	bool bBoostAffectsRootMotion = false;
	
public:
	UPROPERTY(Category="Character Movement: Walking", EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTagContainer SnareLevels;
	
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="x"))
	TMap<FGameplayTag, float> SnareScalars;

	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="x"))
	bool bSnareAffectsRootMotion = true;
	
public:
	/** Example implementation of an externally applied debuff modifier that can stack */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite)
	FModifierData Snare;

	UFUNCTION(BlueprintCallable, Category="Character Movement: Walking")
	const FGameplayTag& GetSnareLevel() const
	{
		return Snare.GetModifierLevel();
	}

	UFUNCTION(BlueprintPure, Category="Character Movement: Walking")
	bool IsSnared() const
	{
		return Snare.HasModifier();
	}

	/** Example implementation of a local predicted buff modifier that can stack */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite)
	FModifierData Boost;
	
	UFUNCTION(BlueprintCallable, Category="Character Movement: Walking")
	const FGameplayTag& GetBoostLevel() const
	{
		return Boost.GetModifierLevel();
	}

	UFUNCTION(BlueprintPure, Category="Character Movement: Walking")
	bool IsBoosted() const
	{
		return Boost.HasModifier();
	}

	virtual float GetBoostScalar() const;
	virtual float GetSnareScalar() const;
	virtual float GetMaxSpeedScalar() const;
	virtual float GetRootMotionTranslationScalar() const;
	virtual float GetMaxSpeed() const override;
	virtual void TickCharacterPose(float DeltaTime) override;
	
private:
	FModifierMoveResponseDataContainer ModifierMoveResponseDataContainer;

	FModifierNetworkMoveDataContainer ModifierMoveDataContainer;
	
public:
	virtual void OnClientCorrectionReceived(FNetworkPredictionData_Client_Character& ClientData, float TimeStamp,
	FVector NewLocation, FVector NewVelocity, UPrimitiveComponent* NewBase, FName NewBaseBoneName, bool bHasBase,
	bool bBaseRelativePosition, uint8 ServerMovementMode
#if UE_5_03_OR_LATER
	, FVector ServerGravityDirection) override;
#else
	) override;
#endif
	
	virtual bool ServerCheckClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel,
		const FVector& ClientWorldLocation, const FVector& RelativeClientLocation,
		UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode) override;

	/** Get prediction data for a client game. Should not be used if not running as a client. Allocates the data on demand and can be overridden to allocate a custom override if desired. Result must be a FNetworkPredictionData_Client_Character. */
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;
};

class PREDICTEDMOVEMENT_API FSavedMove_Character_Modifier : public FSavedMove_Character
{
	using Super = FSavedMove_Character;
	
public:
	FSavedMove_Character_Modifier()
	{
	}

	virtual ~FSavedMove_Character_Modifier() override
	{}

	FModifierData Boost;
	FModifierData Snare;

	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PrepMoveFor(ACharacter* C) override;
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;
	virtual void CombineWith(const FSavedMove_Character* OldMove, ACharacter* InCharacter, APlayerController* PC, const FVector& OldStartLocation) override;
	virtual void Clear() override;
	virtual void SetInitialPosition(ACharacter* C) override;
};

class PREDICTEDMOVEMENT_API FNetworkPredictionData_Client_Character_Modifier : public FNetworkPredictionData_Client_Character
{
	using Super = FNetworkPredictionData_Client_Character;

public:
	FNetworkPredictionData_Client_Character_Modifier(const UCharacterMovementComponent& ClientMovement)
	: Super(ClientMovement)
	{}

	virtual FSavedMovePtr AllocateNewMove() override;
};
