import unreal

MATERIAL_PATH = "/Game/Scene_Junkyard/Materials/MSPresets/M_MS_Decal_Material_VT/M_MS_Decal_Material_VT"

material = unreal.load_asset(MATERIAL_PATH)
if not material:
    raise RuntimeError(f"Could not load {MATERIAL_PATH}")

unreal.log(f"Loaded material: {material.get_name()}")

try:
    expressions = material.get_editor_property("expressions")
    unreal.log(f"Expression count: {len(expressions)}")
    for expr in expressions:
        if not expr:
            continue
        name = expr.get_name()
        cls = expr.get_class().get_name()
        details = []
        for prop in ["parameter_name", "sampler_type", "desc"]:
            try:
                value = expr.get_editor_property(prop)
                details.append(f"{prop}={value}")
            except Exception:
                pass
        unreal.log(f"  {cls} {name} {' '.join(details)}")
except Exception as exc:
    unreal.log_warning(f"Could not inspect expressions: {exc}")
