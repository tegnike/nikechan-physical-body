#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FREECAD_CMD="/Applications/FreeCAD.app/Contents/Resources/bin/freecadcmd"

"$FREECAD_CMD" -c <<PY
path = "$ROOT/cad/freecad_generate.py"
globals_dict = {"__file__": path, "__name__": "__main__"}
exec(compile(open(path).read(), path, "exec"), globals_dict)
PY

"/Applications/Blender.app/Contents/MacOS/Blender" --background --python "$ROOT/cad/blender_repair_stl.py"
