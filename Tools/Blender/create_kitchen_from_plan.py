from pathlib import Path
import math

import bpy
from mathutils import Vector


ROOT_DIR = Path(__file__).resolve().parents[2]
OUTPUT_DIR = ROOT_DIR / "ExternalAssets" / "KitchenPlan"
BLEND_PATH = OUTPUT_DIR / "KitchenPlan.blend"
FBX_PATH = OUTPUT_DIR / "KitchenPlan.fbx"
PREVIEW_PATH = OUTPUT_DIR / "KitchenPlan.png"
TOP_PREVIEW_PATH = OUTPUT_DIR / "KitchenPlan_top.png"

ROOM_WIDTH = 4.56
ROOM_DEPTH = 2.90
ROOM_HEIGHT = 2.70
WALL_THICKNESS = 0.12

COUNTER_HEIGHT = 0.92
COUNTER_THICKNESS = 0.04
COUNTER_TOP_Z = COUNTER_HEIGHT - (COUNTER_THICKNESS / 2.0)
BASE_CABINET_HEIGHT = COUNTER_HEIGHT - COUNTER_THICKNESS
BASE_CARCASS_HEIGHT = BASE_CABINET_HEIGHT - 0.10
TOE_KICK_HEIGHT = 0.10
BASE_CABINET_DEPTH = 0.60
COUNTER_DEPTH = 0.64
TALL_CABINET_DEPTH = 0.65
TALL_CABINET_HEIGHT = 2.25
PANEL_THICKNESS = 0.02

WINDOW_WIDTH = 2.02
WINDOW_SILL_HEIGHT = 1.08
WINDOW_HEIGHT = 1.02
WINDOW_FRAME_THICKNESS = 0.05


def reset_scene():
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete(use_global=False)

    for material in list(bpy.data.materials):
        if material.users == 0:
            bpy.data.materials.remove(material)


def configure_scene():
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 1.0
    scene.render.engine = "CYCLES"
    scene.cycles.samples = 64
    scene.render.resolution_x = 1600
    scene.render.resolution_y = 1200
    scene.render.image_settings.file_format = "PNG"
    scene.render.filepath = str(PREVIEW_PATH)
    scene.view_settings.exposure = -1.2

    world = scene.world
    world.use_nodes = True
    background = world.node_tree.nodes.get("Background")
    if background is not None:
        background.inputs[0].default_value = (0.80, 0.82, 0.86, 1.0)
        background.inputs[1].default_value = 0.22


def make_principled_material(name, color, roughness=0.45, metallic=0.0):
    material = bpy.data.materials.new(name=name)
    material.use_nodes = True
    nodes = material.node_tree.nodes
    principled = nodes.get("Principled BSDF")
    principled.inputs["Base Color"].default_value = (*color, 1.0)
    principled.inputs["Roughness"].default_value = roughness
    principled.inputs["Metallic"].default_value = metallic
    return material


def make_glass_material(name, color, roughness=0.02):
    material = bpy.data.materials.new(name=name)
    material.use_nodes = True
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    nodes.clear()

    output = nodes.new(type="ShaderNodeOutputMaterial")
    glass = nodes.new(type="ShaderNodeBsdfGlass")
    glass.inputs["Color"].default_value = (*color, 1.0)
    glass.inputs["Roughness"].default_value = roughness
    glass.location = (-180, 0)
    output.location = (40, 0)
    links.new(glass.outputs["BSDF"], output.inputs["Surface"])
    return material


def ensure_material(obj, material):
    if obj.data.materials:
        obj.data.materials[0] = material
    else:
        obj.data.materials.append(material)


def add_bevel(obj, width=0.0025, segments=2):
    modifier = obj.modifiers.new(name="Bevel", type="BEVEL")
    modifier.width = width
    modifier.segments = segments
    modifier.limit_method = "ANGLE"


def add_box(name, dimensions, location, material=None, bevel=0.0025):
    bpy.ops.mesh.primitive_cube_add(location=location)
    obj = bpy.context.active_object
    obj.name = name
    obj.dimensions = dimensions
    if material is not None:
        ensure_material(obj, material)
    if bevel > 0.0:
        add_bevel(obj, width=bevel)
    return obj


def add_cylinder(name, radius, depth, location, rotation=(0.0, 0.0, 0.0), material=None):
    bpy.ops.mesh.primitive_cylinder_add(
        vertices=32,
        radius=radius,
        depth=depth,
        location=location,
        rotation=rotation,
    )
    obj = bpy.context.active_object
    obj.name = name
    if material is not None:
        ensure_material(obj, material)
    bpy.ops.object.shade_smooth()
    return obj


def axes_for_facing(facing):
    if facing == "south":
        return Vector((1.0, 0.0, 0.0)), Vector((0.0, 1.0, 0.0)), Vector((0.0, -1.0, 0.0))
    if facing == "north":
        return Vector((1.0, 0.0, 0.0)), Vector((0.0, 1.0, 0.0)), Vector((0.0, 1.0, 0.0))
    if facing == "east":
        return Vector((0.0, 1.0, 0.0)), Vector((1.0, 0.0, 0.0)), Vector((1.0, 0.0, 0.0))
    if facing == "west":
        return Vector((0.0, 1.0, 0.0)), Vector((1.0, 0.0, 0.0)), Vector((-1.0, 0.0, 0.0))
    raise ValueError(f"Unsupported facing: {facing}")


def world_dimensions(facing, width, depth, height):
    if facing in {"south", "north"}:
        return (width, depth, height)
    return (depth, width, height)


def panel_dimensions(facing, width, height, thickness=PANEL_THICKNESS):
    if facing in {"south", "north"}:
        return (width, thickness, height)
    return (thickness, width, height)


def front_panel_location(center, facing, depth, z, thickness=PANEL_THICKNESS, inset=0.002):
    _, _, front_dir = axes_for_facing(facing)
    return Vector(center) + (front_dir * ((depth / 2.0) - (thickness / 2.0) - inset)) + Vector((0.0, 0.0, z))


def add_handle(name, facing, panel_center, panel_width, panel_height, side, material):
    width_axis, _, front_dir = axes_for_facing(facing)
    horizontal_offset = (panel_width * 0.5) - 0.06
    if side == "left":
        handle_pos = Vector(panel_center) - (width_axis * horizontal_offset)
    else:
        handle_pos = Vector(panel_center) + (width_axis * horizontal_offset)

    handle_pos += front_dir * 0.012
    handle_pos.z += 0.02
    handle_dims = panel_dimensions(facing, 0.012, min(panel_height * 0.55, 0.50), 0.012)
    add_box(name, handle_dims, handle_pos, material=material, bevel=0.001)


def add_split_panels(name_prefix, center, facing, width, depth, height, panel_kind, front_material, handle_material):
    gap = 0.012
    width_axis, _, _ = axes_for_facing(facing)
    panel_z = (TOE_KICK_HEIGHT + (height / 2.0))

    if panel_kind in {"double", "sink"}:
        panel_width = (width - (gap * 3.0)) / 2.0
        left_center = front_panel_location(center, facing, depth, panel_z) - (width_axis * ((panel_width / 2.0) + (gap / 2.0)))
        right_center = front_panel_location(center, facing, depth, panel_z) + (width_axis * ((panel_width / 2.0) + (gap / 2.0)))
        panel_dims = panel_dimensions(facing, panel_width, height - 0.04)
        left_panel = add_box(f"{name_prefix}_DoorL", panel_dims, left_center, material=front_material)
        right_panel = add_box(f"{name_prefix}_DoorR", panel_dims, right_center, material=front_material)
        add_handle(f"{name_prefix}_HandleL", facing, left_panel.location, panel_width, height - 0.04, "right", handle_material)
        add_handle(f"{name_prefix}_HandleR", facing, right_panel.location, panel_width, height - 0.04, "left", handle_material)
        return

    if panel_kind == "drawers":
        drawer_heights = [0.22, 0.24, height - 0.06 - 0.22 - 0.24]
        z_cursor = TOE_KICK_HEIGHT + height - 0.03
        for index, drawer_height in enumerate(drawer_heights):
            z_cursor -= drawer_height / 2.0
            drawer_center = front_panel_location(center, facing, depth, z_cursor)
            drawer_dims = panel_dimensions(facing, width - 0.02, drawer_height - gap)
            add_box(f"{name_prefix}_Drawer{index + 1}", drawer_dims, drawer_center, material=front_material)
            z_cursor -= drawer_height / 2.0
        return

    if panel_kind == "dishwasher":
        outer = add_box(
            f"{name_prefix}_Door",
            panel_dimensions(facing, width - 0.02, height - 0.03),
            front_panel_location(center, facing, depth, panel_z),
            material=front_material,
        )
        control_strip_center = Vector(outer.location)
        control_strip_center.z += ((height - 0.03) / 2.0) - 0.06
        control_strip_dims = panel_dimensions(facing, width - 0.03, 0.08, 0.01)
        add_box(f"{name_prefix}_Control", control_strip_dims, control_strip_center, material=handle_material, bevel=0.001)
        return

    if panel_kind == "oven":
        lower_center = front_panel_location(center, facing, depth, TOE_KICK_HEIGHT + 0.34)
        glass_dims = panel_dimensions(facing, width - 0.04, 0.56, 0.01)
        add_box(f"{name_prefix}_Glass", glass_dims, lower_center, material=front_material, bevel=0.001)

        top_strip_center = front_panel_location(center, facing, depth, TOE_KICK_HEIGHT + height - 0.10)
        strip_dims = panel_dimensions(facing, width - 0.04, 0.14, 0.01)
        add_box(f"{name_prefix}_Controls", strip_dims, top_strip_center, material=front_material, bevel=0.001)

        add_handle(
            f"{name_prefix}_Handle",
            facing,
            Vector(front_panel_location(center, facing, depth, TOE_KICK_HEIGHT + 0.58)),
            width - 0.10,
            0.02,
            "right",
            handle_material,
        )
        return

    add_box(
        f"{name_prefix}_Door",
        panel_dimensions(facing, width - 0.02, height - 0.04),
        front_panel_location(center, facing, depth, panel_z),
        material=front_material,
    )


def create_base_module(name, center, width, depth, facing, box_material, front_material, handle_material, panel_kind):
    _, _, front_dir = axes_for_facing(facing)

    carcass_center = Vector(center)
    carcass_center.z = TOE_KICK_HEIGHT + (BASE_CARCASS_HEIGHT / 2.0)
    carcass_dims = world_dimensions(facing, width, depth, BASE_CARCASS_HEIGHT)
    add_box(f"{name}_Body", carcass_dims, carcass_center, material=box_material)

    toe_center = Vector(center) - (front_dir * 0.05)
    toe_center.z = TOE_KICK_HEIGHT / 2.0
    toe_dims = world_dimensions(facing, width - 0.04, depth - 0.10, TOE_KICK_HEIGHT)
    add_box(f"{name}_Toe", toe_dims, toe_center, material=box_material, bevel=0.0015)

    add_split_panels(name, center, facing, width, depth, BASE_CARCASS_HEIGHT, panel_kind, front_material, handle_material)


def create_tall_module(name, center, width, depth, facing, box_material, front_material, handle_material, panel_kind):
    body_height = TALL_CABINET_HEIGHT
    body_center = Vector(center)
    body_center.z = body_height / 2.0
    body_dims = world_dimensions(facing, width, depth, body_height)
    add_box(f"{name}_Body", body_dims, body_center, material=box_material)

    panel_height = body_height - 0.04
    panel_center = front_panel_location(center, facing, depth, body_height / 2.0)
    panel_dims = panel_dimensions(facing, width - 0.03, panel_height)
    add_box(f"{name}_Front", panel_dims, panel_center, material=front_material)

    add_handle(
        f"{name}_Handle",
        facing,
        panel_center + Vector((0.0, 0.0, 0.15)),
        width - 0.10,
        panel_height,
        "right" if panel_kind == "fridge" else "left",
        handle_material,
    )

    if panel_kind == "fridge":
        split_z = 1.48
        trim_dims = panel_dimensions(facing, width - 0.04, 0.018, 0.008)
        split_center = Vector(panel_center)
        split_center.z = split_z
        add_box(f"{name}_Split", trim_dims, split_center, material=handle_material, bevel=0.001)


def build_room(materials):
    floor = add_box(
        "Floor",
        (ROOM_WIDTH, ROOM_DEPTH, 0.03),
        (0.0, 0.0, -0.015),
        material=materials["floor"],
        bevel=0.0,
    )
    floor.cycles.is_shadow_catcher = False

    side_width = (ROOM_WIDTH - WINDOW_WIDTH) / 2.0
    back_wall_y = (ROOM_DEPTH / 2.0) + (WALL_THICKNESS / 2.0)
    side_wall_x = (ROOM_WIDTH / 2.0) + (WALL_THICKNESS / 2.0)

    add_box(
        "Wall_Left",
        (WALL_THICKNESS, ROOM_DEPTH, ROOM_HEIGHT),
        (-side_wall_x, 0.0, ROOM_HEIGHT / 2.0),
        material=materials["wall"],
        bevel=0.0,
    )
    add_box(
        "Wall_Right",
        (WALL_THICKNESS, ROOM_DEPTH, ROOM_HEIGHT),
        (side_wall_x, 0.0, ROOM_HEIGHT / 2.0),
        material=materials["wall"],
        bevel=0.0,
    )
    add_box(
        "Wall_Back_Left",
        (side_width, WALL_THICKNESS, ROOM_HEIGHT),
        (-(WINDOW_WIDTH / 2.0) - (side_width / 2.0), back_wall_y, ROOM_HEIGHT / 2.0),
        material=materials["wall"],
        bevel=0.0,
    )
    add_box(
        "Wall_Back_Right",
        (side_width, WALL_THICKNESS, ROOM_HEIGHT),
        ((WINDOW_WIDTH / 2.0) + (side_width / 2.0), back_wall_y, ROOM_HEIGHT / 2.0),
        material=materials["wall"],
        bevel=0.0,
    )
    add_box(
        "Wall_Back_BelowWindow",
        (WINDOW_WIDTH, WALL_THICKNESS, WINDOW_SILL_HEIGHT),
        (0.0, back_wall_y, WINDOW_SILL_HEIGHT / 2.0),
        material=materials["wall"],
        bevel=0.0,
    )
    upper_height = ROOM_HEIGHT - WINDOW_SILL_HEIGHT - WINDOW_HEIGHT
    add_box(
        "Wall_Back_AboveWindow",
        (WINDOW_WIDTH, WALL_THICKNESS, upper_height),
        (0.0, back_wall_y, WINDOW_SILL_HEIGHT + WINDOW_HEIGHT + (upper_height / 2.0)),
        material=materials["wall"],
        bevel=0.0,
    )

    window_center_z = WINDOW_SILL_HEIGHT + (WINDOW_HEIGHT / 2.0)
    frame_y = back_wall_y - (WALL_THICKNESS / 2.0) + 0.01
    glass_y = frame_y - 0.01
    frame_z_top = WINDOW_SILL_HEIGHT + WINDOW_HEIGHT - (WINDOW_FRAME_THICKNESS / 2.0)
    frame_z_bottom = WINDOW_SILL_HEIGHT + (WINDOW_FRAME_THICKNESS / 2.0)

    add_box(
        "Window_Frame_Top",
        (WINDOW_WIDTH, WINDOW_FRAME_THICKNESS, WINDOW_FRAME_THICKNESS),
        (0.0, frame_y, frame_z_top),
        material=materials["frame"],
        bevel=0.001,
    )
    add_box(
        "Window_Frame_Bottom",
        (WINDOW_WIDTH, WINDOW_FRAME_THICKNESS, WINDOW_FRAME_THICKNESS),
        (0.0, frame_y, frame_z_bottom),
        material=materials["frame"],
        bevel=0.001,
    )

    side_x = (WINDOW_WIDTH / 2.0) - (WINDOW_FRAME_THICKNESS / 2.0)
    add_box(
        "Window_Frame_Left",
        (WINDOW_FRAME_THICKNESS, WINDOW_FRAME_THICKNESS, WINDOW_HEIGHT),
        (-side_x, frame_y, window_center_z),
        material=materials["frame"],
        bevel=0.001,
    )
    add_box(
        "Window_Frame_Right",
        (WINDOW_FRAME_THICKNESS, WINDOW_FRAME_THICKNESS, WINDOW_HEIGHT),
        (side_x, frame_y, window_center_z),
        material=materials["frame"],
        bevel=0.001,
    )
    add_box(
        "Window_Mullion",
        (WINDOW_FRAME_THICKNESS, WINDOW_FRAME_THICKNESS, WINDOW_HEIGHT),
        (0.0, frame_y, window_center_z),
        material=materials["frame"],
        bevel=0.001,
    )
    add_box(
        "Window_Glass_Left",
        ((WINDOW_WIDTH / 2.0) - WINDOW_FRAME_THICKNESS * 1.5, 0.008, WINDOW_HEIGHT - WINDOW_FRAME_THICKNESS * 2.0),
        (-(WINDOW_WIDTH / 4.0), glass_y, window_center_z),
        material=materials["glass"],
        bevel=0.0,
    )
    add_box(
        "Window_Glass_Right",
        ((WINDOW_WIDTH / 2.0) - WINDOW_FRAME_THICKNESS * 1.5, 0.008, WINDOW_HEIGHT - WINDOW_FRAME_THICKNESS * 2.0),
        ((WINDOW_WIDTH / 4.0), glass_y, window_center_z),
        material=materials["glass"],
        bevel=0.0,
    )
    add_box(
        "Window_Sill",
        (WINDOW_WIDTH + 0.08, 0.20, 0.03),
        (0.0, (ROOM_DEPTH / 2.0) - 0.02, WINDOW_SILL_HEIGHT - 0.015),
        material=materials["countertop"],
        bevel=0.001,
    )


def build_countertops(materials, left_length, right_length):
    back_counter = add_box(
        "Counter_Back",
        (ROOM_WIDTH, COUNTER_DEPTH, COUNTER_THICKNESS),
        (0.0, (ROOM_DEPTH / 2.0) - (COUNTER_DEPTH / 2.0), COUNTER_TOP_Z),
        material=materials["countertop"],
        bevel=0.002,
    )

    left_counter_center_y = (ROOM_DEPTH / 2.0) - COUNTER_DEPTH - (left_length / 2.0)
    add_box(
        "Counter_Left",
        (COUNTER_DEPTH, left_length, COUNTER_THICKNESS),
        (-(ROOM_WIDTH / 2.0) + (COUNTER_DEPTH / 2.0), left_counter_center_y, COUNTER_TOP_Z),
        material=materials["countertop"],
        bevel=0.002,
    )

    right_counter_center_y = (ROOM_DEPTH / 2.0) - COUNTER_DEPTH - (right_length / 2.0)
    add_box(
        "Counter_Right",
        (COUNTER_DEPTH, right_length, COUNTER_THICKNESS),
        ((ROOM_WIDTH / 2.0) - (COUNTER_DEPTH / 2.0), right_counter_center_y, COUNTER_TOP_Z),
        material=materials["countertop"],
        bevel=0.002,
    )

    add_box(
        "Splashback",
        (ROOM_WIDTH, 0.02, 0.16),
        (0.0, (ROOM_DEPTH / 2.0) - 0.01, COUNTER_HEIGHT + 0.08),
        material=materials["wall"],
        bevel=0.0,
    )
    return back_counter


def module_centers_along_x(widths):
    cursor = -(ROOM_WIDTH / 2.0)
    result = []
    for width in widths:
        result.append(cursor + (width / 2.0))
        cursor += width
    return result


def module_centers_along_y(lengths):
    cursor = (ROOM_DEPTH / 2.0) - COUNTER_DEPTH
    result = []
    for length in lengths:
        result.append(cursor - (length / 2.0))
        cursor -= length
    return result


def build_cabinetry(materials):
    back_modules = [
        {"width": 0.70, "kind": "drawers"},
        {"width": 0.60, "kind": "double"},
        {"width": 0.60, "kind": "dishwasher"},
        {"width": 0.80, "kind": "sink"},
        {"width": 0.60, "kind": "double"},
        {"width": 0.56, "kind": "double"},
        {"width": 0.70, "kind": "drawers"},
    ]
    left_modules = [
        {"length": 0.75, "depth": BASE_CABINET_DEPTH, "kind": "double", "type": "base"},
        {"length": 0.80, "depth": TALL_CABINET_DEPTH, "kind": "fridge", "type": "tall"},
    ]
    right_modules = [
        {"length": 0.58, "depth": BASE_CABINET_DEPTH, "kind": "double", "type": "base"},
        {"length": 0.90, "depth": BASE_CABINET_DEPTH, "kind": "oven", "type": "base"},
        {"length": 0.57, "depth": TALL_CABINET_DEPTH, "kind": "pantry", "type": "tall"},
    ]

    back_y = (ROOM_DEPTH / 2.0) - (BASE_CABINET_DEPTH / 2.0)
    back_centers = module_centers_along_x([module["width"] for module in back_modules])
    for index, module in enumerate(back_modules):
        create_base_module(
            f"Back_{index + 1}",
            (back_centers[index], back_y, 0.0),
            module["width"],
            BASE_CABINET_DEPTH,
            "south",
            materials["cabinet_box"],
            materials["cabinet_front"],
            materials["metal"],
            module["kind"],
        )

    left_y_centers = module_centers_along_y([module["length"] for module in left_modules])
    for index, module in enumerate(left_modules):
        center_x = -(ROOM_WIDTH / 2.0) + (module["depth"] / 2.0)
        center = (center_x, left_y_centers[index], 0.0)
        if module["type"] == "base":
            create_base_module(
                f"Left_{index + 1}",
                center,
                module["length"],
                module["depth"],
                "east",
                materials["cabinet_box"],
                materials["cabinet_front"],
                materials["metal"],
                module["kind"],
            )
        else:
            create_tall_module(
                f"Left_{index + 1}",
                center,
                module["length"],
                module["depth"],
                "east",
                materials["tall_box"],
                materials["tall_front"],
                materials["metal"],
                module["kind"],
            )

    right_y_centers = module_centers_along_y([module["length"] for module in right_modules])
    for index, module in enumerate(right_modules):
        center_x = (ROOM_WIDTH / 2.0) - (module["depth"] / 2.0)
        center = (center_x, right_y_centers[index], 0.0)
        if module["type"] == "base":
            create_base_module(
                f"Right_{index + 1}",
                center,
                module["length"],
                module["depth"],
                "west",
                materials["cabinet_box"],
                materials["cabinet_front"],
                materials["metal"],
                module["kind"],
            )
        else:
            create_tall_module(
                f"Right_{index + 1}",
                center,
                module["length"],
                module["depth"],
                "west",
                materials["tall_box"],
                materials["tall_front"],
                materials["metal"],
                module["kind"],
            )

    left_counter_length = sum(module["length"] for module in left_modules if module["type"] == "base")
    right_counter_length = sum(module["length"] for module in right_modules if module["type"] == "base")
    build_countertops(materials, left_counter_length, right_counter_length)

    sink_center_x = back_centers[3]
    sink_center_y = (ROOM_DEPTH / 2.0) - (COUNTER_DEPTH / 2.0) + 0.01
    sink_lip = add_box(
        "Sink_Lip",
        (0.66, 0.48, 0.025),
        (sink_center_x, sink_center_y, COUNTER_HEIGHT - 0.012),
        material=materials["metal"],
        bevel=0.0015,
    )
    add_box(
        "Sink_Basin",
        (0.58, 0.40, 0.22),
        (sink_center_x, sink_center_y, COUNTER_HEIGHT - 0.12),
        material=materials["steel_dark"],
        bevel=0.001,
    )
    faucet_x = sink_center_x + 0.18
    faucet_y = sink_center_y + 0.12
    add_cylinder(
        "Faucet_Stem",
        0.012,
        0.30,
        (faucet_x, faucet_y, COUNTER_HEIGHT + 0.13),
        material=materials["metal"],
    )
    add_cylinder(
        "Faucet_Spout",
        0.010,
        0.22,
        (faucet_x - 0.08, faucet_y, COUNTER_HEIGHT + 0.24),
        rotation=(math.radians(90.0), 0.0, 0.0),
        material=materials["metal"],
    )
    add_cylinder(
        "Faucet_Drop",
        0.010,
        0.12,
        (faucet_x - 0.16, faucet_y, COUNTER_HEIGHT + 0.18),
        material=materials["metal"],
    )

    cooktop_center_y = right_y_centers[1]
    cooktop_center_x = (ROOM_WIDTH / 2.0) - (COUNTER_DEPTH / 2.0) + 0.01
    add_box(
        "Cooktop_Glass",
        (0.40, 0.56, 0.012),
        (cooktop_center_x, cooktop_center_y, COUNTER_HEIGHT + 0.006),
        material=materials["appliance_black"],
        bevel=0.001,
    )
    burner_offsets = [
        (-0.09, -0.14),
        (-0.09, 0.14),
        (0.09, -0.14),
        (0.09, 0.14),
    ]
    for index, (offset_x, offset_y) in enumerate(burner_offsets):
        add_cylinder(
            f"Cooktop_Burner_{index + 1}",
            0.055,
            0.004,
            (cooktop_center_x + offset_x, cooktop_center_y + offset_y, COUNTER_HEIGHT + 0.013),
            material=materials["steel_dark"],
        )

    return {
        "sink_center": (sink_center_x, sink_center_y),
        "cooktop_center": (cooktop_center_x, cooktop_center_y),
        "sink_lip": sink_lip,
    }


def add_lighting_and_camera():
    bpy.ops.object.light_add(type="SUN", location=(0.0, 0.0, 5.0))
    sun = bpy.context.active_object
    sun.name = "Sun"
    sun.data.energy = 1.2
    sun.rotation_euler = (math.radians(35.0), math.radians(-10.0), math.radians(25.0))

    bpy.ops.object.light_add(type="AREA", location=(0.0, -2.0, 2.8))
    area = bpy.context.active_object
    area.name = "Area_Key"
    area.data.energy = 1800.0
    area.data.shape = "RECTANGLE"
    area.data.size = 4.0
    area.data.size_y = 2.8
    area.rotation_euler = (math.radians(62.0), 0.0, 0.0)

    bpy.ops.object.empty_add(type="PLAIN_AXES", location=(0.0, 0.10, 0.95))
    focus = bpy.context.active_object
    focus.name = "Camera_Target"

    bpy.ops.object.camera_add(location=(0.0, -5.9, 3.1))
    camera = bpy.context.active_object
    camera.name = "Kitchen_Camera"
    camera.data.lens = 28
    camera.data.sensor_width = 36

    constraint = camera.constraints.new(type="TRACK_TO")
    constraint.target = focus
    constraint.track_axis = "TRACK_NEGATIVE_Z"
    constraint.up_axis = "UP_Y"

    bpy.context.scene.camera = camera
    
    bpy.ops.object.camera_add(location=(0.0, 0.0, 7.0), rotation=(0.0, 0.0, 0.0))
    top_camera = bpy.context.active_object
    top_camera.name = "Kitchen_Top_Camera"
    top_camera.data.type = "ORTHO"
    top_camera.data.ortho_scale = 5.4

    return {
        "perspective": camera,
        "top": top_camera,
    }


def export_scene(cameras):
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND_PATH))
    bpy.ops.export_scene.fbx(
        filepath=str(FBX_PATH),
        use_selection=False,
        apply_unit_scale=True,
        bake_space_transform=True,
        mesh_smooth_type="FACE",
    )
    bpy.context.scene.camera = cameras["perspective"]
    bpy.context.scene.render.filepath = str(PREVIEW_PATH)
    bpy.ops.render.render(write_still=True)
    bpy.context.scene.camera = cameras["top"]
    bpy.context.scene.render.filepath = str(TOP_PREVIEW_PATH)
    bpy.ops.render.render(write_still=True)


def main():
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    reset_scene()
    configure_scene()

    materials = {
        "wall": make_principled_material("Wall", (0.88, 0.89, 0.90), roughness=0.78),
        "floor": make_principled_material("Floor", (0.64, 0.64, 0.63), roughness=0.88),
        "cabinet_box": make_principled_material("CabinetBox", (0.72, 0.74, 0.77), roughness=0.58),
        "cabinet_front": make_principled_material("CabinetFront", (0.82, 0.84, 0.86), roughness=0.42),
        "tall_box": make_principled_material("TallBox", (0.55, 0.57, 0.61), roughness=0.55),
        "tall_front": make_principled_material("TallFront", (0.76, 0.78, 0.81), roughness=0.40),
        "countertop": make_principled_material("Countertop", (0.83, 0.83, 0.81), roughness=0.22),
        "metal": make_principled_material("Metal", (0.76, 0.77, 0.79), roughness=0.18, metallic=1.0),
        "steel_dark": make_principled_material("SteelDark", (0.44, 0.45, 0.47), roughness=0.26, metallic=1.0),
        "appliance_black": make_principled_material("ApplianceBlack", (0.09, 0.10, 0.11), roughness=0.12),
        "frame": make_principled_material("Frame", (0.87, 0.87, 0.87), roughness=0.32),
        "glass": make_glass_material("Glass", (0.88, 0.94, 1.0)),
    }

    build_room(materials)
    build_cabinetry(materials)
    cameras = add_lighting_and_camera()
    export_scene(cameras)


if __name__ == "__main__":
    main()
