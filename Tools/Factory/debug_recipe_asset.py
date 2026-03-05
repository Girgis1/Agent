import unreal

asset_path = "/Game/Factory/Recipes/DA_Recipe_IronOreToIronBar"
asset = unreal.EditorAssetLibrary.load_asset(asset_path)

if not asset:
    unreal.log_error(f"Missing asset: {asset_path}")
else:
    cls_name = asset.get_class().get_name()
    unreal.log(f"Recipe asset class: {cls_name}")

    for prop in ["recipe_id", "display_name", "allowed_machine_tag", "craft_time_seconds", "inputs", "outputs"]:
        try:
            value = asset.get_editor_property(prop)
            unreal.log(f"{prop}: {value}")
        except Exception as exc:
            unreal.log_warning(f"Failed reading '{prop}': {exc}")
