// Copyright (c) 2023 Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "ModifierTypes.h"
#include "ModifierMovement.generated.h"

namespace FModifierTags
{
	PREDICTEDMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Modifier_Type_Local_SlowFall);
	PREDICTEDMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Modifier_Type_Local_SlowFall_25);
	PREDICTEDMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Modifier_Type_Local_SlowFall_50);
	PREDICTEDMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Modifier_Type_Local_SlowFall_75);
	PREDICTEDMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Modifier_Type_Local_SlowFall_100);
	PREDICTEDMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Modifier_Type_Local_Boost);
	PREDICTEDMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Modifier_Type_Local_Boost_25);
	PREDICTEDMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Modifier_Type_Local_Boost_50);
	PREDICTEDMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Modifier_Type_Local_Boost_75);
	PREDICTEDMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Modifier_Type_Server_Snare);
	PREDICTEDMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Modifier_Type_Server_Snare_25);
	PREDICTEDMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Modifier_Type_Server_Snare_50);
	PREDICTEDMOVEMENT_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Modifier_Type_Server_Snare_75);
}

struct PREDICTEDMOVEMENT_API FModifierMoveResponseDataContainer : FCharacterMoveResponseDataContainer
{  // Server ➜ Client
	using Super = FCharacterMoveResponseDataContainer;

	FMovementModifier Snare;
	
	virtual void ServerFillResponseData(const UCharacterMovementComponent& CharacterMovement, const FClientAdjustment& PendingAdjustment) override;
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap) override;
};

struct PREDICTEDMOVEMENT_API FModifierNetworkMoveData : FCharacterNetworkMoveData
{  // Client ➜ Server
public:
    using Super = FCharacterNetworkMoveData;
 
    FModifierNetworkMoveData()
    {}

	FMovementModifier Boost;
	FMovementModifier SlowFall;
	FMovementModifier Snare;
	
    virtual void ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType) override;
    virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType) override;
};
 
struct PREDICTEDMOVEMENT_API FModifierNetworkMoveDataContainer : FCharacterNetworkMoveDataContainer
{  // Client ➜ Server
public:
    using Super = FCharacterNetworkMoveDataContainer;
 
    FModifierNetworkMoveDataContainer()
    {
    	NewMoveData = &MoveData[0];
    	PendingMoveData = &MoveData[1];
    	OldMoveData = &MoveData[2];
    }
 
private:
    FModifierNetworkMoveData MoveData[3];
};

/**
 * Supports stackable modifiers that can affect movement
 * 
 * Snare is used as an example of a Server modifier, you can duplicate
 * the Snare implementation for your own modifiers
 */
UCLASS()
class PREDICTEDMOVEMENT_API UModifierMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UModifierMovement(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	virtual void PostLoad() override;
	virtual void SetUpdatedComponent(USceneComponent* NewUpdatedComponent) override;
	virtual void SetUpdatedCharacter();

protected:
	/** Character movement component belongs to */
	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<AModifierCharacter> ModifierCharacterOwner;
	
public:
	/**
	 * Boost increases movement speed and acceleration
	 * Scaling applied on a per-boost-level basis
	 * Every tag defined here must also be defined in the FModifierData Boost property
	 */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite)
	TMap<FGameplayTag, FMovementModifierParams> BoostLevels;

	/**
	 * If True, Boost will affect root motion
	 * This allows boosts to scale up root motion translation
	 * This is disabled by default because it can increase attack range, dodge range, etc.
	 */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite)
	bool bBoostAffectsRootMotion = false;
	
public:
	/**
	 * Slow Fall reduces GravityScale
	 * Scaling applied on a per-slow-fall-level basis
	 * Every tag defined here must also be defined in the FModifierData SlowFall property
	 */
	UPROPERTY(Category="Character Movement: Jumping / Falling", EditAnywhere, BlueprintReadWrite)
	TMap<FGameplayTag, FFallingModifierParams> SlowFallLevels;
	
public:
	/**
	 * Snare reduces movement speed and acceleration
	 * Scaling applied on a per-snare-level basis
	 * Every tag defined here must also be defined in the FModifierData Snare property
	 */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite)
	TMap<FGameplayTag, FMovementModifierParams> SnareLevels;

	/**
	 * If True, Snare will affect root motion
	 * This allows snares to scale down root motion translation
	 */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite, meta=(ForceUnits="x"))
	bool bSnareAffectsRootMotion = true;

public:
	/** Example implementation of a local predicted Local modifier that can stack */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite)
	FMovementModifier Boost;
	
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

	UFUNCTION(BlueprintPure, Category="Character Movement: Walking")
	bool WantsBoost() const
	{
		return Boost.WantsModifier();
	}

	/**
	 * @param ModifierLevel The level to check for - optional, leave empty if check is generic
	 * @return True if we can boost
	 */
	UFUNCTION(BlueprintPure, Category="Character Movement: Walking", meta=(Categories="Modifier.Type.Local.Boost"))
	virtual bool CanBoostInCurrentState(FGameplayTag ModifierLevel) const;

	virtual void OnStartBoost() {}
	virtual void OnEndBoost() {}

public:
	/** Example implementation of a local predicted Local modifier that can stack */
	UPROPERTY(Category="Character Movement: Jumping / Falling", EditAnywhere, BlueprintReadWrite)
	FMovementModifier SlowFall;
	
	UFUNCTION(BlueprintCallable, Category="Character Movement: Jumping / Falling")
	const FGameplayTag& GetSlowFallLevel() const
	{
		return SlowFall.GetModifierLevel();
	}

	UFUNCTION(BlueprintPure, Category="Character Movement: Jumping / Falling")
	bool IsSlowFall() const
	{
		return SlowFall.HasModifier();
	}
	
	UFUNCTION(BlueprintPure, Category="Character Movement: Jumping / Falling")
	bool WantsSlowFall() const
	{
		return SlowFall.WantsModifier();
	}

	/**
	 * @param ModifierLevel The level to check for - optional, leave empty if check is generic
	 * @return True if we can slow fall
	 */
	UFUNCTION(BlueprintPure, Category="Character Movement: Jumping / Falling", meta=(Categories="Modifier.Type.Local.SlowFall"))
	virtual bool CanSlowFallInCurrentState(FGameplayTag ModifierLevel) const;

	virtual void OnStartSlowFall();
	virtual void OnEndSlowFall() {}

public:
	/** Client auth parameters mapped to a source gameplay tag */
	UPROPERTY(Category="Character Movement (Networking)", EditAnywhere, BlueprintReadOnly)
	TMap<FGameplayTag, FClientAuthParams> ClientAuthParams;

	UPROPERTY()
	FClientAuthStack ClientAuthStack;
	
public:
	/** Example implementation of an externally applied non-predicted Server modifier that can stack */
	UPROPERTY(Category="Character Movement: Walking", EditAnywhere, BlueprintReadWrite)
	FMovementModifier Snare;

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
	
	UFUNCTION(BlueprintPure, Category="Character Movement: Walking")
	bool WantsSnare() const
	{
		return Snare.WantsModifier();
	}

	/**
	 * @param ModifierLevel The level to check for - optional, leave empty if check is generic
	 * @return True if we can be snared
	 */
	UFUNCTION(BlueprintPure, Category="Character Movement: Walking", meta=(Categories="Modifier.Type.Server.Snare"))
	virtual bool CanBeSnaredInCurrentState(FGameplayTag ModifierLevel) const;

	virtual void OnStartSnare() {}
	virtual void OnEndSnare() {}

public:
	const FMovementModifierParams* GetBoostLevelParams() const { return BoostLevels.Find(GetBoostLevel()); }
	const FFallingModifierParams* GetSlowFallLevelParams() const { return SlowFallLevels.Find(GetSlowFallLevel()); }
	const FMovementModifierParams* GetSnareLevelParams() const { return SnareLevels.Find(GetSnareLevel()); }
	
	virtual float GetBoostSpeedScalar() const { return GetBoostLevelParams() ? GetBoostLevelParams()->MaxWalkSpeed : 1.f; }
	virtual float GetBoostAccelScalar() const { return GetBoostLevelParams() ? GetBoostLevelParams()->MaxAcceleration : 1.f; }
	virtual float GetBoostBrakingScalar() const { return GetBoostLevelParams() ? GetBoostLevelParams()->BrakingDeceleration : 1.f; }
	virtual float GetBoostGroundFrictionScalar() const { return GetBoostLevelParams() ? GetBoostLevelParams()->GroundFriction : 1.f; }
	virtual float GetBoostBrakingFrictionScalar() const { return GetBoostLevelParams() ? GetBoostLevelParams()->BrakingFriction : 1.f; }
	
	virtual float GetSnareSpeedScalar() const { return GetSnareLevelParams() ? GetSnareLevelParams()->MaxWalkSpeed : 1.f; }
	virtual float GetSnareAccelScalar() const { return GetSnareLevelParams() ? GetSnareLevelParams()->MaxAcceleration : 1.f; }
	virtual float GetSnareBrakingScalar() const { return GetSnareLevelParams() ? GetSnareLevelParams()->BrakingDeceleration : 1.f; }
	virtual float GetSnareGroundFrictionScalar() const { return GetSnareLevelParams() ? GetSnareLevelParams()->GroundFriction : 1.f; }
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

	virtual float GetGravityZ() const override;
	virtual FVector GetAirControl(float DeltaTime, float TickAirControl, const FVector& FallAcceleration) override;
	
	virtual void UpdateCharacterStateBeforeMovement(float DeltaTime) override;
	virtual void UpdateCharacterStateAfterMovement(float DeltaTime) override;

public:
	virtual FClientAuthData* GetClientAuthData();
	FClientAuthParams* GetClientAuthParamsForSource(const FGameplayTag& Source) { return ClientAuthParams.Find(Source); }
	virtual FClientAuthParams GetClientAuthParams(const FClientAuthData* ClientAuthData);

protected:
	/**
	 * Called when the client's position is rejected by the server entirely due to excessive difference
	 * @param ClientLoc The client's location
	 * @param ServerLoc The server's location
	 * @param LocDiff The difference between the client and server locations
	 */
	virtual void OnClientAuthRejected(const FVector& ClientLoc, const FVector& ServerLoc, const FVector& LocDiff) {}

public:
	/** 
	 * Grant the client position authority, based on the current state of the character.
	 * @param ClientAuthSource What the client is requesting authority for, not used by default, requires override
	 * @param OverrideDuration Override the default client authority time, -1.f to use default
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Character Movement (Networking)")
	virtual void InitClientAuthority(FGameplayTag ClientAuthSource, float OverrideDuration = -1.f);
	
protected:
	virtual bool ServerShouldGrantClientPositionAuthority(FVector& ClientLoc);
	
private:
	FModifierMoveResponseDataContainer ModifierMoveResponseDataContainer;

	FModifierNetworkMoveDataContainer ModifierMoveDataContainer;

protected:
	virtual bool ClientUpdatePositionAfterServerUpdate() override;
	virtual bool CanDelaySendingMove(const FSavedMovePtr& NewMove) override;
	
public:
	virtual void ServerMove_PerformMovement(const FCharacterNetworkMoveData& MoveData) override;
	virtual void ClientHandleMoveResponse(const FCharacterMoveResponseDataContainer& MoveResponse) override;

	virtual bool ServerCheckClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel,
		const FVector& ClientWorldLocation, const FVector& RelativeClientLocation,
		UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode) override;

	virtual void ServerMoveHandleClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel,
		const FVector& RelativeClientLocation, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName,
		uint8 ClientMovementMode) override;
	
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
	{}

	virtual ~FSavedMove_Character_Modifier() override
	{}

	FMovementModifier Boost;
	FMovementModifier EndBoost;
	FMovementModifier SlowFall;
	FMovementModifier EndSlowFall;
	FMovementModifier Snare;
	FMovementModifier EndSnare;

	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character& ClientData) override;
	virtual void PostUpdate(ACharacter* C, EPostUpdateMode PostUpdateMode) override;
	virtual bool IsImportantMove(const FSavedMovePtr& LastAckedMove) const override;
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
