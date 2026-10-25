# Spawns blueprints in a scratch level and logs their mesh components with relative transforms
# and mesh bounds. Used to learn how a pack's modular blueprints are put together.
#
# Run with:  powershell -File Scripts/run_python.ps1 Scripts/Editor/inspect_blueprints.py

import unreal

BLUEPRINTS = [
    "/Game/Modular_Rural_Cabin/Meshes/Modular/Blueprints/Wall_Door_4m",
    "/Game/Modular_Rural_Cabin/Meshes/Modular/Blueprints/Wall_Door_Window_B_4m",
    "/Game/Modular_Rural_Cabin/Meshes/Modular/Blueprints/Porch_4x4m",
]
MESHES = [
    "/Game/Modular_Rural_Cabin/Meshes/Modular/Door_01",
    "/Game/Modular_Rural_Cabin/Meshes/Modular/Wall_Door_4m",
    "/Game/Military_Radio/StaticMesh/SM_Ampfilter",
    "/Game/Military_Radio/StaticMesh/SM_Radio",
    "/Game/OldFurniturePack_01/Meshes/SM_Table_01",
    "/Game/OldFurniturePack_01/Meshes/SM_WoodenShelf_02",
]

unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).new_level("/Temp/InspectScratch")
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

for path in BLUEPRINTS:
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    actor = actors.spawn_actor_from_class(cls, unreal.Vector(0, 0, 0))
    for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
        mesh = comp.static_mesh
        t = comp.get_relative_transform()
        unreal.log("BP %s :: %s mesh=%s loc=(%.0f %.0f %.0f) rot=(%.0f %.0f %.0f) collision=%s" % (
            path.split("/")[-1], comp.get_name(), mesh.get_name() if mesh else None,
            t.translation.x, t.translation.y, t.translation.z,
            t.rotation.rotator().roll, t.rotation.rotator().pitch, t.rotation.rotator().yaw,
            comp.get_collision_profile_name()))

for path in MESHES:
    mesh = unreal.load_asset(path)
    box = mesh.get_bounding_box()
    unreal.log("MESH %s min(%.0f %.0f %.0f) max(%.0f %.0f %.0f)" % (
        path.split("/")[-1], box.min.x, box.min.y, box.min.z, box.max.x, box.max.y, box.max.z))
