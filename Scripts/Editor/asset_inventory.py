# Writes Saved/asset_inventory.txt: every static mesh (with size in cm), sound,
# material and map in the third-party packs, so level scripts can pick pieces by name.
#
# Run with:  powershell -File Scripts/run_python.ps1 Scripts/Editor/asset_inventory.py

import os
import unreal

PACKS = [
    "/Game/AbandonedPowerPlant",
    "/Game/Essential_Foosteps_SK",
    "/Game/Military_Radio",
    "/Game/Modular_Rural_Cabin",
    "/Game/OldFurniturePack_01",
    "/Game/Procedural_Sea_Waves",
    "/Game/Procedurally_Generated_Wind",
    "/Game/Thunder_Sounds",
    "/Game/Imported",
]

registry = unreal.AssetRegistryHelpers.get_asset_registry()
lines = []

for pack in PACKS:
    assets = registry.get_assets_by_path(pack, recursive=True)
    if not assets:
        continue
    lines.append("=== " + pack)
    by_class = {}
    for data in assets:
        by_class.setdefault(str(data.asset_class_path.asset_name), []).append(data)

    for cls in sorted(by_class):
        entries = sorted(by_class[cls], key=lambda d: str(d.package_name))
        lines.append("  -- %s (%d)" % (cls, len(entries)))
        for data in entries:
            path = str(data.package_name)
            if cls == "StaticMesh":
                mesh = unreal.load_asset(path)
                box = mesh.get_bounding_box()
                size = box.max - box.min
                lines.append("    %s  [%d x %d x %d]" % (path, size.x, size.y, size.z))
            else:
                lines.append("    " + path)

out = os.path.join(unreal.Paths.project_saved_dir(), "asset_inventory.txt")
with open(out, "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
unreal.log("Wrote " + out)
