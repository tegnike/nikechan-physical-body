#!/usr/bin/env python3
"""Generate FreeCAD-based STEP/STL printables for Nikechan RoverC body."""

from __future__ import annotations

from pathlib import Path

import FreeCAD as App
import MeshPart
import Part


ROOT = Path(__file__).resolve().parent
OUT = ROOT / "freecad-output"
SHELL_STEP = ROOT / "vendor" / "stack-chan-v1" / "shell.step"


def box(name: str, x: float, y: float, z: float, sx: float, sy: float, sz: float) -> Part.Shape:
    return Part.makeBox(sx, sy, sz, App.Vector(x - sx / 2, y - sy / 2, z))


def cyl(name: str, x: float, y: float, z: float, radius: float, height: float) -> Part.Shape:
    return Part.makeCylinder(radius, height, App.Vector(x, y, z), App.Vector(0, 0, 1))


def rounded_slot(x: float, y: float, z: float, length: float, width: float, height: float, along: str = "x") -> Part.Shape:
    if along == "x":
        core = box("slot_core", x, y, z, max(0.01, length - width), width, height)
        end_a = cyl("slot_end_a", x - (length - width) / 2, y, z, width / 2, height)
        end_b = cyl("slot_end_b", x + (length - width) / 2, y, z, width / 2, height)
    else:
        core = box("slot_core", x, y, z, width, max(0.01, length - width), height)
        end_a = cyl("slot_end_a", x, y - (length - width) / 2, z, width / 2, height)
        end_b = cyl("slot_end_b", x, y + (length - width) / 2, z, width / 2, height)
    return core.fuse([end_a, end_b])


def prism_x(name: str, x: float, width: float, yz_points: list[tuple[float, float]]) -> Part.Shape:
    polygon = [App.Vector(0, y, z) for y, z in yz_points]
    polygon.append(polygon[0])
    wire = Part.makePolygon(polygon)
    face = Part.Face(wire)
    solid = face.extrude(App.Vector(width, 0, 0))
    solid.translate(App.Vector(x - width / 2, 0, 0))
    return solid


def fuse_all(shapes: list[Part.Shape]) -> Part.Shape:
    base = shapes[0]
    for shape in shapes[1:]:
        base = base.fuse(shape)
    base = base.removeSplitter()
    return base


def export_shape(name: str, shape: Part.Shape) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    step_path = OUT / f"{name}.step"
    stl_path = OUT / f"{name}.stl"
    shape.exportStep(str(step_path))
    mesh = MeshPart.meshFromShape(
        Shape=shape,
        LinearDeflection=0.12,
        AngularDeflection=0.35,
        Relative=False,
    )
    mesh.write(str(stl_path))
    print(step_path)
    print(stl_path)


def export_step(name: str, shape: Part.Shape) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    step_path = OUT / f"{name}.step"
    shape.exportStep(str(step_path))
    print(step_path)


def normalized_stackchan_shell() -> Part.Shape:
    shape = Part.Shape()
    shape.read(str(SHELL_STEP))
    bbox = shape.BoundBox
    shape.translate(App.Vector(-bbox.Center.x, -bbox.Center.y, -bbox.ZMin))
    return shape.removeSplitter()


def translate_to_z0(shape: Part.Shape) -> Part.Shape:
    moved = shape.copy()
    moved.translate(App.Vector(0, 0, -moved.BoundBox.ZMin))
    return moved


def make_integrated_mount_shell() -> Part.Shape:
    shell = normalized_stackchan_shell()
    pieces: list[Part.Shape] = [shell]
    cutters: list[Part.Shape] = []

    # Direct RoverC mount: two side rails intersect the real shell body and
    # become part of the shell itself. The center under the shell stays open so
    # RoverC wiring remains accessible.
    # v7 shape restored: no raised seam-fill blocks. The rails are widened
    # inward at the same low height to increase bottom contact area without
    # covering the three-hole columns or making the screws/pins reach deeper.
    pieces.append(box("direct_roverc_left_rail", -27, 0, -0.2, 18, 26, 2.6))
    pieces.append(box("direct_roverc_right_rail", 27, 0, -0.2, 18, 26, 2.6))

    # RoverC side attachment pattern: three holes on each side. The slots are
    # set at 8 mm pitch to match the visible LEGO Technic-style
    # three-hole columns on each side of RoverC Pro.
    for x in (-31, 31):
        for y in (-8, 0, 8):
            pieces.append(cyl("direct_side_mount_pad", x, y, -0.2, 5.6, 2.6))
            cutters.append(cyl("lego_side_hole", x, y, -1.5, 2.6, 7))

    modified = fuse_all(pieces)
    for cutter in cutters:
        modified = modified.cut(cutter)
    return translate_to_z0(modified.removeSplitter())


def make_roverc_tray() -> Part.Shape:
    pieces: list[Part.Shape] = []
    cutters: list[Part.Shape] = []

    # Base plate.
    pieces.append(box("base_plate", 0, 0, 0, 72, 78, 4))

    # Low shell locating rim. Inner opening is intentionally loose around the
    # 54 x 42.5 mm Stack-chan shell footprint.
    pieces.extend(
        [
            box("rim_front", 0, -26, 4, 64, 4, 4),
            box("rim_rear", 0, 26, 4, 64, 4, 4),
            box("rim_left", -31, 0, 4, 4, 52, 4),
            box("rim_right", 31, 0, 4, 4, 52, 4),
        ]
    )

    # Front/rear shell bosses, matching the integrated shell's screw ears.
    for y in (-26.75, 26.75):
        pieces.append(box("tab_boss_bar", 0, y, 4, 58, 9, 4))
        for x in (-20, 20):
            pieces.append(cyl("tab_boss", x, y, 4, 5.5, 5))
            cutters.append(rounded_slot(x, y, 2, 10, 3.6, 10, along="x"))

    # CoreS3/DinBase top adjustment slots. These are left as adjustable rails
    # until the real DinBase hole choice is confirmed.
    for y in (-12, 12):
        for x in (-23, 23):
            pieces.append(box("cores3_slot_pad", x, y, 8, 18, 10, 3))
            cutters.append(rounded_slot(x, y, 7, 10, 3.6, 8, along="x"))

    # RoverC bottom attachment slots.
    for y in (-27, 27):
        for x in (-22, 22):
            pieces.append(cyl("bottom_slot_reinforcement", x, y, 0, 5.2, 4))
            cutters.append(rounded_slot(x, y, -1, 16, 4.8, 8, along="x"))

    tray = fuse_all(pieces)
    for cutter in cutters:
        tray = tray.cut(cutter)
    return tray.removeSplitter()

def make_assembly(tray: Part.Shape, shell: Part.Shape) -> Part.Shape:
    placed_shell = shell.copy()
    placed_shell.translate(App.Vector(0, 0, 8))
    return tray.fuse(placed_shell).removeSplitter()


def main() -> None:
    doc = App.newDocument("nikechan_roverc_body")
    shell = make_integrated_mount_shell()

    for name, shape in [
        ("stackchan_shell_roverc_inward_foot_mount_freecad_v10", shell),
    ]:
        obj = doc.addObject("Part::Feature", name)
        obj.Shape = shape
    doc.recompute()
    fcstd_path = OUT / "nikechan_roverc_body_freecad_v1.FCStd"
    OUT.mkdir(parents=True, exist_ok=True)
    doc.saveAs(str(fcstd_path))
    print(fcstd_path)

    export_shape("stackchan_shell_roverc_inward_foot_mount_freecad_v10", shell)


if __name__ == "__main__":
    main()
