import unreal


MATERIAL_ROOT = "/Game/Factory/Materials"


def _safe_get(obj, prop: str):
    try:
        return obj.get_editor_property(prop)
    except Exception:
        return None


def _safe_set(obj, prop: str, value) -> bool:
    try:
        obj.set_editor_property(prop, value)
        return True
    except Exception:
        return False


def _asset_class_name(asset_data) -> str:
    try:
        return str(asset_data.asset_class_path.asset_name)
    except Exception:
        return str(asset_data.asset_class)


def _is_name_empty(name_value) -> bool:
    as_text = str(name_value) if name_value is not None else ""
    return as_text in ("", "None", "NAME_None")


def main():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    assets = registry.get_assets_by_path(MATERIAL_ROOT, recursive=True)

    migrated_count = 0
    skipped_count = 0

    for asset_data in assets:
        if _asset_class_name(asset_data) not in ("MaterialDefinitionAsset", "ResourceDefinitionAsset"):
            continue

        material = asset_data.get_asset()
        if not material:
            continue

        did_change = False

        material_id = _safe_get(material, "material_id")
        if _is_name_empty(material_id):
            fallback_material_id = material.get_name().replace("DA_Resource_", "").replace("DA_Material_", "")
            if fallback_material_id and _safe_set(material, "material_id", unreal.Name(fallback_material_id)):
                did_change = True
                material_id = fallback_material_id

        raw_output = _safe_get(material, "raw_output_actor_class")
        pure_output = _safe_get(material, "pure_output_actor_class")

        if raw_output is None and pure_output is None:
            skipped_count += 1
            continue

        if not pure_output and raw_output:
            if _safe_set(material, "pure_output_actor_class", raw_output):
                did_change = True

        color_id = _safe_get(material, "color_id")
        if _is_name_empty(color_id):
            fallback_id = material_id if not _is_name_empty(material_id) else material.get_name()
            if not _is_name_empty(fallback_id):
                if _safe_set(material, "color_id", unreal.Name(str(fallback_id))):
                    did_change = True

        visual_color = _safe_get(material, "visual_color")
        debug_color = _safe_get(material, "debug_color")
        if visual_color is None and debug_color is not None:
            if _safe_set(material, "visual_color", debug_color):
                did_change = True

        if did_change:
            unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False)
            migrated_count += 1
            unreal.log(f"[MaterialMigration] Updated {material.get_path_name()}")
        else:
            skipped_count += 1

    unreal.log_warning(
        f"[MaterialMigration] Done. Updated={migrated_count}, Skipped={skipped_count}, Root={MATERIAL_ROOT}"
    )


if __name__ == "__main__":
    main()
