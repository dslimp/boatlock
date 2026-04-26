#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

run() {
  echo
  echo "==> $*"
  "$@"
}

run_in() {
  local dir="$1"
  shift
  echo
  echo "==> (cd ${dir#$repo_root/} && $*)"
  (cd "$dir" && "$@")
}

run_in "$repo_root/boatlock" platformio test -e native
run_in "$repo_root/boatlock" pio run -e esp32s3

run python3 "$repo_root/tools/sim/test_sim_core.py"
run python3 "$repo_root/tools/sim/run_sim.py" --check --json-out /tmp/boatlock-local-core-report.json
run python3 "$repo_root/tools/sim/run_sim.py" --scenario-set all --check --json-out /tmp/boatlock-local-all-report.json
run python3 "$repo_root/tools/sim/test_soak.py"
run python3 "$repo_root/tools/sim/run_soak.py" --hours 6 --check --json-out /tmp/boatlock-local-soak-report.json

run_in "$repo_root/boatlock_ui" flutter analyze
run_in "$repo_root/boatlock_ui" flutter test

run python3 "$repo_root/tools/ci/check_firmware_version.py"
run python3 "$repo_root/tools/ci/check_config_schema_version.py"
run_in "$repo_root" pytest -q tools/ci/test_*.py
run bash "$repo_root/tools/ci/check_firmware_werror.sh"
run bash "$repo_root/tools/ci/check_firmware_cppcheck.sh"

echo
echo "BoatLock local pre-powered gate passed."
