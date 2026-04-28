#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"
source "${REPO_ROOT}/tools/android/common.sh"

BUILD_FIRST=1
INSTALL_APP=1
WAIT_SECS=45
ANDROID_SERIAL=""
APP_MODE="basic"
CYCLE_BLUETOOTH=0
RESET_ESP32=0
OTA_FIRMWARE=""
OTA_MANIFEST=""
OTA_PORT=18080
OTA_SHA256=""
OTA_LATEST_RELEASE=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --reconnect)
      APP_MODE="reconnect"
      CYCLE_BLUETOOTH=1
      shift
      ;;
    --manual)
      APP_MODE="manual"
      shift
      ;;
    --status)
      APP_MODE="status"
      shift
      ;;
    --sim)
      APP_MODE="sim"
      shift
      ;;
    --sim-suite)
      APP_MODE="sim_suite"
      shift
      ;;
    --anchor)
      APP_MODE="anchor"
      shift
      ;;
    --compass)
      APP_MODE="compass"
      shift
      ;;
    --gps)
      APP_MODE="gps"
      shift
      ;;
    --mode)
      APP_MODE="${2:?missing app check mode}"
      shift 2
      ;;
    --ota)
      APP_MODE="ota"
      shift
      ;;
    --ota-latest-release)
      APP_MODE="ota"
      OTA_LATEST_RELEASE=1
      shift
      ;;
    --esp-reset)
      APP_MODE="reconnect"
      RESET_ESP32=1
      shift
      ;;
    --ota-firmware)
      OTA_FIRMWARE="${2:?missing OTA firmware path}"
      shift 2
      ;;
    --ota-manifest)
      OTA_MANIFEST="${2:?missing OTA manifest path}"
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
    --serial)
      ANDROID_SERIAL="${2:?missing android serial}"
      shift 2
      ;;
    *)
      echo "unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

boatlock_validate_app_check_mode "${APP_MODE}"

if [[ "${CYCLE_BLUETOOTH}" -eq 1 && "${RESET_ESP32}" -eq 1 ]]; then
  echo "choose only one reconnect trigger: --reconnect or --esp-reset" >&2
  exit 1
fi

if [[ "${BUILD_FIRST}" -eq 1 ]]; then
  "${REPO_ROOT}/tools/android/build-app-apk.sh" >/dev/null
fi

if [[ -z "${ANDROID_SERIAL}" && "${BOATLOCK_NH02_ANDROID_WIFI_ADB:-1}" -eq 1 ]]; then
  ANDROID_SERIAL="$(boatlock_nh02_android_wifi_serial)"
  if [[ -z "${ANDROID_SERIAL}" ]]; then
    echo "could not enable Android ADB Wi-Fi" >&2
    exit 1
  fi
fi

if [[ "${INSTALL_APP}" -eq 1 && ! -f "${BOATLOCK_ANDROID_APK}" ]]; then
  echo "Android app APK not found at ${BOATLOCK_ANDROID_APK}" >&2
  exit 1
fi

remote_apk_path=""
if [[ "${INSTALL_APP}" -eq 1 ]]; then
  remote_shell "mkdir -p '${BOATLOCK_NH02_REMOTE_ANDROID_STAGE}'"
  remote_apk_path="${BOATLOCK_NH02_REMOTE_ANDROID_STAGE}/$(basename "${BOATLOCK_ANDROID_APK}")"
  "${RSYNC_BASE[@]}" "${BOATLOCK_ANDROID_APK}" "${BOATLOCK_NH02_SSH_TARGET}:${remote_apk_path}"
  printf 'apk_stage=%s\n' "${remote_apk_path}"
fi

remote_ota_path=""
if [[ -n "${OTA_FIRMWARE}" ]]; then
  if [[ ! -f "${OTA_FIRMWARE}" ]]; then
    echo "OTA firmware not found: ${OTA_FIRMWARE}" >&2
    exit 1
  fi
  if [[ -n "${OTA_MANIFEST}" && ! -f "${OTA_MANIFEST}" ]]; then
    echo "OTA manifest not found: ${OTA_MANIFEST}" >&2
    exit 1
  fi
  remote_shell "mkdir -p '${BOATLOCK_NH02_REMOTE_ANDROID_STAGE}'"
  remote_ota_path="${BOATLOCK_NH02_REMOTE_ANDROID_STAGE}/firmware.bin"
  "${RSYNC_BASE[@]}" "${OTA_FIRMWARE}" "${BOATLOCK_NH02_SSH_TARGET}:${remote_ota_path}"
  if [[ -z "${OTA_SHA256}" ]]; then
    OTA_SHA256="$(shasum -a 256 "${OTA_FIRMWARE}" | awk '{print $1}')"
  fi
  printf 'firmware_stage=%s sha256=%s\n' "${remote_ota_path}" "${OTA_SHA256}"
  if [[ -n "${OTA_MANIFEST}" ]]; then
    "${RSYNC_BASE[@]}" "${OTA_MANIFEST}" "${BOATLOCK_NH02_SSH_TARGET}:${BOATLOCK_NH02_REMOTE_ANDROID_STAGE}/manifest.json"
    printf 'manifest_stage=%s\n' "${BOATLOCK_NH02_REMOTE_ANDROID_STAGE}/manifest.json"
  fi
fi

serial_arg=""
if [[ -n "${ANDROID_SERIAL}" ]]; then
  serial_arg="--serial '${ANDROID_SERIAL}'"
fi

perm_args=""
for perm in "${ADB_PERMISSIONS[@]}"; do
  perm_args+=" --perm '${perm}'"
done

install_arg=""
if [[ "${INSTALL_APP}" -eq 0 ]]; then
  install_arg="--no-install"
fi

cycle_arg=""
if [[ "${CYCLE_BLUETOOTH}" -eq 1 ]]; then
  cycle_arg="--cycle-bluetooth"
fi

reset_arg=""
if [[ "${RESET_ESP32}" -eq 1 ]]; then
  reset_arg="--reset-esp32 --esp32-reset-bin '${BOATLOCK_NH02_REMOTE_RESET_BIN}'"
fi

ota_arg=""
if [[ -n "${remote_ota_path}" ]]; then
  ota_arg="--ota-firmware '${remote_ota_path}' --ota-port '${OTA_PORT}' --ota-sha256 '${OTA_SHA256}'"
  if [[ "${OTA_LATEST_RELEASE}" -eq 1 ]]; then
    ota_arg+=" --ota-latest-release"
  fi
elif [[ "${OTA_LATEST_RELEASE}" -eq 1 ]]; then
  ota_arg="--ota-latest-release"
fi

printf 'remote_app_check mode=%s install=%s wait_secs=%s serial=%s ota=%s\n' \
  "${APP_MODE}" "${INSTALL_APP}" "${WAIT_SECS}" "${ANDROID_SERIAL:-auto}" "$([[ -n "${remote_ota_path}" ]] && echo 1 || echo 0)"

remote_shell "'${BOATLOCK_NH02_REMOTE_ANDROID_SMOKE_BIN}' \
  --package '${BOATLOCK_ANDROID_PACKAGE}' \
  --activity '${BOATLOCK_ANDROID_ACTIVITY}' \
  --mode '${APP_MODE}' \
  --wait-secs '${WAIT_SECS}' \
  ${install_arg} \
  ${cycle_arg} \
  ${reset_arg} \
  ${ota_arg} \
  ${serial_arg} \
  ${perm_args} \
  ${remote_apk_path:+--apk '${remote_apk_path}'}"
