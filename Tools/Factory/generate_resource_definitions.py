"""
Create or update DA_Resource_* material definition assets from JSON presets.

Run in Unreal Editor Python:
  exec(open(r"D:/UnrealEngine/Agent/Tools/Factory/generate_resource_definitions.py").read())
"""

import json
import os
import unreal


PACKAGE_PATH = "/Game/Factory/Materials"
PRESET_FILENAME = "resource_definitions_v1.json"


def _resolve_material_definition_class():
    cls = unreal.load_class(None, "/Script/Agent.MaterialDefinitionAsset")
    if cls:
        return cls

    # Compatibility fallback for older redirected name.
    cls = unreal.load_class(None, "/Script/Agent.ResourceDefinitionAsset")
    if cls:
        return cls

    raise RuntimeError("Could not resolve MaterialDefinitionAsset class")


def _load_presets():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    preset_path = os.path.join(script_dir, PRESET_FILENAME)
    with open(preset_path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def _load_or_create_material_asset(asset_name: str, material_class):
    asset_path = f"{PACKAGE_PATH}/{asset_name}.{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        return unreal.EditorAssetLibrary.load_asset(asset_path), False

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", material_class)
    created_asset = asset_tools.create_asset(asset_name, PACKAGE_PATH, material_class, factory)
    return created_asset, True


def _safe_set(asset, prop: str, value):
    try:
        asset.set_editor_property(prop, value)
        return True
    except Exception:
        return False


def _load_actor_class(path: str):
    if not path:
        return None

    actor_class = unreal.EditorAssetLibrary.load_blueprint_class(path)
    if actor_class:
        return actor_class

    try:
        actor_class = unreal.load_class(None, path)
    except Exception:
        actor_class = None
    return actor_class


def _parse_linear_color(raw_value):
    if isinstance(raw_value, dict):
        return unreal.LinearColor(
            float(raw_value.get("r", raw_value.get("R", 1.0))),
            float(raw_value.get("g", raw_value.get("G", 1.0))),
            float(raw_value.get("b", raw_value.get("B", 1.0))),
            float(raw_value.get("a", raw_value.get("A", 1.0))),
        )

    if isinstance(raw_value, (list, tuple)) and len(raw_value) >= 3:
        alpha = float(raw_value[3]) if len(raw_value) > 3 else 1.0
        return unreal.LinearColor(float(raw_value[0]), float(raw_value[1]), float(raw_value[2]), alpha)

    return None


def _apply_material_fields(asset, entry):
    resolved_material_id = entry.get("material_id", entry["asset_name"])
    _safe_set(asset, "material_id", unreal.Name(resolved_material_id))
    _safe_set(asset, "display_name", entry["display_name"])
    _safe_set(asset, "mass_per_unit_kg", float(entry["mass_per_unit_kg"]))

    color_id = entry.get("color_id", resolved_material_id)
    if color_id:
        _safe_set(asset, "color_id", unreal.Name(color_id))

    visual_color = _parse_linear_color(entry.get("visual_color"))
    if visual_color is not None:
        _safe_set(asset, "visual_color", visual_color)
        _safe_set(asset, "debug_color", visual_color)

    raw_output_class = _load_actor_class(entry.get("raw_output_actor_class", ""))
    if raw_output_class:
        _safe_set(asset, "raw_output_actor_class", raw_output_class)

    pure_output_class = _load_actor_class(entry.get("pure_output_actor_class", ""))
    if pure_output_class:
        _safe_set(asset, "pure_output_actor_class", pure_output_class)


def main():
    if not unreal.EditorAssetLibrary.does_directory_exist(PACKAGE_PATH):
        unreal.EditorAssetLibrary.make_directory(PACKAGE_PATH)

    material_class = _resolve_material_definition_class()
    presets = _load_presets()
    created_count = 0
    updated_count = 0

    for entry in presets:
        asset_name = entry["asset_name"]
        asset, was_created = _load_or_create_material_asset(asset_name, material_class)
        if not asset:
            unreal.log_error(f"[MaterialDA] Failed to load/create {asset_name}")
            continue

        _apply_material_fields(asset, entry)

        if was_created:
            created_count += 1
        else:
            updated_count += 1

        unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
        unreal.log(f"[MaterialDA] {'Created' if was_created else 'Updated'} {asset.get_path_name()}")

    unreal.EditorAssetLibrary.save_directory(PACKAGE_PATH, only_if_is_dirty=True, recursive=True)
    unreal.log(f"[MaterialDA] Done. Created={created_count}, Updated={updated_count}, Total={len(presets)}")


if __name__ == "__main__":
    main()
