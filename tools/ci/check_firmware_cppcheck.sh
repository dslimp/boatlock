#!/usr/bin/env bash
set -euo pipefail

if [[ -n "${CPPCHECK_BIN:-}" ]]; then
  cppcheck_bin="$CPPCHECK_BIN"
elif command -v cppcheck >/dev/null 2>&1; then
  cppcheck_bin="$(command -v cppcheck)"
elif [[ -x "$HOME/.platformio/packages/tool-cppcheck/cppcheck" ]]; then
  cppcheck_bin="$HOME/.platformio/packages/tool-cppcheck/cppcheck"
else
  echo "cppcheck binary not found" >&2
  exit 2
fi

"$cppcheck_bin" \
  --language=c++ \
  --std=c++17 \
  --enable=warning,performance,portability \
  --error-exitcode=1 \
  -Iboatlock \
  -Iboatlock/test/mocks \
  boatlock/GnssQualityGate.h \
  boatlock/AnchorSupervisor.h \
  boatlock/Settings.h \
  boatlock/MotorControl.h \
  boatlock/BleCommandHandler.h \
  boatlock/StepperControl.h \
  boatlock/AnchorProfiles.h \
  boatlock/AnchorReasons.h
