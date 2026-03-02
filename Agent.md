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
- Source-control note: do not gitignore `Content/__ExternalActors__` in this project. The template maps are using external actor packages there, so those files are part of the real level data, not disposable generated junk.

## Current Gameplay Systems
- `AAgentCharacter` in `Source/Agent/AgentCharacter.*` now coordinates player movement, camera mode switching, and spawning the persistent companion drone.
- `ADroneCompanion` in `Source/Agent/DroneCompanion.*` is now the drone "vehicle" actor. It always exists in the world once spawned and owns its own physics body and drone camera.
- Press `V` to cycle camera modes in this order:
  - `Third Person`: the actual camera is always the character's normal `FollowCamera` on the spring arm, but the handoff is now faked. From `First Person`, the view instantly jumps to the drone camera's last transform (only if the drone is within `800` uu / 8 meters of the spring target), the real drone is despawned, and then a temporary character-owned transition camera lerps into the normal spring-arm camera position. A hidden-shadow-only proxy mesh rides with that camera path for aesthetics.
  - `Drone Pilot`: entering from `Third Person` now respawns the real drone exactly at the current third-person camera transform and instantly switches to it, then the drone camera itself performs a short internal camera mount / FOV transition into the pilot view so the handoff feels more like the map-mode camera transition instead of a hard snap.
  - `First Person`: leaving `Drone Pilot` is now an instant swap back to the player's first-person camera, with no camera lerp.
- Controller `Y` (`Gamepad_FaceButton_Top`) now mirrors `V` and cycles camera modes the same way.
- Drone pilot control modes now cycle in this order:
  - `Complex`
  - `Horizon`
  - `Horizon + Hover`
  - `Simple`
  - `Free Fly`
  - `Roll`
- `B` on keyboard and `D-pad Left` on controller cycle forward through those six modes.
- Controller shoulders now cycle the same mode list instead of acting as direct toggles:
  - right bumper cycles forward
  - left bumper cycles backward
- Mode meanings:
  - `Complex`: full acro / rate with no self-level and no hover assist
  - `Horizon`: self-level (`Angle / Horizon`) on, hover assist off
  - `Horizon + Hover`: self-level plus helicopter-style hover assist (neutral throttle sits near hover, but tilt still drives forward flight)
  - `Simple`: a first-time-user DJI-style assisted mode tuned closer to consumer `Normal` mode than a hard beginner lock. It auto-levels, uses gradual velocity-style horizontal assist with smoother acceleration/deceleration, keeps vertical hover compensation active, and lets the camera tilt independently. Simple-mode left/right input is now standard again on both keyboard and controller: left goes left, right goes right.
  - `Free Fly`: a spectator-style look-to-fly mode that intentionally ignores the rigid-body drone feel. It now moves kinematically like a smooth Unreal spectator camera instead of using the elastic physics response from the main drone modes.
  - `Roll`: a grounded first-person ball mode. The camera stays visually upright while the drone body rolls underneath it, movement is torque-based on the ground, and `Space` / controller `A` trigger a roll jump. Roll mode bypasses the drone crash / recovery system entirely, resets inherited flight momentum on entry, snaps to an upright yaw-only pose above the floor if it's near the ground, and only applies roll drive while grounded so it behaves like its own ball controller instead of inheriting flight-state reactions.
  - Roll mode's upright camera override should only run during active `PilotControlled` roll play; it must not override scripted camera transitions such as pilot entry or map/minimap camera transitions.
  - Roll mode can apply a small visual camera lean while steering left/right. This is only a camera effect; it should not feed back into physics or self-righting.
- When entering `Drone Pilot`, a temporary entry assist still forces both self-level and hover on until the player gives real up/down throttle input, so the drone does not immediately drop on camera switch.
- Press `M` on keyboard or the controller back/view button for map controls:
- Keyboard `M` and controller back/view now both have split behavior:
  - tap: toggles the normal full `Map Mode`
  - hold: enters a temporary `MiniMap` view instead
- the drone camera becomes a top-down view
  - if `Map Mode` is entered while in `Third Person`, the real drone is spawned first at the current third-person camera transform because `Third Person` no longer keeps a live drone actor around
  - entering map mode from `Third Person` or `First Person` still raises the drone upward from its current location
  - entering map mode while already in `Drone Pilot` does not auto-lift; it keeps the current world position and only animates the camera down into map view
  - entering or leaving map mode now uses a smooth 2-second internal camera transition for pitch / FOV instead of snapping
  - `WASD` / left stick pan in X/Y
  - `Q` / `E` move altitude down / up
  - `RT` moves altitude down and `LT` moves altitude up
- Held `MiniMap` mode is a latched overhead follow state:
  - entering minimap now forces the player camera to first person while the drone flies to a fixed sky height above the player
  - the drone follows the player instead of taking manual pan input
  - pressing `V` while in minimap exits minimap and returns the drone to first-person buddy behavior
  - pressing the map button again while minimap is active jumps the view to the drone's minimap camera instead of exiting minimap
- The drone is no longer a detached hidden sphere inside the character. Its visible physics body is the authoritative collision body in all modes.
- The drone body visual scale is exposed on `ADroneCompanion` as `DroneBodyVisualScale`.
- The third-person proxy mesh is only for the faked third-person camera path. When leaving third person, it must stop casting hidden shadow immediately; hiding it is not enough because `bCastHiddenShadow` will otherwise keep the shadow alive.
- `BuddyFollow` now has its own exposed max speed (`BuddyMaxLinearSpeed`) so the companion can drift back over visibly instead of rocketing to the player.
- The first-person camera transform should be tuned directly on the inherited `FirstPersonCamera` component in the character Blueprint. Do not reapply `FirstPersonCameraOffset` at runtime, or it will override the Blueprint placement.
- Drone controls in `Drone Pilot`:
  - `Complex`, `Horizon`, and `Horizon + Hover`:
    - Keyboard: `W` / `S` pitch, `A` / `D` roll, `Q` / `E` yaw, `R` / `F` thrust
    - Controller: left stick `X` yaw, left stick `Y` thrust, right stick `X` roll, right stick `Y` pitch
  - `Simple`:
    - Keyboard: `W` / `S` tilt forward / back, `A` / `D` tilt left / right, `Q` / `E` yaw, `R` / `F` collective up / down
    - Controller: left stick tilts the drone, right stick `X` yaws, right stick `Y` tilts the camera, `RT` adds thrust, `LT` adds reverse thrust
  - `Free Fly`:
    - Keyboard: `WASD` move, `Q` / `E` and `R` / `F` move vertically, mouse / look controls where the camera faces
    - Controller: left stick moves, right stick looks, `RT` rises, `LT` drops
  - `Roll`:
    - Keyboard: `WASD` roll the ball along the ground relative to camera yaw, `Space` jumps
    - Controller: left stick rolls the ball along the ground relative to camera yaw, right stick looks, `A` jumps
  - Shared:
    - D-pad up / down adjusts drone camera FOV in `+5` / `-5` steps
- Drone systems currently include:
  - always-simulating rigid-body flight on the drone actor
  - pilot mode with six presets: `Complex`, `Horizon`, `Horizon + Hover`, `Simple`, `Free Fly`, and `Roll`
  - optional complex-mode hover-thrust assist that keeps the drone flying like a thrust vehicle while making neutral throttle sit near hover instead of dropping immediately
  - a rebuilt simple mode that now uses DJI-like assisted horizontal velocity control, vertical hover compensation, stronger braking, faster default tilt, camera tilt on the right stick, and an optional follow-target distance cap
  - a dedicated spectator-style free-fly mode that now teleports smoothly like a utility camera and does not inherit the physics "snap" / elastic response from the main drone modes
  - a fully faked third-person camera path where the player uses the stock spring-arm `FollowCamera`, the real drone is despawned in third person, and a hidden-shadow proxy mesh is attached to the character camera path purely for visual continuity
  - a dedicated top-down map mode with its own movement model and a 2-second camera pitch / FOV transition in and out
  - a temporary held `MiniMap` top-down follow mode that tracks the player from a fixed overhead height
  - camera mount transitions are driven internally by `ADroneCompanion`, and the per-tick mode sync should not force-refresh the camera mount anymore unless the mode actually changes; this keeps the map transition and the third-person-to-drone pilot-entry transition visible again
  - an autonomous buddy-follow mode so the drone can ride near the player's left shoulder during first person
  - exposed crash detection and timed recovery while the drone remains physics-simulated
  - crash recovery now recovers on either condition: the recovery timer expires or the drone settles below the crash velocity thresholds; it no longer waits for both, which prevents long "fall all the way to the ground" lockouts
  - crash recovery still waits until linear speed is below `CrashSelfRightActivationSpeed` (default `200` uu/s) before forcing self-right correction, even if the player has acro mode selected
  - `Horizon + Hover` / stabilized hover now temporarily disables hover assist if the drone is upside down enough and slower than `PilotHoverSelfRightActivationSpeed` (default `20` uu/s); once the body is upright again (`PilotHoverSelfRightRestoreUpDot`), hover assist automatically restores. This prevents hover from pinning the drone upside down against the ground.
  - crash detection now uses pre-impact velocity plus collision impulse, so hard hits are measured before physics has a chance to bleed the speed off
  - runtime physical material tuning for friction and bounce
  - live on-screen drone debug output with mode, crash state, impact info, pilot inputs, and camera tilt
  - roll mode now has a dedicated output-log diagnostic path (`bLogRollModeDiagnostics`, `RollModeLogInterval`) that logs roll entry, ignored roll collisions, and throttled `RollTick` state snapshots so we can inspect grounding, forced rotations, linear/angular velocity, and camera lean when the ball misbehaves

## Current Direction
- The drone is now being treated as a persistent companion vehicle instead of an effect inside the character camera rig.
- The main camera fantasy is a three-mode loop: normal third-person camera with a drone proxy animation, direct drone piloting, and first-person character view with the drone as a left-shoulder companion.
- Physics changes should continue pushing the main drone modes toward a believable flying rigid body that can be knocked around; `Free Fly` is the intentional exception as a utility spectator-style mode.
- Guardrail for future edits: in any drone mode where character look input is locked (`Complex`, `Horizon`, `Horizon + Hover`, and `Simple`), never drive planar movement from `GetControlRotation()` / `ViewReferenceRotation`. That frame freezes when look is locked and makes the drone feel world-locked. Use the drone body's yaw (or the live drone camera yaw if camera yaw is ever decoupled) as the movement frame instead. `Free Fly` and `Map` are the exceptions because they intentionally use view-driven movement.
- Speed rule for the main piloted modes: keep the user-facing max speed aligned at `2600` uu/s unless they explicitly ask for a different cap. That currently applies to the rigid-body clamp, `Simple`, `Free Fly`, and map translation speeds. `BuddyFollow` is intentionally exempt because it has its own slower return cap.

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
