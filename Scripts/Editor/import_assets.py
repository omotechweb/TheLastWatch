# Imports the raw downloads in Imports/unpacked into /Game/Imported and builds their materials.
#   - Megascans "Huge Nordic Coastal Cliff" (FBX + 4K maps)  -> /Game/Imported/Cliffs
#   - "Lighthouse" by DylanHayes, CC-BY (FBX + textures)     -> /Game/Imported/Lighthouse
# Also creates the shared master materials in /Game/Materials.
#
# Run with:  powershell -File Scripts/run_python.ps1 Scripts/Editor/import_assets.py
# Safe to re-run; existing assets are replaced.

import os
import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
UNPACKED = os.path.join(PROJECT, "Imports", "unpacked")

assets = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def import_file(filename, destination, name=None, options=None):
    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = destination
    if name:
        task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = True
    if options:
        task.options = options
    tools.import_asset_tasks([task])
    paths = list(task.imported_object_paths)
    if not paths:
        raise RuntimeError("Import failed: " + filename)
    return paths


def fbx_options(materials):
    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_animations = False
    options.import_materials = materials
    options.import_textures = materials
    options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    data = options.static_mesh_import_data
    data.combine_meshes = True
    data.generate_lightmap_u_vs = False
    data.auto_generate_collision = True
    data.build_nanite = True
    return options


def import_texture(filename, destination, name, kind):
    """kind: 'color', 'normal' or 'mask' (linear single channel)"""
    path = import_file(filename, destination, name)[0]
    texture = unreal.load_asset(path)
    if kind == "normal":
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        texture.set_editor_property("srgb", False)
    elif kind == "mask":
        texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
        texture.set_editor_property("srgb", False)
    assets.save_loaded_asset(texture)
    return texture


def recreate(path, asset_class, factory):
    if assets.does_asset_exist(path):
        assets.delete_asset(path)
    folder, name = path.rsplit("/", 1)
    return tools.create_asset(name, folder, asset_class, factory)


def texture_param(material, name, texture, sampler, x, y, uv=None):
    node = mel.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("texture", texture)
    node.set_editor_property("sampler_type", sampler)
    if uv:
        mel.connect_material_expressions(uv, "", node, "UVs")
    return node


# --- master material: scanned PBR assets ------------------------------------------

def build_scan_master(defaults):
    """M_ScanPBR: BaseColor / Normal / Roughness / AO texture parameters, UV tiling."""
    material = recreate("/Game/Materials/M_ScanPBR", unreal.Material, unreal.MaterialFactoryNew())

    coords = mel.create_material_expression(material, unreal.MaterialExpressionTextureCoordinate, -900, 0)
    tiling = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -900, 120)
    tiling.set_editor_property("parameter_name", "Tiling")
    tiling.set_editor_property("default_value", 1.0)
    uv = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -700, 40)
    mel.connect_material_expressions(coords, "", uv, "A")
    mel.connect_material_expressions(tiling, "", uv, "B")

    color = texture_param(material, "BaseColor", defaults["color"], unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, -450, -300, uv)
    normal = texture_param(material, "Normal", defaults["normal"], unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, -450, 0, uv)
    rough = texture_param(material, "Roughness", defaults["rough"], unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -450, 300, uv)
    ao = texture_param(material, "AO", defaults["ao"], unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, -450, 600, uv)

    # darken/tint for wet night rock without touching the textures
    tint = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -450, -500)
    tint.set_editor_property("parameter_name", "Tint")
    tint.set_editor_property("default_value", unreal.LinearColor(1, 1, 1, 1))
    tinted = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -150, -350)
    mel.connect_material_expressions(color, "RGB", tinted, "A")
    mel.connect_material_expressions(tint, "", tinted, "B")

    mel.connect_material_property(tinted, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(normal, "RGB", unreal.MaterialProperty.MP_NORMAL)
    mel.connect_material_property(rough, "R", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(ao, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)

    material.set_editor_property("used_with_nanite", True)
    mel.recompile_material(material)
    assets.save_loaded_asset(material)
    return material


def make_instance(path, parent, textures, tiling=1.0, tint=None):
    instance = recreate(path, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(instance, parent)
    for name, texture in textures.items():
        mel.set_material_instance_texture_parameter_value(instance, name, texture)
    mel.set_material_instance_scalar_parameter_value(instance, "Tiling", tiling)
    if tint:
        mel.set_material_instance_vector_parameter_value(instance, "Tint", tint)
    assets.save_loaded_asset(instance)
    return instance


# --- cliff ---------------------------------------------------------------------------

cliff_dir = os.path.join(UNPACKED, "cliff")
prefix = os.path.join(cliff_dir, "Huge_Nordic_Coastal_Cliff_ullqbb1ga_High_4K_")
tex_dest = "/Game/Imported/Cliffs/Textures"
cliff_tex = {
    "color": import_texture(prefix + "BaseColor.jpg", tex_dest, "T_NordicCliff_BaseColor", "color"),
    "normal": import_texture(prefix + "Normal.jpg", tex_dest, "T_NordicCliff_Normal", "normal"),
    "rough": import_texture(prefix + "Roughness.jpg", tex_dest, "T_NordicCliff_Roughness", "mask"),
    "ao": import_texture(prefix + "AO.jpg", tex_dest, "T_NordicCliff_AO", "mask"),
}

scan_master = build_scan_master(cliff_tex)
cliff_mi = make_instance("/Game/Imported/Cliffs/MI_NordicCliff", scan_master, {
    "BaseColor": cliff_tex["color"], "Normal": cliff_tex["normal"],
    "Roughness": cliff_tex["rough"], "AO": cliff_tex["ao"],
}, tint=unreal.LinearColor(0.75, 0.75, 0.78, 1))

cliff_mesh_path = import_file(os.path.join(cliff_dir, "Huge_Nordic_Coastal_Cliff_ullqbb1ga_High.fbx"),
                              "/Game/Imported/Cliffs", "SM_HugeNordicCliff", fbx_options(False))[0]
cliff_mesh = unreal.load_asset(cliff_mesh_path)
cliff_mesh.set_material(0, cliff_mi)
assets.save_loaded_asset(cliff_mesh)

# --- lighthouse ----------------------------------------------------------------------

lighthouse_paths = import_file(os.path.join(UNPACKED, "lighthouse", "src", "Lighthouse", "Ligthouse.fbx"),
                               "/Game/Imported/Lighthouse", "SM_Lighthouse", fbx_options(True))

# --- report ----------------------------------------------------------------------------

for path in [cliff_mesh_path] + lighthouse_paths:
    asset = unreal.load_asset(path)
    if isinstance(asset, unreal.StaticMesh):
        box = asset.get_bounding_box()
        unreal.log("MESH %s min(%.0f %.0f %.0f) max(%.0f %.0f %.0f) materials=%s" % (
            path, box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z,
            [m.get_editor_property("material_interface").get_name() if m.get_editor_property("material_interface") else None
             for m in asset.get_editor_property("static_materials")]))
    else:
        unreal.log("ASSET %s (%s)" % (path, asset.get_class().get_name()))
