import unreal


IRON_MATERIAL_PATH = "/Game/Factory/Materials/DA_Resource_Iron"
IRON_BAR_BP_PATH = "/Game/Factory/Material_BP/BP_IronBar"
TARGET_RECIPES = [
    "/Game/Factory/Recipes/R_IronOre_to_IronBar",
    "/Game/Factory/Recipes/R_IronBar_to_IronBar",
]


def _fail(message: str) -> None:
    unreal.log_error(message)
    raise RuntimeError(message)


def _load_asset(path: str):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        _fail(f"Missing asset: {path}")
    return asset


def _resolve_struct_type(*candidate_names):
    for name in candidate_names:
        if hasattr(unreal, name):
            return getattr(unreal, name)
    return None


def _set_any_material_input_type(input_entry) -> None:
    if not hasattr(unreal, "RecipeInputType"):
        return

    try:
        input_entry.set_editor_property("input_type", unreal.RecipeInputType.ANY_MATERIAL)
    except Exception:
        # Keep default if enum reflection differs in this engine build.
        pass


def _build_any_material_input(iron_material):
    input_struct_type = _resolve_struct_type("RecipeMaterialInputEntry")
    if not input_struct_type:
        _fail("Could not resolve RecipeMaterialInputEntry struct")

    entry = input_struct_type()
    entry.set_editor_property("quantity_units", 1)
    _set_any_material_input_type(entry)

    # Any Material mode with iron whitelist.
    try:
        entry.set_editor_property("b_use_whitelist", True)
        entry.set_editor_property("whitelist_materials", [iron_material])
    except Exception:
        pass

    try:
        entry.set_editor_property("b_use_blacklist", False)
        entry.set_editor_property("blacklist_materials", [])
    except Exception:
        pass

    return entry


def _build_output_entry(output_actor_class):
    output_struct_type = _resolve_struct_type("RecipeBlueprintOutputEntry")
    if not output_struct_type:
        _fail("Could not resolve RecipeBlueprintOutputEntry struct")

    entry = output_struct_type()
    entry.set_editor_property("output_actor_class", output_actor_class)
    return entry


def _migrate_recipe(recipe_path: str, iron_material, iron_bar_bp_class):
    recipe = _load_asset(recipe_path)

    recipe.set_editor_property("inputs", [_build_any_material_input(iron_material)])
    recipe.set_editor_property("outputs", [_build_output_entry(iron_bar_bp_class)])

    unreal.EditorAssetLibrary.save_loaded_asset(recipe, only_if_is_dirty=False)
    unreal.log(f"[FactoryMigration] Updated {recipe.get_path_name()}")


def main():
    iron_material = _load_asset(IRON_MATERIAL_PATH)
    iron_bar_bp_class = unreal.EditorAssetLibrary.load_blueprint_class(IRON_BAR_BP_PATH)
    if not iron_bar_bp_class:
        _fail(f"Missing Blueprint class: {IRON_BAR_BP_PATH}")

    for recipe_path in TARGET_RECIPES:
        _migrate_recipe(recipe_path, iron_material, iron_bar_bp_class)

    unreal.log_warning("[FactoryMigration] Recipe input migration complete")


if __name__ == "__main__":
    main()
