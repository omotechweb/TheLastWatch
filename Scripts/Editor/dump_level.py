# Dumps every actor of a level (class, label, mesh, transform) to Saved/level_dump_<name>.txt.
# Used to study how a pack's showcase map assembles its modular pieces.
#
# Run with:  powershell -File Scripts/run_python.ps1 Scripts/Editor/dump_level.py
# Set LEVELS below to the maps to dump.

import os
import unreal

LEVELS = [
    "/Game/Modular_Rural_Cabin/Maps/Modular_Cabins_Showcase",
    "/Game/Modular_Rural_Cabin/Maps/Rural_Cabins",
]

level_editor = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def describe(actor):
    loc = actor.get_actor_location()
    rot = actor.get_actor_rotation()
    scale = actor.get_actor_scale3d()
    mesh = ""
    if isinstance(actor, unreal.StaticMeshActor):
        sm = actor.static_mesh_component.static_mesh
        mesh = sm.get_path_name().split(".")[0] if sm else ""
    return "%s | %s | %s | loc(%.0f %.0f %.0f) rot(%.0f %.0f %.0f) scale(%.2f %.2f %.2f)" % (
        actor.get_class().get_path_name(), actor.get_actor_label(), mesh,
        loc.x, loc.y, loc.z, rot.roll, rot.pitch, rot.yaw, scale.x, scale.y, scale.z)


for level in LEVELS:
    if not level_editor.load_level(level):
        unreal.log_error("Could not load " + level)
        continue
    lines = [describe(a) for a in actors.get_all_level_actors()]
    name = level.split("/")[-1]
    out = os.path.join(unreal.Paths.project_saved_dir(), "level_dump_%s.txt" % name)
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(sorted(lines)))
    unreal.log("Wrote %s (%d actors)" % (out, len(lines)))
