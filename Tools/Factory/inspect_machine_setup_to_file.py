import json
import os
import unreal


def prop(obj, names, default=None):
    for name in names:
        try:
            return obj.get_editor_property(name)
        except Exception:
            pass
    return default


def obj_path(obj):
    if not obj:
        return ""
    try:
        return obj.get_path_name()
    except Exception:
        return str(obj)


def to_name(value):
    if value is None:
        return ""
    return str(value)


def resolved_material_id(material):
    if not material:
        return ""
    for fn in ("get_resolved_material_id", "get_resolved_resource_id"):
        try:
            resolver = getattr(material, fn)
            return to_name(resolver())
        except Exception:
            pass
    return ""


def dump_recipe(recipe):
    if not recipe:
        return None

    result = {
        "path": obj_path(recipe),
        "class": recipe.get_class().get_name(),
        "recipe_id": to_name(prop(recipe, ["recipe_id", "RecipeId"])),
        "allowed_machine_tag": to_name(prop(recipe, ["allowed_machine_tag", "AllowedMachineTag"])),
        "craft_time_seconds": prop(recipe, ["craft_time_seconds", "CraftTimeSeconds"], 0.0),
        "inputs": [],
        "outputs": [],
    }

    for entry in prop(recipe, ["inputs", "Inputs"], []) or []:
        resource = prop(entry, ["resource", "Resource"])
        material_id = resolved_material_id(resource)
        result["inputs"].append(
            {
                "resource": obj_path(resource),
                "material_id": material_id,
                "quantity_units": prop(entry, ["quantity_units", "QuantityUnits"], 0),
            }
        )

    for entry in prop(recipe, ["outputs", "Outputs"], []) or []:
        resource = prop(entry, ["resource", "Resource"])
        material_id = resolved_material_id(resource)
        result["outputs"].append(
            {
                "resource": obj_path(resource),
                "material_id": material_id,
                "quantity_units": prop(entry, ["quantity_units", "QuantityUnits"], 0),
            }
        )

    return result


def dump_machine_blueprint(bp_path):
    result = {"bp_path": bp_path}
    asset = unreal.EditorAssetLibrary.load_asset(bp_path)
    if not asset:
        result["error"] = "missing blueprint asset"
        return result

    result["asset_class"] = asset.get_class().get_name()
    result["asset_path"] = obj_path(asset)
    result["parent_class"] = obj_path(prop(asset, ["parent_class", "ParentClass"]))

    gen_class = prop(asset, ["generated_class", "GeneratedClass"])
    if not gen_class:
        short_name = bp_path.split("/")[-1]
        gen_class = unreal.load_object(None, f"{bp_path}.{short_name}_C")
    result["generated_class"] = obj_path(gen_class)
    if not gen_class:
        result["error"] = "missing generated class"
        return result

    cdo = unreal.get_default_object(gen_class)
    if not cdo:
        result["error"] = "missing cdo"
        return result

    components = cdo.get_components_by_class(unreal.ActorComponent)
    result["components"] = []
    for comp in components:
        result["components"].append({"name": comp.get_name(), "class": comp.get_class().get_name()})

    machine_component = None
    input_volume = None
    output_volume = None
    for comp in components:
        cname = comp.get_class().get_name()
        if cname == "MachineComponent":
            machine_component = comp
        elif cname == "InputVolume":
            input_volume = comp
        elif cname == "OutputVolume":
            output_volume = comp

    if machine_component:
        recipes = prop(machine_component, ["recipes", "Recipes"], []) or []
        result["machine_component"] = {
            "enabled": prop(machine_component, ["b_enabled", "bEnabled"]),
            "speed": prop(machine_component, ["speed", "Speed"]),
            "recipe_any": prop(machine_component, ["b_recipe_any", "bRecipeAny"]),
            "active_recipe_index": prop(machine_component, ["active_recipe_index", "ActiveRecipeIndex"]),
            "machine_tag": to_name(prop(machine_component, ["machine_tag", "MachineTag"])),
            "capacity_units": prop(machine_component, ["capacity_units", "CapacityUnits"]),
            "use_whitelist": prop(machine_component, ["b_use_whitelist", "bUseWhitelist"]),
            "use_blacklist": prop(machine_component, ["b_use_blacklist", "bUseBlacklist"]),
            "whitelist_resources": [],
            "blacklist_resources": [],
            "recipes": [dump_recipe(recipe) for recipe in recipes],
        }

        whitelist = prop(machine_component, ["whitelist_resources", "WhitelistResources"], []) or []
        for item in whitelist:
            result["machine_component"]["whitelist_resources"].append(
                {"path": obj_path(item), "material_id": resolved_material_id(item)}
            )

        blacklist = prop(machine_component, ["blacklist_resources", "BlacklistResources"], []) or []
        for item in blacklist:
            result["machine_component"]["blacklist_resources"].append(
                {"path": obj_path(item), "material_id": resolved_material_id(item)}
            )
    else:
        result["machine_component"] = None

    if input_volume:
        result["input_volume"] = {
            "enabled": prop(input_volume, ["b_enabled", "bEnabled"]),
            "intake_interval": prop(input_volume, ["intake_interval", "IntakeInterval"]),
            "destroy_accepted_payloads": prop(input_volume, ["b_destroy_accepted_payloads", "bDestroyAcceptedPayloads"]),
        }
    else:
        result["input_volume"] = None

    if output_volume:
        result["output_volume"] = {
            "enabled": prop(output_volume, ["b_enabled", "bEnabled"]),
            "auto_output": prop(output_volume, ["b_auto_output", "bAutoOutput"]),
            "spawn_interval": prop(output_volume, ["spawn_interval", "SpawnInterval"]),
            "output_chunk_units": prop(output_volume, ["output_chunk_units", "OutputChunkUnits"]),
            "payload_actor_class": obj_path(prop(output_volume, ["payload_actor_class", "PayloadActorClass"])),
            "output_arrow_name": to_name(prop(output_volume, ["output_arrow_component_name", "OutputArrowComponentName"])),
        }
    else:
        result["output_volume"] = None

    return result


def dump_resource_actor(bp_path):
    result = {"bp_path": bp_path}
    asset = unreal.EditorAssetLibrary.load_asset(bp_path)
    if not asset:
        result["error"] = "missing blueprint asset"
        return result

    gen_class = prop(asset, ["generated_class", "GeneratedClass"])
    if not gen_class:
        short_name = bp_path.split("/")[-1]
        gen_class = unreal.load_object(None, f"{bp_path}.{short_name}_C")
    if not gen_class:
        result["error"] = "missing generated class"
        return result

    cdo = unreal.get_default_object(gen_class)
    if not cdo:
        result["error"] = "missing cdo"
        return result

    components = cdo.get_components_by_class(unreal.ActorComponent)
    resource_component = None
    for comp in components:
        if comp.get_class().get_name() == "ResourceComponent":
            resource_component = comp
            break

    if not resource_component:
        result["error"] = "missing resource component"
        return result

    materials = prop(resource_component, ["materials", "Materials"], []) or []
    rows = []
    for entry in materials:
        res = prop(entry, ["resource", "Resource"])
        material_id = resolved_material_id(res)
        rows.append(
            {
                "resource_path": obj_path(res),
                "material_id": material_id,
                "use_range": prop(entry, ["b_use_range", "bUseRange"]),
                "units": prop(entry, ["units", "Units"]),
                "min_units": prop(entry, ["min_units", "MinUnits"]),
                "max_units": prop(entry, ["max_units", "MaxUnits"]),
            }
        )

    result["resource_component"] = {
        "materials_count": len(rows),
        "materials": rows,
        "randomized": prop(resource_component, ["b_use_randomized_contents", "bUseRandomizedContents"]),
    }
    return result


def list_assets_under(path):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    assets = registry.get_assets_by_path(path, recursive=True)
    result = []
    for data in assets:
        result.append({"package": str(data.package_name), "class": str(data.asset_class_path.asset_name)})
    return result


def main():
    report = {
        "smelter": dump_machine_blueprint("/Game/Factory/BP_Smelter"),
        "recipe": dump_recipe(unreal.EditorAssetLibrary.load_asset("/Game/Factory/Recipes/DA_Recipe_IronOreToIronBar")),
        "iron_ore_resource": dump_resource_actor("/Game/Factory/BP_IronOre"),
        "factory_assets": list_assets_under("/Game/Factory"),
    }

    project_dir = unreal.Paths.project_dir()
    output_path = os.path.join(project_dir, "Saved", "MachineInspectReport.json")
    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(report, f, indent=2)

    unreal.log_warning(f"Wrote machine inspection report: {output_path}")


if __name__ == "__main__":
    main()
