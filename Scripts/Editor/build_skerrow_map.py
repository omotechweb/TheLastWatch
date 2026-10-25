# Builds /Game/Maps/L_Skerrow: Skerrow Rock at night.
# Terrain from build_island_terrain.py, cliffs, sea, lighthouse with its beam, the keeper's
# cottage and the radio shed (copied from the cabin pack's showcase map), the pier,
# furniture, interactables, sky, fog and ambient sound.
#
# Run with:  powershell -File Scripts/run_python.ps1 Scripts/Editor/build_skerrow_map.py
# Run build_island_terrain.py first. Re-running rebuilds the map from scratch.

import math
import os
import random
import sys
import unreal

sys.path.insert(0, os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Scripts", "Editor"))
import island_shape as island  # noqa: E402

MAP_PATH = "/Game/Maps/L_Skerrow"
SHOWCASE = "/Game/Modular_Rural_Cabin/Maps/Modular_Cabins_Showcase"

level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
rng = random.Random(island.SEED)


def load(path):
    asset = unreal.load_asset(path)
    if not asset:
        raise RuntimeError("Missing asset " + path)
    return asset


def V(x, y, z):
    return unreal.Vector(float(x), float(y), float(z))


def R(yaw, pitch=0.0, roll=0.0):
    return unreal.Rotator(roll, pitch, yaw)


def place_mesh(mesh_path, location, yaw=0.0, scale=1.0, folder="Props", label=None, pitch=0.0, roll=0.0):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, location, R(yaw, pitch, roll))
    actor.static_mesh_component.set_static_mesh(load(mesh_path))
    if not isinstance(scale, (tuple, list)):
        scale = (scale, scale, scale)
    actor.set_actor_scale3d(V(*scale))
    actor.set_folder_path(folder)
    if label:
        actor.set_actor_label(label)
    return actor


def mesh_bounds(mesh_path):
    box = load(mesh_path).get_bounding_box()
    return box.min, box.max


def rotate(x, y, yaw_degrees):
    c, s = math.cos(math.radians(yaw_degrees)), math.sin(math.radians(yaw_degrees))
    return x * c - y * s, x * s + y * c


# --- read the showcase cabins before switching level -----------------------------------------

def capture_cluster(center, radius):
    """Class, transform and mesh of every showcase actor within radius of center (XY)."""
    captured = []
    for actor in actors.get_all_level_actors():
        loc = actor.get_actor_location()
        if math.hypot(loc.x - center[0], loc.y - center[1]) > radius:
            continue
        cls = actor.get_class()
        if cls.get_name() in ("BP_Sky_Sphere_C",) or "ShowcaseFloor" in actor.get_actor_label():
            continue
        mesh = None
        if isinstance(actor, unreal.StaticMeshActor):
            sm = actor.static_mesh_component.static_mesh
            mesh = sm.get_path_name() if sm else None
            if mesh and "ShowcaseFloor" in mesh:
                continue
        rot = actor.get_actor_rotation()
        captured.append({
            "class": cls, "mesh": mesh,
            "x": loc.x - center[0], "y": loc.y - center[1], "z": loc.z,
            "yaw": rot.yaw, "pitch": rot.pitch, "roll": rot.roll,
            "scale": actor.get_actor_scale3d(),
        })
    return captured


if not level_editor.load_level(SHOWCASE):
    raise RuntimeError("Could not load " + SHOWCASE)

# 4x8 m cabin with a veranda (keeper's cottage) and 4x4 m cabin (radio shed);
# their long axis in the showcase points along yaw -80 and +50 respectively
COTTAGE_SRC = ((-2662.0, -1238.0), 560.0, -80.0)
SHED_SRC = ((-1030.0, -1223.0), 380.0, 50.0)
cottage_parts = capture_cluster(*COTTAGE_SRC[:2])
shed_parts = capture_cluster(*SHED_SRC[:2])
unreal.log("Captured cottage %d parts, shed %d parts" % (len(cottage_parts), len(shed_parts)))

# --- new level ----------------------------------------------------------------------------------

if unreal.EditorAssetLibrary.does_asset_exist(MAP_PATH):
    unreal.EditorAssetLibrary.delete_asset(MAP_PATH)
if not level_editor.new_level(MAP_PATH):
    raise RuntimeError("Could not create " + MAP_PATH)

world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def ground(x, y):
    return island.height(x, y)


def trace_down(x, y, top=6000.0, ignore=None):
    """Highest blocking surface under (x, y) in the level being built"""
    hit = unreal.SystemLibrary.line_trace_single(
        world, V(x, y, top), V(x, y, -3000), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False,
        ignore or [], unreal.DrawDebugTrace.NONE, True)
    if hit is None:
        return None
    result = hit.to_tuple()
    blocking, location = result[0], result[4]
    return location.z if blocking else None


# --- terrain, sea, cliffs ----------------------------------------------------------------------------

terrain = place_mesh("/Game/Island/SM_IslandTerrain", V(0, 0, 0), folder="Island", label="Terrain")

sea = place_mesh("/Engine/BasicShapes/Plane", V(0, 0, 0), scale=(3000, 3000, 1), folder="Island", label="Sea")
sea.static_mesh_component.set_material(0, load("/Game/Island/M_Ocean"))
sea.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)

CLIFF = "/Game/Imported/Cliffs/SM_HugeNordicCliff"
for ring, (r, z, scale_range, step) in enumerate([(0.98, -350.0, (2.0, 2.7), 9.0), (1.04, -700.0, (2.4, 3.2), 14.0)]):
    angle = rng.uniform(0, step)
    while angle < 360.0:
        x, y = island.coast_point(angle, r)
        if island.landing_weight(x, y) < 0.15:
            yaw = math.degrees(math.atan2(y, x)) + rng.uniform(-25, 25)  # long side along the coast
            place_mesh(CLIFF, V(x, y, z + rng.uniform(-150, 150)), yaw=yaw, scale=rng.uniform(*scale_range),
                       folder="Island/Cliffs")
        angle += step * rng.uniform(0.8, 1.2)

# a few sea stacks off the coast
for angle in (20.0, 75.0, 140.0, 300.0, 335.0):
    x, y = island.coast_point(angle, 1.18)
    place_mesh(CLIFF, V(x, y, -1100), yaw=rng.uniform(0, 360), scale=rng.uniform(1.6, 2.4),
               folder="Island/Cliffs", pitch=rng.uniform(-8, 8))

# --- lighthouse ------------------------------------------------------------------------------------------

LIGHTHOUSE_SCALE = 2.2
lx, ly, _ = island.PADS["lighthouse"]
lz = island.pad_height("lighthouse")
lh_min, lh_max = mesh_bounds("/Game/Imported/Lighthouse/SM_Lighthouse")
lighthouse = place_mesh("/Game/Imported/Lighthouse/SM_Lighthouse", V(lx, ly, lz - lh_min.z * LIGHTHOUSE_SCALE - 20),
                        yaw=200.0, scale=LIGHTHOUSE_SCALE, folder="Lighthouse", label="Lighthouse")
# the lamp sits in the glazed lantern near the top of the model
lamp_z = lz + (lh_max.z - lh_min.z) * LIGHTHOUSE_SCALE * 0.86
beam = actors.spawn_actor_from_class(unreal.LighthouseBeam, V(lx, ly, lamp_z))
beam.set_actor_label("LighthouseBeam")
beam.set_folder_path("Lighthouse")

# --- buildings ------------------------------------------------------------------------------------------------


def build_from_parts(parts, pad, extra_yaw, folder):
    """Recreates captured showcase actors around a pad; returns spawned actors."""
    px, py, _ = island.PADS[pad]
    pz = island.pad_height(pad)
    spawned = []
    for part in parts:
        x, y = rotate(part["x"], part["y"], extra_yaw)
        location = V(px + x, py + y, pz + part["z"])
        rotation = R(part["yaw"] + extra_yaw, part["pitch"], part["roll"])
        if part["mesh"]:
            actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, location, rotation)
            actor.static_mesh_component.set_static_mesh(load(part["mesh"].split(".")[0]))
        else:
            actor = actors.spawn_actor_from_class(part["class"], location, rotation)
        if actor:
            actor.set_actor_scale3d(part["scale"])
            actor.set_folder_path(folder)
            spawned.append(actor)
    return spawned


def replace_doors(spawned, pad, folder, required_item=""):
    """Swaps the cabin pack's fixed doors for our openable KeeperDoor."""
    px, py, _ = island.PADS[pad]
    door_mesh = load("/Game/Modular_Rural_Cabin/Meshes/Modular/Door_01")
    count = 0
    for actor in spawned:
        for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
            if comp.static_mesh != door_mesh:
                continue
            location = comp.get_world_location()
            rotation = comp.get_world_rotation()
            comp.set_visibility(False)
            comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)

            door = actors.spawn_actor_from_class(unreal.KeeperDoor, location, rotation)
            door.set_folder_path(folder)
            mesh = door.get_component_by_class(unreal.StaticMeshComponent)
            mesh.set_static_mesh(door_mesh)
            mesh.set_relative_location(V(0, 0, 0), False, False)
            mesh.set_relative_scale3d(V(1, 1, 1))
            mesh.set_material(0, door_mesh.get_material(0))
            door.set_editor_property("tint_placeholder", False)
            if required_item:
                door.set_editor_property("required_item_id", required_item)

            # swing inwards: pick the direction that moves the panel towards the cabin centre
            yaw = rotation.yaw
            best = None
            for open_yaw in (100.0, -100.0):
                mx, my = rotate(0.0, -48.0, yaw + open_yaw)
                d = math.hypot(location.x + mx - px, location.y + my - py)
                if best is None or d < best[0]:
                    best = (d, open_yaw)
            door.set_editor_property("open_yaw", best[1])
            count += 1
    unreal.log("%s: replaced %d doors" % (pad, count))


COTTAGE_YAW = 35.0
SHED_YAW = -20.0
cottage = build_from_parts(cottage_parts, "cottage", COTTAGE_YAW, "Cottage")
shed = build_from_parts(shed_parts, "shed", SHED_YAW, "Shed")
replace_doors(cottage, "cottage", "Cottage")
replace_doors(shed, "shed", "Shed", required_item="key_storeroom")


def cabin_point(pad, src_long_yaw, extra_yaw, along, across):
    """World XY of a point given along/across the cabin's long axis, relative to its centre."""
    px, py, _ = island.PADS[pad]
    axis = src_long_yaw + extra_yaw
    x, y = rotate(along, across, axis)
    return px + x, py + y, axis


def place_on_floor(mesh_path, pad, src_yaw, extra_yaw, along, across, yaw_offset=0.0, folder="Props", scale=1.0):
    x, y, axis = cabin_point(pad, src_yaw, extra_yaw, along, across)
    z = trace_down(x, y, top=island.pad_height(pad) + 250.0)
    if z is None:
        z = island.pad_height(pad) + 60.0
    return place_mesh(mesh_path, V(x, y, z), yaw=axis + yaw_offset, folder=folder, scale=scale)


def top_of(actor):
    origin, extent = actor.get_actor_bounds(False)
    return origin.z + extent.z


# cottage interior (4 x 8 m): table with Elias's journal, shelf with the store room key
FURN = "/Game/OldFurniturePack_01/Meshes/"
C = ("cottage", COTTAGE_SRC[2], COTTAGE_YAW)
table = place_on_floor(FURN + "SM_Table_01", *C, along=90.0, across=-20.0, folder="Cottage/Interior")
place_on_floor(FURN + "SM_Chair_01", *C, along=90.0, across=70.0, yaw_offset=-90.0, folder="Cottage/Interior")
place_on_floor(FURN + "SM_Chair_02", *C, along=200.0, across=-90.0, yaw_offset=160.0, folder="Cottage/Interior")
place_on_floor(FURN + "SM_Carpet_01", *C, along=-150.0, across=0.0, folder="Cottage/Interior")
shelf = place_on_floor(FURN + "SM_WoodenShelf_02", *C, along=-330.0, across=-150.0, yaw_offset=0.0, folder="Cottage/Interior")
place_on_floor(FURN + "SM_WoodenShelf_03", *C, along=-330.0, across=150.0, yaw_offset=180.0, folder="Cottage/Interior")
place_on_floor("/Game/Modular_Rural_Cabin/Meshes/Props/Old_Sofa", *C, along=-170.0, across=120.0, yaw_offset=180.0, folder="Cottage/Interior")

tx, ty, axis = cabin_point(*C, along=90.0, across=-20.0)
note = actors.spawn_actor_from_class(unreal.NotePickup, V(tx + 20, ty, top_of(table) + 0.5), R(axis + 12.0))
note.set_editor_property("note_id", "elias_01")
note.set_folder_path("Cottage/Interior")

sx, sy, axis = cabin_point(*C, along=-330.0, across=-150.0)
key = actors.spawn_actor_from_class(unreal.ItemPickup, V(sx, sy, top_of(shelf) + 1.5), R(axis + 30.0))
key.set_editor_property("item_id", "key_storeroom")
key.set_folder_path("Cottage/Interior")

cottage_light = actors.spawn_actor_from_class(unreal.PointLight, V(*cabin_point(*C, along=0.0, across=0.0)[:2], island.pad_height("cottage") + 290))
cottage_light.point_light_component.set_intensity_units(unreal.LightUnits.LUMENS)
cottage_light.point_light_component.set_intensity(70.0)
cottage_light.point_light_component.set_light_color(unreal.LinearColor(1.0, 0.68, 0.4, 1.0))
cottage_light.point_light_component.set_attenuation_radius(700.0)
cottage_light.set_folder_path("Cottage/Interior")

# radio shed interior (4 x 4 m): desk with the radio, torn page, cassette
S = ("shed", SHED_SRC[2], SHED_YAW)
desk = place_on_floor("/Game/Modular_Rural_Cabin/Meshes/Props/Wooden_Table_Small", *S, along=0.0, across=-120.0, yaw_offset=90.0, folder="Shed/Interior")
dx, dy, axis = cabin_point(*S, along=0.0, across=-130.0)
radio = actors.spawn_actor_from_class(unreal.RadioSet, V(dx, dy, top_of(desk)), R(axis + 180.0))
radio_mesh = radio.get_component_by_class(unreal.StaticMeshComponent)
radio_mesh.set_static_mesh(load("/Game/Military_Radio/StaticMesh/SM_Ampfilter"))
radio_mesh.set_relative_location(V(0, 0, 0), False, False)
radio_mesh.set_relative_scale3d(V(1, 1, 1))
for i in range(radio_mesh.get_num_materials()):
    radio_mesh.set_material(i, radio_mesh.static_mesh.get_material(i))
radio.set_editor_property("tint_placeholder", False)
radio.set_folder_path("Shed/Interior")

hx, hy, _ = cabin_point(*S, along=45.0, across=-120.0)
place_mesh("/Game/Military_Radio/StaticMesh/SM_Headphones", V(hx, hy, top_of(desk)), yaw=axis + 60.0, folder="Shed/Interior")
place_on_floor(FURN + "SM_Chair_01", *S, along=0.0, across=-40.0, yaw_offset=90.0, folder="Shed/Interior")

px_, py_, _ = cabin_point(*S, along=-55.0, across=-120.0)
page = actors.spawn_actor_from_class(unreal.NotePickup, V(px_, py_, top_of(desk) + 0.5), R(axis - 25.0))
page.set_editor_property("note_id", "elias_02")
page.set_folder_path("Shed/Interior")

crate = place_on_floor("/Game/Modular_Rural_Cabin/Meshes/Props/Wooden_Crate", *S, along=110.0, across=110.0, yaw_offset=20.0, folder="Shed/Interior")
cx, cy, _ = cabin_point(*S, along=110.0, across=110.0)
cassette = actors.spawn_actor_from_class(unreal.ItemPickup, V(cx, cy, top_of(crate) + 1.5), R(axis + 15.0))
cassette.set_editor_property("item_id", "cassette_01")
cassette.set_editor_property("color", unreal.LinearColor(0.08, 0.08, 0.08, 1.0))
cassette.set_folder_path("Shed/Interior")

radio_glow = actors.spawn_actor_from_class(unreal.PointLight, V(dx, dy, top_of(desk) + 60))
radio_glow.point_light_component.set_intensity_units(unreal.LightUnits.LUMENS)
radio_glow.point_light_component.set_intensity(18.0)
radio_glow.point_light_component.set_light_color(unreal.LinearColor(1.0, 0.6, 0.3, 1.0))
radio_glow.point_light_component.set_attenuation_radius(350.0)
radio_glow.set_folder_path("Shed/Interior")

# --- landing: pier, boat, invisible shoreline wall -------------------------------------------------------------

shore_x, shore_y = island.shore_point(island.LANDING_ANGLE)
out_x, out_y = math.cos(math.atan2(shore_y, shore_x)), math.sin(math.atan2(shore_y, shore_x))
pier_path = "/Game/Modular_Rural_Cabin/Meshes/Props/Pier"
p_min, p_max = mesh_bounds(pier_path)
pier_yaw = math.degrees(math.atan2(out_y, out_x)) - 90.0  # the pier is long along its Y axis
pier_center = (shore_x + out_x * 250.0, shore_y + out_y * 250.0)
pier = place_mesh(pier_path, V(pier_center[0], pier_center[1], 130.0 - p_max.z), yaw=pier_yaw, folder="Landing", label="Pier")
place_mesh("/Game/Modular_Rural_Cabin/Meshes/Props/Wooden_Boat",
           V(pier_center[0] + out_y * 260.0, pier_center[1] - out_x * 260.0, -25.0), yaw=pier_yaw + 8.0, folder="Landing")

# block walking into the water along the cove (the cliffs block everywhere else)
for a in range(-40, 41, 4):
    x, y = island.shore_point(island.LANDING_ANGLE + a)
    wall = place_mesh("/Engine/BasicShapes/Cube", V(x, y, 150), yaw=math.degrees(math.atan2(y, x)),
                      scale=(0.3, 9.0, 6.0), folder="Landing/Blockers")
    wall.set_actor_hidden_in_game(True)
    wall.static_mesh_component.set_visibility(False)
# leave a gap where the pier meets the shore
for actor in actors.get_all_level_actors():
    if actor.get_folder_path() == "Landing/Blockers":
        loc = actor.get_actor_location()
        if math.hypot(loc.x - shore_x, loc.y - shore_y) < 300:
            actor.destroy_actor()

# the boat has just dropped the keeper at the end of the pier
start_x, start_y = shore_x + out_x * 430.0, shore_y + out_y * 430.0
start = actors.spawn_actor_from_class(unreal.PlayerStart, V(start_x, start_y, 240.0),
                                      R(math.degrees(math.atan2(-out_y, -out_x))))
start.set_actor_label("PlayerStart")

# --- dressing: grass, shrubs, stones on the plateau --------------------------------------------------------------

FOLIAGE = "/Game/Modular_Rural_Cabin/Meshes/Foliage/"
PROPS = "/Game/Modular_Rural_Cabin/Meshes/Props/"
scatter = [
    (FOLIAGE + "Grass_Patch_1", 160, (0.8, 1.4)),
    (FOLIAGE + "Grass_Patch_2", 120, (0.8, 1.3)),
    (FOLIAGE + "Grass_Patch_Long", 80, (0.9, 1.4)),
    (FOLIAGE + "Shrubs_1", 35, (0.7, 1.1)),
    (PROPS + "Rock_1", 45, (0.8, 2.2)),
    (PROPS + "River_Stone_Set_1", 25, (1.0, 2.0)),
]
for mesh_path, count, scale_range in scatter:
    placed = 0
    attempts = 0
    while placed < count and attempts < count * 20:
        attempts += 1
        x = rng.uniform(-island.A, island.A)
        y = rng.uniform(-island.B, island.B)
        if island.radius(x, y) > 0.9 or island.dirt(x, y) > 0.3:
            continue
        if any(math.hypot(x - px, y - py) < pr + 200 for px, py, pr in island.PADS.values()):
            continue
        place_mesh(mesh_path, V(x, y, ground(x, y) - 5.0), yaw=rng.uniform(0, 360),
                   scale=rng.uniform(*scale_range), folder="Dressing")
        placed += 1

# a few wind-bent pines in the lee of the cottage and fence posts along the cliff path
for i in range(5):
    x = island.PADS["cottage"][0] + rng.uniform(-1600, -700)
    y = island.PADS["cottage"][1] + rng.uniform(300, 1500)
    place_mesh(FOLIAGE + "SM_Pine_Tree_05", V(x, y, ground(x, y) - 20), yaw=rng.uniform(0, 360),
               scale=rng.uniform(0.7, 1.0), folder="Dressing", pitch=rng.uniform(-6, 6))

for i in range(14):
    t = i / 13.0
    ax, ay = island.PATH[-3]
    bx, by = island.PATH[-1]
    x, y = ax + (bx - ax) * t, ay + (by - ay) * t + 320.0
    place_mesh(PROPS + "Fence_Old_" + str(1 + i % 3) + "_2m", V(x, y, ground(x, y)),
               yaw=math.degrees(math.atan2(by - ay, bx - ax)) + rng.uniform(-6, 6), folder="Dressing")

place_mesh(PROPS + "Log_Pile_2", V(island.PADS["cottage"][0] - 420, island.PADS["cottage"][1] - 520,
                                   island.pad_height("cottage")), yaw=COTTAGE_YAW, folder="Cottage")
place_mesh(PROPS + "Metal_Barrel", V(lx - 420, ly - 380, lz), folder="Lighthouse")
place_mesh(PROPS + "Metal_Barrel", V(lx - 360, ly - 450, lz), yaw=40, folder="Lighthouse")
place_mesh(PROPS + "Antenna", V(island.PADS["shed"][0] + 260, island.PADS["shed"][1] + 260,
                                island.pad_height("shed")), folder="Shed")

# --- night sky, moon, fog --------------------------------------------------------------------------------------------

# the moon lights the island but not the atmosphere, so the sky stays black
moon = actors.spawn_actor_from_class(unreal.DirectionalLight, V(0, 0, 5000), R(40.0, -30.0))
moon_light = moon.get_component_by_class(unreal.DirectionalLightComponent)
moon_light.set_intensity(0.5)
moon_light.set_light_color(unreal.LinearColor(0.75, 0.82, 1.0, 1.0))
moon_light.set_editor_property("atmosphere_sun_light", False)
moon_light.set_editor_property("volumetric_scattering_intensity", 0.6)
moon.set_folder_path("Sky")

actors.spawn_actor_from_class(unreal.SkyAtmosphere, V(0, 0, 0)).set_folder_path("Sky")

sky_light = actors.spawn_actor_from_class(unreal.SkyLight, V(0, 0, 3000))
sky_component = sky_light.get_component_by_class(unreal.SkyLightComponent)
sky_component.set_editor_property("real_time_capture", True)
sky_component.set_intensity(1.0)
sky_light.set_folder_path("Sky")

fog = actors.spawn_actor_from_class(unreal.ExponentialHeightFog, V(0, 0, 0))
fog_component = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
fog_component.set_fog_density(0.012)
fog_component.set_fog_height_falloff(0.05)
fog_component.set_fog_inscattering_color(unreal.LinearColor(0.008, 0.012, 0.02, 1.0))
fog_component.set_volumetric_fog(True)
fog_component.set_editor_property("volumetric_fog_extinction_scale", 1.0)
fog.set_folder_path("Sky")

post = actors.spawn_actor_from_class(unreal.PostProcessVolume, V(0, 0, 0))
post.set_editor_property("unbound", True)
settings = post.get_editor_property("settings")
for name, value in [
    ("auto_exposure_min_brightness", -1.9),
    ("auto_exposure_max_brightness", -1.9),
    ("vignette_intensity", 0.55),
    ("film_grain_intensity", 0.22),
    ("bloom_intensity", 0.9),
]:
    settings.set_editor_property("override_" + name, True)
    settings.set_editor_property(name, value)
post.set_editor_property("settings", settings)
post.set_folder_path("Sky")

# --- ambient sound ----------------------------------------------------------------------------------------------------


def ambient(sound_path, location, falloff=None, volume=1.0, label=None):
    sound = actors.spawn_actor_from_class(unreal.AmbientSound, location)
    component = sound.get_component_by_class(unreal.AudioComponent)
    component.set_sound(load(sound_path))
    component.set_editor_property("volume_multiplier", volume)
    if falloff is None:
        component.set_editor_property("allow_spatialization", False)
    else:
        component.set_editor_property("override_attenuation", True)
        attenuation = component.get_editor_property("attenuation_overrides")
        attenuation.set_editor_property("falloff_distance", falloff)
        component.set_editor_property("attenuation_overrides", attenuation)
    sound.set_folder_path("Audio")
    if label:
        sound.set_actor_label(label)
    return sound


ambient("/Game/Procedurally_Generated_Wind/MS_Procedurally_Generated_Wind", V(0, 0, 2000), volume=0.8, label="Wind")
for angle in range(0, 360, 45):
    x, y = island.coast_point(angle, 1.02)
    ambient("/Game/Procedural_Sea_Waves/Metasounds/MS_Seaside", V(x, y, 0), falloff=7000.0, volume=1.2)

# --- world settings, save ------------------------------------------------------------------------------------------------

world.get_world_settings().set_editor_property("default_game_mode", unreal.LastWatchGameMode.static_class())

if not level_editor.save_current_level():
    raise RuntimeError("Could not save " + MAP_PATH)
unreal.log("Built %s (%d actors)" % (MAP_PATH, len(actors.get_all_level_actors())))
unreal.log("PlayerStart at %.0f %.0f, lighthouse lamp z %.0f" % (start_x, start_y, lamp_z))
