# Generates the Skerrow Rock terrain mesh (/Game/Island/SM_IslandTerrain) from island_shape.py
# and its slope/path blended ground material (/Game/Island/M_IslandGround).
#
# Run with:  powershell -File Scripts/run_python.ps1 Scripts/Editor/build_island_terrain.py -Editor
# (-Editor because it imports the baked dirt mask texture)

import os
import sys
import unreal

sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Scripts", "Editor"))
import island_shape as island  # noqa: E402

MESH_PATH = "/Game/Island/SM_IslandTerrain"
MATERIAL_PATH = "/Game/Island/M_IslandGround"
MASK_PATH = "/Game/Island/T_DirtMask"
SIZE = 26000.0
STEPS = 180

assets = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
gs_list = unreal.GeometryScript_List


def connect(a, output, b, input_name):
    """Material connection that fails loudly instead of leaving a pin unplugged"""
    if not mel.connect_material_expressions(a, output, b, input_name):
        raise RuntimeError("Could not connect %s.%s -> %s.%s" % (a.get_class().get_name(), output, b.get_class().get_name(), input_name))


def set_mask(node, channels):
    """ComponentMask with exactly the given channels; the defaults are not all off"""
    for channel in "rgba":
        node.set_editor_property(channel, channel in channels)


def first(value):
    """GeometryScript python bindings return tuples for out params"""
    return value[0] if isinstance(value, tuple) else value


# --- dirt mask ----------------------------------------------------------------------------

def build_dirt_mask(resolution=512):
    """Bakes island.dirt() over the terrain square into a greyscale texture."""
    import struct
    import zlib

    rows = []
    for i in range(resolution):
        y = -SIZE / 2.0 + (i + 0.5) * SIZE / resolution
        row = bytearray([0])  # PNG filter type: none
        for j in range(resolution):
            x = -SIZE / 2.0 + (j + 0.5) * SIZE / resolution
            row.append(int(max(0.0, min(1.0, island.dirt(x, y))) * 255))
        rows.append(bytes(row))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", resolution, resolution, 8, 0, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(b"".join(rows), 9))
           + chunk(b"IEND", b""))
    filename = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "T_DirtMask.png")
    with open(filename, "wb") as f:
        f.write(png)

    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = "/Game/Island"
    task.destination_name = "T_DirtMask"
    task.automated = True
    task.replace_existing = True
    task.save = True
    tools.import_asset_tasks([task])
    texture = unreal.load_asset(MASK_PATH)
    texture.set_editor_property("srgb", False)
    texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
    texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
    texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
    assets.save_loaded_asset(texture)


# --- material ---------------------------------------------------------------------------

def build_ground_material():
    if assets.does_asset_exist(MATERIAL_PATH):
        assets.delete_asset(MATERIAL_PATH)
    material = tools.create_asset("M_IslandGround", "/Game/Island", unreal.Material, unreal.MaterialFactoryNew())

    def node(cls, x, y):
        return mel.create_material_expression(material, cls, x, y)

    def world_uv(tile, x, y):
        position = node(unreal.MaterialExpressionWorldPosition, x - 400, y)
        mask = node(unreal.MaterialExpressionComponentMask, x - 250, y)
        set_mask(mask, "rg")
        connect(position, "", mask, "")
        divide = node(unreal.MaterialExpressionDivide, x - 100, y)
        divide.set_editor_property("const_b", tile)
        connect(mask, "", divide, "A")
        return divide

    def sample(path, uv, x, y, normal=False):
        expression = node(unreal.MaterialExpressionTextureSample, x, y)
        expression.set_editor_property("texture", unreal.load_asset(path))
        expression.set_editor_property("sampler_type",
                                       unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL if normal
                                       else unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        connect(uv, "", expression, "UVs")
        return expression

    def blend(a, b, alpha, x, y, alpha_output=""):
        lerp = node(unreal.MaterialExpressionLinearInterpolate, x, y)
        connect(a, "RGB", lerp, "A")
        connect(b, "RGB", lerp, "B")
        connect(alpha, alpha_output, lerp, "Alpha")
        return lerp

    tex = "/Game/Modular_Rural_Cabin/Textures/Tiling/"
    grass_uv = world_uv(350.0, -1600, -600)
    dirt_uv = world_uv(300.0, -1600, 0)
    rock_uv = world_uv(900.0, -1600, 600)

    grass_c = sample(tex + "Grass_Basecolor", grass_uv, -1200, -800)
    grass_n = sample(tex + "Grass_Normal", grass_uv, -1200, -550, True)
    dirt_c = sample(tex + "Ground_Dirt_Basecolor", dirt_uv, -1200, -150)
    dirt_n = sample(tex + "Ground_Dirt_Normal", dirt_uv, -1200, 100, True)
    rock_c = sample("/Game/Imported/Cliffs/Textures/T_NordicCliff_BaseColor", rock_uv, -1200, 450)
    rock_n = sample("/Game/Imported/Cliffs/Textures/T_NordicCliff_Normal", rock_uv, -1200, 700, True)

    # dirt amount comes from the baked mask, mapped over the whole terrain square
    position = node(unreal.MaterialExpressionWorldPosition, -1900, -350)
    position_xy = node(unreal.MaterialExpressionComponentMask, -1750, -350)
    set_mask(position_xy, "rg")
    connect(position, "", position_xy, "")
    shift = node(unreal.MaterialExpressionAdd, -1600, -350)
    shift.set_editor_property("const_b", SIZE / 2.0)
    connect(position_xy, "", shift, "A")
    mask_uv = node(unreal.MaterialExpressionDivide, -1450, -350)
    mask_uv.set_editor_property("const_b", SIZE)
    connect(shift, "", mask_uv, "A")
    dirt_mask = node(unreal.MaterialExpressionTextureSample, -1200, -350)
    dirt_mask.set_editor_property("texture", unreal.load_asset(MASK_PATH))
    dirt_mask.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    connect(mask_uv, "", dirt_mask, "UVs")
    vertex_color = dirt_mask

    # rock on steep slopes: saturate((1 - normal.z - 0.22) * 5)
    normal_ws = node(unreal.MaterialExpressionVertexNormalWS, -1200, 950)
    normal_z = node(unreal.MaterialExpressionComponentMask, -1050, 950)
    set_mask(normal_z, "b")
    connect(normal_ws, "", normal_z, "")
    one_minus = node(unreal.MaterialExpressionOneMinus, -900, 950)
    connect(normal_z, "", one_minus, "")
    offset = node(unreal.MaterialExpressionSubtract, -780, 950)
    offset.set_editor_property("const_b", 0.22)
    connect(one_minus, "", offset, "A")
    sharpen = node(unreal.MaterialExpressionMultiply, -660, 950)
    sharpen.set_editor_property("const_b", 5.0)
    connect(offset, "", sharpen, "A")
    slope = node(unreal.MaterialExpressionSaturate, -540, 950)
    connect(sharpen, "", slope, "")

    color = blend(grass_c, dirt_c, vertex_color, -600, -500, "R")
    color_lerp = node(unreal.MaterialExpressionLinearInterpolate, -350, -300)
    connect(color, "", color_lerp, "A")
    connect(rock_c, "RGB", color_lerp, "B")
    connect(slope, "", color_lerp, "Alpha")

    # damp, dark Atlantic ground
    darken = node(unreal.MaterialExpressionMultiply, -150, -300)
    darken.set_editor_property("const_b", 0.65)
    connect(color_lerp, "", darken, "A")

    normal = blend(grass_n, dirt_n, vertex_color, -600, 200, "R")
    normal_lerp = node(unreal.MaterialExpressionLinearInterpolate, -350, 300)
    connect(normal, "", normal_lerp, "A")
    connect(rock_n, "RGB", normal_lerp, "B")
    connect(slope, "", normal_lerp, "Alpha")

    # the scanned ground normals are strong enough to read as shards at night; soften them
    flat = node(unreal.MaterialExpressionConstant3Vector, -350, 500)
    flat.set_editor_property("constant", unreal.LinearColor(0, 0, 1, 1))
    soft_normal = node(unreal.MaterialExpressionLinearInterpolate, -150, 300)
    soft_normal.set_editor_property("const_alpha", 0.55)
    connect(normal_lerp, "", soft_normal, "A")
    connect(flat, "", soft_normal, "B")

    roughness = node(unreal.MaterialExpressionConstant, -150, 500)
    roughness.set_editor_property("r", 0.88)

    debug = {"mask": dirt_mask, "mix": color, "rocked": color_lerp}.get(os.environ.get("TLW_GROUND_DEBUG", ""))
    mel.connect_material_property(debug or darken, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(soft_normal, "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)

    mel.recompile_material(material)
    assets.save_loaded_asset(material)
    return material


def build_ocean_material():
    """M_Ocean: opaque, near-black glossy sea with two slowly panning wave normal layers."""
    path = "/Game/Island/M_Ocean"
    if assets.does_asset_exist(path):
        assets.delete_asset(path)
    material = tools.create_asset("M_Ocean", "/Game/Island", unreal.Material, unreal.MaterialFactoryNew())
    normal_tex = unreal.load_asset("/Game/Modular_Rural_Cabin/Textures/Tiling/Water_normal")

    def node(cls, x, y):
        return mel.create_material_expression(material, cls, x, y)

    position = node(unreal.MaterialExpressionWorldPosition, -1400, 0)
    mask = node(unreal.MaterialExpressionComponentMask, -1250, 0)
    set_mask(mask, "rg")
    connect(position, "", mask, "")

    layers = []
    for i, (tile, speed_x, speed_y) in enumerate([(1400.0, 0.012, 0.006), (3100.0, -0.007, 0.01)]):
        divide = node(unreal.MaterialExpressionDivide, -1100, i * 300)
        divide.set_editor_property("const_b", tile)
        connect(mask, "", divide, "A")
        panner = node(unreal.MaterialExpressionPanner, -900, i * 300)
        panner.set_editor_property("speed_x", speed_x)
        panner.set_editor_property("speed_y", speed_y)
        connect(divide, "", panner, "Coordinate")
        sample = node(unreal.MaterialExpressionTextureSample, -650, i * 300)
        sample.set_editor_property("texture", normal_tex)
        sample.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        connect(panner, "", sample, "UVs")
        layers.append(sample)

    add = node(unreal.MaterialExpressionAdd, -400, 150)
    connect(layers[0], "RGB", add, "A")
    connect(layers[1], "RGB", add, "B")
    normalize = node(unreal.MaterialExpressionNormalize, -250, 150)
    connect(add, "", normalize, "")

    color = node(unreal.MaterialExpressionConstant3Vector, -250, -200)
    color.set_editor_property("constant", unreal.LinearColor(0.006, 0.011, 0.016, 1.0))
    roughness = node(unreal.MaterialExpressionConstant, -250, 350)
    roughness.set_editor_property("r", 0.07)

    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(normalize, "", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(roughness, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(material)
    assets.save_loaded_asset(material)


# --- mesh -----------------------------------------------------------------------------------

def build_terrain_mesh(material):
    mesh = unreal.DynamicMesh()
    # one polygroup: a group per quad turns into hard edges, i.e. faceted shading
    primitive = unreal.GeometryScriptPrimitiveOptions()
    primitive.set_editor_property("polygroup_mode", unreal.GeometryScriptPrimitivePolygroupMode.SINGLE_GROUP)
    unreal.GeometryScript_Primitives.append_rectangle_xy(
        mesh, primitive, unreal.Transform(), SIZE, SIZE, STEPS, STEPS)

    positions = gs_list.convert_vector_list_to_array(
        first(unreal.GeometryScript_MeshQueries.get_all_vertex_positions(mesh, False)[1:]))
    positions = first(positions)
    unreal.log("Terrain grid: %d vertices" % len(positions))

    offsets = [unreal.Vector(0.0, 0.0, island.height(p.x, p.y) - p.z) for p in positions]
    unreal.GeometryScript_MeshDeformers.apply_displace_from_per_vertex_vectors(
        mesh, unreal.GeometryScriptMeshSelection(), first(gs_list.convert_array_to_vector_list(offsets)), 1.0)
    # smooth shared normals; per-triangle normals made the ground look faceted
    unreal.GeometryScript_Normals.set_per_vertex_normals(mesh)

    if assets.does_asset_exist(MESH_PATH):
        assets.delete_asset(MESH_PATH)
    options = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    options.set_editor_property("enable_recompute_normals", False)
    options.set_editor_property("enable_collision", True)
    options.set_editor_property("enable_nanite", False)
    static_mesh = first(unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(
        mesh, MESH_PATH, options))
    if not static_mesh:
        raise RuntimeError("Could not create " + MESH_PATH)

    static_mesh.set_material(0, material)
    body = static_mesh.get_editor_property("body_setup")
    body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    assets.save_loaded_asset(static_mesh)

    box = static_mesh.get_bounding_box()
    unreal.log("Built %s z %.0f..%.0f" % (MESH_PATH, box.min.z, box.max.z))


build_dirt_mask()
build_ocean_material()
build_terrain_mesh(build_ground_material())
for name in island.PADS:
    unreal.log("Pad %s height %.0f" % (name, island.pad_height(name)))
