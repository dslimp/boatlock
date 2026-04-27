#!/usr/bin/env bash

set -euo pipefail

APK=""
PACKAGE=""
ACTIVITY=""
WAIT_SECS=45
SERIAL=""
INSTALL_APP=1
APP_MODE="basic"
CYCLE_BLUETOOTH=0
RESET_ESP32=0
ESP32_RESET_BIN="${BOATLOCK_ESP32_RESET_BIN:-/opt/boatlock-hw/bin/boatlock-reset-esp32s3.sh}"
OTA_FIRMWARE=""
OTA_PORT=18080
OTA_SHA256=""
OTA_LATEST_RELEASE=0
OTA_SERVER_PID=""
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
    --mode)
      APP_MODE="${2:?missing app check mode}"
      shift 2
      ;;
    --cycle-bluetooth)
      CYCLE_BLUETOOTH=1
      shift
      ;;
    --reset-esp32)
      RESET_ESP32=1
      shift
      ;;
    --esp32-reset-bin)
      ESP32_RESET_BIN="${2:?missing ESP32 reset helper path}"
      shift 2
      ;;
    --ota-firmware)
      OTA_FIRMWARE="${2:?missing OTA firmware path}"
      shift 2
      ;;
    --ota-port)
      OTA_PORT="${2:?missing OTA port}"
      shift 2
      ;;
    --ota-sha256)
      OTA_SHA256="${2:?missing OTA SHA-256}"
      shift 2
      ;;
    --ota-latest-release)
      OTA_LATEST_RELEASE=1
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

if [[ "${CYCLE_BLUETOOTH}" -eq 1 && "${RESET_ESP32}" -eq 1 ]]; then
  echo "choose only one reconnect trigger: --cycle-bluetooth or --reset-esp32" >&2
  exit 1
fi

if [[ "${RESET_ESP32}" -eq 1 && ! -x "${ESP32_RESET_BIN}" ]]; then
  echo "ESP32 reset helper is missing or not executable: ${ESP32_RESET_BIN}" >&2
  exit 1
fi

if [[ -n "${OTA_FIRMWARE}" && ! -f "${OTA_FIRMWARE}" ]]; then
  echo "OTA firmware not found: ${OTA_FIRMWARE}" >&2
  exit 1
fi

if [[ -n "${OTA_FIRMWARE}" && -z "${OTA_SHA256}" ]]; then
  echo "OTA SHA-256 is required when OTA firmware is provided" >&2
  exit 1
fi

cleanup() {
  if [[ -n "${OTA_SERVER_PID}" ]]; then
    kill "${OTA_SERVER_PID}" >/dev/null 2>&1 || true
    wait "${OTA_SERVER_PID}" >/dev/null 2>&1 || true
  fi
  if [[ -n "${OTA_FIRMWARE}" ]]; then
    "${ADB[@]}" reverse --remove "tcp:${OTA_PORT}" >/dev/null 2>&1 || true
  fi
}
trap cleanup EXIT

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

keep_phone_awake() {
  "${ADB[@]}" shell input keyevent KEYCODE_WAKEUP >/dev/null 2>&1 || true
  "${ADB[@]}" shell wm dismiss-keyguard >/dev/null 2>&1 || true
}

keep_phone_awake

bt_state="$("${ADB[@]}" shell settings get global bluetooth_on 2>/dev/null | tr -d '\r' || true)"
loc_mode="$("${ADB[@]}" shell settings get secure location_mode 2>/dev/null | tr -d '\r' || true)"
printf 'bt_state=%s\n' "${bt_state:-unknown}"
printf 'location_mode=%s\n' "${loc_mode:-unknown}"

if [[ "${INSTALL_APP}" -eq 1 ]]; then
  install_output=""
  install_status=1
  for attempt in 1 2 3; do
    set +e
    install_output="$("${ADB[@]}" install -r "${APK}" 2>&1)"
    install_status=$?
    set -e
    printf '%s\n' "${install_output}"
    if [[ "${install_status}" -eq 0 ]]; then
      break
    fi
    if [[ "${install_output}" != *"INSTALL_FAILED_USER_RESTRICTED"* ]]; then
      break
    fi
    printf 'adb install attempt %s hit USER_RESTRICTED; retrying canonical install\n' "${attempt}" >&2
    sleep 2
  done
  if [[ "${install_status}" -ne 0 ]]; then
    manual_apk="/sdcard/Download/boatlock-app.apk"
    "${ADB[@]}" push "${APK}" "${manual_apk}" >/dev/null 2>&1 || true
    echo "adb install failed; staged APK for phone-side inspection only: ${manual_apk}" >&2
    echo "fix the phone-side install policy, keep the phone unlocked for prompts, then rerun the same smoke command with install enabled" >&2
    echo "--no-install is BLE-runtime debug only and is not acceptance unless explicitly waived" >&2
    exit 20
  fi
fi

for perm in "${PERMISSIONS[@]}"; do
  "${ADB[@]}" shell pm grant "${PACKAGE}" "${perm}" >/dev/null 2>&1 || true
done

if [[ -n "${OTA_FIRMWARE}" ]]; then
  ota_dir="$(dirname "${OTA_FIRMWARE}")"
  ota_name="$(basename "${OTA_FIRMWARE}")"
  if [[ "${ota_name}" != "firmware.bin" ]]; then
    echo "OTA firmware remote path must be named firmware.bin: ${OTA_FIRMWARE}" >&2
    exit 1
  fi
  "${ADB[@]}" reverse --remove "tcp:${OTA_PORT}" >/dev/null 2>&1 || true
  "${ADB[@]}" reverse "tcp:${OTA_PORT}" "tcp:${OTA_PORT}"
  (
    cd "${ota_dir}"
    python3 -m http.server "${OTA_PORT}" --bind 127.0.0.1
  ) >/tmp/boatlock-ota-http.log 2>&1 &
  OTA_SERVER_PID="$!"
  sleep 1
  if ! kill -0 "${OTA_SERVER_PID}" >/dev/null 2>&1; then
    echo "OTA HTTP server failed to start" >&2
    cat /tmp/boatlock-ota-http.log >&2 || true
    exit 1
  fi
  printf 'ota_server=127.0.0.1:%s firmware=%s\n' "${OTA_PORT}" "${OTA_FIRMWARE}"
fi

"${ADB[@]}" logcat -c || true
"${ADB[@]}" shell am force-stop "${PACKAGE}" >/dev/null 2>&1 || true
keep_phone_awake
start_args=(-W -n "${ACTIVITY}" --es boatlock_check_mode "${APP_MODE}")
if [[ -n "${OTA_FIRMWARE}" ]]; then
  start_args+=(--es boatlock_ota_url "http://127.0.0.1:${OTA_PORT}/firmware.bin")
  start_args+=(--es boatlock_ota_sha256 "${OTA_SHA256}")
fi
if [[ "${OTA_LATEST_RELEASE}" -eq 1 ]]; then
  start_args+=(--ez boatlock_ota_latest_release true)
  start_args+=(--es boatlock_firmware_manifest_url "http://127.0.0.1:${OTA_PORT}/manifest.json")
fi
"${ADB[@]}" shell am start "${start_args[@]}" >/dev/null

if [[ "${CYCLE_BLUETOOTH}" -eq 1 || "${RESET_ESP32}" -eq 1 ]]; then
  ready_deadline=$(( $(date +%s) + 60 ))
  ready_line=""
  while [[ $(date +%s) -lt ${ready_deadline} ]]; do
    dump="$("${ADB[@]}" logcat -d 2>/dev/null || true)"
    ready_line="$(printf '%s\n' "${dump}" | grep 'BOATLOCK_SMOKE_STAGE .*first_telemetry' | tail -n 1 || true)"
    if [[ -n "${ready_line}" ]]; then
      break
    fi
    sleep 2
  done
  if [[ -z "${ready_line}" ]]; then
    printf 'reconnect smoke did not reach first telemetry before recovery trigger\n' >&2
    exit 1
  fi
  if [[ "${CYCLE_BLUETOOTH}" -eq 1 ]]; then
    "${ADB[@]}" shell svc bluetooth disable >/dev/null 2>&1 || true
    sleep 7
    "${ADB[@]}" shell svc bluetooth enable >/dev/null 2>&1 || true
  else
    "${ESP32_RESET_BIN}"
  fi
fi

deadline=$(( $(date +%s) + WAIT_SECS ))
result_line=""
progress_line=""
last_progress_line=""

while [[ $(date +%s) -lt ${deadline} ]]; do
  keep_phone_awake
  dump="$("${ADB[@]}" logcat -d 2>/dev/null || true)"
  result_line="$(printf '%s\n' "${dump}" | grep 'BOATLOCK_SMOKE_RESULT ' | tail -n 1 || true)"
  if [[ -n "${result_line}" ]]; then
    break
  fi
  progress_line="$(printf '%s\n' "${dump}" | grep -E 'BOATLOCK_OTA_PROGRESS|BoatLockAppCheck.*ota_progress|\\[OTA\\] progress|BLE OTA progress|BoatLockAppCheck.*sim_suite_report|BOATLOCK_SMOKE_STAGE .*sim_suite_' | tail -n 1 || true)"
  if [[ -n "${progress_line}" && "${progress_line}" != "${last_progress_line}" ]]; then
    printf '%s\n' "${progress_line}"
    last_progress_line="${progress_line}"
  fi
  sleep 2
done

if [[ -z "${result_line}" ]]; then
  printf 'no BOATLOCK_SMOKE_RESULT found in logcat within %ss\n' "${WAIT_SECS}" >&2
  printf 'recent relevant logcat:\n' >&2
  "${ADB[@]}" logcat -d 2>/dev/null | grep -E 'BoatLockSmoke|BoatLockAppCheck|BOATLOCK_SMOKE_RESULT|BOATLOCK_OTA_PROGRESS|FlutterBluePlus|flutter|\\[OTA\\]|\\[SIM\\]' | tail -n 160 >&2 || true
  exit 1
fi

printf '%s\n' "${result_line}"

if [[ "${result_line}" == *'"pass":true'* ]]; then
  exit 0
fi

exit 1
