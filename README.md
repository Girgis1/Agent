# Agent

`Agent` is an Unreal Engine 5.7 third-person C++ project built from the default third-person template and heavily expanded with a persistent drone companion / camera system.

The project currently supports:
- Normal third-person character play
- A direct drone pilot mode
- A first-person character mode
- A top-down map mode
- Multiple drone control presets, from FPV-style flight to a spectator-style free fly camera

## Engine

- Unreal Engine `5.7`
- Project file: `Agent.uproject`

## Core Systems

### Camera Modes

Press `V` to cycle through:

1. `Third Person`
Uses the standard third-person spring-arm camera. The drone is faked here with a shadow proxy mesh for visual continuity.

2. `Drone Pilot`
Switches view to the real drone and enables direct flight control.

3. `First Person`
Switches view to the character's first-person camera while the drone runs as a companion.

### Map Mode

Press `M` (or the controller back/view button) to toggle `Map Mode`.

In map mode:
- The drone camera points downward
- Movement is top-down pan + height adjustment
- Entering from `Drone Pilot` keeps the drone in place and animates only the camera
- Entering from character camera modes can raise the drone upward into map view

## Drone Control Modes

Press `B` on keyboard or `D-pad Left` on controller to cycle:

1. `Complex`
Full acro / rate style flight.

2. `Horizon`
Self-level enabled, hover assist off.

3. `Horizon + Hover`
Self-level plus hover-style thrust assist.

4. `Simple`
Beginner-friendly assisted drone mode inspired by consumer DJI-style flight.

5. `Free Fly`
A smooth Unreal spectator-style free camera mode.

## Default Controls

### Shared

- `V`: cycle camera mode
- `M`: toggle map mode
- `B`: cycle drone control mode
- `Gamepad D-pad Left`: cycle drone control mode
- `Gamepad Back/View`: toggle map mode
- `Gamepad D-pad Up/Down`: tilt drone camera up/down

### Drone Pilot

#### Complex / Horizon / Horizon + Hover

Keyboard:
- `W/S`: pitch
- `A/D`: roll
- `Q/E`: yaw
- `R/F`: thrust up/down

Controller:
- Left stick `X`: yaw
- Left stick `Y`: thrust
- Right stick `X`: roll
- Right stick `Y`: pitch

#### Simple

Keyboard:
- `W/S`: tilt forward/back
- `A/D`: tilt left/right
- `Q/E`: yaw
- `R/F`: thrust up/down

Controller:
- Left stick: tilt / directional movement
- Right stick `X`: yaw
- Right stick `Y`: camera tilt
- `RT`: thrust
- `LT`: reverse thrust

#### Free Fly

Keyboard:
- `WASD`: move
- `Q/E` and `R/F`: move vertically
- Mouse: look

Controller:
- Left stick: move
- Right stick: look
- `RT`: rise
- `LT`: descend

### Map Mode

Keyboard:
- `WASD`: pan
- `Q/E`: move down/up

Controller:
- Left stick: pan
- `LT/RT`: move up/down

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

