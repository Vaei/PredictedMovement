# Predicted Movement <img align="right" width=128, height=128 src="https://github.com/Vaei/PredictedMovement/blob/main/Resources/Icon128.png">

> [!IMPORTANT]
> PredictedMovement offers several shells usually with a derived `UCharacterMovementComponent` and `ACharacter` which form a single net predicted movement state.

> [!NOTE]
> For BP users the `single-cmc` branch contains a feature-rich `ACharacter` you can use right away.
> <br>Check out the Demo content in the plugin's content folder or [read more about it here.](https://github.com/Vaei/PredictedMovement/wiki/Single%E2%80%90CMC-Branch)

# How to Use

> [!NOTE]
> [Read the Wiki to learn how to use Predicted Movement](https://github.com/Vaei/PredictedMovement/wiki/How-to-Use)

# Abilities

## Prone
Similar functionality as crouching. Also supports change in radius. Has a lock timer, to prevent early exit from prone.

Compatible with crouching, and should switch properly between Crouch➜Prone, Stand➜Prone, Prone➜Crouch, etc.

## Strafe
A shell intended for switching to / from a strafe mode, however the actual functionality is not implemented as it is project-dependent. This is useful for duplicating into your own movement states, e.g. Aiming Down Sights

## Sprint
It makes you sprint by changing your movement properties when activated.

## Stamina
Net predicted stamina and drain state. It also includes a correction mechanism.

## Modifiers
Modifiers are similar to states such as sprinting, however instead of a single on/off state, they contain multiple levels, e.g. `Boost Level 1-5`.

Included Modifiers: `Boost`, `Snare`, `SlowFall`. The `single-cmc` branch includes also `Slow` and `Haste` (boost, but only affects Sprint).

Modifiers come in three formats: `Local Predicted`, `Local Predicted + Corrections`, and `Server Initiated`. [Read about the differences here.](https://github.com/Vaei/PredictedMovement/wiki/Modifiers)

Furthermore, `Server Initiated` modifiers include a functional and production tested client authority solution, that allows limited/partial location authority for the client when the server applies a Modifier to your character, to prevent problematic de-sync. [You can read about this here](https://github.com/Vaei/PredictedMovement/wiki/Client-Authority).

The included modifiers can be duplicated to achieve plenty of other effects.

## Gait Modes
`single-cmc` includes Stroll, Walk, Run, Sprint gait modes as well as AimDownSights.

Stroll was added for the sake of NPCs who are not doing anything, but you don't want them to remain completely stationary.

# Demonstration
I use PredictedMovement in my own projects, here you can see the character sprinting, consuming stamina, strafing, and proning with high latency (>220ms) and `p.netshowcorrections 1`. As you can see, there is no desync. This was from version 1 of PredictedMovement and it has only improved since.

https://youtu.be/SHVm57AMruc

# Changelog

### 2.1.3
* Add further missing templates for clang

### 2.1.2
* Specify template type as `NotifyModifierChanged<uint8>` instead of inferring for the sake of clang

### 2.1.1
* Improve base eye height handling on `single-cmc`

### 2.1.0
* Movement Modifiers recreated from scratch
  * Supports `LocalPredicted`, `ServerInitiated`, but also `LocalPredicted_WithCorrection`
* Client Authority overhauled, now handles client corrections fully
* `single-cmc` branch added with full BP support and demo content

### 2.0.1
* Change from `uint32:1` to `uint8:1`

### 2.0.0
* Movement Modifiers added
  * Supports Local Predicted & Server Initiated modifiers
  * Boost, SlowFall, Snare included by default
* Client Authority solution added
  * For Server Initiated modifiers in particular
* `CanSprintInCurrentState()` no longer checks `IsSprintWithinAllowableInputAngle()`
  * `IsSprintWithinAllowableInputAngle()` exposed for BP (`ASprintCharacter`) and should be checked individually for when considered sprinting
    * Additional function added `IsSprintingInEffect()` that checks both `IsSprintingAtSpeed() && IsSprintWithinAllowableInputAngle()`
    * e.g. `if (IsSprintingAtSpeed())` becomes `if (IsSprintingInEffect())`
  * The goal is to not have it completely exit/re-entry sprinting state repeatedly when the angle continues to fail the check momentarily
  * Exposed `MaxInputAngleSprint` which was previously 50.f
* Movement Abilities updated to use push model for replication
* Wiki added

### 1.5.2
* Version number follows Unreal format 1.0.5.2 > 1.5.2
* Added System/PredictedMovementVersioning.h for improved pre-processor support
* Added helper comments to StaminaMovement.h

### 1.5.1
* Pre-processor macros for OnClientCorrectionReceived to support 5.3

### 1.5.0
* Replaced ClientHandleMoveResponse with OnClientCorrectionReceived

### 1.4.0
* Fixed a bug causing SimulatedProxy to factor ProneLock incorrectly, causing it to become stuck in Prone
* Moved prone lock definitions to the .cpp

### 1.3.3
* Initialize a few uninitialized properties
* Add `UE_INLINE_GENERATED_CPP_BY_NAME` to all relevant .cpp

### 1.3.2
* SprintMovement added option (`bUseMaxAccelerationSprintingOnlyAtSpeed`) for using `MaxAccelerationSprinting` when `IsSprintingAtSpeed()` is false

### 1.3.1
* Added NetworkStaminaCorrectionThreshold and exposed via UPROPERTY()

### 1.3.0
* `UStaminaMovement::ServerCheckClientError` added by [PR from vorixo](https://github.com/Vaei/PredictedMovement/pull/3/commits/d03a4ec5f7dd07c48e8a9f0b21255fc258adb865)

### 1.2.3
* Remove `bForceIWYU`

### 1.2.2
* Changed `FSavedMove_Character_Prone::PrepMoveFor` to use `Super` instead of `FSavedMove_Character`

### 1.2.1
* Removed `IsSprintingAtSpeed()` check for `USprintMovement::GetMaxSpeed()`

### 1.2.0
* Prone lock timer implemented properly (with prediction instead of timer handle) to fix desync
* Can no longer jump while prone

### 1.1.5
* Remove ground checks for sprint and strafe movement properties (walking off a ledge shouldn't momentarily change to normal speed, etc)

### 1.1.4
* ClientHandleMoveResponse use LogNetPlayerMovement instead of LogTemp
* Minor readability change to ClientHandleMoveResponse and a helpful comment

### 1.1.3
* Add export macro and remove final for StaminaMoveResponseDataContainer

### 1.1.2
* Expose stamina as protected
* Change ObjectInitializer property name from OI
* Fixed incorrect information in comment for recovering stamina

### 1.1.1
* Removed accidental include

### 1.1.0
* Removed stamina inheritance from sprint
* Fixed CanSprint() being used unnecessarily, CanSprintInCurrentState is utilized properly here
* Moved sprint input checking to virtual func `IsSprintWithinAllowableInputAngle`
* Sprint checks 3D velocity on ground, replaced magic number with UPROPERTY `VelocityCheckMitigatorSprinting`
* Moved implementations out of headers
* Greatly improved comments

(Thanks to cedric & vori for the code review)

### 1.0.2
* Fixed issue where stamina corrections were being overwritten

### 1.0.1
* Added Stamina

### 1.0.0
* Initial Release
