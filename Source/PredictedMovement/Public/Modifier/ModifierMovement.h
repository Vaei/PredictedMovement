// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ModifierImpl.h"
#include "ModifierTypes.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "System/PredictedMovementVersioning.h"
#include "ModifierMovement.generated.h"

class AModifierCharacter;

using TMod_Local = FMovementModifier_LocalPredicted;
using TMod_LocalCorrection = FMovementModifier_WithCorrection;
using TMod_Server = FMovementModifier_WithCorrection;

struct PREDICTEDMOVEMENT_API FModifierMoveResponseDataContainer : FCharacterMoveResponseDataContainer
{  // Server ➜ Client
	using Super = FCharacterMoveResponseDataContainer;

	/*
	 * Used by the server to send Modifier data to the client
	 * LocalPredicted modifiers are not sent, as the server does not correct input states
	 */
	
	FModifierMoveResponse BoostCorrection;
	FModifierMoveResponse BoostServer;
	FModifierMoveResponse SnareServer;

	/** Tell the client how much location authority they have */
	float ClientAuthAlpha = 0.f;

	/** No need to send the float if the client has no authority */
	bool bHasClientAuthAlpha;

	virtual void ServerFillResponseData(const UCharacterMovementComponent& CharacterMovement, const FClientAdjustment& PendingAdjustment) override;
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap) override;
};

struct PREDICTEDMOVEMENT_API FModifierNetworkMoveData : FCharacterNetworkMoveData
{  // Client ➜ Server
public:
	typedef FCharacterNetworkMoveData Super;
 
	FModifierNetworkMoveData()
	{}

	/*
	 * Used by the client to send Modifier data to the server
	 * If local predicted, this data is based on player input, and the server will apply it
	 * Otherwise, the server will compare the client and server data to know when to send a correction
	 */
	
	FModifierMoveData_LocalPredicted BoostLocal;
	FModifierMoveData_WithCorrection BoostCorrection;
	FModifierMoveData_ServerInitiated BoostServer;
	FModifierMoveData_ServerInitiated SnareServer;
	FModifierMoveData_LocalPredicted SlowFallLocal;
	
	virtual void ClientFillNetworkMoveData(const FSavedMove_Character& ClientMove, ENetworkMoveType MoveType) override;
	virtual bool Serialize(UCharacterMovementComponent& CharacterMovement, FArchive& Ar, UPackageMap* PackageMap, ENetworkMoveType MoveType) override;
};
 
struct PREDICTEDMOVEMENT_API FModifierNetworkMoveDataContainer : FCharacterNetworkMoveDataContainer
{  // Client ➜ Server
public:
	typedef FCharacterNetworkMoveDataContainer Super;
 
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
 * Supports stackable modifiers such as Boost, Snare, and SlowFall.
 * Duplicate the implementations to add your own modifiers. Don't forget to do the same for the character class.
 */
UCLASS()
class PREDICTEDMOVEMENT_API UModifierMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()

private:
	/** Character movement component belongs to */
	UPROPERTY(Transient, DuplicateTransient)
	TObjectPtr<AModifierCharacter> ModifierCharacterOwner;

public:
	/**
	 * Boost modifies movement properties such as speed and acceleration
	 * Scaling applied on a per-Boost-level basis
	 */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite)
	TMap<FGameplayTag, FMovementModifierParams> Boost;

	/**
	 * Limits the maximum number of Boost levels that can be applied to the character
	 * This value is shared between each type of Boost
	 * It limits both the number being serialized and sent over the network, as well as having gameplay implications
	 * Priority is granted in order, because modifiers consume the remaining slots, so LocalPredicted -> WithCorrection - ServerInitiated
	 */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite, meta=(InlineEditConditionToggle))
	bool bLimitMaxBoosts = true;
	
	/**
	 * Maximum number of Boost levels that can be applied to the character
	 * This value is shared between each type of Boost
	 * It limits both the number being serialized and sent over the network, as well as having gameplay implications
	 * Priority is granted in order, because modifiers consume the remaining slots, so LocalPredicted -> WithCorrection - ServerInitiated
	 */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite, meta=(ClampMin=1, UIMin=1, UIMax=32, EditCondition="bLimitMaxBoosts"))
	int32 MaxBoosts = 8;

	/** Indexed list of Boost levels, used to determine the current Boost level based on index */
	UPROPERTY()
	TArray<FGameplayTag> BoostLevels;

	/** The method used to calculate Boost levels */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite)
	EModifierLevelMethod BoostLevelMethod;
	
	/** Local Predicted Boost based on Player Input */
	TMod_Local BoostLocal;

	/** Local Predicted Boost based on Player Input, that can be corrected by the server when a mismatch occurs */
	TMod_LocalCorrection BoostCorrection;

	/** Server Initiated Boost that is sent to the Client via a correction */
	TMod_Server BoostServer;

public:
	/**
	 * Snare modifies movement properties such as speed and acceleration
	 * Scaling applied on a per-Snare-level basis
	 */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite)
	TMap<FGameplayTag, FMovementModifierParams> Snare;

	/**
	 * Limits the maximum number of Snare levels that can be applied to the character
	 * This value is shared between each type of Snare
	 * It limits both the number being serialized and sent over the network, as well as having gameplay implications
	 */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite, meta=(InlineEditConditionToggle))
	bool bLimitMaxSnares = true;
	
	/**
	 * Maximum number of Snare levels that can be applied to the character
	 * This value is shared between each type of Snare
	 * It limits both the number being serialized and sent over the network, as well as having gameplay implications
	 */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite, meta=(ClampMin=1, UIMin=1, UIMax=32, EditCondition="bLimitMaxSnares"))
	int32 MaxSnares = 8;

	/** Indexed list of Snare levels, used to determine the current Snare level based on index */
	UPROPERTY()
	TArray<FGameplayTag> SnareLevels;

	/** The method used to calculate Snare levels */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite)
	EModifierLevelMethod SnareLevelMethod;

	/** Server Initiated Snare that is sent to the Client via a correction */
	TMod_Server SnareServer;

public:
	/**
	 * SlowFall changes falling properties, such as gravity and air control
	 * Scaling applied on a per-SlowFall-level basis
	 */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite)
	TMap<FGameplayTag, FFallingModifierParams> SlowFall;

	/**
	 * Limits the maximum number of SlowFall levels that can be applied to the character
	 * This value is shared between each type of SlowFall
	 * It limits both the number being serialized and sent over the network, as well as having gameplay implications
	 * Priority is granted in order, because modifiers consume the remaining slots, so LocalPredicted -> WithCorrection - ServerInitiated
	 */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite, meta=(InlineEditConditionToggle))
	bool bLimitMaxSlowFalls = true;
	
	/**
	 * Maximum number of SlowFall levels that can be applied to the character
	 * This value is shared between each type of SlowFall
	 * It limits both the number being serialized and sent over the network, as well as having gameplay implications
	 * Priority is granted in order, because modifiers consume the remaining slots, so LocalPredicted -> WithCorrection - ServerInitiated
	 */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite, meta=(ClampMin=1, UIMin=1, UIMax=32, EditCondition="bLimitMaxSlowFalls"))
	int32 MaxSlowFalls = 8;

	/** Indexed list of SlowFall levels, used to determine the current SlowFall level */
	UPROPERTY()
	TArray<FGameplayTag> SlowFallLevels;

	/** The method used to calculate SlowFall levels */
	UPROPERTY(Category="Character Movement: Modifiers", EditAnywhere, BlueprintReadWrite)
	EModifierLevelMethod SlowFallLevelMethod;

	/** Local Predicted SlowFall based on Player Input */
	TMod_Local SlowFallLocal;
	
public:
	/** Client auth parameters mapped to a source gameplay tag */
	UPROPERTY(Category="Character Movement (Networking)", EditAnywhere, BlueprintReadOnly)
	TMap<FGameplayTag, FClientAuthParams> ClientAuthParams;

	UPROPERTY()
	FClientAuthStack ClientAuthStack;

	UPROPERTY()
	float ClientAuthAlpha = 0.f;

	UPROPERTY()
	uint64 ClientAuthIdCounter = 0;
	
public:
	UModifierMovement(const FObjectInitializer& ObjectInitializer);

	virtual bool HasValidData() const override;
	virtual void PostLoad() override;
	virtual void SetUpdatedComponent(USceneComponent* NewUpdatedComponent) override;

public:
	virtual float GetMaxAcceleration() const override;
	virtual float GetMaxSpeed() const override;
	virtual float GetMaxBrakingDeceleration() const override;
	virtual float GetGroundFriction(float DefaultGroundFriction) const;
	virtual float GetBrakingFriction() const;
	virtual float GetRootMotionTranslationScalar() const;
	
	virtual float GetGravityZ() const override;
	virtual FVector GetAirControl(float DeltaTime, float TickAirControl, const FVector& FallAcceleration) override;
	
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	virtual void ApplyVelocityBraking(float DeltaTime, float Friction, float BrakingDeceleration) override;

public:
	/* Boost Implementation */

	uint8 BoostLevel = NO_MODIFIER;
	bool IsBoostActive() const { return BoostLevel != NO_MODIFIER; }
	const FMovementModifierParams* GetBoostParams() const { return Boost.Find(GetBoostLevel()); }
	FGameplayTag GetBoostLevel() const { return BoostLevels.IsValidIndex(BoostLevel) ? BoostLevels[BoostLevel] : FGameplayTag::EmptyTag; }
	uint8 GetBoostLevelIndex(const FGameplayTag& Level) const { return BoostLevels.IndexOfByKey(Level) > INDEX_NONE ? BoostLevels.IndexOfByKey(Level) : NO_MODIFIER; }
	virtual bool CanBoostInCurrentState() const;

	float GetBoostSpeedScalar() const { return GetBoostParams() ? GetBoostParams()->MaxWalkSpeed : 1.f; }
	float GetBoostAccelScalar() const { return GetBoostParams() ? GetBoostParams()->MaxAcceleration : 1.f; }
	float GetBoostBrakingScalar() const { return GetBoostParams() ? GetBoostParams()->BrakingDeceleration : 1.f; }
	float GetBoostGroundFrictionScalar() const { return GetBoostParams() ? GetBoostParams()->GroundFriction : 1.f; }
	float GetBoostBrakingFrictionScalar() const { return GetBoostParams() ? GetBoostParams()->BrakingFriction : 1.f; }
	bool BoostAffectsRootMotion() const { return GetBoostParams() ? GetBoostParams()->bAffectsRootMotion : false; }
	
	/* ~Boost Implementation */

public:
	/* Snare Implementation */

	uint8 SnareLevel = NO_MODIFIER;
	bool IsSnareActive() const { return SnareLevel != NO_MODIFIER; }
	const FMovementModifierParams* GetSnareParams() const { return Snare.Find(GetSnareLevel()); }
	FGameplayTag GetSnareLevel() const { return SnareLevels.IsValidIndex(SnareLevel) ? SnareLevels[SnareLevel] : FGameplayTag::EmptyTag; }
	uint8 GetSnareLevelIndex(const FGameplayTag& Level) const { return SnareLevels.IndexOfByKey(Level) > INDEX_NONE ? SnareLevels.IndexOfByKey(Level) : NO_MODIFIER; }
	virtual bool CanSnareInCurrentState() const;

	float GetSnareSpeedScalar() const { return GetSnareParams() ? GetSnareParams()->MaxWalkSpeed : 1.f; }
	float GetSnareAccelScalar() const { return GetSnareParams() ? GetSnareParams()->MaxAcceleration : 1.f; }
	float GetSnareBrakingScalar() const { return GetSnareParams() ? GetSnareParams()->BrakingDeceleration : 1.f; }
	float GetSnareGroundFrictionScalar() const { return GetSnareParams() ? GetSnareParams()->GroundFriction : 1.f; }
	float GetSnareBrakingFrictionScalar() const { return GetSnareParams() ? GetSnareParams()->BrakingFriction : 1.f; }
	bool SnareAffectsRootMotion() const { return GetSnareParams() ? GetSnareParams()->bAffectsRootMotion : false; }
	
	/* ~Snare Implementation */

public:
	/* SlowFall Implementation */

	uint8 SlowFallLevel = NO_MODIFIER;
	bool IsSlowFallActive() const { return SlowFallLevel != NO_MODIFIER; }
	const FFallingModifierParams* GetSlowFallParams() const { return SlowFall.Find(GetSlowFallLevel()); }
	FGameplayTag GetSlowFallLevel() const { return SlowFallLevels.IsValidIndex(SlowFallLevel) ? SlowFallLevels[SlowFallLevel] : FGameplayTag::EmptyTag; }
	uint8 GetSlowFallLevelIndex(const FGameplayTag& Level) const { return SlowFallLevels.IndexOfByKey(Level) > INDEX_NONE ? SlowFallLevels.IndexOfByKey(Level) : NO_MODIFIER; }
	virtual bool CanSlowFallInCurrentState() const;

	virtual float GetSlowFallGravityZScalar() const { return GetSlowFallParams() ? GetSlowFallParams()->GetGravityScalar(Velocity) : 1.f; }
	virtual bool RemoveVelocityZOnSlowFallStart() const;

	/* ~SlowFall Implementation */

public:
	virtual void ProcessModifierMovementState();
	virtual void UpdateModifierMovementState();

	virtual void UpdateCharacterStateBeforeMovement(float DeltaSeconds) override;
	virtual void UpdateCharacterStateAfterMovement(float DeltaSeconds) override;
	
public:
	/* Client Auth Implementation */

	virtual FClientAuthData* ProcessClientAuthData();
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
	virtual void GrantClientAuthority(FGameplayTag ClientAuthSource, float OverrideDuration = -1.f);

protected:
	virtual bool ServerShouldGrantClientPositionAuthority(FVector& ClientLoc, FClientAuthData*& AuthData);
	
	/* ~Client Auth Implementation */
	
public:
	virtual void ServerMove_PerformMovement(const FCharacterNetworkMoveData& MoveData) override;

protected:
	virtual bool ServerCheckClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel,
		const FVector& ClientWorldLocation, const FVector& RelativeClientLocation,
		UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName, uint8 ClientMovementMode) override;

	virtual void ServerMoveHandleClientError(float ClientTimeStamp, float DeltaTime, const FVector& Accel,
		const FVector& RelativeClientLocation, UPrimitiveComponent* ClientMovementBase, FName ClientBaseBoneName,
		uint8 ClientMovementMode) override;

public:
	virtual void ClientAdjustPosition_Implementation(float TimeStamp, FVector NewLoc, FVector NewVel,
		UPrimitiveComponent* NewBase, FName NewBaseBoneName, bool bHasBase, bool bBaseRelativePosition,
		uint8 ServerMovementMode, TOptional<FRotator> OptionalRotation = TOptional<FRotator>()) override;

protected:
	virtual void OnClientCorrectionReceived(class FNetworkPredictionData_Client_Character& ClientData, float TimeStamp,
		FVector NewLocation, FVector NewVelocity, UPrimitiveComponent* NewBase, FName NewBaseBoneName, bool bHasBase,
		bool bBaseRelativePosition, uint8 ServerMovementMode
#if UE_5_03_OR_LATER
		, FVector ServerGravityDirection) override;
#else
	) override;
#endif

	virtual bool ClientUpdatePositionAfterServerUpdate() override;

protected:
	virtual void TickCharacterPose(float DeltaTime) override;  // ACharacter::GetAnimRootMotionTranslationScale() is non-virtual so we have to duplicate this entire function
	
private:
	FModifierNetworkMoveDataContainer ModifierMoveDataContainer;
	FModifierMoveResponseDataContainer ModifierMoveResponseDataContainer;
	
public:
	/** Get prediction data for a client game. Should not be used if not running as a client. Allocates the data on demand and can be overridden to allocate a custom override if desired. Result must be a FNetworkPredictionData_Client_Character. */
	virtual class FNetworkPredictionData_Client* GetPredictionData_Client() const override;
};

class PREDICTEDMOVEMENT_API FSavedMove_Character_Modifier : public FSavedMove_Character
{
	using Super = FSavedMove_Character;

public:
	FSavedMove_Character_Modifier()
	{}

	virtual ~FSavedMove_Character_Modifier() override
	{}

	FModifierSavedMove BoostLocal;
	FModifierSavedMove_WithCorrection BoostCorrection;
	FModifierSavedMove_ServerInitiated BoostServer;
	
	FModifierSavedMove_ServerInitiated SnareServer;

	FModifierSavedMove SlowFallLocal;

	uint8 BoostLevel = NO_MODIFIER;
	uint8 SnareLevel = NO_MODIFIER;
	uint8 SlowFallLevel = NO_MODIFIER;
	
	/** Clear saved move properties, so it can be re-used. */
	virtual void Clear() override;

	/** Called to set up this saved move (when initially created) to make a predictive correction. */
	virtual void SetMoveFor(ACharacter* C, float InDeltaTime, FVector const& NewAccel, class FNetworkPredictionData_Client_Character & ClientData) override;

	/** Returns true if this move can be combined with NewMove for replication without changing any behavior */
	virtual bool CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override;

	/** Set the properties describing the position, etc. of the moved pawn at the start of the move. */
	virtual void SetInitialPosition(ACharacter* C) override;

	/** Combine this move with an older move and update relevant state. */
	virtual void CombineWith(const FSavedMove_Character* OldMove, ACharacter* InCharacter, APlayerController* PC, const FVector& OldStartLocation) override;
	
	/** Set the properties describing the final position, etc. of the moved pawn. */
	virtual void PostUpdate(ACharacter* C, EPostUpdateMode PostUpdateMode) override;

	/** Returns true if this move is an "important" move that should be sent again if not acked by the server */
	virtual bool IsImportantMove(const FSavedMovePtr& LastAckedMove) const override;
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
