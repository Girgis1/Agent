# Agent Project Notes

## Working Preferences
- NEVER run an Unreal build unless the user explicitly asks for it.
- Unrequested builds waste usage and should be avoided.
- Keep this file updated as gameplay systems change so future sessions can pick up quickly.

## Project Context
- Project root: `d:\UnrealEngine\Agent`
- Engine version: Unreal Engine 5.7
- Base template: third-person C++ template using Enhanced Input
- Module note: `Source/Agent/Agent.Build.cs` must include `PhysicsCore` because the drone crash system uses `UPhysicalMaterial`.

## Current Gameplay Systems
- `AAgentCharacter` in `Source/Agent/AgentCharacter.*` now coordinates player movement, camera mode switching, and spawning the persistent companion drone.
- `ADroneCompanion` in `Source/Agent/DroneCompanion.*` is now the drone "vehicle" actor. It always exists in the world once spawned and owns its own physics body and drone camera.
- Press `V` to cycle camera modes in this order:
  - `Third Person`: the actual camera is the character's normal `FollowCamera` on the spring arm. The drone sphere visually lerps to that same camera transform so it looks like the drone is becoming the camera while keeping a visible body and shadow.
  - `Drone Pilot`: the view target stays on the drone, and player drone inputs directly fly the drone.
  - `First Person`: the view target switches to the player's first-person camera, and the drone returns to the left-shoulder buddy position beside the player.
- Press `B` on keyboard or `D-pad Left` on controller to toggle the drone pilot control scheme:
  - `Complex`: the existing FPV-style acro controls
  - `Simple`: an easier smooth-flying mode with auto-leveling, planar movement, and trigger-based altitude
- In `Complex` mode, use the controller bumpers to pick the flight feel:
  - left bumper: toggle `Hover Assist`: instead of locking altitude, it adds enough tilt-aware baseline thrust to roughly counter gravity, and your throttle input adds or subtracts from that hover baseline like a helicopter collective
  - right bumper: toggle `Angle / Horizon` self-level assist on and off (`Acro / Rate` when off)
  - `Simple` and `Map` already auto-level by design, so the main effect is in advanced flight
  - when entering `Drone Pilot`, a temporary entry assist forces both of those aids on until the player gives real up/down throttle input, so the drone does not immediately drop on camera switch
- Press `M` on keyboard or the controller back/view button to toggle `Map Mode`:
  - the drone camera becomes a top-down view
  - the drone rises upward from its current location
  - `WASD` / left stick pan in X/Y
  - `Q` / `E` move altitude down / up
  - `RT` moves altitude down and `LT` moves altitude up
- The drone is no longer a detached hidden sphere inside the character. Its visible physics body is the authoritative collision body in all modes.
- The drone body visual scale is exposed on `ADroneCompanion` as `DroneBodyVisualScale`.
- Drone controls in `Drone Pilot`:
  - `Complex`:
    - Keyboard: `W` / `S` pitch, `A` / `D` roll, `Q` / `E` yaw, `R` / `F` thrust
    - Controller: left stick `X` yaw, left stick `Y` thrust, right stick `X` roll, right stick `Y` pitch
  - `Simple`:
    - Keyboard: `WASD` horizontal movement, `Q` / `E` (and `R` / `F`) altitude
    - Controller: left stick move, `RT` up, `LT` down
  - Shared:
    - D-pad up / down adjusts drone camera tilt outside of map mode
- Drone systems currently include:
  - always-simulating rigid-body flight on the drone actor
  - pilot mode with switchable complex and simple flight models
  - optional complex-mode hover-thrust assist that keeps the drone flying like a thrust vehicle while making neutral throttle sit near hover instead of dropping immediately
  - a faked third-person camera path where the player uses the stock spring-arm `FollowCamera`, while the drone sphere animates to that same transform as a visual proxy
  - a dedicated top-down map mode with its own movement model
  - an autonomous buddy-follow mode so the drone can ride near the player's left shoulder during first person
  - exposed crash detection and timed recovery while the drone remains physics-simulated
  - crash recovery now waits until linear speed is below `CrashSelfRightActivationSpeed` (default `200` uu/s) before forcing self-right correction, even if the player has acro mode selected
  - crash detection now uses pre-impact velocity plus collision impulse, so hard hits are measured before physics has a chance to bleed the speed off
  - runtime physical material tuning for friction and bounce
  - live on-screen drone debug output with mode, crash state, impact info, pilot inputs, and camera tilt

## Current Direction
- The drone is now being treated as a persistent companion vehicle instead of an effect inside the character camera rig.
- The main camera fantasy is a three-mode loop: normal third-person camera with a drone proxy animation, direct drone piloting, and first-person character view with the drone as a left-shoulder companion.
- Physics changes should continue pushing the drone toward a believable flying rigid body that can be knocked around, not a free-fly spectator camera.

## Future Version Notes
- Continue tuning the flight feel toward professional FPV sims like DRL / Liftoff / VelociDrone style handling.
- Prioritize tuning exposed drone variables before adding cosmetic systems; the key settings now live mostly on `ADroneCompanion`.
- The current overhaul uses a dedicated drone actor and should stay actor-based rather than moving back to an embedded character-camera implementation.
- If the roll-on-ground feel still needs more authenticity, the next likely upgrade is replacing angular-velocity targeting with direct torque control.
- Likely future additions:
  - configurable rates / expo / deadzones
  - better drone mesh / collision shape than a perfect sphere
  - arming / disarming flow
  - optional FPV HUD / OSD
  - better collision response, surface scraping, and obstacle avoidance in autonomous modes
  - optional sim features like battery sag, propwash, or camera tilt

## Build Status
- Last confirmed successful builds on 2026-03-02:
  - `AgentEditor Win64 Development`
  - `Agent Win64 Development`
- These builds were revalidated after the acro control and crash-physics rewrite on 2026-03-02.
- The most recent failed `AgentEditor` attempt on 2026-03-02 was blocked by Unreal Live Coding being active, not a confirmed compile error in the module source.
- Do not rebuild automatically after edits unless the user asks.
