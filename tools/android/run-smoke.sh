#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

BUILD_FIRST=1
INSTALL_APP=1
WAIT_SECS=45
SMOKE_MODE="basic"
CYCLE_BLUETOOTH=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --reconnect)
      SMOKE_MODE="reconnect"
      CYCLE_BLUETOOTH=1
      shift
      ;;
    --manual)
      SMOKE_MODE="manual"
      shift
      ;;
    --no-build)
      BUILD_FIRST=0
      shift
      ;;
    --no-install)
      INSTALL_APP=0
      shift
      ;;
    --wait-secs)
      WAIT_SECS="${2:?missing wait seconds}"
      shift 2
      ;;
    *)
      echo "unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

if [[ "${BUILD_FIRST}" -eq 1 ]]; then
  "${SCRIPT_DIR}/build-smoke-apk.sh" --mode "${SMOKE_MODE}" >/dev/null
fi

"${BOATLOCK_ANDROID_ADB_BIN}" start-server >/dev/null
"${BOATLOCK_ANDROID_ADB_BIN}" wait-for-device

if ! "${BOATLOCK_ANDROID_ADB_BIN}" get-state >/dev/null 2>&1; then
  echo "adb device is not ready" >&2
  exit 1
fi

if [[ "${INSTALL_APP}" -eq 1 ]]; then
  if ! "${BOATLOCK_ANDROID_ADB_BIN}" install -r "${BOATLOCK_ANDROID_APK}"; then
    manual_apk="/sdcard/Download/boatlock-smoke.apk"
    "${BOATLOCK_ANDROID_ADB_BIN}" push "${BOATLOCK_ANDROID_APK}" "${manual_apk}" >/dev/null 2>&1 || true
    echo "adb install failed; staged APK for phone-side inspection only: ${manual_apk}" >&2
    echo "fix the phone-side install policy, keep the phone unlocked for prompts, then rerun the same smoke command with install enabled" >&2
    echo "--no-install is BLE-runtime debug only and is not acceptance unless explicitly waived" >&2
    exit 20
  fi
fi

for perm in "${ADB_PERMISSIONS[@]}"; do
  "${BOATLOCK_ANDROID_ADB_BIN}" shell pm grant "${BOATLOCK_ANDROID_PACKAGE}" "${perm}" >/dev/null 2>&1 || true
done

"${BOATLOCK_ANDROID_ADB_BIN}" logcat -c
"${BOATLOCK_ANDROID_ADB_BIN}" shell am force-stop "${BOATLOCK_ANDROID_PACKAGE}" >/dev/null 2>&1 || true
"${BOATLOCK_ANDROID_ADB_BIN}" shell am start -W -n "${BOATLOCK_ANDROID_ACTIVITY}" >/dev/null

if [[ "${CYCLE_BLUETOOTH}" -eq 1 ]]; then
  ready_deadline=$(( $(date +%s) + 60 ))
  ready_line=""
  while [[ $(date +%s) -lt ${ready_deadline} ]]; do
    dump="$("${BOATLOCK_ANDROID_ADB_BIN}" logcat -d 2>/dev/null || true)"
    ready_line="$(printf '%s\n' "${dump}" | rg 'BOATLOCK_SMOKE_STAGE .*first_telemetry' | tail -n 1 || true)"
    if [[ -n "${ready_line}" ]]; then
      break
    fi
    sleep 2
  done
  if [[ -z "${ready_line}" ]]; then
    echo "reconnect smoke did not reach first telemetry before Bluetooth cycle" >&2
    exit 1
  fi
  "${BOATLOCK_ANDROID_ADB_BIN}" shell svc bluetooth disable >/dev/null 2>&1 || true
  sleep 7
  "${BOATLOCK_ANDROID_ADB_BIN}" shell svc bluetooth enable >/dev/null 2>&1 || true
fi

deadline=$(( $(date +%s) + WAIT_SECS ))
result_line=""

while [[ $(date +%s) -lt ${deadline} ]]; do
  dump="$("${BOATLOCK_ANDROID_ADB_BIN}" logcat -d 2>/dev/null || true)"
  result_line="$(printf '%s\n' "${dump}" | rg 'BOATLOCK_SMOKE_RESULT ' | tail -n 1 || true)"
  if [[ -n "${result_line}" ]]; then
    break
  fi
  sleep 2
done

if [[ -z "${result_line}" ]]; then
  echo "no BOATLOCK_SMOKE_RESULT found in logcat within ${WAIT_SECS}s" >&2
  exit 1
fi

printf '%s\n' "${result_line}"

if [[ "${result_line}" == *'"pass":true'* ]]; then
  exit 0
fi

exit 1
