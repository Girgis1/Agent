# Kitchen Plan Blender Output

This kitchen was modeled from the supplied floor plan as a clean, editable Blender blockout.

Assumptions used:
- Room size follows the plan at `4.56 m x 2.90 m`.
- Rear window opening is centered and uses the labeled `2.02 m` width.
- Standard kitchen defaults were used where the plan did not specify heights or depths:
  `2.70 m` wall height, `0.92 m` counter height, `0.60 m` base depth, `0.65 m` tall-unit depth.
- The left run is modeled shorter than the right run to match the drawing footprint, with a tall fridge-style unit on the left and a tall pantry-style unit on the right.
- Cabinet faces, sink, cooktop, and window are intentionally simple so the scene is easy to restyle or dimension-adjust.

Generated files:
- `KitchenPlan.blend`
- `KitchenPlan.fbx`
- `KitchenPlan.png`
- `KitchenPlan_top.png`

To rebuild the scene:

```powershell
"C:\Program Files\Blender Foundation\Blender 4.5\blender.exe" --background --python "d:\UnrealEngine\Agent\Tools\Blender\create_kitchen_from_plan.py"
```
