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
- dedicated payload actor plus generic simulating bodies still work on conveyors
- no networking
- no save system
- no corner belts, stackers, or splitters yet

## Controls

### Conveyor Placement

- `1` / controller `X`: enter or exit conveyor placement mode
- keyboard `1`: select `Conveyor`
- keyboard `2`: select `Storage Bin`
- keyboard `3`: select `Resource Spawner`
- controller `X`: toggle placement for the currently selected buildable
- `Left Mouse` / controller `RT`: place the currently selected factory buildable
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

Shared speed control is now driven by the player character's master settings:

- `ConveyorMasterBeltSpeed`
- `ConveyorMasterBeltAcceleration`

Placed conveyors read from those shared values by default so tuning one place affects all belts consistently.

### First Factory Loop Actors

- `AFactoryPayloadActor`
  - dedicated physics payload actor
  - carries a `PayloadType` name for future recipes

- `AStorageBin`
  - intake on the back face
  - destroys incoming payload actors
  - stores counts by `PayloadType`
  - no hidden actor storage

- `AResourceSpawnerMachine`
  - outputs from the front face
  - periodically spawns payload actors
  - skips spawning if the output is physically blocked

### Placement

Placement mode currently uses:

- camera forward trace to find an aim point
- downward trace to find a horizontal build surface
- grid snapping for final location
- overlap validation for blocked placements
- a preview actor plus debug box / direction arrow for validity feedback
- manual in-place rotation
- conveyor face snapping:
  - if the aim trace hits an existing factory buildable, the new buildable snaps to the hit actor's nearest side face
  - this allows continuing belt lines in mid-air without needing ground under the next tile
  - this currently supports conveyors, storage bins, and resource spawners

## Planned Next Steps

1. Validate the straight conveyor on the default map with physics cubes.
2. Tune overlap clearances so floor placement is reliable.
3. Add a better visual belt mesh / scrolling belt material.
4. Make the placement preview visually distinguish conveyor vs bin vs spawner.
5. Add conveyor adjacency rules for clean line building.
6. Add corners as the second transport buildable.

## Guardrails

- Do not build the project unless explicitly requested by the user.
- Keep conveyor placement restricted to character camera modes for now.
- Keep factory gameplay in `Source/Agent/Factory/` instead of growing `AgentCharacter` more than necessary.

## Resource Rebuild Status

The first post-conveyor resource slice is rebuilt and compiling again.

### Completed

- `UResourceDefinitionAsset` and `UResourceCompositionAsset` are back.
- `UResourceComponent` and `AResourceSalvageActor` are back for physical salvage authoring.
- `AFactoryPayloadActor` now carries a real resource id and quantity.
- `AStorageBin` is now routed through `UStorageVolumeComponent`.
- `UShredderVolumeComponent`, `UMachineInputVolumeComponent`, and `UMachineOutputVolumeComponent` are restored as modular Blueprint-placeable volumes.
- `AShredderMachine` and `AProcessorMachine` are restored as placeable machine shells.
- Editor-facing quantity fields use whole units, while runtime still stores `x1000` precision internally.
- Generic buckets are blacklist-driven only: accept everything unless blocked.

### Immediate Next Steps

1. Create the first real `DA_Resource_*` assets (`Metal`, `Plastic`, `Stone`, `IronOre`).
2. Create the first `UResourceCompositionAsset` examples for multi-output salvage such as `CarDoor`.
3. Make a reusable `BP_IronOre` from `AResourceSalvageActor` and drive its mesh family through `MeshVariants`.
4. Decide whether simple-item authoring should stay split between the BP and `UResourceComponent`, or move more of that setup into the `DA_Resource_*` assets.
