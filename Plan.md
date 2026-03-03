# Conveyor MVP Plan

## Goal

Build the first factory-system vertical slice:

- placeable straight conveyors
- `1m x 1m` footprint (`100 uu x 100 uu`)
- snap to a fixed grid
- move physics objects that land on top
- work from `Third Person` and `First Person` only

This is the foundation for later factory systems such as corners, splitters, lifts, placement rules, and automation gameplay.

## Current MVP Scope

- one straight conveyor tile only
- flat placement only
- 90-degree rotation only
- physics cubes / generic simulating bodies as payloads
- no networking
- no save system
- no corner belts, stackers, or splitters yet

## Controls

### Conveyor Placement

- `1` / controller `X`: enter or exit conveyor placement mode
- `Left Mouse` / controller `RT`: place conveyor
- `Right Mouse` / controller `B`: cancel placement mode
- `R` / controller `Right Bumper`: rotate right by `90`
- controller `Left Bumper`: rotate left by `90`

### Drone Modes

- `B`: cycle forward on keyboard
- controller `D-pad Left`: cycle backward
- controller `D-pad Right`: cycle forward

The bumpers are reserved for conveyor rotation while placing.

## Technical Decisions

### Grid

- Grid size is fixed at `100 uu`
- Conveyor origin is placed on the build surface at the center of the tile
- Placement yaw snaps to quarter turns only

### Collision Channels

- `BuildPlacement` = `ECC_GameTraceChannel1`
- `FactoryBuildable` = `ECC_GameTraceChannel2`

These channels are reserved in `DefaultEngine.ini`.

### Conveyor Movement

The straight conveyor uses:

- a blocking support collision volume
- a separate top overlap volume to detect payloads
- per-tick velocity change in the conveyor's forward direction

The belt accelerates payloads toward a target belt speed instead of teleporting them.

### Placement

Placement mode currently uses:

- camera forward trace to find an aim point
- downward trace to find a horizontal build surface
- grid snapping for final location
- overlap validation for blocked placements
- a preview actor plus debug box / direction arrow for validity feedback

## Planned Next Steps

1. Validate the straight conveyor on the default map with physics cubes.
2. Tune overlap clearances so floor placement is reliable.
3. Add a better visual belt mesh / scrolling belt material.
4. Add placement chaining so consecutive placements feel fast.
5. Add conveyor adjacency rules for clean line building.
6. Add corners as the second buildable.

## Guardrails

- Do not build the project unless explicitly requested by the user.
- Keep conveyor placement restricted to character camera modes for now.
- Keep factory gameplay in `Source/Agent/Factory/` instead of growing `AgentCharacter` more than necessary.
