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


def _apply_material_fields(asset, entry):
    asset.set_editor_property("resource_id", unreal.Name(entry["resource_id"]))
    asset.set_editor_property("display_name", entry["display_name"])
    asset.set_editor_property("mass_per_unit_kg", float(entry["mass_per_unit_kg"]))


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
