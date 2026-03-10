# Mixed Material Shader Contract

This document describes the runtime parameters now pushed by `AMixedMaterialActor`.

Use one master material for mixed payloads and expose these parameters.

Preset actor classes:

- `AMixedMaterialVoronoiActor` (forces `Voronoi`)
- `AMixedMaterialGradientActor` (forces `LinearGradient`)

## Core Parameters

- `MixMode` (Scalar): `0=WeightedColor`, `1=Voronoi`, `2=LinearGradient`
- `MixLayerCount` (Scalar): active layer count (`1..4`)
- `MixSeed` (Scalar): stable composition seed (`0..1`)
- `VoronoiScale` (Scalar)
- `VoronoiEdgeWidth` (Scalar)
- `GradientDirection` (Vector2 in XY, sent as Vector4)
- `GradientSharpness` (Scalar)

## Per-Layer Colors and Weights

- `MixColorA` / `MixWeightA` / `MixColorIdA`
- `MixColorB` / `MixWeightB` / `MixColorIdB`
- `MixColorC` / `MixWeightC` / `MixColorIdC`
- `MixColorD` / `MixWeightD` / `MixColorIdD`

`MixColorId*` is a normalized hash from `ColorId` and is useful for deterministic noise offsets.

## Per-Layer Texture Slots

Base color:

- `MixBaseTexA`, `MixBaseTexB`, `MixBaseTexC`, `MixBaseTexD`

Normal:

- `MixNormalTexA`, `MixNormalTexB`, `MixNormalTexC`, `MixNormalTexD`

Packed (ORM / RMA / MRA):

- `MixPackedTexA`, `MixPackedTexB`, `MixPackedTexC`, `MixPackedTexD`

The actor auto-tries to read these from each source material instance using common parameter names:

- Base color candidates: `BaseColorTexture`, `BaseColorTex`, `AlbedoTexture`, `AlbedoTex`, `Albedo`, `DiffuseTexture`, `DiffuseTex`, `ColorTexture`, `ColorMap`
- Normal candidates: `NormalTexture`, `NormalTex`, `NormalMap`
- Packed candidates: `PackedTexture`, `PackedTex`, `ORM`, `MRA`, `RMA`, `RoughnessMetallicAO`

## Suggested Material Logic

1. Normalize `MixWeightA..D`.
2. Build cumulative weights.
3. For `MixMode=1` (Voronoi): use Voronoi noise + `MixSeed` + `MixColorId*` jitter to choose layer index.
4. For `MixMode=2` (LinearGradient): project UV/world-position on `GradientDirection`, sharpen with `GradientSharpness`, then remap by cumulative weights.
5. Sample `MixBaseTex*`, `MixNormalTex*`, `MixPackedTex*` by selected layer and blend seams with edge softness.
6. Fall back to color-only (`MixColor*`) if a texture slot is missing.
