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
	/**
	 * Scaling applied on a per-boost-level basis
	 * Every tag defined here must also be defined in the FModifierData Boost property
	 */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="x"))
	TMap<FGameplayTag, FCommonModifierParams> BoostScalars;

	/**
	 * If True, Boost will affect root motion
	 * This allows boosts to scale up root motion translation
	 * This is disabled by default because it can increase attack range, dodge range, etc.
	 */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="x"))
	bool bBoostAffectsRootMotion = false;
	
public:
	/**
	 * Scaling applied on a per-snare-level basis
	 * Every tag defined here must also be defined in the FModifierData Snare property
	 */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0", UIMin="0", ForceUnits="x"))
	TMap<FGameplayTag, FCommonModifierParams> SnareScalars;

	/**
	 * If True, Snare will affect root motion
	 * This allows snares to scale down root motion translation
	 */
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

	const FCommonModifierParams* GetBoostLevelParams() const { return BoostScalars.Find(GetBoostLevel()); }
	const FCommonModifierParams* GetSnareLevelParams() const { return SnareScalars.Find(GetSnareLevel()); }
	virtual float GetBoostSpeedScalar() const { return GetBoostLevelParams() ? GetBoostLevelParams()->MaxWalkSpeed : 1.f; }
	virtual float GetSnareSpeedScalar() const { return GetSnareLevelParams() ? GetSnareLevelParams()->MaxWalkSpeed : 1.f; }
	virtual float GetBoostAccelScalar() const { return GetBoostLevelParams() ? GetBoostLevelParams()->MaxAcceleration : 1.f; }
	virtual float GetSnareAccelScalar() const { return GetSnareLevelParams() ? GetSnareLevelParams()->MaxAcceleration : 1.f; }
	virtual float GetBoostBrakingScalar() const { return GetBoostLevelParams() ? GetBoostLevelParams()->BrakingDeceleration : 1.f; }
	virtual float GetSnareBrakingScalar() const { return GetSnareLevelParams() ? GetSnareLevelParams()->BrakingDeceleration : 1.f; }
	virtual float GetBoostGroundFrictionScalar() const { return GetBoostLevelParams() ? GetBoostLevelParams()->GroundFriction : 1.f; }
	virtual float GetSnareGroundFrictionScalar() const { return GetSnareLevelParams() ? GetSnareLevelParams()->GroundFriction : 1.f; }
	virtual float GetBoostBrakingFrictionScalar() const { return GetBoostLevelParams() ? GetBoostLevelParams()->BrakingFriction : 1.f; }
	virtual float GetSnareBrakingFrictionScalar() const { return GetSnareLevelParams() ? GetSnareLevelParams()->BrakingFriction : 1.f; }
	virtual float GetMaxSpeedScalar() const { return GetBoostSpeedScalar() * GetSnareSpeedScalar(); }
	virtual float GetMaxAccelScalar() const { return GetBoostAccelScalar() * GetSnareAccelScalar(); }
	virtual float GetMaxBrakingScalar() const { return GetBoostBrakingScalar() * GetSnareBrakingScalar(); }
	virtual float GetMaxGroundFrictionScalar() const { return GetBoostGroundFrictionScalar() * GetSnareGroundFrictionScalar(); }
	virtual float GetBrakingFrictionScalar() const { return GetBoostBrakingFrictionScalar() * GetSnareBrakingFrictionScalar(); }

	virtual float GetRootMotionTranslationScalar() const;
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxAcceleration() const override;
	virtual float GetMaxBrakingDeceleration() const override;
	virtual float GetGroundFriction() const;
	virtual float GetBrakingFriction() const;

	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	virtual void ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration) override;
	
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

protected:
	virtual void TickCharacterPose(float DeltaTime) override;  // ACharacter::GetAnimRootMotionTranslationScale() is non-virtual so we have to duplicate this entire function
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
