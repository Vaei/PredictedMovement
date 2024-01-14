# PredictedMovement
This plugin offers several shells usually with a derived `UCharacterMovementComponent` and `ACharacter` which form a single net predicted ability.

# Example
You can [find the Third Person Template example project here](https://github.com/Vaei/PredictedMovementExample)

# Predicted Abilities

## Prone
Similar functionality as crouching. Also supports change in radius. Has a lock timer, to prevent early exit from prone.

Compatible with crouching, and should switch properly between crouch->prone, stand->prone, prone->crouch, etc.

## Strafe
A shell intended for switching to / from a strafe mode, however the actual functionality is not implemented as it is project-dependent.

## Sprint
It makes you sprint by changing your movement properties when activated.

## Stamina
Net predicted stamina and drain state. It also includes a correction mechanism.

# Demonstration
I use this in my own project, here you can see the character sprinting, consuming stamina, strafing, and proning with high latency (>220ms) and `p.netshowcorrections 1`. As you can see, there is no desync.

https://youtu.be/SHVm57AMruc

# Requirements
C++ knowledge is a must, and this cannot be used without C++.

# Frequently Asked Questions
### Why is such an elaborate solution needed, can't I simply replicate the max speed?
`ACharacter` & `UCharacterMovementComponent` have built-in client-side prediction to provide what feels like latency-free movement, yet with server authority to prevent hacking/cheating.

If you replicate anything that the character's predicted movement uses, this will happen outside of the prediction framework and result in considerable desync. It may appear to work in testing, but until you introduce latency, you won't see the actual result.

This solution introduces the abilities and stamina into the prediction framework so that it doesn't desync.

### What is client-side prediction?
This is a complex topic beyond the scope of this readme. Here are some resources:
* [Gabriel Gambetta's "Fast-Paced Multiplayer"](https://www.gabrielgambetta.com/client-server-game-architecture.html). There are 4 parts to this.
* [Video detailing the `UCharacterMovementComponent` class](https://www.youtube.com/watch?v=dOkuIvKCvpg)

Client-side prediction with server authority is considered the gold standard, however it comes with downsides; it is vastly more complex to develop, takes considerably more time to develop, and has high processing costs so for larger games such as battle royales or high player-count shooters it can become incredibly prohibitive for a server to process the prediction of so many players.

### Why is the `ACharacter` class required for RPCs? Can't you just replicate the `UCharacterMovementComponent`?
Replication routes through the `AActor` class, even for `UActorComponent`, these components send an additional `uint8` that identifies which component is replicating. By manually routing through the `ACharacter` class we forego the bandwidth cost of the additional `uint8`. It is an optimization that the engine establishes the standard for.

### Why are there multiple `UCharacterMovementComponent` and `ACharacter`?
Developers who use this plugin need to fork it and change the inheritance so the abilities they want are present in their project's resulting `ACharacter`, thus they do not end up with abilities that their project does not require. There may be a `link` branch that has all abilties linked together for you but it is not always up to date, if this is the case, you can still use it as an example.

### Why isn't `PrepMoveFor` and `SetMoveFor` being used for Stamina?
Corrections occur in `UCharacterMovementComponent::ClientHandleMoveResponse` and the information is sent in `FStaminaMoveResponseDataContainer::ServerFillResponseData`.

`PrepMoveFor` and `SetMoveFor` will actually undo the correction.

A paraphrased explanation of why by Cedric 'eXi':

Let's say the server corrects it to 5 and your move still has the old value of 50. Your saved moves will continue sprinting longer and cause another correction. Instead of running out of stamina you will probably have enough to keep sprinting and then send a new server move with a location that the server can't reach. The boolean (`bStaminaDrained`) also comes from the server. Doesn't sound right to override it with the saved move, and for saving it into the saved move, the InitialPosition function should be enough. That's what epic does, at least for the values that are used in `CombineWith`.

### Why should CharacterMovementComponent handle Stamina instead of GAS Attributes?
The CharacterMovementComponent has its own ecosystem for prediction. Performing any form of external action that modifies data which the CMC uses to predict movement may result in corrections.

Furthermore, not following the whole setup for concepts like SavedMoves also removes the ability of the CMC to reconcile whatever was predicted after a corrected move.

There are also restrictions on GAS side that limit the ability to predict properly, such as GEs not allowing Stacks and Removal to be predicted.

As a general note, the game that spawned this plugin tried every available method to implement Stamina via GAS and was unable to have it working correctly in a networked environment. This is the only method that worked. There are no known or published solutions to achieve GAS-based Stamina. Inadequate and limited results could be achieved, but nothing that is sufficient for this game, and likely not sufficient for most games.

# Usage
Because each ability is a combination of `UCharacterMovementComponent` and `ACharacter` you will need to modify the plugin to inherit the abilities you want for your project. I recommend forking this repository to do that, so you can still pull in any future commits.

It is vitally important that you remember to change the inheritance for `FSavedMove_Character` and `FNetworkPredictionData_Client_Character` also at the bottom of the Movement header files AND change the `using Super = ...` to the new Super class! If you experience extreme desync, its because you forgot to do this.

Alternatively, the 'link' branch includes all abilities, you simply need to derive the top-most ability (stamina for movement and sprint for character). This branch may in some cases fall behind main, however.

Note that Stamina does not have an `ACharacter` because no Character-specific functionality is required.

Once that is complete, you simply need to call the equivalent input function.

## Stamina
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
solution is to call `GetCharacterMovement()->FlushServerMoves()` from the Character. It is worthwhile to expose 
this to blueprint.
The correction mechanism implemented in `UStaminaMovement::ServerCheckClientError` will trigger a correction if 
the Stamina desyncs in 2 units by default (see `UStaminaMovement::NetworkStaminaCorrectionThreshold`). Note that Stamina corrections won't necessarily correct also our character Transform.

This is not designed to work with blueprint, at all, anything you want exposed to blueprint you will need to do it
Better yet, add accessors from your Character and perhaps a broadcast event for UI to use.

This solution is provided by Cedric 'eXi' Neukirchen and has been repurposed for net predicted Stamina.

## Strafe
Strafe is a shell intended for changing to and from a strafing state, however the actual implementation of
"what strafe does" is highly dependant on a project, so override the functions and define the behaviour yourself.

Generally `AStrafeCharacter::OnStartStrafe` might change `bUseControllerRotationYaw = true` and
`bOrientRotationToMovement = false` then and `AStrafeCharacter::OnEndStrafe` would change them back.

Alternatively you could override any `ACharacter` and `UCharacterMovementComponent` functions that use
`bUseControllerRotationYaw` or `bOrientRotationToMovement` and check `IsStrafing`, but that implementation will be
more advanced and often unnecessary.

# Changelog

### 1.0.3.2
* SprintMovement added option (`bUseMaxAccelerationSprintingOnlyAtSpeed`) for using `MaxAccelerationSprinting` when `IsSprintingAtSpeed()` is false

### 1.0.3.1
* Added NetworkStaminaCorrectionThreshold and exposed via UPROPERTY()

### 1.0.3.0
* `UStaminaMovement::ServerCheckClientError` added by [PR from vorixo](https://github.com/Vaei/PredictedMovement/pull/3/commits/d03a4ec5f7dd07c48e8a9f0b21255fc258adb865)

### 1.0.2.3
* Remove `bForceIWYU`

### 1.0.2.2
* Changed `FSavedMove_Character_Prone::PrepMoveFor` to use `Super` instead of `FSavedMove_Character`

### 1.0.2.1
* Removed `IsSprintingAtSpeed()` check for `USprintMovement::GetMaxSpeed()`

### 1.0.2.0
* Prone lock timer implemented properly (with prediction instead of timer handle) to fix desync
* Can no longer jump while prone

### 1.0.1.5
* Remove ground checks for sprint and strafe movement properties (walking off a ledge shouldn't momentarily change to normal speed, etc)

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
