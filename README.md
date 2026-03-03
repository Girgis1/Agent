# Agent

`Agent` is an Unreal Engine 5.7 third-person C++ project built from the default third-person template and heavily expanded with a persistent drone companion / camera system.

The project currently supports:
- Normal third-person character play
- A direct drone pilot mode
- A first-person character mode
- A top-down map mode
- Multiple drone control presets, from FPV-style flight to a separate spectator-style dev camera

## Engine

- Unreal Engine `5.7`
- Project file: `Agent.uproject`

## Core Systems

### Camera Modes

Tap `V` (or controller `Y`) to toggle the character cameras. Hold it to enter `Drone Pilot`:

1. `Third Person`
Uses the standard third-person spring-arm camera. The drone is faked here with a shadow proxy mesh for visual continuity.

2. `Drone Pilot`
Switches view to the real drone and enables direct flight control.

3. `First Person`
Switches view to the character's first-person camera while the drone runs as a companion.

### Map Mode

Tap `M` (or the controller back/view button) to toggle `Map Mode`. Hold it to enter `MiniMap`.

In map mode:
- The drone camera points downward
- Movement is top-down pan + height adjustment
- Entering from `Drone Pilot` keeps the drone in place and animates only the camera
- Entering from character camera modes can raise the drone upward into map view

## Drone Control Modes

Use `B` on keyboard to cycle forward, or `Gamepad D-pad Left / Right` to cycle backward / forward:

1. `Complex`
Full acro / rate style flight.

2. `Horizon`
Self-level enabled, hover assist off.

3. `Horizon + Hover`
Self-level plus hover-style thrust assist.

4. `Simple`
Beginner-friendly assisted drone mode inspired by consumer DJI-style flight.

5. `Free Fly`
Camera-relative 6DoF flight that can freely pitch through vertical and roll upside down.

6. `Roll`
Grounded / trick mode that preserves momentum and uses the roll-jump charge system.

7. `Spectator Cam`
A plain Unreal-style dev fly camera with simple first-person fly controls.

## Controls

### On Foot

- Character locomotion still uses the project's Enhanced Input mappings (default template bindings are `WASD`, mouse look, and `Space`, plus left stick / right stick / `Gamepad A`).
- `E`: hold a physics object from the exact pinch point if one is in range; otherwise it falls back to the drone's context-sensitive right-side input.
- `[` / `]`: decrease / increase player pickup strength by `0.1` (drone pickup strength stays synced to `10%` of the player value).
- `Gamepad RT`: hold to pick up a physics object, or place a buildable while factory placement mode is active.
- `Gamepad LT` while holding an object: enable loose pickup rotation; right stick guides the held object relative to the active camera.

### Camera And View

- `V` / `Gamepad Y`: tap toggles between `First Person` and `Third Person`. Hold for about `0.25s` to enter `Drone Pilot`.
- `M` / `Gamepad View/Back`: tap toggles `Map Mode`. Hold for about `0.35s` to enter `MiniMap`. Tapping again while already in `MiniMap` focuses the drone camera.
- `Gamepad D-pad Up / Down`: tilt the drone camera up / down in `5` degree steps.

### Drone Pilot Shared

- `B`: cycle drone control mode forward.
- `Gamepad D-pad Left / Right`: cycle drone control mode backward / forward.
- `Space` / `Gamepad A`: context-sensitive roll action.
- From any non-map drone flight mode, pressing it enters `Roll` and starts charging the next roll jump.
- While already in `Roll`, hold to charge and release to jump if grounded.
- Releasing while airborne in `Roll` smoothly lifts the drone back toward the last flight mode.

### Drone Pilot: Complex / Horizon / Horizon + Hover

- Keyboard: `W/S` pitch, `A/D` roll, `Q/E` yaw, `R/F` vertical thrust.
- Controller: left stick `X` yaw, left stick `Y` thrust, right stick `X` roll, right stick `Y` pitch.

### Drone Pilot: Simple

- Keyboard: `W/S` forward / back tilt, `A/D` left / right tilt, `Q/E` yaw, `R/F` vertical thrust.
- Controller: left stick moves the drone, right stick `X` yaws, right stick `Y` tilts the camera, `RT/LT` move vertically.

### Drone Pilot: Free Fly

- Keyboard: `WASD` move relative to the current view, `E/R` move up, `Q/F` move down, mouse look controls orientation.
- Controller: left stick moves, right stick looks, `RT` rises, `LT` descends.

### Drone Pilot: Roll

- Keyboard: `W/S` drive forward / back, `A/D` drive left / right relative to the camera, mouse look controls aim, `Space` charges / jumps.
- Controller: left stick drives, right stick looks, `Gamepad A` charges / jumps.

### Drone Pilot: Spectator Cam

- Keyboard: `WASD` move, `E/R` move up, `Q/F` move down, mouse look.
- Controller: left stick moves, right stick looks, `RT` rises, `LT` descends.

### Map Mode

- Keyboard: `WASD` pan, `E/R` move up, `Q/F` move down.
- Controller: left stick pans, `RT` moves up, `LT` moves down.

### Factory Placement

- `1`: toggle conveyor placement.
- `2`: toggle storage bin placement.
- `3`: toggle resource spawner placement.
- `Gamepad X`: toggle the current factory placement mode.
- `Left Mouse Button` / `Gamepad RT`: place the current buildable.
- `Right Mouse Button` / `Gamepad B`: cancel placement.
- `Gamepad LB / RB`: rotate the placement preview.

## Key Source Files

- `Source/Agent/AgentCharacter.*`
Character movement, camera mode switching, drone spawning, and player input routing.

- `Source/Agent/DroneCompanion.*`
Drone actor, physics, pilot logic, camera transitions, map mode, crash handling, and autonomous companion behavior.

- `Agent.md`
Internal running notes used to preserve design intent and implementation rules between sessions.

## Notes

- The drone is a real world actor with a visible physics body.
- Third-person uses the standard character camera; the drone presence there is mostly a visual effect.
- The main user-facing max speed is currently aligned around `2600` uu/s for the major piloted modes.
- `BuddyFollow` intentionally returns more slowly so the companion drifts back instead of snapping.

## Building

Open `Agent.uproject` in Unreal Engine 5.7 and build through the editor or Visual Studio as a normal Unreal C++ project.
