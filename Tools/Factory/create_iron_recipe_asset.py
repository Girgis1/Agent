import unreal


PACKAGE_PATH = "/Game/Factory/Recipes"
ASSET_NAME = "R_IronOre_to_IronBar"
ASSET_PATH = f"{PACKAGE_PATH}/{ASSET_NAME}"
IRON_MATERIAL_PATH = "/Game/Factory/Materials/DA_Resource_Iron"
IRON_BAR_BP_PATH = "/Game/Factory/Material_BP/BP_IronBar"


def _fail(message: str) -> None:
    unreal.log_error(message)
    raise RuntimeError(message)


def _load_asset(path: str):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        _fail(f"Missing asset: {path}")
    return asset


def _load_blueprint_class(path: str):
    bp_class = unreal.EditorAssetLibrary.load_blueprint_class(path)
    if not bp_class:
        _fail(f"Missing Blueprint class: {path}")
    return bp_class


def _resolve_recipe_class():
    recipe_class = unreal.load_class(None, "/Script/Agent.RecipeAsset")
    if recipe_class:
        return recipe_class

    recipe_class = unreal.load_class(None, "/Script/Agent.FactoryRecipeAsset")
    if recipe_class:
        return recipe_class

    _fail("Could not resolve RecipeAsset class from /Script/Agent")


def _resolve_struct_type(*candidate_names: str):
    for name in candidate_names:
        if hasattr(unreal, name):
            return getattr(unreal, name)
    return None


def _create_asset_if_missing(recipe_class):
    if not unreal.EditorAssetLibrary.does_directory_exist(PACKAGE_PATH):
        unreal.EditorAssetLibrary.make_directory(PACKAGE_PATH)

    if unreal.EditorAssetLibrary.does_asset_exist(ASSET_PATH):
        return unreal.EditorAssetLibrary.load_asset(ASSET_PATH)

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    data_factory = unreal.DataAssetFactory()
    data_factory.set_editor_property("data_asset_class", recipe_class)
    created = asset_tools.create_asset(ASSET_NAME, PACKAGE_PATH, recipe_class, data_factory)
    if not created:
        _fail(f"Failed to create recipe asset: {ASSET_PATH}")
    return created


def _set_specific_material_input_type(input_entry):
    if not hasattr(unreal, "RecipeInputType"):
        return

    try:
        input_entry.set_editor_property("input_type", unreal.RecipeInputType.SPECIFIC_MATERIAL)
    except Exception:
        # Enum name can vary between engine versions; keep default if unavailable.
        pass


def _build_input_entry(material_asset, quantity_units: int):
    struct_type = _resolve_struct_type("RecipeMaterialInputEntry", "FactoryRecipeResourceEntry", "RecipeResourceEntry")
    if not struct_type:
        _fail("Could not resolve input entry struct type")

    entry = struct_type()
    try:
        entry.set_editor_property("material", material_asset)
    except Exception:
        try:
            entry.set_editor_property("resource", material_asset)
        except Exception:
            pass

    try:
        entry.set_editor_property("quantity_units", max(1, int(quantity_units)))
    except Exception:
        try:
            entry.set_editor_property("quantityUnits", max(1, int(quantity_units)))
        except Exception:
            pass

    _set_specific_material_input_type(entry)
    return entry


def _build_output_entry(output_actor_class):
    struct_type = _resolve_struct_type("RecipeBlueprintOutputEntry", "FactoryRecipeResourceEntry", "RecipeResourceEntry")
    if not struct_type:
        _fail("Could not resolve output entry struct type")

    entry = struct_type()
    try:
        entry.set_editor_property("output_actor_class", output_actor_class)
    except Exception:
        # Legacy fallback: output entry still expects a material/resource data asset.
        _fail("Output struct does not expose output_actor_class; project schema is older than expected")

    return entry


def main() -> None:
    recipe_class = _resolve_recipe_class()
    recipe_asset = _create_asset_if_missing(recipe_class)
    if not recipe_asset:
        _fail(f"Failed to load/create {ASSET_PATH}")

    iron_material = _load_asset(IRON_MATERIAL_PATH)
    iron_bar_bp_class = _load_blueprint_class(IRON_BAR_BP_PATH)

    inputs = [_build_input_entry(iron_material, 1)]
    outputs = [_build_output_entry(iron_bar_bp_class)]

    recipe_asset.set_editor_property("recipe_id", "Recipe_IronOreToIronBar")
    recipe_asset.set_editor_property("display_name", unreal.Text("Iron Ore -> Iron Bar"))
    recipe_asset.set_editor_property("craft_time_seconds", 2.0)
    recipe_asset.set_editor_property("inputs", inputs)
    recipe_asset.set_editor_property("outputs", outputs)

    if not unreal.EditorAssetLibrary.save_asset(ASSET_PATH, only_if_is_dirty=False):
        _fail(f"Failed to save recipe asset: {ASSET_PATH}")

    unreal.log(f"[Factory] Created/updated recipe asset: {ASSET_PATH}")


if __name__ == "__main__":
    main()
