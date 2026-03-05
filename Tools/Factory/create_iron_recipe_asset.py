import unreal


def _fail(message: str) -> None:
    unreal.log_error(message)
    raise RuntimeError(message)


def _load_resource(path: str):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        _fail(f"Missing resource asset: {path}")
    return asset


def _build_recipe_entry(resource_asset, quantity_units: int, use_new_entry_type: bool):
    if use_new_entry_type and hasattr(unreal, "RecipeResourceEntry"):
        entry = unreal.RecipeResourceEntry()
        entry.set_editor_property("resource", resource_asset)
        entry.set_editor_property("quantity_units", max(1, int(quantity_units)))
        return entry

    entry = unreal.FactoryRecipeResourceEntry()
    entry.set_editor_property("resource", resource_asset)
    entry.set_editor_property("quantity_units", max(1, int(quantity_units)))
    return entry


def main() -> None:
    recipe_class = unreal.load_class(None, "/Script/Agent.RecipeAsset")
    if not recipe_class:
        recipe_class = unreal.load_class(None, "/Script/Agent.FactoryRecipeAsset")
    if not recipe_class:
        _fail("Could not resolve /Script/Agent.RecipeAsset or /Script/Agent.FactoryRecipeAsset")

    package_path = "/Game/Factory/Recipes"
    asset_name = "DA_Recipe_IronOreToIronBar"
    asset_path = f"{package_path}/{asset_name}"

    if not unreal.EditorAssetLibrary.does_directory_exist(package_path):
        unreal.EditorAssetLibrary.make_directory(package_path)

    recipe_asset = None
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        recipe_asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not recipe_asset:
        data_factory = unreal.DataAssetFactory()
        data_factory.set_editor_property("data_asset_class", recipe_class)
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        recipe_asset = asset_tools.create_asset(asset_name, package_path, recipe_class, data_factory)

    if not recipe_asset:
        _fail(f"Failed to create asset: {asset_path}")

    asset_class_name = recipe_asset.get_class().get_name() if recipe_asset else ""
    use_new_entry_type = asset_class_name == "RecipeAsset"

    iron_ore = _load_resource("/Game/Factory/Resources/DA_Resource_IronOre")
    iron_bar = _load_resource("/Game/Factory/Resources/DA_Resource_IronBar")

    inputs = [_build_recipe_entry(iron_ore, 1, use_new_entry_type)]
    outputs = [_build_recipe_entry(iron_bar, 1, use_new_entry_type)]

    recipe_asset.set_editor_property("recipe_id", "Recipe_IronOreToIronBar")
    recipe_asset.set_editor_property("display_name", unreal.Text("Iron Ore -> Iron Bar"))
    recipe_asset.set_editor_property("allowed_machine_tag", "Machine")
    recipe_asset.set_editor_property("craft_time_seconds", 2.0)
    recipe_asset.set_editor_property("inputs", inputs)
    recipe_asset.set_editor_property("outputs", outputs)

    if not unreal.EditorAssetLibrary.save_asset(asset_path, only_if_is_dirty=False):
        _fail(f"Failed to save recipe asset: {asset_path}")

    unreal.log(f"Created/updated recipe asset: {asset_path}")


if __name__ == "__main__":
    main()
