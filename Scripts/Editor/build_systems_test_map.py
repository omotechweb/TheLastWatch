# Builds /Game/Maps/Dev/L_SystemsTest: a two-room greybox for testing interaction,
# notes, inventory, the locked door and the radio.
#
# Run with:  powershell -File Scripts/run_python.ps1 Scripts/Editor/build_systems_test_map.py
# Re-running rebuilds the map from scratch.

import unreal

MAP_PATH = "/Game/Maps/Dev/L_SystemsTest"

level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
cube = unreal.load_asset("/Engine/BasicShapes/Cube")


def block(label, center, size, folder="Greybox"):
    """Static cube spanning `size` (cm) around `center`."""
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(*center))
    actor.static_mesh_component.set_static_mesh(cube)
    actor.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    return actor


def spawn(cls, label, location, yaw=0.0, folder="Interactables", **properties):
    actor = actors.spawn_actor_from_class(cls, unreal.Vector(*location), unreal.Rotator(0.0, 0.0, yaw))
    actor.set_actor_label(label)
    actor.set_folder_path(folder)
    for name, value in properties.items():
        actor.set_editor_property(name, value)
    return actor


def point_light(label, location, lumens, color, radius):
    light = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(*location))
    component = light.point_light_component
    component.set_intensity_units(unreal.LightUnits.LUMENS)
    component.set_intensity(lumens)
    component.set_light_color(color)
    component.set_attenuation_radius(radius)
    light.set_actor_label(label)
    light.set_folder_path("Lighting")
    return light


# --- level -----------------------------------------------------------------

if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
    unreal.EditorAssetLibrary.delete_asset(MAP_PATH)

if not level_editor.new_level(MAP_PATH):
    raise RuntimeError("Could not create " + MAP_PATH)

# --- rooms -----------------------------------------------------------------
# Main room: x -500..500, back (store) room: x 500..900, y -400..400, height 300.
# The store room is behind a locked door at x = 500.

H = 300.0  # ceiling height
T = 20.0   # wall thickness

block("Floor", (200, 0, -10), (1400 + T, 800 + T, 20))
block("Ceiling", (200, 0, H + 10), (1400 + T, 800 + T, 20))
block("Wall_North", (200, 400, H / 2), (1400 + T, T, H))
block("Wall_South", (200, -400, H / 2), (1400 + T, T, H))
block("Wall_West", (-500, 0, H / 2), (T, 800, H))
block("Wall_East", (900, 0, H / 2), (T, 800, H))

# partition with a 110 cm doorway centred on y = 0
block("Partition_A", (500, -227.5, H / 2), (T, 345, H))
block("Partition_B", (500, 227.5, H / 2), (T, 345, H))
block("Partition_Lintel", (500, 0, 255), (T, 110, 90))

# furniture
block("Table", (0, 220, 37.5), (160, 90, 75), folder="Furniture")
block("RadioDesk", (-380, -260, 37.5), (120, 70, 75), folder="Furniture")
block("Shelf", (300, -350, 60), (80, 30, 120), folder="Furniture")
block("Crate_A", (760, 260, 30), (80, 80, 60), folder="Furniture")
block("Crate_B", (800, -250, 30), (80, 80, 60), folder="Furniture")

# --- interactables -----------------------------------------------------------

spawn(unreal.NotePickup, "Note_Elias01", (0, 220, 75.5), yaw=15.0, note_id="elias_01")
spawn(unreal.RadioSet, "Radio", (-380, -260, 75.0), yaw=90.0)
spawn(unreal.ItemPickup, "Key_Storeroom", (300, -350, 121.5), yaw=30.0, item_id="key_storeroom")
spawn(unreal.KeeperDoor, "Door_Storeroom", (500, -55, 0), required_item_id="key_storeroom")
spawn(unreal.NotePickup, "Note_Elias02", (760, 260, 60.5), yaw=-20.0, note_id="elias_02")
spawn(unreal.ItemPickup, "Cassette", (800, -250, 61.5), yaw=10.0, item_id="cassette_01",
      color=unreal.LinearColor(0.08, 0.08, 0.08, 1.0))

# --- lighting ----------------------------------------------------------------

point_light("Light_Main", (0, 100, 260), 160.0, unreal.LinearColor(1.0, 0.72, 0.45, 1.0), 900.0)
point_light("Light_Radio", (-380, -260, 150), 60.0, unreal.LinearColor(1.0, 0.65, 0.4, 1.0), 350.0)
point_light("Light_Store", (700, 0, 260), 25.0, unreal.LinearColor(0.55, 0.65, 1.0, 1.0), 600.0)

fog = actors.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
fog.set_folder_path("Lighting")

# fixed exposure so dark rooms stay dark instead of being brightened by eye adaptation
post = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
post.set_actor_label("PostProcess")
post.set_folder_path("Lighting")
post.set_editor_property("unbound", True)
settings = post.get_editor_property("settings")
settings.set_editor_property("override_auto_exposure_min_brightness", True)
settings.set_editor_property("auto_exposure_min_brightness", 0.0)
settings.set_editor_property("override_auto_exposure_max_brightness", True)
settings.set_editor_property("auto_exposure_max_brightness", 0.0)
settings.set_editor_property("override_vignette_intensity", True)
settings.set_editor_property("vignette_intensity", 0.6)
settings.set_editor_property("override_film_grain_intensity", True)
settings.set_editor_property("film_grain_intensity", 0.25)
post.set_editor_property("settings", settings)

# --- player --------------------------------------------------------------------

start = actors.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(-300, 150, 100))
start.set_actor_label("PlayerStart")

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
world.get_world_settings().set_editor_property("default_game_mode", unreal.LastWatchGameMode.static_class())

if not level_editor.save_current_level():
    raise RuntimeError("Could not save " + MAP_PATH)

unreal.log("Built " + MAP_PATH)
