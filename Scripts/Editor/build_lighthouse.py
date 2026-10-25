# Builds the walk-in lighthouse in /Game/Lighthouse from the imported exterior model:
#   SM_LighthouseExterior - the DylanHayes model scaled to size, with a real doorway cut where
#                           its painted front door was (render only, no collision)
#   SM_LighthouseShell    - concrete wall inside the tower: collision for both sides + door jamb
#   SM_LighthouseStairs   - floating spiral stair around a centre post, ground to lantern floor
#   SM_LighthouseFloors   - ground floor and the lantern floor with a hatch over the stair top
#
# All meshes share one frame: origin = centre of the tower base at ground level, +Y = doorway.
# The map places them together at one transform.
#
# Run with:  powershell -File Scripts/run_python.ps1 Scripts/Editor/build_lighthouse.py

import math
import unreal

SCALE = 2.2                 # model units -> game size (about 27 m tall)
MODEL_BASE_Z = -166.0       # lowest point of the imported model

# measured from the model (see git history of this file), already scaled
DOOR_HALF_WIDTH = 88.0
DOOR_HEIGHT = 335.0
DOOR_Y = 363.0              # painted door surface

# interior layout
BASE_OUTER, SHAFT_OUTER = 330.0, 216.0      # shell outside radius at ground / shaft
BASE_INNER, SHAFT_INNER = 300.0, 188.0      # shell inside radius at ground / shaft
LANTERN_INNER = 205.0
CONE_TOP = 695.0            # where the flared base becomes the straight shaft
LANTERN_FLOOR = 1880.0
LANTERN_GLASS_BOTTOM = 2140.0   # parapet height; above this the lantern is glazed
LANTERN_GLASS_TOP = 2390.0
SHELL_TOP = 2420.0
STEP_HEIGHT = 20.0
STAIR_INNER = 38.0
STAIR_WIDTH = 135.0
STEP_ANGLE = 14.0
STAIR_START_ANGLE = 270.0   # start opposite the door

assets = unreal.EditorAssetLibrary
gs = unreal.GeometryScript_Primitives
boolean = unreal.GeometryScript_MeshBooleans


def first(value):
    return value[0] if isinstance(value, tuple) else value


def transform(x=0.0, y=0.0, z=0.0, yaw=0.0, scale=1.0):
    return unreal.Transform(unreal.Vector(x, y, z), unreal.Rotator(0.0, 0.0, yaw), unreal.Vector(scale, scale, scale))


def options(single_group=True):
    o = unreal.GeometryScriptPrimitiveOptions()
    if single_group:
        o.set_editor_property("polygroup_mode", unreal.GeometryScriptPrimitivePolygroupMode.SINGLE_GROUP)
    return o


def solid(builder):
    mesh = unreal.DynamicMesh()
    builder(mesh)
    return mesh


def apply(target, tool, operation):
    boolean.apply_mesh_boolean(target, unreal.Transform(), tool, unreal.Transform(), operation,
                               unreal.GeometryScriptMeshBooleanOptions())


def finish(mesh, path, material_path, uv_size=300.0):
    """World-scale box UVs, split normals, save as a static mesh with complex collision."""
    unreal.GeometryScript_UVs.set_mesh_u_vs_from_box_projection(
        mesh, 0, transform(scale=uv_size), unreal.GeometryScriptMeshSelection())
    unreal.GeometryScript_Normals.compute_split_normals(
        mesh, unreal.GeometryScriptSplitNormalsOptions(), unreal.GeometryScriptCalculateNormalsOptions())

    if assets.does_asset_exist(path):
        assets.delete_asset(path)
    create = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    create.set_editor_property("enable_recompute_normals", False)
    create.set_editor_property("enable_collision", True)
    create.set_editor_property("enable_nanite", False)
    static_mesh = first(unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(mesh, path, create))
    if material_path:
        static_mesh.set_material(0, unreal.load_asset(material_path))
    static_mesh.get_editor_property("body_setup").set_editor_property(
        "collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    assets.save_loaded_asset(static_mesh)
    unreal.log("Built " + path)
    return static_mesh


def inner_radius(z):
    """Radius of the walkable space inside the tower at height z."""
    if z < CONE_TOP:
        return BASE_INNER + (SHAFT_INNER - BASE_INNER) * max(0.0, z) / CONE_TOP
    if z < LANTERN_FLOOR:
        return SHAFT_INNER
    return LANTERN_INNER


def door_box():
    return solid(lambda m: gs.append_box(m, options(), transform(0.0, DOOR_Y - 60.0, 0.0),
                                         DOOR_HALF_WIDTH * 2.0, 220.0, DOOR_HEIGHT, 0, 0, 0,
                                         unreal.GeometryScriptPrimitiveOriginMode.BASE))


# --- materials ---------------------------------------------------------------------------------------------------

def build_simple_material(path, color_texture, normal_texture, roughness, tint=(1.0, 1.0, 1.0)):
    """Plain textured material for generated meshes. The cabin pack's own instances read vertex
    colour as a paint mask, which turns GeometryScript meshes (white vertex colour) red."""
    mel = unreal.MaterialEditingLibrary
    if assets.does_asset_exist(path):
        assets.delete_asset(path)
    folder, name = path.rsplit("/", 1)
    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())

    color = mel.create_material_expression(material, unreal.MaterialExpressionTextureSample, -500, -100)
    color.set_editor_property("texture", unreal.load_asset(color_texture))
    tint_node = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -200, -100)
    tint_node.set_editor_property("const_b", 1.0)
    tint_color = mel.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -500, -300)
    tint_color.set_editor_property("constant", unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
    mel.connect_material_expressions(color, "RGB", tint_node, "A")
    mel.connect_material_expressions(tint_color, "", tint_node, "B")

    normal = mel.create_material_expression(material, unreal.MaterialExpressionTextureSample, -500, 150)
    normal.set_editor_property("texture", unreal.load_asset(normal_texture))
    normal.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    rough = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -200, 300)
    rough.set_editor_property("r", roughness)

    mel.connect_material_property(tint_node, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(material)
    assets.save_loaded_asset(material)


TILING = "/Game/Modular_Rural_Cabin/Textures/Tiling/"
FLOOR_MATERIAL = "/Game/Lighthouse/M_LighthouseFloor"
STAIR_MATERIAL = "/Game/Lighthouse/M_LighthouseStairs"


# --- exterior: scaled model with the front door cut out ------------------------------------------------

def build_exterior():
    source = unreal.load_asset("/Game/Imported/Lighthouse/SM_Lighthouse")
    mesh = unreal.DynamicMesh()
    unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(
        source, mesh, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
    unreal.GeometryScript_MeshTransforms.transform_mesh(
        mesh, transform(z=-MODEL_BASE_Z * SCALE, scale=SCALE))

    # remove every triangle touching the doorway (the painted door quad and the wall around it)
    box = unreal.Box(unreal.Vector(-DOOR_HALF_WIDTH, DOOR_Y - 80.0, -20.0),
                     unreal.Vector(DOOR_HALF_WIDTH, DOOR_Y + 60.0, DOOR_HEIGHT))
    selection = first(unreal.GeometryScript_MeshSelection.select_mesh_elements_in_box(
        mesh, box, unreal.GeometryScriptMeshSelectionType.TRIANGLES, False, 1)[1:])
    deleted = unreal.GeometryScript_MeshEdits.delete_selected_triangles_from_mesh(mesh, selection)
    unreal.log("Exterior: removed %s triangles for the doorway" % (deleted[1] if isinstance(deleted, tuple) else deleted,))

    # the model's base is a solid plinth; its top cap would sit above the interior floor
    # and the gallery deck runs straight through the tower at the lantern; both would block the
    # interior. Remove every triangle whose centre lies inside the walkable shaft at those heights.
    queries = unreal.GeometryScript_MeshQueries
    triangles = first(unreal.GeometryScript_List.convert_index_list_to_array(first(queries.get_all_triangle_i_ds(mesh)[1:])))
    doomed = []
    for t in triangles:
        a, b, c = queries.get_triangle_positions(mesh, t)[1:4]
        x, y, z = (a.x + b.x + c.x) / 3.0, (a.y + b.y + c.y) / 3.0, (a.z + b.z + c.z) / 3.0
        r = math.hypot(x, y)
        in_plinth = r < BASE_INNER and -50.0 < z < 90.0
        # the model also has floor discs inside the shaft (about 11.5 m and 16 m up) that cut
        # across the stair; anything inside the walkable shaft below the lens has to go
        in_shaft = r < inner_radius(z) and 90.0 <= z < LANTERN_GLASS_BOTTOM + 40.0
        if in_plinth or in_shaft:
            doomed.append(t)
    unreal.GeometryScript_MeshEdits.delete_triangles_from_mesh(
        mesh, first(unreal.GeometryScript_List.convert_array_to_index_list(doomed)))
    unreal.log("Exterior: removed %d plinth/deck triangles inside the shaft" % len(doomed))

    path = "/Game/Lighthouse/SM_LighthouseExterior"
    if assets.does_asset_exist(path):
        assets.delete_asset(path)
    create = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    create.set_editor_property("enable_recompute_normals", False)
    create.set_editor_property("enable_collision", False)
    create.set_editor_property("enable_nanite", True)
    static_mesh = first(unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(mesh, path, create))
    # keep the model's materials, slot by slot
    for index, slot in enumerate(source.get_editor_property("static_materials")):
        static_mesh.set_material(index, slot.get_editor_property("material_interface"))
    assets.save_loaded_asset(static_mesh)
    unreal.log("Built " + path)


# --- shell: outer solid minus inner solid minus doorway ------------------------------------------------------

def build_shell():
    def outer(m):
        gs.append_cone(m, options(), transform(), BASE_OUTER, SHAFT_OUTER, CONE_TOP, 48, 4, True,
                       unreal.GeometryScriptPrimitiveOriginMode.BASE)
        gs.append_cylinder(m, options(), transform(z=CONE_TOP - 5.0), SHAFT_OUTER, SHELL_TOP - CONE_TOP + 5.0, 48, 4, True,
                           unreal.GeometryScriptPrimitiveOriginMode.BASE)

    def inner(m):
        gs.append_cone(m, options(), transform(z=-10.0), BASE_INNER + 2.0, SHAFT_INNER, CONE_TOP + 10.0, 48, 4, True,
                       unreal.GeometryScriptPrimitiveOriginMode.BASE)
        gs.append_cylinder(m, options(), transform(z=CONE_TOP - 5.0), SHAFT_INNER, LANTERN_FLOOR - CONE_TOP + 10.0, 48, 4, True,
                           unreal.GeometryScriptPrimitiveOriginMode.BASE)
        gs.append_cylinder(m, options(), transform(z=LANTERN_FLOOR), LANTERN_INNER, SHELL_TOP - 30.0 - LANTERN_FLOOR, 48, 2, True,
                           unreal.GeometryScriptPrimitiveOriginMode.BASE)

    shell = solid(outer)
    unreal.GeometryScript_MeshBooleans.apply_mesh_self_union(shell, unreal.GeometryScriptMeshSelfUnionOptions())
    inside = solid(inner)
    unreal.GeometryScript_MeshBooleans.apply_mesh_self_union(inside, unreal.GeometryScriptMeshSelfUnionOptions())
    apply(shell, inside, unreal.GeometryScriptBooleanOperation.SUBTRACT)
    apply(shell, door_box(), unreal.GeometryScriptBooleanOperation.SUBTRACT)

    # open the lantern wall at lamp height: the beams are unshadowed, so a wall there would be
    # floodlit; the model's own glazing shows instead and LanternGlass keeps the keeper inside
    band = solid(lambda m: gs.append_cylinder(m, options(), transform(z=LANTERN_GLASS_BOTTOM), SHAFT_OUTER + 40.0,
                                              LANTERN_GLASS_TOP - LANTERN_GLASS_BOTTOM, 48, 0, True,
                                              unreal.GeometryScriptPrimitiveOriginMode.BASE))
    apply(shell, band, unreal.GeometryScriptBooleanOperation.SUBTRACT)
    finish(shell, "/Game/Lighthouse/SM_LighthouseShell", "/Game/AbandonedPowerPlant/Assets/Walls/MI_Wall_Painted_Bottom_01")


def build_lantern_glass():
    """Invisible ring that stops the keeper walking out through the lantern windows."""
    glass = solid(lambda m: gs.append_cylinder(m, options(), transform(z=LANTERN_GLASS_BOTTOM - 5.0), LANTERN_INNER + 8.0,
                                               LANTERN_GLASS_TOP - LANTERN_GLASS_BOTTOM + 10.0, 32, 0, True,
                                               unreal.GeometryScriptPrimitiveOriginMode.BASE))
    inner = solid(lambda m: gs.append_cylinder(m, options(), transform(z=LANTERN_GLASS_BOTTOM - 20.0), LANTERN_INNER,
                                               LANTERN_GLASS_TOP - LANTERN_GLASS_BOTTOM + 40.0, 32, 0, True,
                                               unreal.GeometryScriptPrimitiveOriginMode.BASE))
    apply(glass, inner, unreal.GeometryScriptBooleanOperation.SUBTRACT)
    finish(glass, "/Game/Lighthouse/SM_LighthouseLanternGlass", None)


# --- stairs ------------------------------------------------------------------------------------------------------

def stair_count():
    return int(round(LANTERN_FLOOR / STEP_HEIGHT))


def stair_end_angle():
    return STAIR_START_ANGLE + STEP_ANGLE * stair_count()


def build_stairs():
    steps = stair_count()

    def stairs(m):
        gs.append_curved_stairs(m, options(False), transform(yaw=STAIR_START_ANGLE), STAIR_WIDTH, STEP_HEIGHT,
                                STAIR_INNER, STEP_ANGLE * steps, steps, True)
        # centre post
        gs.append_cylinder(m, options(), transform(), STAIR_INNER - 4.0, LANTERN_FLOOR, 16, 0, True,
                           unreal.GeometryScriptPrimitiveOriginMode.BASE)

    finish(solid(stairs), "/Game/Lighthouse/SM_LighthouseStairs", STAIR_MATERIAL, 150.0)


# --- floors -------------------------------------------------------------------------------------------------------

def build_floors():
    end = math.radians(stair_end_angle())

    def floors(m):
        # sits just above the exterior model's red base cap
        gs.append_cylinder(m, options(), transform(z=0.0), BASE_INNER + 5.0, 14.0, 48, 0, True,
                           unreal.GeometryScriptPrimitiveOriginMode.BASE)

    mesh = solid(floors)

    lantern = solid(lambda m: gs.append_cylinder(m, options(), transform(z=LANTERN_FLOOR - 15.0), LANTERN_INNER + 5.0, 15.0,
                                                 48, 0, True, unreal.GeometryScriptPrimitiveOriginMode.BASE))
    # hatch: the keeper (192 cm) needs head room over the last ~10 steps, i.e. the last 140 degrees
    # of the stair; open the floor over the last 200 degrees with a chain of boxes along the arc
    mid_radius = STAIR_INNER + STAIR_WIDTH * 0.5
    end_degrees = math.degrees(end)
    angle = end_degrees - 200.0
    while angle <= end_degrees + 15.0:
        hx, hy = mid_radius * math.cos(math.radians(angle)), mid_radius * math.sin(math.radians(angle))
        hatch = solid(lambda m, x=hx, y=hy, a=angle: gs.append_box(
            m, options(), transform(x, y, LANTERN_FLOOR - 40.0, yaw=a), STAIR_WIDTH + 40.0, 70.0, 80.0, 0, 0, 0,
            unreal.GeometryScriptPrimitiveOriginMode.BASE))
        apply(lantern, hatch, unreal.GeometryScriptBooleanOperation.SUBTRACT)
        angle += 15.0
    unreal.GeometryScript_MeshEdits.append_mesh(mesh, lantern, unreal.Transform())
    finish(mesh, "/Game/Lighthouse/SM_LighthouseFloors", FLOOR_MATERIAL, 250.0)


if __name__ == "__main__":
    build_simple_material(FLOOR_MATERIAL, TILING + "Wood_Planks_Floor_basecolor", TILING + "Wood_Planks_Floor_normal", 0.8, (0.7, 0.62, 0.55))
    build_simple_material(STAIR_MATERIAL, TILING + "Rust_Generic_BaseColor", TILING + "Rust_Generic_Normal", 0.7, (0.6, 0.6, 0.6))
    build_exterior()
    build_shell()
    build_lantern_glass()
    build_stairs()
    build_floors()
    unreal.log("Lighthouse: %d steps, stair ends at %.0f degrees" % (stair_count(), stair_end_angle() % 360.0))
