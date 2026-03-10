import unreal


def _log(msg: str) -> None:
    unreal.log(msg)


def _warn(msg: str) -> None:
    unreal.log_warning(msg)


def _err(msg: str) -> None:
    unreal.log_error(msg)


def _prop(obj, names):
    for name in names:
        try:
            return obj.get_editor_property(name)
        except Exception:
            pass
    return None


def _obj_path(obj) -> str:
    if not obj:
        return "None"
    try:
        return obj.get_path_name()
    except Exception:
        return str(obj)


def _resolved_material_id(material):
    if not material:
        return None
    for fn in ("get_resolved_material_id", "get_resolved_resource_id"):
        try:
            resolver = getattr(material, fn)
            return resolver()
        except Exception:
            pass
    return None


def _dump_recipe(recipe):
    if not recipe:
        _warn("  Recipe: None")
        return

    _log(f"  Recipe: {_obj_path(recipe)} ({recipe.get_class().get_name()})")
    recipe_id = _prop(recipe, ["recipe_id", "recipeId", "RecipeId"])
    allowed_tag = _prop(recipe, ["allowed_machine_tag", "allowedMachineTag", "AllowedMachineTag"])
    craft_time = _prop(recipe, ["craft_time_seconds", "craftTimeSeconds", "CraftTimeSeconds"])
    inputs = _prop(recipe, ["inputs", "Inputs"]) or []
    outputs = _prop(recipe, ["outputs", "Outputs"]) or []

    _log(f"    RecipeId={recipe_id} AllowedMachineTag={allowed_tag} CraftTimeSeconds={craft_time}")

    def dump_entries(label, entries):
        _log(f"    {label}: {len(entries)} entries")
        for idx, entry in enumerate(entries):
            resource = _prop(entry, ["resource", "Resource"])
            qty_units = _prop(entry, ["quantity_units", "quantityUnits", "QuantityUnits"])
            rid = None
            if resource:
                rid = _resolved_material_id(resource)
                if rid is None:
                    rid = "<no get_resolved_material_id>"
            _log(f"      [{idx}] Resource={_obj_path(resource)} MaterialId={rid} QuantityUnits={qty_units}")

    dump_entries("Inputs", inputs)
    dump_entries("Outputs", outputs)


def _dump_machine_blueprint(bp_path: str):
    asset = unreal.EditorAssetLibrary.load_asset(bp_path)
    if not asset:
        _err(f"Missing blueprint: {bp_path}")
        _dump_factory_assets()
        return

    _log(f"Blueprint: {bp_path}")
    _log(f"  AssetClass={asset.get_class().get_name()}")

    gen_class = _prop(asset, ["generated_class", "GeneratedClass"])
    if not gen_class:
        # Commandlet mode can fail to expose generated_class via editor property.
        gen_class = unreal.load_object(None, f"{bp_path}.{bp_path.split('/')[-1]}_C")

    parent_class = _prop(asset, ["parent_class", "ParentClass"])
    _log(f"  ParentClass={_obj_path(parent_class)}")
    _log(f"  GeneratedClass={_obj_path(gen_class)}")
    if not gen_class:
        _err("  Missing generated class")
        _dump_factory_assets()
        return

    cdo = unreal.get_default_object(gen_class)
    if not cdo:
        _err("  Missing CDO")
        return

    components = cdo.get_components_by_class(unreal.ActorComponent)
    _log(f"  Components={len(components)}")

    machine_component = None
    input_volume = None
    output_volume = None

    for comp in components:
        cname = comp.get_class().get_name()
        _log(f"    - {comp.get_name()} ({cname})")
        if cname == "MachineComponent":
            machine_component = comp
        elif cname == "InputVolume":
            input_volume = comp
        elif cname == "OutputVolume":
            output_volume = comp

    if machine_component:
        _log("  MachineComponent settings:")
        b_enabled = _prop(machine_component, ["b_enabled", "bEnabled"])
        speed = _prop(machine_component, ["speed", "Speed"])
        b_recipe_any = _prop(machine_component, ["b_recipe_any", "bRecipeAny"])
        active_index = _prop(machine_component, ["active_recipe_index", "activeRecipeIndex"])
        machine_tag = _prop(machine_component, ["machine_tag", "machineTag"])
        capacity_units = _prop(machine_component, ["capacity_units", "capacityUnits"])
        b_whitelist = _prop(machine_component, ["b_use_whitelist", "bUseWhitelist"])
        whitelist = _prop(machine_component, ["whitelist_resources", "whitelistResources"]) or []
        b_blacklist = _prop(machine_component, ["b_use_blacklist", "bUseBlacklist"])
        blacklist = _prop(machine_component, ["blacklist_resources", "blacklistResources"]) or []
        recipes = _prop(machine_component, ["recipes", "Recipes"]) or []

        _log(f"    Enabled={b_enabled} Speed={speed} RecipeAny={b_recipe_any} ActiveRecipeIndex={active_index}")
        _log(f"    MachineTag={machine_tag} CapacityUnits={capacity_units}")
        _log(f"    UseWhitelist={b_whitelist} WhitelistCount={len(whitelist)}")
        for i, item in enumerate(whitelist):
            rid = _resolved_material_id(item)
            _log(f"      whitelist[{i}] {_obj_path(item)} MaterialId={rid}")
        _log(f"    UseBlacklist={b_blacklist} BlacklistCount={len(blacklist)}")
        for i, item in enumerate(blacklist):
            rid = _resolved_material_id(item)
            _log(f"      blacklist[{i}] {_obj_path(item)} MaterialId={rid}")
        _log(f"    RecipesCount={len(recipes)}")
        for i, recipe in enumerate(recipes):
            _log(f"      recipes[{i}] {_obj_path(recipe)}")
            _dump_recipe(recipe)
    else:
        _warn("  No MachineComponent found on CDO")

    if input_volume:
        _log("  InputVolume settings:")
        b_enabled = _prop(input_volume, ["b_enabled", "bEnabled"])
        interval = _prop(input_volume, ["intake_interval", "intakeInterval"])
        destroy_payloads = _prop(input_volume, ["b_destroy_accepted_payloads", "bDestroyAcceptedPayloads"])
        _log(f"    Enabled={b_enabled} IntakeInterval={interval} DestroyAcceptedPayloads={destroy_payloads}")
    else:
        _warn("  No InputVolume found on CDO")

    if output_volume:
        _log("  OutputVolume settings:")
        b_enabled = _prop(output_volume, ["b_enabled", "bEnabled"])
        b_auto = _prop(output_volume, ["b_auto_output", "bAutoOutput"])
        interval = _prop(output_volume, ["spawn_interval", "spawnInterval"])
        chunk_units = _prop(output_volume, ["output_chunk_units", "outputChunkUnits"])
        payload_class = _prop(output_volume, ["payload_actor_class", "payloadActorClass"])
        _log(
            f"    Enabled={b_enabled} AutoOutput={b_auto} SpawnInterval={interval} "
            f"OutputChunkUnits={chunk_units} PayloadClass={_obj_path(payload_class)}"
        )
    else:
        _warn("  No OutputVolume found on CDO")


def main():
    _dump_machine_blueprint("/Game/Factory/BP_Smelter")
    recipe_asset = unreal.EditorAssetLibrary.load_asset("/Game/Factory/Recipes/DA_Recipe_IronOreToIronBar")
    if recipe_asset:
        _log("Standalone recipe dump:")
        _dump_recipe(recipe_asset)
    else:
        _warn("Standalone recipe asset missing: /Game/Factory/Recipes/DA_Recipe_IronOreToIronBar")


def _dump_factory_assets():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    assets = registry.get_assets_by_path("/Game/Factory", recursive=True)
    _log("Factory assets discovered:")
    for asset_data in assets:
        _log(f"  {asset_data.package_name} ({asset_data.asset_class_path.asset_name})")


if __name__ == "__main__":
    main()
