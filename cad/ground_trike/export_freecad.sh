#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
freecadcmd="${FREECADCMD:-/Applications/FreeCAD.app/Contents/Resources/bin/freecadcmd}"

if [[ ! -x "$freecadcmd" ]]; then
  echo "FreeCAD command not found: $freecadcmd" >&2
  echo "Set FREECADCMD=/path/to/freecadcmd and retry." >&2
  exit 1
fi

BOATLOCK_FREECAD_SCRIPT="$script_dir/boatlock_ground_trike_freecad.py" "$freecadcmd" <<'PY'
import os
import traceback

script = os.environ["BOATLOCK_FREECAD_SCRIPT"]
namespace = {
    "__file__": script,
    "__name__": "__main__",
}
try:
    with open(script, "r", encoding="utf-8") as handle:
        exec(compile(handle.read(), script, "exec"), namespace)
except Exception:
    traceback.print_exc()
    raise SystemExit(1)
PY
