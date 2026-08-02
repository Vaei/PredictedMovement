// Copyright (c) Jared Taylor


#include "ListenServer/ListenServerMovement.h"

#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(ListenServerMovement)

namespace ListenServerMovementCVars
{
	static int32 ListenServerExtrapolation = 1;
	static FAutoConsoleVariableRef CVarListenServerExtrapolation(
		TEXT("p.ListenServerExtrapolation"),
		ListenServerExtrapolation,
		TEXT("Whether to extrapolate the mesh of remote autonomous proxies on listen servers.\n")
		TEXT("0: Disable, 1: Enable"),
		ECVF_Default);

	static float ListenServerExtrapolationSmoothTime = -1.f;
	static FAutoConsoleVariableRef CVarListenServerExtrapolationSmoothTime(
		TEXT("p.ListenServerExtrapolation.SmoothTime"),
		ListenServerExtrapolationSmoothTime,
		TEXT("Override the mesh extrapolation smoothing time.\n")
		TEXT("Negative uses the component property"),
		ECVF_Default);

	// Engine-side gate for listen server mesh smoothing, it lives in an engine-private namespace so it has to be found by name
	static IConsoleVariable* GetNetEnableListenServerSmoothingCVar()
	{
		static IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("p.NetEnableListenServerSmoothing"));
		return CVar;
	}
}

UListenServerMovement::UListenServerMovement(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{}

bool UListenServerMovement::HasListenServerMeshSmoothing() const
{
	if (!HasValidData() || !IsNetMode(NM_ListenServer))
	{
		return false;
	}

	if (CharacterOwner->GetRemoteRole() != ROLE_AutonomousProxy || CharacterOwner->IsLocallyControlled())
	{
		return false;
	}

	if (NetworkSmoothingMode == ENetworkSmoothingMode::Disabled || !CharacterOwner->GetMesh())
	{
		return false;
	}

	// Without the engine gate nothing consumes or decays the offset, so it must not be written to
	const IConsoleVariable* CVar = ListenServerMovementCVars::GetNetEnableListenServerSmoothingCVar();
	return CVar && CVar->GetInt() != 0;
}

#if UE_5_06_OR_LATER
bool UListenServerMovement::ShouldExtrapolateListenServerMesh() const
{
	if (!bListenServerMeshExtrapolation || ListenServerMovementCVars::ListenServerExtrapolation == 0)
	{
		return false;
	}

	if (!HasListenServerMeshSmoothing() || NetworkSmoothingMode != ENetworkSmoothingMode::Exponential)
	{
		return false;
	}

	// Ragdoll: UpdateVisuals early-outs on a simulating mesh, so the offset would accumulate and pop on recovery
	const USkeletalMeshComponent* Mesh = CharacterOwner->GetMesh();
	if (Mesh->IsSimulatingPhysics() || Mesh->GetAttachParent() != UpdatedComponent)
	{
		return false;
	}

	// Root motion velocity is per-frame anim output, and the animation already carries the visual motion
	return !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity();
}

void UListenServerMovement::ResetMeshExtrapolation()
{
	TimeSinceLastAutonomousUpdate = 0.f;
	LastAutonomousRotation = UpdatedComponent ? UpdatedComponent->GetComponentQuat() : FQuat::Identity;
	LastAutonomousRotationDelta = FQuat::Identity;
	LastAutonomousRotationDeltaTime = 0.f;
	bPendingAutonomousUpdate = false;
}

void UListenServerMovement::ServerAutonomousProxyTick(float DeltaSeconds)
{
	Super::ServerAutonomousProxyTick(DeltaSeconds);

	if (!ShouldExtrapolateListenServerMesh())
	{
		ResetMeshExtrapolation();
		return;
	}

	const FQuat CapsuleRotation = UpdatedComponent->GetComponentQuat();

	// Consume at frame level, a batched ServerMoveDual delivers several MoveAutonomous calls in one frame
	if (bPendingAutonomousUpdate)
	{
		if (TimeSinceLastAutonomousUpdate > UE_KINDA_SMALL_NUMBER)
		{
			LastAutonomousRotationDelta = CapsuleRotation * LastAutonomousRotation.Inverse();
			LastAutonomousRotationDeltaTime = TimeSinceLastAutonomousUpdate;
		}

		LastAutonomousRotation = CapsuleRotation;
		TimeSinceLastAutonomousUpdate = 0.f;
		bPendingAutonomousUpdate = false;
	}

	TimeSinceLastAutonomousUpdate += DeltaSeconds;

	// TickComponent only calls SmoothClientPosition when smoothing is incomplete, and it does so after this
	bNetworkSmoothingComplete = false;
}

void UListenServerMovement::SmoothClientPosition(float DeltaSeconds)
{
	FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character();
	if (!ClientData || !ShouldExtrapolateListenServerMesh())
	{
		Super::SmoothClientPosition(DeltaSeconds);
		return;
	}

	// The engine decays the offset toward zero, which is what produces the packet-rate speed pulse. This path
	// replaces that entirely rather than computing it and discarding the result.
	const FVector PreTranslationOffset = ClientData->MeshTranslationOffset;
	const FQuat PreRotationOffset = ClientData->MeshRotationOffset;

	// Ramp the extrapolation out when updates stop arriving, a hard cut would step the mesh velocity to zero
	const float FadeAlpha = 1.f - FMath::SmoothStep(MeshExtrapolationFadeStartTime, MeshExtrapolationMaxTime,
		TimeSinceLastAutonomousUpdate);

	// The target advances at the last known velocity, and drops by the same amount the capsule jumps when the
	// next move arrives, so the mesh position and velocity are both continuous across a packet boundary
	const FVector TargetTranslationOffset =
		(Velocity * TimeSinceLastAutonomousUpdate * FadeAlpha).GetClampedToMaxSize(MaxMeshExtrapolationDistance);

	FQuat TargetRotationOffset = FQuat::Identity;
	if (MeshExtrapolationRotationMode != EPredMeshExtrapolationRotation::Disabled &&
		LastAutonomousRotationDeltaTime > UE_KINDA_SMALL_NUMBER)
	{
		const float RotationAlpha = (TimeSinceLastAutonomousUpdate / LastAutonomousRotationDeltaTime) * FadeAlpha;
		FQuat WorldDelta = FQuat::Slerp(FQuat::Identity, LastAutonomousRotationDelta, RotationAlpha).GetNormalized();

		if (MeshExtrapolationRotationMode == EPredMeshExtrapolationRotation::Yaw)
		{
			// Keep only the twist about the gravity axis so pitch and roll aren't extrapolated
			const FVector UpAxis = -GetGravityDirection();
			WorldDelta = FQuat(UpAxis, WorldDelta.GetTwistAngle(UpAxis));
		}

		const float MaxAngle = FMath::DegreesToRadians(MaxMeshExtrapolationRotation);
		if (WorldDelta.GetAngle() > MaxAngle)
		{
			WorldDelta = FQuat(WorldDelta.GetRotationAxis(), MaxAngle);
		}

		// UpdateVisuals applies MeshRotationOffset relative to the capsule, so express the world delta there
		const FQuat CapsuleRotation = UpdatedComponent->GetComponentQuat();
		TargetRotationOffset = (CapsuleRotation.Inverse() * WorldDelta * CapsuleRotation).GetNormalized();
	}

	const float SmoothTime = ListenServerMovementCVars::ListenServerExtrapolationSmoothTime >= 0.f
		? ListenServerMovementCVars::ListenServerExtrapolationSmoothTime
		: MeshExtrapolationSmoothTime;
	const float Alpha = FMath::Clamp(DeltaSeconds / FMath::Max(SmoothTime, UE_KINDA_SMALL_NUMBER), 0.f, 1.f);

	ClientData->MeshTranslationOffset = FMath::Lerp(PreTranslationOffset, TargetTranslationOffset, Alpha);
	ClientData->OriginalMeshTranslationOffset = ClientData->MeshTranslationOffset;
	ClientData->MeshRotationOffset = FQuat::Slerp(PreRotationOffset, TargetRotationOffset, Alpha).GetNormalized();

	bNetworkSmoothingComplete = false;

	SmoothClientPosition_UpdateVisuals();
}

void UListenServerMovement::OnTeleported()
{
	Super::OnTeleported();

	// SmoothCorrection's clamp only bounds a single correction delta, and a gameplay teleport never calls it at
	// all, so the offset has to be dropped here or the mesh smears across the teleport
	if (HasListenServerMeshSmoothing())
	{
		if (FNetworkPredictionData_Client_Character* ClientData = GetPredictionData_Client_Character())
		{
			ClientData->MeshTranslationOffset = FVector::ZeroVector;
			ClientData->OriginalMeshTranslationOffset = FVector::ZeroVector;
			ClientData->MeshRotationOffset = ClientData->MeshRotationTarget;
			bNetworkSmoothingComplete = true;
		}
	}

	ResetMeshExtrapolation();
}
#endif

void UListenServerMovement::MoveAutonomous(float ClientTimeStamp, float DeltaTime, uint8 CompressedFlags,
	const FVector& NewAccel)
{
	Super::MoveAutonomous(ClientTimeStamp, DeltaTime, CompressedFlags, NewAccel);

#if UE_5_06_OR_LATER
	// Flag only, the frame-level consume is in ServerAutonomousProxyTick so batched sub-moves collapse into one
	if (HasListenServerMeshSmoothing())
	{
		bPendingAutonomousUpdate = true;
	}
#endif
}
