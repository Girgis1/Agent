# Conveyor MVP Plan

## Handoff Note

- Restored conveyor surface-velocity routing:
  - shared world-level tuning now lives on `AFactoryWorldConfig`
  - `ConveyorMasterBeltSpeed` defaults to `60 uu/s`
  - `AConveyorBeltStraight::GetSurfaceVelocity()` is the shared belt output
  - self-driven movers (player, current character variants, drone, and future AI movers) should use `UConveyorSurfaceVelocityComponent`
  - passive physics payloads still use the support-checked physics path

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
- keyboard `3`: select `Smelter`
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
- a shared `GetSurfaceVelocity()` output in the conveyor's forward direction

Passive physics payloads still use direct along-belt velocity control, but self-driven movers should consume the belt's surface velocity during their own movement update instead of being treated like payloads.

Shared speed control is now driven by a world-level config actor:

- `AFactoryWorldConfig`
- `ConveyorMasterBeltSpeed`

Placed conveyors read from that shared value by default so tuning one place affects all belts consistently.

### First Factory Loop Actors

- `AFactoryPayloadActor`
  - dedicated physics payload actor
  - carries a `PayloadType` name for future recipes

- `AStorageBin`
  - intake on the back face
  - destroys incoming payload actors
  - stores counts by `PayloadType`
  - no hidden actor storage

- `ASmelterMachine`
  - consumes input resources from `UMachineInputVolumeComponent`
  - crafts outputs via `UFactoryRecipeProcessorComponent`
  - emits crafted resource chunks through `UMachineOutputVolumeComponent`

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
  - this currently supports conveyors, storage bins, and smelters

## Planned Next Steps

1. Validate the straight conveyor on the default map with physics cubes.
2. Tune overlap clearances so floor placement is reliable.
3. Add a better visual belt mesh / scrolling belt material.
4. Make the placement preview visually distinguish conveyor vs bin vs smelter.
5. Add conveyor adjacency rules for clean line building.
6. Add corners as the second transport buildable.

## Guardrails

- Do not build the project unless explicitly requested by the user.
- Keep conveyor placement restricted to character camera modes for now.
- Keep factory gameplay in `Source/Agent/Factory/` instead of growing `AgentCharacter` more than necessary.

## Resource Rebuild Status

The first post-conveyor resource slice is rebuilt and compiling again.

### Completed

- `UResourceDefinitionAsset` (`DA_Resource_*`) is back and now includes `MassPerUnitKg`.
- `UResourceComponent` now supports direct multi-material authoring via `Materials` entries.
- `AResourceActor` is the new reusable physics resource base actor.
- `AResourceSalvageActor` is removed and redirected to `AResourceActor`.
- `AFactoryPayloadActor` now carries a real resource id and quantity.
- `AStorageBin` is now routed through `UStorageVolumeComponent`.
- `UShredderVolumeComponent`, `UMachineInputVolumeComponent`, and `UMachineOutputVolumeComponent` are restored as modular Blueprint-placeable volumes.
- `AShredderMachine` and `ASmelterMachine` are restored as placeable machine shells.
- `UFactoryRecipeAsset` (`DA_Recipe_*`) now defines machine recipes with:
  - inputs / outputs in whole units
  - craft time
  - machine tag gate
- `UFactoryRecipeProcessorComponent` now drives processor runtime:
  - atomic input consumption at craft start
  - deterministic craft progress timer
  - pause/resume support
  - buffered output handoff to machine output volume
  - states: `Idle`, `WaitingForInput`, `Crafting`, `WaitingForOutputSpace`, `Paused`, `Error`
- Editor-facing quantity fields use whole units, while runtime still stores `x1000` precision internally.
- Generic buckets are blacklist-driven only: accept everything unless blocked.
- `UResourceComponent` randomized-content workflow is implemented:
  - random material picking from the defined entries
  - range-based per-material units
  - optional total-units normalization
  - generated contents locked per spawned actor instance
- World-config mass formula is implemented:
  - `FinalMassKg = TotalMaterialWeightKg + (BaseMassKg * ResourceBaseMassMultiplier)`
  - `BaseMassKg` comes from the mesh/body physics mass settings
  - global scalar in `AFactoryWorldConfig`
- World-config craft speed control is implemented:
  - `AFactoryWorldConfig.FactoryCraftSpeedMultiplier`

### Immediate Next Steps

1. Create first production `DA_Recipe_*` assets (for example `IronOre -> IronBar`).
2. Expose smelter placement in player build-mode flow alongside conveyor/storage.
3. Add machine UI/debug readout for active recipe, progress, and block reason.
4. Add recipe-family machine tags (Smelter/Assembler/Refinery) and authoring validation.
