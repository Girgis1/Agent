# Ragdoll Integration Plan (Agent Project)

## Purpose

Define a full, integration-safe plan for adding ragdoll to the main playable character (`AAgentCharacter`) with:

- Button press to enter ragdoll.
- Auto get-up once physical velocity is low for a sustained duration.
- Minimal regressions across drone/camera/factory/interaction systems.

This document is intentionally detailed so we catch hidden dependencies before touching code.

---

## Current Project Reality (What We Have Today)

### Active runtime path (default map/mode)

- Default map and game mode resolve to third-person blueprint flow.
- `BP_ThirdPersonGameMode` points to:
  - `BP_ThirdPersonCharacter` (inherits `AAgentCharacter`).
  - `BP_ThirdPersonPlayerController` (inherits `AAgentPlayerController`).

### Main character complexity (`AAgentCharacter`)

`AAgentCharacter` is not a plain character. It owns and coordinates:

- Multi-view camera state:
  - `ThirdPerson`, `FirstPerson`, `DronePilot`.
  - `Map` and `MiniMap` overlays/flows.
- Drone actor lifecycle and control mode switching.
- Pickup physics handle workflow (hold/rotate/strength tuning).
- Modular interaction components (`Interactor`, dual-handle grab, vehicle push).
- Vehicle entry/exit control pass-through.
- Factory placement mode and preview actor.
- Direct key binds mixed with Enhanced Input action binds.

This means ragdoll touches more than mesh physics; it must be treated as a top-level locomotion/control state.

### Existing ragdoll references in repo

- Combat variant (`ACombatCharacter`, `ACombatEnemy`) contains basic partial/full ragdoll examples:
  - `SetSimulatePhysics(true)`, `SetPhysicsBlendWeight`, impulse-on-hit.
- Content has ragdoll-related assets:
  - `IA_Ragdoll`, `IA_GetUp`, `IMC_Ragdoll`, `BPI_Ragdoll`.
  - `RagdollAnimations/*` (stand-up and related clips).

Important: those assets are not currently wired into the active `/Game/Input/IMC_Default` path used by the default controller flow.

---

## Goals / Non-Goals

## Goals

- Deterministic and stable ragdoll entry/exit for `AAgentCharacter`.
- No soft-locks (stuck in interaction, map mode, drone mode, etc.).
- Automatic recovery based on low velocity (not manual get-up only).
- Add clear debug hooks to tune thresholds quickly.

## Non-Goals (for first iteration)

- Perfect animation-matched stand-up from arbitrary pose.
- Network replication/multiplayer correctness.
- Combat damage-driven ragdoll blending reuse in this pass.
- Full cinematic camera rig for ragdoll.

---

## High-Level Design

Use explicit state, not ad-hoc booleans spread across functions.

## Proposed ragdoll state model

- `Normal`
- `Ragdoll`
- `Recovering` (optional but recommended if stand-up montage or delayed restore is used)

Minimal version can be `bool bIsRagdolling`, but enum is safer for future expansion.

## Entry trigger

- Single “ragdoll toggle” input.
- Recommended first pass:
  - Bind direct key in `SetupPlayerInputComponent` (fast path).
  - Later migrate to Enhanced Input action in active IMC if needed.

## Auto-recovery signal

- Poll physics linear speed each tick while ragdolling.
- Require:
  - Speed below threshold **and**
  - Stable for N seconds.

This avoids instant get-up from tiny velocity dips.

---

## Integration Matrix (Where Ragdoll Must Intercept Existing Systems)

## 1) Movement / input routing

Functions impacted:

- `DoMove`
- `DoLook` (optional restriction)
- `DoJumpStart`
- `DoJumpEnd`

Plan:

- Block movement/jump while ragdolled.
- Keep or block look input based on camera feel target (decide early).

Risk if missed:

- Player can inject character movement while mesh simulates physics, causing jitter or capsule desync.

## 2) Camera and view-mode state machine

Functions impacted:

- `CycleViewMode`, `ApplyViewMode`
- `ToggleMapMode`, `EnterMiniMapMode`, `ExitMiniMapMode`

Plan:

- During ragdoll, disallow view-mode transitions or enforce one safe view (recommended: stay in current non-map character view).
- Prevent entering map/minimap while ragdolled.

Risk if missed:

- View target swaps to drone while body ragdolls offscreen.
- Camera transition actors/third-person proxy become visually incoherent.

## 3) Drone systems

Systems impacted:

- Drone pilot control state syncing.
- Roll-mode and map-mode transitions.

Plan:

- Force ragdoll to be character-centric:
  - No entry to ragdoll while in active drone pilot/map if we want v1 simplicity, or
  - If allowed, forcibly exit those states before enabling ragdoll.

Risk if missed:

- Competing control authority between drone and ragdoll character.

## 4) Interaction stack

Systems impacted:

- `InteractorComponent` active interaction.
- `DualHandleGrabComponent`.
- `GrabVehiclePushComponent`.
- Pickup physics handle (`TryBeginPickup` / `EndPickup` / `UpdateHeldPickup`).

Plan:

- On ragdoll enter:
  - End active interaction.
  - End dual-handle/vehicle push.
  - Force `EndPickup`.
- While ragdolled:
  - Return false from interaction capability checks.

Risk if missed:

- Held objects remain constrained to the player while body ragdolls.
- Cart/vehicle interactions continue to drive inputs.

## 5) Vehicle possession flow

System impacted:

- `VehicleInteractionComponent` control handoff.

Plan:

- Block ragdoll entry while controlling vehicle (or first force exit vehicle safely).

Risk if missed:

- Mixed movement ownership between vehicle pawn and ragdoll character.

## 6) Factory placement mode

Systems impacted:

- Placement mode entry/update/place/cancel functions.

Plan:

- On ragdoll enter: `ExitConveyorPlacementMode`.
- While ragdolled: capability checks return false.

Risk if missed:

- Ghost placement previews and build actions during ragdoll.

## 7) Collision / capsule / mesh physics handoff

Core technical area:

- Capsule must stop driving movement during ragdoll.
- Skeletal mesh collision profile must support ragdoll.
- Mesh physics must wake and fully simulate.

Plan:

- Enter:
  - Disable character movement.
  - Disable or reduce capsule collision influence.
  - Switch mesh collision profile to `Ragdoll`.
  - Enable mesh body simulation.
- Exit:
  - Disable mesh simulation.
  - Restore mesh relative transform/collision profile.
  - Restore capsule collision + movement mode.
  - Reposition actor capsule near ragdoll final location before resuming control.

Risk if missed:

- Character snaps back to old capsule location.
- Mesh/capsule separate permanently.
- Character falls through floor or blocks itself.

---

## Detailed Runtime Flow

## Enter Ragdoll (button press)

1. Validate preconditions:
   - Not already ragdolled.
   - Not in blocked control states (vehicle control, optional drone/map states).
2. Snapshot data for recovery:
   - Current actor transform (or at least yaw).
   - Mesh relative transform.
   - Mesh/capsule offset needed for actor relocation.
   - Original mesh collision profile.
3. Hard-stop incompatible systems:
   - End interactions/grabs/push/pickup.
   - Exit placement mode.
4. Suspend locomotion:
   - `StopMovementImmediately`.
   - `DisableMovement`.
5. Switch physics ownership to mesh:
   - Capsule collision disable or minimal mode.
   - Mesh collision profile => `Ragdoll`.
   - Enable body simulation and wake rigid bodies.
6. Mark ragdoll state active and reset velocity timers.

## Ragdoll Tick

Each tick while ragdolling:

1. Read physics speed (prefer pelvis/root body velocity).
2. Compare to threshold.
3. If below threshold:
   - Accumulate stable-low-velocity timer.
4. If above threshold:
   - Reset stable timer.
5. When timer exceeds required duration:
   - Start recovery.

## Exit Ragdoll (auto recovery)

1. Sample final ragdoll pose data:
   - World location to anchor capsule.
   - Optional yaw source.
2. Disable mesh simulation cleanly.
3. Restore mesh transform baseline and mesh collision profile.
4. Re-enable capsule collision.
5. Move actor/capsule to recovered location:
   - Prefer sweep-safe relocation to avoid embedding.
   - Add fallback if blocked.
6. Restore movement mode (`Walking`).
7. Clear ragdoll/recovery state and timers.
8. Resume normal input gating.

---

## Critical Technical Decisions (Must Decide Up Front)

## Decision 1: Input source

Options:

- Direct key bind in C++ (fastest, lowest asset churn).
- Enhanced Input action in active `/Game/Input/IMC_Default` (cleaner long-term).

Recommendation:

- Start direct key bind for bring-up, then migrate to EI action once behavior is stable.

## Decision 2: Allow ragdoll in drone/map states?

Options:

- Disallow for v1 (simpler and safer).
- Allow, but force transition back to character mode before ragdoll.

Recommendation:

- Disallow initially to reduce state explosion.

## Decision 3: Recovery style

Options:

- Instant stand (physics off + snap capsule + restore locomotion).
- Stand-up montage (front/back get-up) with short `Recovering` state.

Recommendation:

- Instant stand for phase 1.
- Montage-based recovery as phase 2 polish.

## Decision 4: Velocity bone source

Options:

- Mesh root velocity.
- Pelvis velocity.

Recommendation:

- Pelvis if available; fallback to root.

---

## Edge Cases and Mitigations

## Edge case: Missing or invalid physics asset

Symptoms:

- No proper body simulation, or simulation warnings.

Mitigation:

- Guard entry with checks and log warning once.

## Edge case: Ragdoll on steep slope or stairs

Symptoms:

- Recovery snaps into geometry or air.

Mitigation:

- Post-ragdoll capsule relocation with floor trace/sweep correction.

## Edge case: Recovery while still slightly moving

Symptoms:

- Twitching loop enter/exit.

Mitigation:

- Stable-time hysteresis (speed must remain low for sustained duration).

## Edge case: Input spam during transitions

Symptoms:

- Reentrant entry/exit calls.

Mitigation:

- Early return guards per state (`Normal` only enter; `Ragdoll` only exit).

## Edge case: Active pickup constraint during ragdoll

Symptoms:

- Object remains tethered or explodes.

Mitigation:

- Force `EndPickup` before enabling ragdoll.

## Edge case: Map/minimap/drone transition during ragdoll

Symptoms:

- View target confusion.

Mitigation:

- Block map/view toggles while ragdolling.

## Edge case: Vehicle controlled when ragdoll requested

Symptoms:

- Control authority conflict.

Mitigation:

- Ignore ragdoll request or cleanly exit vehicle first.

---

## File-Level Change Plan (When Implementation Starts)

Primary:

- `Source/Agent/AgentCharacter.h`
  - Add ragdoll state + tunables + function declarations.
- `Source/Agent/AgentCharacter.cpp`
  - Add input bind, enter/exit logic, tick monitoring, and system guards.

Possible later:

- `Content/Input/*` or `Content/ThirdPerson/Input/*`
  - If migrating to Enhanced Input action wiring.
- Animation blueprints / montage assets
  - If adding stand-up animation phase.

---

## Suggested Tunables (Initial Defaults)

- `RagdollLowSpeedThreshold`: `10-25 cm/s` (start ~`15`).
- `RagdollLowSpeedDuration`: `0.75-1.25 s` (start ~`1.0`).
- `RagdollVelocityBoneName`: `pelvis`.
- Optional cool-down to prevent immediate retrigger: `0.3-0.5 s`.

All tunables should be `EditAnywhere` on character for fast in-editor tuning.

---

## Debug and Telemetry Plan

Add temporary debug output:

- Current ragdoll state.
- Current sampled ragdoll speed.
- Current low-speed accumulated time.
- Reason for blocked entry (vehicle/map mode/etc.).

Optional:

- On-screen debug text while tuning.
- `UE_LOG` category for ragdoll lifecycle events.

---

## Validation / Test Matrix

## Smoke tests

- Press ragdoll key in third-person idle.
- Press ragdoll key while moving.
- Verify auto get-up after settling.

## Interaction tests

- Ragdoll while:
  - Holding pickup object.
  - In active interact mode.
  - Grabbing dual-handle.
  - Pushing vehicle.
- Confirm all are ended cleanly and no stuck state persists.

## Camera/state tests

- Attempt view mode switch while ragdolled.
- Attempt map/minimap while ragdolled.
- Ensure no camera target corruption.

## Factory tests

- Enter placement mode, then ragdoll.
- Confirm preview actor is cleaned up and placement disabled.

## Movement recovery tests

- Ragdoll on flat ground, slope, stairs, near walls.
- Validate recovery capsule relocation does not embed in geometry.

## Stress tests

- Rapid repeated ragdoll presses.
- Ragdoll near dynamic physics clutter.

---

## Rollout Strategy

## Phase 1: Functional

- Toggle ragdoll in/out with auto low-speed recovery.
- Hard block conflicting systems.
- No stand-up montage.

## Phase 2: Integration polish

- Better camera behavior during ragdoll.
- Cleaner actor relocation.
- Optional front/back stand-up montage.

## Phase 3: UX polish

- Dedicated input action mapping.
- Optional VFX/SFX hooks.
- Optional damage-based trigger reuse.

---

## Acceptance Criteria

1. Character enters ragdoll reliably from allowed states.
2. Character auto-recovers after low-speed settle without manual input.
3. No persistent broken state in:
   - movement
   - camera/view modes
   - drone flow
   - pickup/interactions
   - factory placement.
4. No frequent clipping/teleport artifacts in standard map traversal.

---

## Open Questions (Need Product Decisions Before Final Implementation)

1. Should ragdoll be allowed during drone pilot/map/minimap, or only in first/third person?
2. Should look input remain active while ragdolled?
3. Should button press be a toggle (press again to force get-up) or one-way (auto only)?
4. Do we want stand-up animation in v1 or instant recovery only?
5. Do we want fall-damage/impact-based auto-ragdoll later, or only manual trigger?

---

## Summary

Ragdoll is straightforward in isolation, but this project’s rich character stack makes integration the real work.  
If we follow this plan and gate every conflicting subsystem at a single character-state layer, we can ship a reliable v1 quickly and then layer animation polish safely.

