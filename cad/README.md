# Nikechan Physical Body CAD

FreeCAD/Blender generated print data for mounting a CoreS3/Stack-chan shell
directly on a RoverC Pro body.

## Generate

Run from the repository root:

```bash
cad/run_freecad_generate.sh
```

This generates FreeCAD source/output under `cad/freecad-output/`, then re-exports
the printable STL through Blender into `cad/cura-ready/`.

## Print This

Use only this Cura-ready STL file for printing:

1. `cad/cura-ready/stackchan_shell_roverc_inward_foot_mount_cura_v10.stl`

There is no separate tray in the current design.

## Current Files

- `cad/freecad-output/nikechan_roverc_body_freecad_v1.FCStd`
- `cad/freecad-output/stackchan_shell_roverc_inward_foot_mount_freecad_v10.step`
- `cad/freecad-output/stackchan_shell_roverc_inward_foot_mount_freecad_v10.stl`
- `cad/cura-ready/stackchan_shell_roverc_inward_foot_mount_cura_v10.stl`

## Design Notes

- The Stack-chan shell is modified as a single body.
- The lower mount is integrated into the shell as flush, short left/right side rails.
- The center under the Stack-chan shell is open so RoverC wiring remains
  accessible.
- Six round LEGO-style mounting holes are built into the compact side rails: three on each side at 8 mm pitch, matching the visible RoverC side-hole columns. The rails are widened inward at the same low height to increase bottom contact area without making the screw/pin holes deeper.
- The Cura-ready STL is re-exported through Blender after FreeCAD output to
  avoid slicer warnings from FreeCAD's raw STL export.

## Sources

- CoreS3 official size:
  - Main unit: 54.0 x 54.0 x 15.5 mm
  - CoreS3 + DinBase: 69.0 x 54.0 x 31.5 mm
  - Official model-size DXF: `cad/vendor/m5stack-cores3/cores3.dxf`
- RoverC Pro official size:
  - 120.0 x 75.0 x 58.0 mm
- Stack-chan v1 shell:
  - `cad/vendor/stack-chan-v1/shell.step`
  - `cad/vendor/stack-chan-v1/shell.stl`
  - Apache-2.0 license retained in `cad/vendor/stack-chan-v1/LICENSE`

References:

- <https://docs.m5stack.com/en/core/CoreS3>
- <https://docs.m5stack.com/en/hat/hat_roverc_pro>
- <https://github.com/m5stack/M5_Hardware/tree/master/Products/K036-B_RoverC-Pro/Structures>
- <https://github.com/stack-chan/stack-chan>

## Print Settings

Suggested PLA+ settings:

- 0.20 mm layer height
- 3 walls
- 30-40% infill
- supports may be needed depending on slicer orientation
