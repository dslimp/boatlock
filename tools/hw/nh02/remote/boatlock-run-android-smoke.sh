#!/usr/bin/env bash

set -euo pipefail

APK=""
PACKAGE=""
ACTIVITY=""
WAIT_SECS=45
SERIAL=""
INSTALL_APP=1
PERMISSIONS=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --apk)
      APK="${2:?missing apk path}"
      shift 2
      ;;
    --no-install)
      INSTALL_APP=0
      shift
      ;;
    --package)
      PACKAGE="${2:?missing package name}"
      shift 2
      ;;
    --activity)
      ACTIVITY="${2:?missing activity name}"
      shift 2
      ;;
    --wait-secs)
      WAIT_SECS="${2:?missing wait seconds}"
      shift 2
      ;;
    --serial)
      SERIAL="${2:?missing serial}"
      shift 2
      ;;
    --perm)
      PERMISSIONS+=("${2:?missing permission}")
      shift 2
      ;;
    *)
      echo "unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

if [[ -z "${PACKAGE}" || -z "${ACTIVITY}" ]]; then
  echo "package and activity are required" >&2
  exit 1
fi

if [[ "${INSTALL_APP}" -eq 1 ]]; then
  if [[ -z "${APK}" ]]; then
    echo "apk is required unless --no-install is used" >&2
    exit 1
  fi
  if [[ ! -f "${APK}" ]]; then
    echo "apk not found: ${APK}" >&2
    exit 1
  fi
fi

ADB=(adb)
if [[ -n "${SERIAL}" ]]; then
  ADB+=(-s "${SERIAL}")
fi

adb start-server >/dev/null 2>&1 || true

if [[ -z "${SERIAL}" ]]; then
  adb wait-for-device
fi

state="$("${ADB[@]}" get-state 2>/dev/null || true)"
if [[ "${state}" != "device" ]]; then
  echo "adb target is not ready: state='${state:-missing}'" >&2
  adb devices -l >&2 || true
  exit 1
fi

bt_state="$("${ADB[@]}" shell settings get global bluetooth_on 2>/dev/null | tr -d '\r' || true)"
loc_mode="$("${ADB[@]}" shell settings get secure location_mode 2>/dev/null | tr -d '\r' || true)"
printf 'bt_state=%s\n' "${bt_state:-unknown}"
printf 'location_mode=%s\n' "${loc_mode:-unknown}"

if [[ "${INSTALL_APP}" -eq 1 ]]; then
  if ! "${ADB[@]}" install -r "${APK}"; then
    manual_apk="/sdcard/Download/boatlock-smoke.apk"
    "${ADB[@]}" push "${APK}" "${manual_apk}" >/dev/null 2>&1 || true
    echo "adb install failed; staged APK for manual phone-side install: ${manual_apk}" >&2
    echo "unlock the phone, install boatlock-smoke.apk from Downloads, then rerun with --no-install" >&2
    exit 20
  fi
fi

for perm in "${PERMISSIONS[@]}"; do
  "${ADB[@]}" shell pm grant "${PACKAGE}" "${perm}" >/dev/null 2>&1 || true
done

"${ADB[@]}" logcat -c || true
"${ADB[@]}" shell am force-stop "${PACKAGE}" >/dev/null 2>&1 || true
"${ADB[@]}" shell am start -W -n "${ACTIVITY}" >/dev/null

deadline=$(( $(date +%s) + WAIT_SECS ))
result_line=""

while [[ $(date +%s) -lt ${deadline} ]]; do
  dump="$("${ADB[@]}" logcat -d 2>/dev/null || true)"
  result_line="$(printf '%s\n' "${dump}" | grep 'BOATLOCK_SMOKE_RESULT ' | tail -n 1 || true)"
  if [[ -n "${result_line}" ]]; then
    break
  fi
  sleep 2
done

if [[ -z "${result_line}" ]]; then
  printf 'no BOATLOCK_SMOKE_RESULT found in logcat within %ss\n' "${WAIT_SECS}" >&2
  printf 'recent relevant logcat:\n' >&2
  "${ADB[@]}" logcat -d 2>/dev/null | grep -E 'BoatLockSmoke|BOATLOCK_SMOKE_RESULT|FlutterBluePlus|flutter' | tail -n 120 >&2 || true
  exit 1
fi

printf '%s\n' "${result_line}"

if [[ "${result_line}" == *'"pass":true'* ]]; then
  exit 0
fi

exit 1
