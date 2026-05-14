#!/usr/bin/env python3
"""Re-export FreeCAD STL files through Blender for Cura compatibility."""

from __future__ import annotations

import sys
from pathlib import Path

import bpy


ROOT = Path(__file__).resolve().parent
SRC = ROOT / "freecad-output"
OUT = ROOT / "cura-ready"

FILES = [
    "stackchan_shell_roverc_inward_foot_mount_freecad_v10.stl",
]


def import_stl(path: Path) -> None:
    if hasattr(bpy.ops.wm, "stl_import"):
        bpy.ops.wm.stl_import(filepath=str(path))
    else:
        bpy.ops.import_mesh.stl(filepath=str(path))


def export_stl(path: Path) -> None:
    if hasattr(bpy.ops.wm, "stl_export"):
        bpy.ops.wm.stl_export(filepath=str(path), export_selected_objects=True)
    else:
        bpy.ops.export_mesh.stl(filepath=str(path), use_selection=True)


def repair_one(name: str) -> None:
    bpy.ops.object.select_all(action="SELECT")
    bpy.ops.object.delete()

    import_stl(SRC / name)
    bpy.ops.object.select_all(action="SELECT")
    bpy.context.view_layer.objects.active = bpy.context.selected_objects[0]

    for obj in bpy.context.selected_objects:
        obj.select_set(True)
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="SELECT")
        bpy.ops.mesh.remove_doubles(threshold=0.0001)
        bpy.ops.mesh.normals_make_consistent(inside=False)
        bpy.ops.mesh.delete_loose()
        bpy.ops.object.mode_set(mode="OBJECT")

    OUT.mkdir(parents=True, exist_ok=True)
    export_stl(OUT / name.replace("_freecad_", "_cura_"))
    print(OUT / name.replace("_freecad_", "_cura_"))


def main() -> None:
    for name in FILES:
        repair_one(name)


if __name__ == "__main__":
    main()
