import unreal

BP_PATH = "/Game/Tools/BP_Broom"
DEF_PATH = "/Game/Tools/Data/DA_BroomToolDefinition_Default"

bp_class = unreal.EditorAssetLibrary.load_blueprint_class(BP_PATH)
if not bp_class:
    raise RuntimeError(f"Failed to load blueprint class: {BP_PATH}")

tool_def = unreal.EditorAssetLibrary.load_asset(DEF_PATH)
if not tool_def:
    raise RuntimeError(f"Failed to load tool definition asset: {DEF_PATH}")

cdo = unreal.get_default_object(bp_class)
if not cdo:
    raise RuntimeError("Failed to resolve CDO")

tool_world_components = cdo.get_components_by_class(unreal.ToolWorldComponent)
if not tool_world_components:
    raise RuntimeError("No ToolWorldComponent found on BP_Broom")

for tool_world in tool_world_components:
    tool_world.set_editor_property("tool_definition", tool_def)

unreal.EditorAssetLibrary.save_asset(BP_PATH, only_if_is_dirty=False)
unreal.log("[ToolAssets] Assigned default tool definition to /Game/Tools/BP_Broom")
