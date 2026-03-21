# Tools V2 Setup (Single-Player)

This is the canonical setup for equipable broom/tools in Agent.
There is no legacy `HeldToolComponent` / `HoldableToolComponent` path anymore.

## Runtime architecture

- Character owns one `UToolSystemComponent`.
- Each world tool actor (for example `BP_Broom`) owns one `UToolWorldComponent`.
- Each tool actor also owns one behavior component (for broom: `UBroomSweepBehaviorComponent`).
- Behavior drives procedural pose and interactions while equipped.
- `ABroomToolActor` now ships as a native prefab-style base actor with all references auto-wired.

## Fast path (recommended)

1. Create a Blueprint class from `ABroomToolActor` (name it `BP_Broom`).
2. Set the mesh on `ToolMesh`.
3. Optionally pick `ToolDefinitionAsset`:
   - `/Game/Tools/Data/DA_BroomToolDefinition_Default`
   - `/Game/Tools/Data/DA_BroomToolDefinition_Heavy`
4. Move `GripPoint` and `HeadPoint` to match your mesh.
5. Done: references are auto-synced via native construction.

## Character setup (`BP_ThirdPersonCharacter`)

1. Ensure C++ parent is `AAgentCharacter` with `ToolSystemComponent` present.
2. Ensure mesh has socket `HandGrip_R` (or set another socket in tool definition asset).
3. Ensure first-person camera selection still points to active camera component name:
   - `FirstPersonCameraComponentName = FirstPersonCamera1`
4. Keep existing input mappings:
   - `OnVehicleInteractPressed` toggles equip/drop.
   - LMB / controller place input drives push while equipped.

## Broom asset setup (`BP_Broom`)

Required components on the broom actor:

1. Physics primitive (`StaticMeshComponent` usually root)
   - Simulate Physics: `true` while in world
   - Collision: blocks world as needed for chaos interactions
2. `GripPoint` (`SceneComponent`) near handle pivot in player's hand
3. `HeadPoint` (`SceneComponent`) at broom head contact area
4. `BroomSweepBehaviorComponent`
5. `ToolWorldComponent`
6. Optional `DirtBrushComponent` for decal cleaning

`ToolWorldComponent` required references:

- `ToolDefinition` -> assign a `BroomToolDefinition` asset
- `PhysicsPrimitiveReference` -> broom physics mesh
- `GripPointReference` -> `GripPoint`
- `HeadPointReference` -> `HeadPoint`
- `BehaviorComponentReference` -> `BroomSweepBehaviorComponent`
- `DirtBrushReference` -> optional `DirtBrushComponent`

Notes:

- Component references use `UseComponentPicker`; assign from the same actor.
- If setup is invalid, logs now print: `Tool setup invalid for ...` with exact missing reference.

## Broom definition asset (`DA_BroomToolDefinition`)

Create `BroomToolDefinition` data asset and tune:

- `HandSocketName = HandGrip_R`
- Pose:
  - `RestHeadOffset`
  - `SweepHeadOffset`
  - follow speeds (idle/sweep/catch)
- Ground follow:
  - trace heights/depth
  - follow strength
  - clearance
- Collision and slide:
  - `HeadCollisionRadius`
  - `CollisionSlideFraction`
- Push behavior:
  - `PushImpulse`
  - `PushApplyIntervalSeconds`
  - `MinPushTransferFraction`
  - `StrengthToPushExponent`
  - `CatchMassOverStrengthRatio`

## Gameplay behavior now

- Equip/drop: interact button (`OnVehicleInteractPressed`) near a valid tool.
- Sweep push: hold primary use (LMB/controller place input).
- Release primary use: returns toward rest pose automatically.
- No lift mode: left-trigger lift path has been removed from tool logic.
- While dropped, broom is a normal physics actor in world.

## Strength integration

Push transfer scales by:

- character `GetHeldToolStrengthKg()`
- hit component mass
- broom definition tuning

Heavy/static catches trigger temporary soft-follow mode to preserve chaotic feel.

## Debugging checklist

If broom does not equip:

1. Check output log for `Tool setup invalid for` message.
2. Confirm all `ToolWorldComponent` references are assigned.
3. Confirm `ToolDefinition` is assigned and `MaxEquipDistance` is not tiny.
4. Confirm character mesh has `HandGrip_R` socket (or update definition socket name).
5. Confirm broom actor is within interaction distance.

If broom equips but does not sweep decals:

1. Assign `DirtBrushReference`.
2. Enable `bAllowDirtWhileEquipped` in definition.
3. Verify broom head reaches ground (ground follow trace tuning).

## Future modular tools

To add another tool type:

1. Create `UToolDefinition` subclass for tuning.
2. Create `UToolBehaviorComponent` subclass for equipped behavior.
3. Build actor BP with `ToolWorldComponent` + required points/references.
4. Reuse `ToolSystemComponent` unchanged.

## Asset generation script

Run this to regenerate the default tool assets:

`D:\UnrealEngine\UE_5.7\Engine\Binaries\Win64\UnrealEditor-Cmd.exe D:\UnrealEngine\Agent\Agent.uproject -ExecutePythonScript="D:/UnrealEngine/Agent/Tools/create_tool_data_assets.py" -unattended -nop4 -nosplash -NoSound`

Generated assets:

- `/Game/Tools/BP_BroomToolActor`
- `/Game/Tools/Data/DA_BroomToolDefinition_Default`
- `/Game/Tools/Data/DA_BroomToolDefinition_Heavy`
