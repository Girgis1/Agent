import json
import os
import unreal


MACHINE_BLUEPRINTS = [
    "/Game/Factory/BP_Smelter",
    "/Game/Factory/BP_Shredder",
    "/Game/Factory/BP_Atomiser_Input",
    "/Game/Factory/BP_Atomiser_Output",
    "/Game/Factory/BP_Compactor",
]

OUTPUT_PATH = "Saved/FactoryPipelineAudit.json"


def _safe_text(value):
    try:
        return str(value)
    except Exception:
        return "<unprintable>"


def _safe_get(obj, prop: str):
    try:
        return obj.get_editor_property(prop)
    except Exception:
        return None


def _asset_class_name(asset_data) -> str:
    try:
        return _safe_text(asset_data.asset_class_path.asset_name)
    except Exception:
        return _safe_text(asset_data.asset_class)


def _load_blueprint_cdo(bp_path: str):
    bp_asset = unreal.EditorAssetLibrary.load_asset(bp_path)
    if not bp_asset:
        return None, None, "missing blueprint asset"

    generated_class = _safe_get(bp_asset, "generated_class")
    if not generated_class:
        short_name = bp_path.split("/")[-1]
        generated_class = unreal.load_object(None, f"{bp_path}.{short_name}_C")
    if not generated_class:
        return bp_asset, None, "missing generated class"

    cdo = unreal.get_default_object(generated_class)
    if not cdo:
        return bp_asset, None, "missing CDO"

    return bp_asset, cdo, None


def _dump_machine_blueprint(bp_path: str):
    bp_asset, cdo, error = _load_blueprint_cdo(bp_path)
    row = {"path": bp_path, "error": error}
    if bp_asset:
        row["asset_class"] = bp_asset.get_class().get_name()
        row["asset_path"] = bp_asset.get_path_name()
    if error:
        return row

    components = cdo.get_components_by_class(unreal.ActorComponent)
    row["components"] = []
    for comp in components:
        row["components"].append({"name": comp.get_name(), "class": comp.get_class().get_name()})

    return row


def _dump_recipe_asset(recipe):
    row = {
        "path": recipe.get_path_name(),
        "recipe_id": _safe_text(_safe_get(recipe, "recipe_id")),
        "display_name": _safe_text(_safe_get(recipe, "display_name")),
        "craft_time_seconds": _safe_get(recipe, "craft_time_seconds"),
        "inputs": [],
        "outputs": [],
    }

    for entry in _safe_get(recipe, "inputs") or []:
        row["inputs"].append(
            {
                "input_type": _safe_text(_safe_get(entry, "input_type")),
                "material": _safe_text(_safe_get(entry, "material")),
                "item_actor_class": _safe_text(_safe_get(entry, "item_actor_class")),
                "quantity_units": _safe_get(entry, "quantity_units"),
                "use_whitelist": _safe_get(entry, "b_use_whitelist"),
                "whitelist_material_count": len(_safe_get(entry, "whitelist_materials") or []),
                "use_blacklist": _safe_get(entry, "b_use_blacklist"),
                "blacklist_material_count": len(_safe_get(entry, "blacklist_materials") or []),
            }
        )

    for entry in _safe_get(recipe, "outputs") or []:
        row["outputs"].append(
            {
                "output_actor_class": _safe_text(_safe_get(entry, "output_actor_class")),
            }
        )

    return row


def _collect_recipe_assets():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    assets = registry.get_assets_by_path("/Game/Factory/Recipes", recursive=True)
    rows = []
    for asset_data in assets:
        cls_name = _asset_class_name(asset_data)
        if cls_name != "RecipeAsset":
            continue

        recipe = asset_data.get_asset()
        if not recipe:
            continue

        rows.append(_dump_recipe_asset(recipe))

    rows.sort(key=lambda x: x["path"])
    return rows


def _collect_material_assets():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    assets = registry.get_assets_by_path("/Game/Factory/Materials", recursive=True)
    rows = []
    for asset_data in assets:
        cls_name = _asset_class_name(asset_data)
        if cls_name not in ("MaterialDefinitionAsset", "ResourceDefinitionAsset"):
            continue

        material = asset_data.get_asset()
        if not material:
            continue

        rows.append(
            {
                "path": material.get_path_name(),
                "material_id": _safe_text(_safe_get(material, "material_id")),
                "display_name": _safe_text(_safe_get(material, "display_name")),
                "mass_per_unit_kg": _safe_get(material, "mass_per_unit_kg"),
                "color_id": _safe_text(_safe_get(material, "color_id")),
                "visual_color": _safe_text(_safe_get(material, "visual_color")),
                "visual_material": _safe_text(_safe_get(material, "visual_material")),
                "raw_output_actor_class": _safe_text(_safe_get(material, "raw_output_actor_class")),
                "pure_output_actor_class": _safe_text(_safe_get(material, "pure_output_actor_class")),
            }
        )

    rows.sort(key=lambda x: x["path"])
    return rows


def _build_validation_report(material_rows):
    duplicate_color_ids = {}
    missing_material_id = []
    missing_raw_output = []
    missing_pure_output = []

    for row in material_rows:
        material_id = (row.get("material_id") or "").strip()
        if material_id in ("", "None", "NAME_None"):
            missing_material_id.append(row["path"])

        color_id = (row.get("color_id") or "").strip()
        if color_id and color_id not in ("None", "NAME_None"):
            duplicate_color_ids.setdefault(color_id, []).append(row["path"])

        if row.get("raw_output_actor_class", "").strip() in ("", "None"):
            missing_raw_output.append(row["path"])

        if row.get("pure_output_actor_class", "").strip() in ("", "None"):
            missing_pure_output.append(row["path"])

    duplicate_color_ids = {k: v for k, v in duplicate_color_ids.items() if len(v) > 1}
    return {
        "duplicate_color_ids": duplicate_color_ids,
        "missing_material_id": sorted(missing_material_id),
        "missing_raw_output_actor_class": sorted(missing_raw_output),
        "missing_pure_output_actor_class": sorted(missing_pure_output),
    }


def main():
    material_rows = _collect_material_assets()
    report = {
        "machines": [_dump_machine_blueprint(path) for path in MACHINE_BLUEPRINTS],
        "recipes": _collect_recipe_assets(),
        "materials": material_rows,
        "validation": _build_validation_report(material_rows),
    }

    project_dir = unreal.Paths.project_dir()
    output_path = os.path.join(project_dir, OUTPUT_PATH)
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2)

    unreal.log_warning(f"[FactoryAudit] Wrote {output_path}")


if __name__ == "__main__":
    main()
