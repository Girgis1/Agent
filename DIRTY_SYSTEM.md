# Dirty System

The dirt gameplay path is now decal-first.

## Runtime Types

- `ADirtDecalActor`
  - Place this in the level anywhere you want grime to exist.
  - Owns the runtime dirt mask, current dirtyness value, spotless threshold, and deferred decal material instance.
  - Supports optional auto-destroy after the patch becomes spotless.

- `UDirtDecalSubsystem`
  - World registry for active dirt decals.
  - Lets beams, bots, and machines paint decals by world hit point without tracing the decals directly.

- `UDirtBrushComponent`
  - Reusable applicator for anything that should clean or dirty dirt decals.
  - Supports `Hit`, `Trail`, and `Area`.
  - Supports `Clean` and `Dirty`.

- `FDirtBrushStamp`
  - Shared runtime payload used by the beam, bots, and machine brushes.

## Existing Integrations

- `UAgentBeamToolComponent`
  - Any beam hit now paints dirt decals first.
  - Existing health and damage behavior is unchanged.
  - Mesh-based `UDirtySurfaceComponent` support still exists as a fallback path.

- `AMiningBotActor`
  - Mining bots already include a passive `Trail + Clean` brush.
  - Once a dirt decal is placed on the floor, the bot will clean through it automatically.

## Decal Material Contract

You still need a deferred decal material or material instance in the editor.
The C++ code creates the runtime mask and pushes parameters into that material, but it does not generate the material asset for you.

Recommended texture parameters:

- `Dirty_MaskTexture`
- `Dirty_PatternTexture`

Recommended scalar parameters:

- `Dirty_Dirtyness`
- `Dirty_Intensity`
- `Dirty_IsSpotless`
- `Dirty_CleanRoughness`
- `Dirty_DirtyRoughness`
- `Dirty_CleanSpecular`
- `Dirty_DirtySpecular`
- `Dirty_CleanMetallic`
- `Dirty_DirtyMetallic`

Recommended vector parameters:

- `Dirty_Tint`

Recommended decal material logic:

```text
Pattern = Dirt texture sample
Mask = Runtime dirt mask sample
DirtAlpha = saturate(Pattern * Dirty_Intensity * Mask)

BaseColor = lerp(CleanBaseColor, DirtyBaseColor, DirtAlpha)
Roughness = lerp(Dirty_CleanRoughness, Dirty_DirtyRoughness, DirtAlpha)
Specular = lerp(Dirty_CleanSpecular, Dirty_DirtySpecular, DirtAlpha)
Metallic = lerp(Dirty_CleanMetallic, Dirty_DirtyMetallic, DirtAlpha)
```

Use a grayscale dirt pattern texture with soft gradients. White should mean heavy grime influence, black should mean little or no grime.

## Authoring Workflow

## Simple Laser Test

If you only want to prove that the laser can erase a dirt decal:

1. Place `/Game/DirtyTest/BP_DirtDecal_LaserTest`.
2. Make sure the receiving mesh has `Receives Decals` enabled.
3. Hit Play and hold the beam on the decal.
4. Watch the Output Log for:
   - `initialized smooth mask mode`
   - `applied clean brush at UV=...`

That Blueprint uses `/Game/DirtyTest/M_DirtDecal_MaskTest`, which samples the runtime `Dirty_MaskTexture` and fades the decal out where the beam paints the mask down.

### Dirt Decal

1. Create a deferred decal material or material instance with the parameter names above.
2. Place `ADirtDecalActor` in the level, or make a Blueprint child from it.
3. Assign your decal material to `BaseDecalMaterial`.
4. Set:
   - `InitialDirtyness`
   - `DirtIntensity`
   - `DirtPatternTexture`
   - `SpotlessThreshold`
   - `bAutoDestroyWhenSpotless`
   - `DestroyDelayAfterSpotlessSeconds`
   - tint and roughness/specular/metallic values
5. Size and rotate the `DirtDecalComponent` so it covers the wall, floor, or machine grime patch you want.

### Clean Or Dirty Brush

1. Add `UDirtBrushComponent` to the actor or Blueprint that should affect dirt.
2. Set:
   - `BrushMode`
   - `ApplicationType`
   - `BrushTexture`
   - `BrushSizeCm`
   - `BrushStrengthPerSecond`
   - `BrushHardness`
3. Use:
   - `Hit` for direct hits
   - `Trail` for movers like mining bots or roombas
   - `Area` for dirty machines like grinders or exhaust sources

## Level Setup Notes

- Receiver meshes need `Receives Decals` enabled.
- Dirt decals are independent from the underlying mesh material.
- `Support UV From Hit Results` is still enabled in project settings for the older mesh-surface path, but the decal path does not depend on mesh UVs.
- `UDirtySurfaceComponent` is still in the project if you want a mesh-material dirt path later, but decals are now the recommended world dirt workflow.
