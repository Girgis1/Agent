import unreal


ASSET_PATH = "/Game/Factory/Recipes/R_IronOre_to_IronBar"


def _safe_get(obj, prop: str):
    try:
        return obj.get_editor_property(prop)
    except Exception:
        return None


def _safe_text(value):
    try:
        return str(value)
    except Exception:
        return "<unprintable>"


def _dump_input(entry, index: int) -> None:
    input_type = _safe_get(entry, "input_type")
    material = _safe_get(entry, "material")
    item_class = _safe_get(entry, "item_actor_class")
    qty = _safe_get(entry, "quantity_units")
    use_whitelist = _safe_get(entry, "b_use_whitelist")
    use_blacklist = _safe_get(entry, "b_use_blacklist")
    whitelist = _safe_get(entry, "whitelist_materials") or []
    blacklist = _safe_get(entry, "blacklist_materials") or []

    unreal.log(
        f"input[{index}] type={_safe_text(input_type)} qty={_safe_text(qty)} "
        f"material={_safe_text(material)} item_class={_safe_text(item_class)} "
        f"use_whitelist={_safe_text(use_whitelist)} whitelist={len(whitelist)} "
        f"use_blacklist={_safe_text(use_blacklist)} blacklist={len(blacklist)}"
    )


def _dump_output(entry, index: int) -> None:
    output_class = _safe_get(entry, "output_actor_class")
    unreal.log(f"output[{index}] output_actor_class={_safe_text(output_class)}")


def main() -> None:
    asset = unreal.EditorAssetLibrary.load_asset(ASSET_PATH)
    if not asset:
        unreal.log_error(f"Missing asset: {ASSET_PATH}")
        return

    unreal.log(f"Recipe asset class: {asset.get_class().get_name()}")
    for prop in ["recipe_id", "display_name", "craft_time_seconds"]:
        value = _safe_get(asset, prop)
        unreal.log(f"{prop}: {_safe_text(value)}")

    inputs = _safe_get(asset, "inputs") or []
    outputs = _safe_get(asset, "outputs") or []
    unreal.log(f"inputs_count: {len(inputs)}")
    unreal.log(f"outputs_count: {len(outputs)}")

    for i, entry in enumerate(inputs):
        _dump_input(entry, i)

    for i, entry in enumerate(outputs):
        _dump_output(entry, i)


if __name__ == "__main__":
    main()
