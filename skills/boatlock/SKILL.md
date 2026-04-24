---
name: boatlock
description: "Use when working in the BoatLock repository: ESP32-S3 firmware in `boatlock/`, the Flutter app in `boatlock_ui/`, the BLE ASCII protocol, BNO08x and GNSS behavior, security pairing/auth, or the on-device and offline HIL simulation workflow."
---

# BoatLock

## Overview

Use this skill for any code, review, debugging, test, or docs task in the BoatLock repo. It keeps firmware, BLE, Flutter, security, and simulation behavior aligned.

## Quick Start

- Treat code as the source of truth when prose docs disagree.
- Treat simplicity as the default and prefer the smallest clear change that removes code or branching without widening behavior.
- Do not keep backward compatibility code in firmware, Flutter, tooling, or docs unless the user explicitly asks for compatibility in the current task.
- Because BoatLock is not released, delete obsolete commands, fields, shims, and UI flows; keep historical context only in `WORKLOG.md`.
- Firmware lives in `boatlock/`, the Flutter client in `boatlock_ui/`, protocol/schema docs in `docs/`, and simulation plus CI helpers in `tools/`.
- Production firmware builds only top-level `boatlock/*.cpp`. Files under `boatlock/debug/` are manual sketches and must not silently replace production logic.
- This repo often has local WIP in firmware files. Check `git status` before editing and work with existing changes instead of reverting them.

## Read The Right Reference

- Firmware, hardware, runtime state, and invariants: `references/firmware.md`
- BLE protocol, Flutter coupling, and security envelope: `references/ble-ui.md`
- Build, test, simulation, and release/version workflow: `references/validation.md`
- External best practices and comparable systems: `references/external-patterns.md`
- Real hardware acceptance on `nh02` and Android USB/BLE smoke planning: `../boatlock-hardware-acceptance/SKILL.md`

## Execution Log

- Keep the repo work trail in `WORKLOG.md`.
- Append one entry per meaningful stage with:
  - scope
  - decisions
  - validation
  - self-review
  - what should be promoted into the skill/reference set

## Self-Review Loop

1. Before editing a module, run a targeted external best-practice pass from official/primary sources for that module.
2. Record the applied baseline in `WORKLOG.md`; promote durable patterns into `references/external-patterns.md`.
3. Identify the narrowest useful cut.
4. After editing, inspect whether code became simpler or only moved around.
5. After validation, record the remaining weakness, not just the pass result.
6. If the user corrected a reusable workflow or scope assumption, capture it before continuing.
7. Promote only stable lessons into `references/*.md` or this skill; keep temporary task notes in `WORKLOG.md`.

## Execution Discipline

- Treat a detour as unfinished work, not as the fix.
- Fix the real blocker before defaulting to a workaround.
- Do not continue through an alternate path, side probe, fallback data source, or partial workaround until the blocker is fixed or the user explicitly waives that fix.
- Do not start a second path while the primary path is still making forward progress.
- Do not call a live build, flash, or validation run hung without concrete evidence.
- If a tracked wrapper or documented workflow is broken, fix that canonical path in the same turn unless the user explicitly waives it.
- If a tracked wrapper returns wrong, stale, or ambiguous output, fix the wrapper or its guidance before trusting downstream conclusions.
- When validating a tracked runtime or hardware path, the verdict must come from that path itself; use extra reads only as blocker-debug context.
- If the tracked validation run is still executing, report it as in progress rather than substituting a manual answer.
- If a detour already created the wrong artifact shape, reconcile that artifact through the canonical path before moving on.
- Do not treat cleanup-only or style-only requests as permission to change behavior. Call out any semantic fix separately.
- Preserve documented prerequisite order for wrappers and skills instead of editing around an out-of-order step.
- For hardware-side writes, prefer checked repo scripts and helpers over inline shell mutation.
- If a failure exists only inside the sandbox for connectivity or host access, rerun the real probe host-side before treating it as a target issue.

## High-Risk Invariants

- Compass support in main firmware is BNO08x only.
- Removed flows such as `SET_HEADING`, `EMU_COMPASS`, route commands, and log-export commands should stay removed unless the task explicitly restores them end-to-end.
- Protocol compatibility shims should stay removed unless the task explicitly restores compatibility end-to-end.
- GPS position prefers hardware fix, then phone GPS fallback. Heading comes from onboard BNO08x only, optionally corrected by movement-derived GPS course.
- Anchor point saving must be explicit and validated; never add default arguments or helper paths that arm Anchor as a side effect of storing coordinates.
- BLE identity and UUIDs are fixed: `BoatLock`, `12ab`, `34cd`, `56ef`, `78ab`.
- When `secPaired=1`, control and write commands must go through `SEC_CMD`; pairing opens only from the hardware STOP long-press flow.
- On-device HIL scenarios `S0..S19` are part of the regression surface.

## Working Pattern

1. Read only the reference file that matches the task.
2. Check the relevant official/primary external references before refactoring the module; if none apply, write that down.
3. Make the smallest change that fixes the problem without widening protocol or hardware scope by accident.
4. If a BLE command, status field, or telemetry key changes, update firmware, Flutter parsing, tests, and docs together.
5. Validate the narrowest relevant surface before finishing.
6. Add or update a `WORKLOG.md` stage entry for every meaningful work slice.
7. Run a brief self-review and promote only durable insights into the skill/reference files.
