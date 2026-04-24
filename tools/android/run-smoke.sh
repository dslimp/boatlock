#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

BUILD_FIRST=1
INSTALL_APP=1
WAIT_SECS=45

while [[ $# -gt 0 ]]; do
  case "$1" in
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
  "${SCRIPT_DIR}/build-smoke-apk.sh" >/dev/null
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
    echo "adb install failed; staged APK for manual phone-side install: ${manual_apk}" >&2
    echo "unlock the phone, install boatlock-smoke.apk from Downloads, then rerun with --no-install" >&2
    exit 20
  fi
fi

for perm in "${ADB_PERMISSIONS[@]}"; do
  "${BOATLOCK_ANDROID_ADB_BIN}" shell pm grant "${BOATLOCK_ANDROID_PACKAGE}" "${perm}" >/dev/null 2>&1 || true
done

"${BOATLOCK_ANDROID_ADB_BIN}" logcat -c
"${BOATLOCK_ANDROID_ADB_BIN}" shell am force-stop "${BOATLOCK_ANDROID_PACKAGE}" >/dev/null 2>&1 || true
"${BOATLOCK_ANDROID_ADB_BIN}" shell am start -W -n "${BOATLOCK_ANDROID_ACTIVITY}" >/dev/null

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
