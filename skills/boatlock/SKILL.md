---
name: boatlock
description: "Use when working in the BoatLock repository: ESP32-S3 firmware in `boatlock/`, the Flutter app in `boatlock_ui/`, the BLE ASCII protocol, BNO08x and GNSS behavior, security pairing/auth, or the on-device and offline HIL simulation workflow."
---

# BoatLock

## Overview

Use this skill for any code, review, debugging, test, or docs task in the BoatLock repo. It keeps firmware, BLE, Flutter, security, and simulation behavior aligned.

## Quick Start

- Treat code as the source of truth when prose docs disagree.
- Firmware lives in `boatlock/`, the Flutter client in `boatlock_ui/`, protocol/schema docs in `docs/`, and simulation plus CI helpers in `tools/`.
- Production firmware builds only top-level `boatlock/*.cpp`. Files under `boatlock/debug/` are manual sketches and must not silently replace production logic.
- This repo often has local WIP in firmware files. Check `git status` before editing and work with existing changes instead of reverting them.

## Read The Right Reference

- Firmware, hardware, runtime state, and invariants: `references/firmware.md`
- BLE protocol, Flutter coupling, and security envelope: `references/ble-ui.md`
- Build, test, simulation, and release/version workflow: `references/validation.md`

## High-Risk Invariants

- Compass support in main firmware is BNO08x only.
- Removed flows such as `SET_HEADING`, `EMU_COMPASS`, route commands, and log-export commands should stay removed unless the task explicitly restores them end-to-end.
- GPS position prefers hardware fix, then phone GPS fallback. Heading comes from onboard BNO08x only, optionally corrected by movement-derived GPS course.
- BLE identity and UUIDs are fixed: `BoatLock`, `12ab`, `34cd`, `56ef`, `78ab`.
- When `secPaired=1`, control and write commands must go through `SEC_CMD`; pairing opens only from the hardware STOP long-press flow.
- On-device HIL scenarios `S0..S19` are part of the regression surface.

## Working Pattern

1. Read only the reference file that matches the task.
2. Make the smallest change that fixes the problem without widening protocol or hardware scope by accident.
3. If a BLE command, status field, or telemetry key changes, update firmware, Flutter parsing, tests, and docs together.
4. Validate the narrowest relevant surface before finishing.
