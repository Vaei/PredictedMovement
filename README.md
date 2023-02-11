# PredictedMovement
This plugin offers several shells usually with a derived `UCharacterMovementComponent` and `ACharacter` which form a single net predicted ability.

## Predicted Abilities

### Prone
Similar functionality as crouching. Also supports change in radius. Has a lock timer, to prevent early exit from prone.

Compatible with crouching, and should switch properly between crouch->prone, stand->prone, prone->crouch, etc.

### Strafe
A shell intended for switching to / from a strafe mode, however the actual functionality is not implemented as it is project-dependent.

### Sprint
Well, it makes you sprint. Check `USprintMovement.h` header for capabilities.

### Stamina
Net predicted stamina and drain state. Read the `UStaminaMovement.h` header for a quick guide on how to use (see below also).

## Example
I use this in my own project, here you can see the character sprinting, consuming stamina, strafing, and proning with high latency (>220ms) and `p.netshowcorrections 1`. As you can see, there is no desync.

https://youtu.be/SHVm57AMruc

## Requirements
C++ knowledge is a must, and this cannot be used without C++.

## Usage
Because each ability is a combination of `UCharacterMovementComponent` and `ACharacter` you will need to modify the plugin to inherit the abilities you want for your project. I recommend forking this repository to do that, so you can still pull in any future commits.

Note that Stamina does not have an `ACharacter` because no Character-specific functionality is required.

Also note that Stamina inherits Sprint by default, if you want Stamina but not Sprint, you will need to remove this inheritance.

Once that is complete, you simply need to call the equivalent input function.

### Stamina
Add a `void CalcStamina(float DeltaTime)` function to your `UCharacterMovementComponent` and call it before Super after overriding `CalcVelocity`.

From your new `CalcStamina` function you can then modify stamina. Eg. `SetStamina(GetStamina() + (10.f * DeltaTime));` will regenerate the stamina.

`CalcVelocity` stems from `PerformMovement` and is called from all primary physics loops that have sub-ticks between frames for incredibly high accuracy. `PerformMovement` should suffice if you have a reason to not update stamina between ticks.

Read `UStaminaMovement.h` for the rest.

You might want to expose Stamina to blueprint from your derived `ACharacter`, possible with multicast delegates that blueprints can bind to, to update the UI.

### Strafe
No guidelines are available for this, if you need strafing then use the events found in `StrafeMovement.h` to determine your implementation strategy.

## Changelog

### 1.0.0.1
Added Stamina

### 1.0.0.0
Initial Release