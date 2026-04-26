# BoatLock CAD

This directory contains printable and inspectable mechanical models for
BoatLock bench and ground-simulator hardware.

## FreeCAD MCP

BoatLock CAD work uses the `neka-nat/freecad-mcp` server:

- MCP package: <https://github.com/neka-nat/freecad-mcp>
- FreeCAD addon path on this workstation:
  `~/Library/Application Support/FreeCAD/v1-1/Mod/FreeCADMCP`
- RPC endpoint when FreeCAD is running: `127.0.0.1:9875`
- Codex MCP server command: `/Users/user/.local/bin/uvx freecad-mcp`

FreeCAD must be running for MCP operations that inspect or mutate the live
model. The addon settings should keep `auto_start_rpc=true`; verify with:

```bash
python3 tools/cad/freecad_mcp_smoke.py
```

Headless, repeatable exports should stay script-driven and committed next to
the model source. For the ground trike:

```bash
cad/ground_trike/export_freecad.sh
```

## Modeling Rules

- Keep source models in text form where practical: FreeCAD Python, OpenSCAD, or
  small structured config files.
- Generated `.FCStd`, STL, screenshots, and temporary mesh outputs belong under
  each model's `out/` directory unless they are intentionally promoted.
- Model load paths explicitly: wheel loads should go into axles, bearings,
  forks, brackets, and deck screws, not motor shafts or printed cosmetic ribs.
- Include reference solids for motors, wheels, bearings, screws, shafts, and
  steering stops before treating a model as printable.
- Print/export scripts must report dimensions and solid-count checks.
