#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

WAIT_SECS=1800
ANDROID_SERIAL=""
INSTALL_HELPERS=1
RUN_BOOT_ACCEPTANCE=1
RESTORE_RELEASE=1
ACCEPTANCE_FLASHED=0

usage() {
  cat >&2 <<'USAGE'
usage: tools/hw/nh02/run-sim-suite.sh [--serial ANDROID_SERIAL] [--wait-secs SECONDS] [--skip-helper-install] [--skip-boot-acceptance] [--no-restore-release]

Runs the standard on-device HIL simulation suite on nh02:
  1. install/refresh tracked nh02 helpers
  2. prove nh02 + Android visibility
  3. flash the acceptance firmware profile
  4. run boot acceptance
  5. run Android production-app e2e mode sim_suite
  6. restore the release firmware profile and run boot acceptance again
USAGE
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help)
      usage
      exit 0
      ;;
    --serial)
      ANDROID_SERIAL="${2:?missing Android serial}"
      shift 2
      ;;
    --wait-secs)
      WAIT_SECS="${2:?missing wait seconds}"
      shift 2
      ;;
    --skip-helper-install)
      INSTALL_HELPERS=0
      shift
      ;;
    --skip-boot-acceptance)
      RUN_BOOT_ACCEPTANCE=0
      shift
      ;;
    --no-restore-release)
      RESTORE_RELEASE=0
      shift
      ;;
    *)
      echo "unknown argument: $1" >&2
      usage
      exit 1
      ;;
  esac
done

restore_release() {
  if [[ "${RESTORE_RELEASE}" -eq 0 || "${ACCEPTANCE_FLASHED}" -eq 0 ]]; then
    return 0
  fi
  printf 'Restoring release firmware profile after sim suite...\n'
  "${SCRIPT_DIR}/flash.sh" --profile release
  if [[ "${RUN_BOOT_ACCEPTANCE}" -eq 1 ]]; then
    "${SCRIPT_DIR}/acceptance.sh" --seconds 20
  fi
}

cleanup() {
  status=$?
  if ! restore_release; then
    status=1
  fi
  exit "${status}"
}
trap cleanup EXIT

if [[ "${INSTALL_HELPERS}" -eq 1 ]]; then
  "${SCRIPT_DIR}/install.sh"
fi

"${SCRIPT_DIR}/status.sh"
"${SCRIPT_DIR}/android-status.sh"

"${SCRIPT_DIR}/flash.sh" --profile acceptance
ACCEPTANCE_FLASHED=1

if [[ "${RUN_BOOT_ACCEPTANCE}" -eq 1 ]]; then
  "${SCRIPT_DIR}/acceptance.sh" --seconds 20
fi

app_args=(--sim-suite --wait-secs "${WAIT_SECS}")
if [[ -n "${ANDROID_SERIAL}" ]]; then
  app_args+=(--serial "${ANDROID_SERIAL}")
fi

"${SCRIPT_DIR}/android-run-app-e2e.sh" "${app_args[@]}"
