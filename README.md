# PredictedMovement
This plugin offers several shells usually with a derived `UCharacterMovementComponent` and `ACharacter` which form a single net predicted ability.

## Predicted Abilities

# Example
You can [find the Third Person Template example project here](https://github.com/Vaei/PredictedMovementExample)

### Prone
Similar functionality as crouching. Also supports change in radius. Has a lock timer, to prevent early exit from prone.

Compatible with crouching, and should switch properly between crouch->prone, stand->prone, prone->crouch, etc.

### Strafe
A shell intended for switching to / from a strafe mode, however the actual functionality is not implemented as it is project-dependent.

### Sprint
Well, it makes you sprint.

### Stamina
Net predicted stamina and drain state.

## Demonstration
I use this in my own project, here you can see the character sprinting, consuming stamina, strafing, and proning with high latency (>220ms) and `p.netshowcorrections 1`. As you can see, there is no desync.

https://youtu.be/SHVm57AMruc

## Requirements
C++ knowledge is a must, and this cannot be used without C++.

## Usage
Because each ability is a combination of `UCharacterMovementComponent` and `ACharacter` you will need to modify the plugin to inherit the abilities you want for your project. I recommend forking this repository to do that, so you can still pull in any future commits.

It is vitally important that you remember to change the inheritance for `FSavedMove_Character` and `FNetworkPredictionData_Client_Character` also at the bottom of the Movement header files AND change the `using Super = ...` to the new Super class! If you experience extreme desync, its because you forgot to do this.

Alternatively, the 'link' branch includes all abilities, you simply need to derive the top-most ability (stamina for movement and sprint for character). This branch may in some cases fall behind main, however.

Note that Stamina does not have an `ACharacter` because no Character-specific functionality is required.

Also note that Stamina inherits Sprint by default, if you want Stamina but not Sprint, you will need to remove this inheritance.

Once that is complete, you simply need to call the equivalent input function.

### Stamina
Add a void `CalcStamina(float DeltaTime)` function to your `UCharacterMovementComponent` and call it before Super after
overriding `CalcVelocity`.

You will want to implement what happens based on the stamina yourself, eg. override `GetMaxSpeed` to move slowly
when `bStaminaDrained`.

Override `OnStaminaChanged` to call (or not) `SetStaminaDrained` based on the needs of your project. Most games will
want to make use of the drain state to prevent rapid sprint re-entry on tiny amounts of regenerated stamina. However,
unless you require the stamina to completely refill before sprinting again, then you'll want to override
`OnStaminaChanged` and change `FMath::IsNearlyEqual(Stamina, MaxStamina)` to multiply `MaxStamina` by the percentage
that must be regained before re-entry (eg. `MaxStamina0.1f is 10%`).

Nothing is presumed about regenerating or draining stamina, if you want to implement those, do it in `CalcVelocity` or
at least `PerformMovement` - `CalcVelocity` stems from `PerformMovement` but exists within the physics subticks for greater
accuracy.

If used with sprinting, `OnStaminaDrained()` should be overridden to call `USprintMovement::UnSprint()`. If you don't
do this, the greater accuracy of `CalcVelocity` is lost because it cannot stop sprinting between frames.

You will need to handle any changes to `MaxStamina`, it is not predicted here.

GAS can modify the Stamina (by calling `SetStamina()`, nothing special required) and it shouldn't desync, however
if you have any delay between the ability activating and the stamina being consumed it will likely desync; the
solution is to call `GetCharacterMovement()->FlushServerMoves()` from the Character.
It is worthwhile to expose this to blueprint.

This is not designed to work with blueprint, at all, anything you want exposed to blueprint you will need to do it
Better yet, add accessors from your Character and perhaps a broadcast event for UI to use.

This solution is provided by Cedric 'eXi' Neukirchen and has been repurposed for net predicted Stamina.

### Strafe
Strafe is a shell intended for changing to and from a strafing state, however the actual implementation of
"what strafe does" is highly dependant on a project, so override the functions and define the behaviour yourself.

Generally `AStrafeCharacter::OnStartStrafe` might change `bUseControllerRotationYaw = true` and
`bOrientRotationToMovement = false` then and `AStrafeCharacter::OnEndStrafe` would change them back.

Alternatively you could override any `ACharacter` and `UCharacterMovementComponent` functions that use
`bUseControllerRotationYaw` or `bOrientRotationToMovement` and check `IsStrafing`, but that implementation will be
more advanced and often unnecessary.

# Why do we need this?
(This is a super short version of a really complex topic. You can watch a more in-depth explanation [here](https://www.youtube.com/watch?v=dOkuIvKCvpg))

You may be wondering: Why can't I just change the speed of my CMC in an RPC and call it a day? What's with all these "Saved Movements"?

The CMC Prediction/Reconciliation system depends on every action taken by the Character to be able to be stored and played back and end up in the exact same state in both the client and the server, that is to say, it has to be deterministic.

(If you are already lost at the "Prediction/Reconciliation" start [here](https://www.gabrielgambetta.com/client-server-game-architecture.html), read all four parts and then come back)

This means that every variable that alters how the Character moves and every change to those variables change has to be recorded in the same "replay-able" way so the server can check that the client predicted movement was correct (Acknowledge the move) or send all the data needed for a correction in case the prediction was wrong (Client adjust position).

When the Client prediction fails and the server sends the corrected package, the Client proceeds to fix its variables and then replay all the saved moves (The moves sent to the server that still haven't had a response) between the corrected move and the present, ending up with a less noticeable (if at all) rubberbanding effect.

## Changelog

### 1.0.1.4
* ClientHandleMoveResponse use LogNetPlayerMovement instead of LogTemp
* Minor readability change to ClientHandleMoveResponse and a helpful comment

### 1.0.1.3
* Add export macro and remove final for StaminaMoveResponseDataContainer

### 1.0.1.2
* Expose stamina as protected
* Change ObjectInitializer property name from OI
* Fixed incorrect information in comment for recovering stamina

### 1.0.1.1
* Removed accidental include

### 1.0.1.0
* Removed stamina inheritance from sprint
* Fixed CanSprint() being used unnecessarily, CanSprintInCurrentState is utilized properly here
* Moved sprint input checking to virtual func `IsSprintWithinAllowableInputAngle`
* Sprint checks 3D velocity on ground, replaced magic number with UPROPERTY `VelocityCheckMitigatorSprinting`
* Moved implementations out of headers
* Greatly improved comments

(Thanks to cedric & vori for the code review)

### 1.0.0.2
* Fixed issue where stamina corrections were being overwritten

### 1.0.0.1
* Added Stamina

### 1.0.0.0
* Initial Release
