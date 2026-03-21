import unreal


DATA_FOLDER = "/Game/Tools/Data"


def log(message: str) -> None:
    unreal.log(f"[ToolAssets] {message}")


def set_property_if_present(asset: unreal.Object, property_name: str, value) -> None:
    tried_names = [property_name]
    if property_name.startswith("b_"):
        tried_names.append(property_name[2:])

    for candidate_name in tried_names:
        try:
            asset.set_editor_property(candidate_name, value)
            return
        except Exception:
            continue

    try:
        # Last chance: UE reflection sometimes expects exact C++-style field names.
        asset.set_editor_property(property_name.replace("_", ""), value)
        return
    except Exception:
        pass

    log(f"Skipped property '{property_name}' on {asset.get_name()}")


def create_or_load_broom_definition(asset_name: str) -> unreal.Object:
    asset_path = f"{DATA_FOLDER}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(asset_path):
        loaded = unreal.EditorAssetLibrary.load_asset(asset_path)
        if loaded:
            log(f"Loaded existing asset: {asset_path}")
            return loaded

    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    factory = unreal.DataAssetFactory()
    try:
        factory.set_editor_property("data_asset_class", unreal.BroomToolDefinition)
    except Exception:
        # Some versions infer this from create_asset's class argument.
        pass

    created = asset_tools.create_asset(
        asset_name=asset_name,
        package_path=DATA_FOLDER,
        asset_class=unreal.BroomToolDefinition,
        factory=factory,
    )
    if not created:
        raise RuntimeError(f"Failed to create asset {asset_path}")

    log(f"Created asset: {asset_path}")
    return created


def configure_default_broom_definition(asset: unreal.Object) -> None:
    set_property_if_present(asset, "max_equip_distance", 280.0)
    set_property_if_present(asset, "hand_socket_name", "HandGrip_R")
    set_property_if_present(asset, "b_disable_collision_while_equipped", False)
    set_property_if_present(asset, "equipped_collision_enabled", unreal.CollisionEnabled.QUERY_ONLY)
    set_property_if_present(asset, "drop_forward_boost_speed", 110.0)

    set_property_if_present(asset, "rest_head_offset", unreal.Vector(145.0, 12.0, -56.0))
    set_property_if_present(asset, "sweep_head_offset", unreal.Vector(170.0, 12.0, -64.0))
    set_property_if_present(asset, "head_follow_speed_idle", 10.0)
    set_property_if_present(asset, "head_follow_speed_sweep", 20.0)
    set_property_if_present(asset, "head_follow_speed_catch", 6.0)

    set_property_if_present(asset, "head_collision_radius", 9.0)
    set_property_if_present(asset, "collision_slide_fraction", 0.4)
    set_property_if_present(asset, "b_enable_ground_follow", True)
    set_property_if_present(asset, "ground_follow_strength", 1.0)
    set_property_if_present(asset, "ground_trace_up_height_cm", 30.0)
    set_property_if_present(asset, "ground_trace_down_depth_cm", 130.0)
    set_property_if_present(asset, "ground_clearance_cm", 2.0)

    set_property_if_present(asset, "push_apply_interval_seconds", 0.04)
    set_property_if_present(asset, "push_impulse", 90000.0)
    set_property_if_present(asset, "min_push_transfer_fraction", 0.08)
    set_property_if_present(asset, "strength_to_push_exponent", 1.0)
    set_property_if_present(asset, "catch_soft_duration_seconds", 0.18)
    set_property_if_present(asset, "catch_mass_over_strength_ratio", 1.25)

    set_property_if_present(asset, "b_allow_dirt_while_equipped", True)
    set_property_if_present(asset, "b_keep_brush_active_when_dropped", True)
    set_property_if_present(asset, "b_draw_debug", False)


def configure_heavy_broom_definition(asset: unreal.Object) -> None:
    set_property_if_present(asset, "max_equip_distance", 260.0)
    set_property_if_present(asset, "hand_socket_name", "HandGrip_R")
    set_property_if_present(asset, "drop_forward_boost_speed", 90.0)

    set_property_if_present(asset, "rest_head_offset", unreal.Vector(148.0, 12.0, -58.0))
    set_property_if_present(asset, "sweep_head_offset", unreal.Vector(174.0, 10.0, -66.0))
    set_property_if_present(asset, "head_follow_speed_idle", 8.0)
    set_property_if_present(asset, "head_follow_speed_sweep", 16.0)
    set_property_if_present(asset, "head_follow_speed_catch", 5.0)

    set_property_if_present(asset, "head_collision_radius", 10.0)
    set_property_if_present(asset, "collision_slide_fraction", 0.35)
    set_property_if_present(asset, "ground_follow_strength", 1.0)

    set_property_if_present(asset, "push_apply_interval_seconds", 0.05)
    set_property_if_present(asset, "push_impulse", 125000.0)
    set_property_if_present(asset, "min_push_transfer_fraction", 0.05)
    set_property_if_present(asset, "strength_to_push_exponent", 1.1)
    set_property_if_present(asset, "catch_soft_duration_seconds", 0.24)
    set_property_if_present(asset, "catch_mass_over_strength_ratio", 1.45)

    set_property_if_present(asset, "b_allow_dirt_while_equipped", True)
    set_property_if_present(asset, "b_keep_brush_active_when_dropped", True)
    set_property_if_present(asset, "b_draw_debug", False)


def create_or_load_broom_blueprint(default_definition_asset: unreal.Object) -> None:
    blueprint_path = "/Game/Tools/BP_BroomToolActor"

    if unreal.EditorAssetLibrary.does_asset_exist(blueprint_path):
        log(f"Loaded existing blueprint: {blueprint_path}")
        blueprint_class = unreal.EditorAssetLibrary.load_blueprint_class(blueprint_path)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.BroomToolActor)
        blueprint_asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
            asset_name="BP_BroomToolActor",
            package_path="/Game/Tools",
            asset_class=unreal.Blueprint,
            factory=factory,
        )
        if not blueprint_asset:
            raise RuntimeError("Failed to create /Game/Tools/BP_BroomToolActor")

        log("Created blueprint: /Game/Tools/BP_BroomToolActor")
        blueprint_class = unreal.EditorAssetLibrary.load_blueprint_class(blueprint_path)

    if not blueprint_class:
        raise RuntimeError("Failed to load BroomToolActor blueprint class.")

    blueprint_cdo = unreal.get_default_object(blueprint_class)
    set_property_if_present(blueprint_cdo, "tool_definition_asset", default_definition_asset)
    unreal.EditorAssetLibrary.save_asset(blueprint_path, only_if_is_dirty=False)
    log("Saved blueprint defaults for /Game/Tools/BP_BroomToolActor")


def main() -> None:
    if not unreal.EditorAssetLibrary.does_directory_exist(DATA_FOLDER):
        unreal.EditorAssetLibrary.make_directory(DATA_FOLDER)
        log(f"Created directory: {DATA_FOLDER}")

    default_asset = create_or_load_broom_definition("DA_BroomToolDefinition_Default")
    configure_default_broom_definition(default_asset)

    heavy_asset = create_or_load_broom_definition("DA_BroomToolDefinition_Heavy")
    configure_heavy_broom_definition(heavy_asset)
    create_or_load_broom_blueprint(default_asset)

    unreal.EditorAssetLibrary.save_asset(f"{DATA_FOLDER}/DA_BroomToolDefinition_Default", only_if_is_dirty=False)
    unreal.EditorAssetLibrary.save_asset(f"{DATA_FOLDER}/DA_BroomToolDefinition_Heavy", only_if_is_dirty=False)
    log("Saved broom data assets.")


if __name__ == "__main__":
    main()
