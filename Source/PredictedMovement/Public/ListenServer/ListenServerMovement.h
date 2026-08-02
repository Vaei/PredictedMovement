// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "System/PredictedMovementVersioning.h"
#include "ListenServerMovement.generated.h"

/** How the mesh rotation is extrapolated for remote autonomous proxies on a listen server */
UENUM(BlueprintType)
enum class EPredMeshExtrapolationRotation : uint8
{
	Disabled,
	Yaw			UMETA(DisplayName="Yaw (Gravity Axis)"),
	Full		UMETA(DisplayName="Full Rotation"),
};

UCLASS()
class PREDICTEDMOVEMENT_API UListenServerMovement : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	UListenServerMovement(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

public:
	/**
	 * Extrapolate the mesh of remote autonomous proxies on a listen server. Requires engine 5.6 or later.
	 *
	 * The engine only interpolates: between ServerMove packets the capsule is frozen, and the mesh offset decays
	 * toward zero, so the mesh speed pulses by V*I/tau every packet and sits a mean V*tau behind. Instead we smooth
	 * the offset toward a target that advances at the last known velocity, which is continuous across a packet
	 * boundary and leaves no mean lag.
	 */
	UPROPERTY(Category="Character Movement (Networking)", EditAnywhere, BlueprintReadWrite)
	bool bListenServerMeshExtrapolation = true;

	/** Full rotation is only meaningful under custom gravity, where pitch and roll change too */
	UPROPERTY(Category="Character Movement (Networking)", EditAnywhere, BlueprintReadWrite, meta=(EditCondition="bListenServerMeshExtrapolation", EditConditionHides))
	EPredMeshExtrapolationRotation MeshExtrapolationRotationMode = EPredMeshExtrapolationRotation::Yaw;

	/** How quickly the mesh converges on the extrapolated target. Only damps error, so it can stay small. */
	UPROPERTY(Category="Character Movement (Networking)", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.001", UIMin="0.001", ForceUnits="s", EditCondition="bListenServerMeshExtrapolation", EditConditionHides))
	float MeshExtrapolationSmoothTime = 0.05f;

	/** Begin fading the extrapolation out after this long without a move from the client */
	UPROPERTY(Category="Character Movement (Networking)", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0", UIMin="0.0", ForceUnits="s", EditCondition="bListenServerMeshExtrapolation", EditConditionHides))
	float MeshExtrapolationFadeStartTime = 0.08f;

	/** Extrapolation is fully faded out here. Ramped, never cut, a cut would step the mesh velocity to zero. */
	UPROPERTY(Category="Character Movement (Networking)", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0", UIMin="0.0", ForceUnits="s", EditCondition="bListenServerMeshExtrapolation", EditConditionHides))
	float MeshExtrapolationMaxTime = 0.15f;

	/** Safety cap. Keep below NetworkMaxSmoothUpdateDistance so a runaway is recoverable by one SmoothCorrection. */
	UPROPERTY(Category="Character Movement (Networking)", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0", UIMin="0.0", ForceUnits="cm", EditCondition="bListenServerMeshExtrapolation", EditConditionHides))
	float MaxMeshExtrapolationDistance = 100.f;

	/** Safety cap on the extrapolated rotation delta */
	UPROPERTY(Category="Character Movement (Networking)", EditAnywhere, BlueprintReadWrite, meta=(ClampMin="0.0", UIMin="0.0", ForceUnits="degrees", EditCondition="bListenServerMeshExtrapolation", EditConditionHides))
	float MaxMeshExtrapolationRotation = 45.f;

protected:
	/** Seconds since the last MoveAutonomous was consumed for this proxy */
	float TimeSinceLastAutonomousUpdate = 0.f;

	/** Capsule rotation as of the last consumed update, and the delta that arrived with it */
	FQuat LastAutonomousRotation = FQuat::Identity;
	FQuat LastAutonomousRotationDelta = FQuat::Identity;
	float LastAutonomousRotationDeltaTime = 0.f;

	/** Set by MoveAutonomous, consumed once per frame by ServerAutonomousProxyTick */
	bool bPendingAutonomousUpdate = false;

public:
	/** True when this is a remote autonomous proxy whose mesh the listen server smooths for the host's local view. */
	virtual bool HasListenServerMeshSmoothing() const;

#if UE_5_06_OR_LATER
	/** True when the mesh should additionally be extrapolated forward from the last known velocity */
	virtual bool ShouldExtrapolateListenServerMesh() const;

	/** Drop any accumulated extrapolation state and re-seed from the next update */
	virtual void ResetMeshExtrapolation();

protected:
	virtual void ServerAutonomousProxyTick(float DeltaSeconds) override;
	virtual void SmoothClientPosition(float DeltaSeconds) override;
	virtual void OnTeleported() override;
#endif

protected:
	virtual void MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags, const FVector& NewAccel) override;
};
