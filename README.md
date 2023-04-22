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
Net predicted stamina and drain state.

# Demonstration
I use this in my own project, here you can see the character sprinting, consuming stamina, strafing, and proning with high latency (>220ms) and `p.netshowcorrections 1`. As you can see, there is no desync.

https://youtu.be/SHVm57AMruc

# Requirements
C++ knowledge is a must, and this cannot be used without C++.

# Frequently Asked Questions
### Why is such an elaborate solution needed, can't I simply replicate the max speed?
`ACharacter` & `UCharacterMovementComponent` have built-in client-side prediction to provide what feels like latency-free movement, yet with server authority. If you replicate anything that it uses, this will happen outside of the prediction framework and result in considerable desync. It may appear to work in testing, but until you introduce latency, you won't see the actual result.

This solution introduces the abilities and stamina into the prediction framework so that it doesn't desync.

### What is client-side prediction?
This is a complex topic beyond the scope of this readme. Here are some resources:
* [Gabriel Gambetta's "Fast-Paced Multiplayer"](https://www.gabrielgambetta.com/client-server-game-architecture.html). There are 4 parts to this.
* [Video detailing the `UCharacterMovementComponent` class](https://www.youtube.com/watch?v=dOkuIvKCvpg)

### Why is the `ACharacter` class required for RPCs? Can't you just replicate the `UCharacterMovementComponent`?
Replication routes through the `AActor` class, even for `UActorComponent`, these components send an additional `uint8` that identifies which component is replicating. By manually routing through the `ACharacter` class we forego the bandwidth cost of the additional `uint8`. It is an optimization that the engine establishes the standard for.

### Why are there multiple `UCharacterMovementComponent` and `ACharacter`?
Developers who use this plugin need to fork it and change the inheritance so the abilities they want are present in their project's resulting `ACharacter`, thus they do not end up with abilities that their project does not require. There may be a `link` branch that has all abilties linked together for you but it is not always up to date, if this is the case, you can still use it as an example.

# Usage
Because each ability is a combination of `UCharacterMovementComponent` and `ACharacter` you will need to modify the plugin to inherit the abilities you want for your project. I recommend forking this repository to do that, so you can still pull in any future commits.

It is vitally important that you remember to change the inheritance for `FSavedMove_Character` and `FNetworkPredictionData_Client_Character` also at the bottom of the Movement header files AND change the `using Super = ...` to the new Super class! If you experience extreme desync, its because you forgot to do this.

Alternatively, the 'link' branch includes all abilities, you simply need to derive the top-most ability (stamina for movement and sprint for character). This branch may in some cases fall behind main, however.

Note that Stamina does not have an `ACharacter` because no Character-specific functionality is required.

Also note that Stamina inherits Sprint by default, if you want Stamina but not Sprint, you will need to remove this inheritance.

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
solution is to call `GetCharacterMovement()->FlushServerMoves()` from the Character.
It is worthwhile to expose this to blueprint.

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
