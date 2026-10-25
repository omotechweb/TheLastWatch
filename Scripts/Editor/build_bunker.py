# Builds the VESPER listening station under the north ruin (/Game/Bunker):
#   SM_BunkerShell - concrete rooms (walls + ceiling), cut as one solid so doorways line up
#   SM_BunkerFloor - floor slabs with their own material
# Frame: origin = floor of the entry room, under the hatch; +X runs down the corridor.
# The map places it high above the island (out of the height fog) and the hatch teleports there.
#
# Run with:  powershell -File Scripts/run_python.ps1 Scripts/Editor/build_bunker.py

import unreal

assets = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
gs = unreal.GeometryScript_Primitives

WALL = 30.0

# rooms: name -> (min x, min y, max x, max y, height)
ROOMS = {
    "entry": (-300.0, -300.0, 300.0, 300.0, 330.0),
    "corridor": (280.0, -130.0, 1320.0, 130.0, 280.0),
    "listening": (1300.0, -480.0, 2350.0, 480.0, 360.0),
    "records": (1850.0, 470.0, 2350.0, 1080.0, 300.0),
}
# extra openings between rooms that only touch: (min x, min y, max x, max y, height)
DOORWAYS = [
    (1980.0, 440.0, 2130.0, 510.0, 230.0),
]


def first(value):
    return value[0] if isinstance(value, tuple) else value


def options():
    o = unreal.GeometryScriptPrimitiveOptions()
    o.set_editor_property("polygroup_mode", unreal.GeometryScriptPrimitivePolygroupMode.SINGLE_GROUP)
    return o


def box(mesh, x0, y0, z0, x1, y1, z1):
    t = unreal.Transform(unreal.Vector((x0 + x1) / 2.0, (y0 + y1) / 2.0, z0), unreal.Rotator(0, 0, 0), unreal.Vector(1, 1, 1))
    gs.append_box(mesh, options(), t, x1 - x0, y1 - y0, z1 - z0, 0, 0, 0, unreal.GeometryScriptPrimitiveOriginMode.BASE)


def save(mesh, path, material_path, uv_size):
    unreal.GeometryScript_UVs.set_mesh_u_vs_from_box_projection(
        mesh, 0, unreal.Transform(unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0), unreal.Vector(uv_size, uv_size, uv_size)),
        unreal.GeometryScriptMeshSelection())
    unreal.GeometryScript_Normals.compute_split_normals(
        mesh, unreal.GeometryScriptSplitNormalsOptions(), unreal.GeometryScriptCalculateNormalsOptions())
    if assets.does_asset_exist(path):
        assets.delete_asset(path)
    create = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    create.set_editor_property("enable_recompute_normals", False)
    create.set_editor_property("enable_collision", True)
    create.set_editor_property("enable_nanite", False)
    static_mesh = first(unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(mesh, path, create))
    static_mesh.set_material(0, unreal.load_asset(material_path))
    static_mesh.get_editor_property("body_setup").set_editor_property(
        "collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    assets.save_loaded_asset(static_mesh)
    unreal.log("Built " + path)


def floor_material():
    """Plain concrete floor from the power plant pack's textures (its own instances expect
    vertex colour masks that generated meshes don't have)."""
    path = "/Game/Bunker/M_BunkerFloor"
    if assets.does_asset_exist(path):
        assets.delete_asset(path)
    material = tools.create_asset("M_BunkerFloor", "/Game/Bunker", unreal.Material, unreal.MaterialFactoryNew())
    tex = "/Game/AbandonedPowerPlant/Assets/Floor/Textures/"
    color = mel.create_material_expression(material, unreal.MaterialExpressionTextureSample, -400, -100)
    color.set_editor_property("texture", unreal.load_asset(tex + "T_Floor_01_Ground_Albedo"))
    normal = mel.create_material_expression(material, unreal.MaterialExpressionTextureSample, -400, 150)
    normal.set_editor_property("texture", unreal.load_asset(tex + "T_Floor_01_Ground_Normal"))
    normal.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    rough = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -200, 350)
    rough.set_editor_property("r", 0.85)
    mel.connect_material_property(color, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(material)
    assets.save_loaded_asset(material)
    return path


def build():
    rooms = list(ROOMS.values())
    lo_x = min(r[0] for r in rooms) - WALL
    lo_y = min(r[1] for r in rooms) - WALL
    hi_x = max(r[2] for r in rooms) + WALL
    hi_y = max(r[3] for r in rooms) + WALL
    hi_z = max(r[4] for r in rooms) + WALL

    shell = unreal.DynamicMesh()
    box(shell, lo_x, lo_y, -WALL, hi_x, hi_y, hi_z)

    hollow = unreal.DynamicMesh()
    for x0, y0, x1, y1, h in rooms + DOORWAYS:
        box(hollow, x0, y0, 0.0, x1, y1, h)
    unreal.GeometryScript_MeshBooleans.apply_mesh_self_union(hollow, unreal.GeometryScriptMeshSelfUnionOptions())
    unreal.GeometryScript_MeshBooleans.apply_mesh_boolean(
        shell, unreal.Transform(), hollow, unreal.Transform(), unreal.GeometryScriptBooleanOperation.SUBTRACT,
        unreal.GeometryScriptMeshBooleanOptions())
    save(shell, "/Game/Bunker/SM_BunkerShell", "/Game/AbandonedPowerPlant/Assets/Walls/MI_Wall_Painted_Bottom_01", 300.0)

    floor = unreal.DynamicMesh()
    for x0, y0, x1, y1, _ in rooms:
        box(floor, x0, y0, -2.0, x1, y1, 1.0)
    unreal.GeometryScript_MeshBooleans.apply_mesh_self_union(floor, unreal.GeometryScriptMeshSelfUnionOptions())
    save(floor, "/Game/Bunker/SM_BunkerFloor", floor_material(), 250.0)


if __name__ == "__main__":
    build()
