# Materials for the runtime effects spawned by AKeeperDirector:
#   /Game/Effects/M_Watcher     - the figure in the fog: near-black, wet sheen, dithered "Fade" (0..1)
#   /Game/Effects/M_RainStreak  - faint translucent rain streaks
#
# Run with:  powershell -File Scripts/run_python.ps1 Scripts/Editor/build_effects.py

import unreal

FOLDER = "/Game/Effects"

assets = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def connect(a, output, b, input_name):
    """Material connection that fails loudly instead of leaving a pin unplugged"""
    if not mel.connect_material_expressions(a, output, b, input_name):
        raise RuntimeError("Could not connect %s.%s -> %s.%s" % (a.get_class().get_name(), output, b.get_class().get_name(), input_name))


def new_material(name):
    path = FOLDER + "/" + name
    if assets.does_asset_exist(path):
        assets.delete_asset(path)
    return tools.create_asset(name, FOLDER, unreal.Material, unreal.MaterialFactoryNew())


def constant(material, value, x, y):
    node = mel.create_material_expression(material, unreal.MaterialExpressionConstant, x, y)
    node.set_editor_property("r", value)
    return node


def build_watcher():
    """Masked so it can dissolve: opacity mask = DitherTemporalAA(Fade)"""
    material = new_material("M_Watcher")
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
    material.set_editor_property("two_sided", True)

    color = mel.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -400, -200)
    color.set_editor_property("constant", unreal.LinearColor(0.004, 0.005, 0.006, 1.0))

    fade = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -600, 200)
    fade.set_editor_property("parameter_name", "Fade")
    fade.set_editor_property("default_value", 1.0)
    dither = mel.create_material_expression(material, unreal.MaterialExpressionMaterialFunctionCall, -400, 200)
    dither.set_editor_property("material_function", unreal.load_asset(
        "/Engine/Functions/Engine_MaterialFunctions02/Utility/DitherTemporalAA"))
    connect(fade, "", dither, "Alpha Threshold")

    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(constant(material, 0.3, -400, 0), "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(constant(material, 0.6, -400, 80), "", unreal.MaterialProperty.MP_SPECULAR)
    mel.connect_material_property(dither, "", unreal.MaterialProperty.MP_OPACITY_MASK)
    mel.recompile_material(material)
    assets.save_loaded_asset(material)


def build_rain():
    material = new_material("M_RainStreak")
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)

    color = mel.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -400, -100)
    color.set_editor_property("constant", unreal.LinearColor(0.32, 0.36, 0.42, 1.0))

    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(constant(material, 0.3, -400, 100), "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(material)
    assets.save_loaded_asset(material)


build_watcher()
build_rain()
unreal.log("Built effect materials in " + FOLDER)
