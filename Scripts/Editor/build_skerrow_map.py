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


# grass, shrubs, reeds and pebbles ship with box collision that blocks the keeper where nothing
# is visible; you walk through them instead
NO_COLLISION = ("/Foliage/", "River_Stone", "Old_Planks_Plank")


def place_mesh(mesh_path, location, yaw=0.0, scale=1.0, folder="Props", label=None, pitch=0.0, roll=0.0):
    actor = actors.spawn_actor_from_class(unreal.StaticMeshActor, location, R(yaw, pitch, roll))
    actor.static_mesh_component.set_static_mesh(load(mesh_path))
    if not isinstance(scale, (tuple, list)):
        scale = (scale, scale, scale)
    actor.set_actor_scale3d(V(*scale))
    actor.set_folder_path(folder)
    if label:
        actor.set_actor_label(label)
    if any(token in mesh_path for token in NO_COLLISION):
        actor.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
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

# collide with the scanned rock itself: its generated convex hull bulges far past the surface,
# which felt like invisible walls around every outcrop
# the same goes for the pier, whose simple collision is only its posts: the keeper fell through
# the deck and got stuck between the posts
for exact_collision in (CLIFF, "/Game/Modular_Rural_Cabin/Meshes/Props/Pier"):
    body = load(exact_collision).get_editor_property("body_setup")
    if body.get_editor_property("collision_trace_flag") != unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE:
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        unreal.EditorAssetLibrary.save_asset(exact_collision)
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

# the walk-in tower from build_lighthouse.py; its pieces share a frame with +Y = doorway
LIGHTHOUSE_YAW = 90.0  # doorway faces west, towards the path
lx, ly, _ = island.PADS["lighthouse"]
lz = island.pad_height("lighthouse") - 8.0


def lighthouse_point(x, y, z):
    wx, wy = rotate(x, y, LIGHTHOUSE_YAW)
    return V(lx + wx, ly + wy, lz + z)


for piece in ("Exterior", "Shell", "Stairs", "Floors", "LanternGlass"):
    part = place_mesh("/Game/Lighthouse/SM_Lighthouse" + piece, V(lx, ly, lz), yaw=LIGHTHOUSE_YAW,
                      folder="Lighthouse", label="Lighthouse" + piece)
    if piece == "LanternGlass":
        part.set_actor_hidden_in_game(True)  # collision only
        part.static_mesh_component.set_visibility(False)
    if piece == "Exterior":
        # looks only; the shell does all the colliding
        part.static_mesh_component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)

# the lamp sits inside the lens at the centre of the lantern room
lamp_z = lz + 2312.0
beam = actors.spawn_actor_from_class(unreal.LighthouseBeam, V(lx, ly, lamp_z))
beam.set_actor_label("LighthouseBeam")
beam.set_folder_path("Lighthouse")

# front door: the cabin pack's plank door stretched to the tower's doorway
door_mesh = load("/Game/Modular_Rural_Cabin/Meshes/Modular/Door_01")
tower_door = actors.spawn_actor_from_class(unreal.KeeperDoor, lighthouse_point(-86.0, 332.0, 0.0), R(LIGHTHOUSE_YAW + 90.0))
tower_door_mesh = tower_door.get_component_by_class(unreal.StaticMeshComponent)
tower_door_mesh.set_static_mesh(door_mesh)
tower_door_mesh.set_material(0, door_mesh.get_material(0))
tower_door_mesh.set_relative_scale3d(V(1.0, 1.8, 1.5))
tower_door_mesh.set_relative_location(V(0, 0, 103.0 * 1.5), False, False)
tower_door.set_editor_property("tint_placeholder", False)
tower_door.set_editor_property("open_yaw", -95.0)  # swings into the tower
tower_door.set_actor_label("LighthouseDoor")
tower_door.set_folder_path("Lighthouse")

# ground floor: oil store and the keeper's log desk
LH_INTERIOR = "Lighthouse/Interior"
for angle, mesh_path in [(20, "Metal_Barrel"), (38, "Metal_Barrel"), (-20, "Metal_Barrel"), (150, "Wooden_Crate"),
                         (170, "Cardboard_Box_2"), (200, "Wooden_Crate_1")]:
    x, y = 225.0 * math.cos(math.radians(angle)), 225.0 * math.sin(math.radians(angle))
    place_mesh("/Game/Modular_Rural_Cabin/Meshes/Props/" + mesh_path, lighthouse_point(x, y, 2.0),
               yaw=LIGHTHOUSE_YAW + angle + rng.uniform(-20, 20), folder=LH_INTERIOR)
place_mesh("/Game/Modular_Rural_Cabin/Meshes/Props/Wooden_Table_Small", lighthouse_point(-215.0, 60.0, 2.0),
           yaw=LIGHTHOUSE_YAW + 90.0, folder=LH_INTERIOR)
place_mesh("/Game/OldFurniturePack_01/Meshes/SM_Chair_02", lighthouse_point(-150.0, 120.0, 2.0),
           yaw=LIGHTHOUSE_YAW + 200.0, folder=LH_INTERIOR)

for height, lumens in [(290.0, 35.0), (1000.0, 18.0), (1650.0, 18.0)]:
    lamp = actors.spawn_actor_from_class(unreal.PointLight, lighthouse_point(0.0, -150.0, height))
    lamp.point_light_component.set_intensity_units(unreal.LightUnits.LUMENS)
    lamp.point_light_component.set_intensity(lumens)
    lamp.point_light_component.set_light_color(unreal.LinearColor(1.0, 0.62, 0.35, 1.0))
    lamp.point_light_component.set_attenuation_radius(600.0)
    lamp.set_folder_path(LH_INTERIOR)

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
replace_doors(shed, "shed", "Shed")


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

# the boat has just dropped the keeper at the end of the pier. The pier mesh's pivot is not at its
# centre, so measure where it actually ended up and stand on its deck, a little in from the sea end
pier_origin, pier_extent = pier.get_actor_bounds(False)
reach = abs(pier_extent.x * out_x) + abs(pier_extent.y * out_y)  # half length along the pier
start_x = pier_origin.x + out_x * (reach - 150.0)
start_y = pier_origin.y + out_y * (reach - 150.0)
deck_z = trace_down(start_x, start_y, top=pier_origin.z + pier_extent.z + 50.0)
start = actors.spawn_actor_from_class(unreal.PlayerStart, V(start_x, start_y, (deck_z or 130.0) + 100.0),
                                      R(math.degrees(math.atan2(-out_y, -out_x))))
unreal.log("Pier deck at z %s, start %.0f %.0f" % (deck_z, start_x, start_y))
start.set_actor_label("PlayerStart")

# --- dressing: grass, shrubs, stones on the plateau --------------------------------------------------------------

FOLIAGE = "/Game/Modular_Rural_Cabin/Meshes/Foliage/"
PROPS = "/Game/Modular_Rural_Cabin/Meshes/Props/"
scatter = [
    (FOLIAGE + "Grass_Patch_1", 480, (0.8, 1.5)),
    (FOLIAGE + "Grass_Patch_2", 340, (0.8, 1.4)),
    (FOLIAGE + "Grass_Patch_Long", 240, (0.9, 1.5)),
    (FOLIAGE + "Shrubs_1", 110, (0.7, 1.2)),
    (FOLIAGE + "Shrubs_1_Single", 80, (0.8, 1.3)),
    (FOLIAGE + "Bush_1", 25, (0.6, 1.0)),
    (PROPS + "Rock_1", 90, (0.8, 2.5)),
    (PROPS + "River_Stone_Set_1", 50, (1.0, 2.0)),
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


def on_ground(mesh_path, x, y, yaw=None, scale=1.0, folder="Dressing", sink=0.0, pitch=0.0, roll=0.0):
    return place_mesh(mesh_path, V(x, y, ground(x, y) - sink), yaw=rng.uniform(0, 360) if yaw is None else yaw,
                      scale=scale, folder=folder, pitch=pitch, roll=roll)


# rock outcrops: small copies of the scanned cliff breaking up the grass near the edges
placed = 0
while placed < 45:
    x, y = rng.uniform(-island.A, island.A), rng.uniform(-island.B, island.B)
    r = island.radius(x, y)
    if not 0.55 < r < 0.86 or island.dirt(x, y) > 0.2 or island.landing_weight(x, y) > 0.3:
        continue
    if any(math.hypot(x - px, y - py) < pr + 300 for px, py, pr in island.PADS.values()):
        continue
    on_ground(CLIFF, x, y, scale=rng.uniform(0.12, 0.32), sink=rng.uniform(40, 120),
              pitch=rng.uniform(-10, 10), roll=rng.uniform(-10, 10), folder="Dressing/Outcrops")
    placed += 1

# power line on wooden poles from the shed to the lighthouse and the cottage
for (ax, ay), (bx, by) in [((2300.0, -1500.0), (6700.0, 100.0)), ((1400.0, -1300.0), (-1100.0, 1700.0))]:
    length = math.hypot(bx - ax, by - ay)
    count = max(2, int(length / 1700.0) + 1)
    heading = math.degrees(math.atan2(by - ay, bx - ax))
    for i in range(count):
        t = i / (count - 1)
        on_ground(PROPS + "Power_Pole_1", ax + (bx - ax) * t, ay + (by - ay) * t, yaw=heading + 90.0,
                  sink=30.0, roll=rng.uniform(-3, 3), folder="Dressing/PowerLine")

# keeper's cottage yard: tool shed, outhouse, firewood, odds and ends
cx, cy, _ = island.PADS["cottage"]
on_ground(PROPS + "Side_Shed", cx - 900.0, cy + 600.0, yaw=COTTAGE_YAW + 90.0, folder="Cottage/Yard")
actors.spawn_actor_from_class(unreal.EditorAssetLibrary.load_blueprint_class(PROPS + "Blueprints/Outhouse"),
                              V(cx + 300.0, cy + 1150.0, ground(cx + 300.0, cy + 1150.0)),
                              R(COTTAGE_YAW + 180.0)).set_folder_path("Cottage/Yard")
for mesh_path, dx, dy in [("Tree_Stump_1", -700, 950), ("Axe_1", -690, 950), ("Log_Pile_1", -1100, 250),
                          ("Log_3", -620, 880), ("Log_5", -760, 1050), ("Wheel_Barrow", -400, 1250),
                          ("Metal_Bucket", -250, 1100), ("Wooden_Crate", 700, 900), ("Pallet_Broken", 820, 700),
                          ("Metal_Barrel", 650, 1150), ("Old_Planks_Plank_2", -980, 640)]:
    x, y = cx + dx, cy + dy
    z_offset = 85.0 if mesh_path == "Axe_1" else 0.0
    place_mesh(PROPS + mesh_path, V(x, y, ground(x, y) + z_offset), yaw=rng.uniform(0, 360),
               folder="Cottage/Yard", pitch=-60.0 if mesh_path == "Axe_1" else 0.0)

# radio shed yard: oil drums, crates, sheet metal, the generator box
sx, sy, _ = island.PADS["shed"]
for mesh_path, dx, dy in [("Metal_Barrel", 520, -420), ("Metal_Barrel", 600, -330), ("Metal_Barrel", 470, -300),
                          ("Wooden_Crate", -520, -480), ("Wooden_Crate_2", -470, -380), ("Cardboard_Box_4", -600, -350),
                          ("Utility_Box_1a", 480, 150), ("Metal_Sheet_1", -650, 200), ("Pallet", 300, -620)]:
    on_ground(PROPS + mesh_path, sx + dx, sy + dy, folder="Shed/Yard")

# lighthouse yard
for mesh_path, dx, dy in [("Wooden_Crate", 450, -600), ("Wooden_Crate_1", 520, -520), ("Pallet", 380, 650),
                          ("Metal_Sheet_2", 620, 450), ("Metal_Bucket", 300, -700)]:
    on_ground(PROPS + mesh_path, lx + dx, ly + dy, folder="Lighthouse/Yard")

# the ruin of an older concrete post on the north side - something was here before the lighthouse keepers
RUIN = "/Game/AbandonedPowerPlant/Assets/"
rx, ry, ryaw = 1300.0, 3800.0, 25.0
for mesh_path, along, across, yaw_offset, sink in [
    ("Walls/SM_Wall_Structure_01", 0.0, 0.0, 0.0, 180.0),
    ("Walls/SM_Wall_Plain_200", 260.0, 120.0, 90.0, 220.0),
    ("Walls/SM_Corner_Bare", -170.0, 0.0, 0.0, 150.0),
    ("Walls/SM_Wall_Window_Small_03", -260.0, 220.0, 90.0, 260.0),
    ("Rubble/SM_Rubble_04", 60.0, 260.0, 30.0, 10.0),
    ("Rubble/SM_Rubble_02", -120.0, 360.0, 0.0, 5.0),
    ("Rubble/SM_Rubble_01", 200.0, -150.0, 70.0, 5.0),
    ("Doors/SM_Door_Single_01", 120.0, 420.0, 80.0, 0.0),
]:
    x, y = rotate(along, across, ryaw)
    pitch = 85.0 if "Door_Single" in mesh_path else 0.0
    on_ground(RUIN + mesh_path, rx + x, ry + y, yaw=ryaw + yaw_offset, sink=sink, pitch=pitch, folder="Dressing/Ruin")

# a wreck hauled up on the shore of the cove, reeds where the stream would be
wx, wy = island.shore_point(island.LANDING_ANGLE + 22.0)
inland = math.atan2(-wy, -wx)
wx, wy = wx + math.cos(inland) * 250.0, wy + math.sin(inland) * 250.0
on_ground(PROPS + "Wooden_Boat", wx, wy, yaw=math.degrees(inland) + 70.0, sink=25.0, roll=18.0, pitch=-6.0, folder="Landing")
for i in range(16):
    a = island.LANDING_ANGLE + rng.uniform(-30, 30)
    sx_, sy_ = island.shore_point(a)
    d = rng.uniform(120, 500)
    on_ground(FOLIAGE + rng.choice(["Cat_Tail", "Cat_Tail_2"]), sx_ - math.cos(math.radians(a)) * d,
              sy_ - math.sin(math.radians(a)) * d, scale=rng.uniform(0.8, 1.2), folder="Landing")

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


wind_sound = ambient("/Game/Procedurally_Generated_Wind/MS_Procedurally_Generated_Wind", V(0, 0, 2000), volume=0.8, label="Wind")
wind_sound.set_editor_property("tags", ["wind"])
for angle in range(0, 360, 45):
    x, y = island.coast_point(angle, 1.02)
    ambient("/Game/Procedural_Sea_Waves/Metasounds/MS_Seaside", V(x, y, 0), falloff=7000.0, volume=1.2)

# --- story -------------------------------------------------------------------------------------------------------------
# Everything Content/Data/Nights.json refers to: tasks, trigger volumes, teleport targets, notes,
# the bunker, the lights at sea, the wet trail and the storm. Ids here must match the JSON.

STORY = "Story"


def tag(actor, *names):
    actor.set_editor_property("tags", list(names))
    return actor


def story_task(task_id, location, size, prompt_key, yaw=0.0, hold=0.0, required="", consume=True, night=None):
    """Invisible box over a prop the keeper uses; see AStoryTask."""
    task = actors.spawn_actor_from_class(unreal.StoryTask, location, R(yaw))
    task.set_editor_property("task_id", task_id)
    task.set_editor_property("prompt_key", prompt_key)
    task.set_editor_property("hold_seconds", hold)
    task.set_editor_property("tint_placeholder", False)
    if required:
        task.set_editor_property("required_item", required)
        task.set_editor_property("consume_item", consume)
    mesh = task.get_component_by_class(unreal.StaticMeshComponent)
    mesh.set_relative_scale3d(V(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    mesh.set_visibility(False)
    task.set_actor_label("Task_" + task_id + ("_n" + night if night else ""))
    task.set_folder_path(STORY)
    if night:
        tag(task, "night:" + night)
    return task


def story_volume(volume_id, location, extent, yaw=0.0):
    volume = actors.spawn_actor_from_class(unreal.StoryVolume, location, R(yaw))
    volume.set_editor_property("volume_id", volume_id)
    volume.set_editor_property("extent", V(*extent))
    volume.set_actor_label("Volume_" + volume_id)
    volume.set_folder_path(STORY)
    return volume


def target(name, location, yaw):
    point = actors.spawn_actor_from_class(unreal.TargetPoint, location, R(yaw))
    point.set_actor_label("Target_" + name)
    point.set_folder_path(STORY)
    return tag(point, name)


def story_note(note_id, location, yaw=0.0, night=None, folder=STORY):
    note_actor = actors.spawn_actor_from_class(unreal.NotePickup, location, R(yaw))
    note_actor.set_editor_property("note_id", note_id)
    note_actor.set_actor_label("Note_" + note_id)
    note_actor.set_folder_path(folder)
    if night:
        tag(note_actor, "night:" + night)
    return note_actor


def teleport_door(door_id, location, target_tag, prompt_key, size, required="", yaw=0.0, mesh_path=None, mesh_scale=None, pitch=0.0):
    door = actors.spawn_actor_from_class(unreal.TeleportDoor, location, R(yaw, pitch))
    door.set_editor_property("door_id", door_id)
    door.set_editor_property("target_tag", target_tag)
    door.set_editor_property("prompt_key", prompt_key)
    door.set_editor_property("tint_placeholder", False)
    if required:
        door.set_editor_property("required_item", required)
    mesh = door.get_component_by_class(unreal.StaticMeshComponent)
    if mesh_path:
        mesh.set_static_mesh(load(mesh_path))
        mesh.set_material(0, mesh.static_mesh.get_material(0))
        mesh.set_relative_scale3d(V(*mesh_scale))
    else:
        mesh.set_relative_scale3d(V(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
        mesh.set_visibility(False)
    door.set_actor_label("Door_" + door_id)
    door.set_folder_path(STORY)
    return door


def story_light(location, lumens, color, radius, folder):
    light = actors.spawn_actor_from_class(unreal.PointLight, location)
    light.point_light_component.set_intensity_units(unreal.LightUnits.LUMENS)
    light.point_light_component.set_intensity(lumens)
    light.point_light_component.set_light_color(color)
    light.point_light_component.set_attenuation_radius(radius)
    light.set_folder_path(folder)
    return light


# -- the lighthouse: store, lantern and the lamp's three chores
LANTERN_FLOOR = 1880.0
story_volume("lighthouse", lighthouse_point(0.0, 0.0, 150.0), (260.0, 260.0, 150.0))
story_volume("lantern", lighthouse_point(0.0, 0.0, LANTERN_FLOOR + 120.0), (195.0, 195.0, 110.0))
story_note("standing_orders", lighthouse_point(-215.0, 60.0, 88.0), yaw=LIGHTHOUSE_YAW + 70.0)

drum_x, drum_y = 225.0 * math.cos(math.radians(20.0)), 225.0 * math.sin(math.radians(20.0))
story_task("fuel_drum", lighthouse_point(drum_x, drum_y, 50.0), (95.0, 95.0, 110.0), "prompt.fuel_drum")

# the lantern floor is only solid opposite the stair hatch (built frame 160..300 degrees)
def lantern_spot(angle, radius=150.0, z=0.0):
    return lighthouse_point(radius * math.cos(math.radians(angle)), radius * math.sin(math.radians(angle)), LANTERN_FLOOR + z)


place_mesh(PROPS + "Utility_Box_2b", lantern_spot(205.0), yaw=LIGHTHOUSE_YAW + 205.0, folder="Lighthouse/Lantern", label="Reservoir")
place_mesh(PROPS + "Utility_Box_1b", lantern_spot(265.0), yaw=LIGHTHOUSE_YAW + 265.0, folder="Lighthouse/Lantern", label="Clockwork")
story_task("refuel", lantern_spot(205.0, z=55.0), (70.0, 70.0, 115.0), "prompt.refuel", yaw=LIGHTHOUSE_YAW, hold=3.0, required="fuel_can")
for task_id, prompt, hold, required in [("wind", "prompt.wind", 4.0, ""), ("inspect_lamp", "prompt.inspect", 0.0, ""),
                                        ("repair", "prompt.repair", 4.0, "spare_cable")]:
    story_task(task_id, lantern_spot(265.0, z=50.0), (75.0, 75.0, 105.0), prompt, yaw=LIGHTHOUSE_YAW, hold=hold, required=required)
story_task("clean_lens", lighthouse_point(0.0, 0.0, 2310.0), (235.0, 235.0, 235.0), "prompt.clean", hold=2.5)
# the last night's choice: feed the lamp, or snuff it
story_task("keep_light", lantern_spot(205.0, z=55.0), (90.0, 90.0, 125.0), "prompt.keep_light", hold=3.0, night="5-5")
story_task("darken", lighthouse_point(0.0, 0.0, 2310.0), (250.0, 250.0, 250.0), "prompt.darken", hold=3.0, night="5-5")

# -- the cottage: arrival, the bed, the journal, the weather log, and later a sodden page
cottage_center = cabin_point(*C, along=0.0, across=0.0)
floor_z = trace_down(cottage_center[0], cottage_center[1], top=island.pad_height("cottage") + 250.0) or island.pad_height("cottage") + 80.0
story_volume("cottage", V(cottage_center[0], cottage_center[1], floor_z + 140.0), (360.0, 165.0, 140.0), yaw=cottage_center[2])
target("wake_cottage", V(*cabin_point(*C, along=-60.0, across=40.0)[:2], floor_z + 100.0), cottage_center[2])
sofa_x, sofa_y, _ = cabin_point(*C, along=-170.0, across=120.0)
story_task("sleep", V(sofa_x, sofa_y, floor_z + 55.0), (285.0, 125.0, 115.0), "prompt.sleep", yaw=cottage_center[2] + 180.0)
story_note("weather_log", V(*cabin_point(*C, along=-330.0, across=-150.0)[:2], top_of(shelf) + 1.0), yaw=cottage_center[2] + 20.0)
story_note("wet_page", V(*cabin_point(*C, along=-40.0, across=95.0)[:2], floor_z + 1.0), yaw=cottage_center[2] - 35.0, night="4-5")

# -- the shed yard tool crate (spare cable, night 2)
sx, sy, _ = island.PADS["shed"]
story_task("toolbox", V(sx - 520.0, sy - 480.0, ground(sx - 520.0, sy - 480.0) + 40.0), (115.0, 115.0, 95.0), "prompt.toolbox")

# -- Elias's notebook, hidden in the wreck on the shore (night 2 on)
story_note("elias_notebook", V(wx, wy, ground(wx, wy) + 45.0), yaw=rng.uniform(0, 360), night="2-5")

# -- the ruin: its hatch leads down to the station
story_volume("ruin", V(rx, ry, ground(rx, ry) + 250.0), (650.0, 650.0, 400.0))
hx, hy = rotate(60.0, 140.0, ryaw)
hatch_z = ground(rx + hx, ry + hy)
# a rusted steel plate set flat in the ground (the power plant door is an open frame; traces went straight through)
teleport_door("hatch_down", V(rx + hx, ry + hy, hatch_z + 2.0), "bunker_arrival", "prompt.hatch_down", (0, 0, 0),
              required="hatch_key", yaw=ryaw, mesh_path=PROPS + "Metal_Sheet_1", mesh_scale=(1.0, 1.0, 1.0))
target("ruin_arrival", V(rx + hx + 160.0, ry + hy, hatch_z + 110.0), ryaw + 180.0)

# -- the VESPER station, built well above the island (out of the height fog); only the hatch leads here
BX, BY, BZ = 0.0, 0.0, 30000.0
BUNKER = "Bunker"


def bunker_point(x, y, z=0.0):
    return V(BX + x, BY + y, BZ + z)


for piece in ("Shell", "Floor"):
    place_mesh("/Game/Bunker/SM_Bunker" + piece, bunker_point(0, 0), folder=BUNKER, label="Bunker" + piece)
target("bunker_arrival", bunker_point(-80.0, 0.0, 100.0), 0.0)
place_mesh(RUIN + "Platforms/SM_Stairs_01", bunker_point(-220.0, 0.0, 0.0), yaw=90.0, folder=BUNKER, label="BunkerStairs")
teleport_door("hatch_up", bunker_point(-255.0, 0.0, 230.0), "ruin_arrival", "prompt.hatch_up", (80.0, 150.0, 140.0))

FLUORESCENT = unreal.LinearColor(0.78, 0.88, 1.0, 1.0)
for x, y, z, lumens in [(0, 0, 300, 45), (800, 0, 255, 22), (1600, -250, 330, 40), (2050, 200, 330, 40), (2100, 800, 270, 30)]:
    story_light(bunker_point(x, y, z), lumens, FLUORESCENT, 900.0, BUNKER)


def bunker_prop(mesh_path, x, y, yaw=0.0, z=0.0, scale=1.0):
    return place_mesh(mesh_path, bunker_point(x, y, z), yaw=yaw, scale=scale, folder=BUNKER)


RADIO = "/Game/Military_Radio/StaticMesh/"
for x in (1560.0, 1960.0):
    desk_actor = bunker_prop(FURN + "SM_Table_02", x, -405.0)
    top = top_of(desk_actor) - BZ
    bunker_prop(RADIO + "SM_Ampfilter", x - 50.0, -420.0, yaw=180.0, z=top)
    bunker_prop(RADIO + "SM_Headphones", x + 50.0, -400.0, yaw=200.0, z=top)
    bunker_prop(FURN + "SM_Chair_01", x, -300.0, yaw=-90.0)
bunker_prop(RADIO + "SM_Telephone", 1700.0, -420.0, z=top)
center_table = bunker_prop(FURN + "SM_Table_01", 1800.0, 150.0, yaw=90.0)
center_top = top_of(center_table) - BZ
tape_deck = bunker_prop(RADIO + "SM_Radio", 1800.0, 110.0, yaw=90.0, z=center_top)
story_task("tape", bunker_point(1800.0, 110.0, center_top + 15.0), (60.0, 60.0, 40.0), "prompt.tape", required="cassette_01", consume=False)
bunker_prop(FURN + "SM_WoodenShelf_04", 2310.0, -150.0, yaw=90.0)
bunker_prop(FURN + "SM_WoodenShelf_03", 1340.0, 300.0, yaw=-90.0)
for x, y, mesh_path in [(150.0, 200.0, PROPS + "Metal_Barrel"), (220.0, 230.0, PROPS + "Metal_Barrel"),
                        (180.0, -220.0, PROPS + "Cardboard_Box_1"), (2250.0, 400.0, PROPS + "Cardboard_Box_5"),
                        (1450.0, 420.0, PROPS + "Cardboard_Box_Open_2")]:
    bunker_prop(mesh_path, x, y, yaw=rng.uniform(0, 360))
records_table = bunker_prop(FURN + "SM_Table_01", 2100.0, 880.0)
records_top = top_of(records_table) - BZ
bunker_prop(FURN + "SM_WoodenShelf_02", 2300.0, 700.0, yaw=90.0)

story_note("vesper_memo", bunker_point(1540.0, -380.0, top + 1.0), yaw=12.0)
story_note("elias_03", bunker_point(1610.0, -360.0, top + 1.0), yaw=-20.0)
story_note("timing_codes", bunker_point(1940.0, -370.0, top + 1.0), yaw=5.0)
story_note("sonar_report", bunker_point(1830.0, 230.0, center_top + 1.0), yaw=80.0)
story_note("hale_file", bunker_point(2050.0, 880.0, records_top + 1.0), yaw=15.0)
story_note("keeper_roster", bunker_point(2160.0, 870.0, records_top + 1.0), yaw=-10.0, night="4-5")

# -- lights at sea that should not be there (shown by the story)
for angle in (25.0, 55.0, 80.0):
    fx, fy = island.coast_point(angle, 1.75)
    fog_light = story_light(V(fx, fy, 350.0), 60000.0, unreal.LinearColor(1.0, 0.93, 0.8, 1.0), 3500.0, STORY)
    fog_light.point_light_component.set_editor_property("volumetric_scattering_intensity", 4.0)
    fog_light.point_light_component.set_cast_shadows(False)
    tag(fog_light, "fog_lights", "night:99-99")

# -- night 4: a wet trail up from the sea to the cottage
LEAK = load("/Game/AbandonedPowerPlant/Assets/Decals/MI_Leak_01")
trail_from = island.coast_point(140.0, 0.93)
trail_to = cabin_point(*C, along=380.0, across=-260.0)
for i in range(18):
    t = i / 17.0
    x = trail_from[0] + (trail_to[0] - trail_from[0]) * t + rng.uniform(-40, 40)
    y = trail_from[1] + (trail_to[1] - trail_from[1]) * t + rng.uniform(-40, 40)
    decal = actors.spawn_actor_from_class(unreal.DecalActor, V(x, y, ground(x, y) + 20.0), R(rng.uniform(0, 360), -90.0))
    component = decal.get_component_by_class(unreal.DecalComponent)
    component.set_decal_material(LEAK)
    component.set_editor_property("decal_size", V(60.0, 55.0, 32.0))
    decal.set_folder_path(STORY + "/WetTrail")
    tag(decal, "night:4-5")

# -- storm (nights 4 and 5)
storm = actors.spawn_actor_from_class(unreal.StormController, V(0, 0, 3000))
storm.set_editor_property("thunder", [load("/Game/Thunder_Sounds/CUE/" + name) for name in (
    "CUE_Thunder_Lightning_I_Without_rain_wind_background_noise_Cue",
    "CUE_Thunder_Lightning_II_Without_rain_wind_background_noise_Cue",
    "CUE_Thunder_Lightning_III_Without_rain_wind_background_noise_Cue")])
storm.set_folder_path(STORY)

# --- world settings, save ------------------------------------------------------------------------------------------------

world.get_world_settings().set_editor_property("default_game_mode", unreal.LastWatchGameMode.static_class())

if not level_editor.save_current_level():
    raise RuntimeError("Could not save " + MAP_PATH)
unreal.log("Built %s (%d actors)" % (MAP_PATH, len(actors.get_all_level_actors())))
unreal.log("PlayerStart at %.0f %.0f, lighthouse lamp z %.0f" % (start_x, start_y, lamp_z))
