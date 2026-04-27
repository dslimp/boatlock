#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

BUILD_APP=1
BUILD_FIRMWARE=1
REFRESH_ANDROID_HELPERS=1
FIRMWARE_BIN="${FIRMWARE_DIR}/.pio/build/esp32s3/firmware.bin"
WAIT_SECS=1800
ANDROID_SERIAL=""
OTA_LATEST_RELEASE=0
OTA_PORT=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-build)
      BUILD_APP=0
      BUILD_FIRMWARE=0
      shift
      ;;
    --no-app-build)
      BUILD_APP=0
      shift
      ;;
    --no-firmware-build)
      BUILD_FIRMWARE=0
      shift
      ;;
    --no-helper-refresh)
      REFRESH_ANDROID_HELPERS=0
      shift
      ;;
    --firmware)
      FIRMWARE_BIN="${2:?missing firmware path}"
      shift 2
      ;;
    --wait-secs)
      WAIT_SECS="${2:?missing wait seconds}"
      shift 2
      ;;
    --serial)
      ANDROID_SERIAL="${2:?missing android serial}"
      shift 2
      ;;
    --ota-port)
      OTA_PORT="${2:?missing OTA port}"
      shift 2
      ;;
    --ota-latest-release)
      OTA_LATEST_RELEASE=1
      shift
      ;;
    *)
      echo "unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

if [[ "${BUILD_FIRMWARE}" -eq 1 && "${FIRMWARE_BIN}" != "${FIRMWARE_DIR}/.pio/build/esp32s3/firmware.bin" ]]; then
  echo "--firmware with a custom path requires --no-firmware-build or --no-build" >&2
  exit 1
fi

if [[ "${BUILD_FIRMWARE}" -eq 1 ]]; then
  (
    cd "${FIRMWARE_DIR}"
    "${BOATLOCK_PIO_BIN}" run -e esp32s3
  )
fi

if [[ ! -f "${FIRMWARE_BIN}" ]]; then
  echo "firmware not found: ${FIRMWARE_BIN}" >&2
  exit 1
fi

if [[ "${BUILD_APP}" -eq 1 ]]; then
  "${REPO_ROOT}/tools/android/build-app-apk.sh"
fi

APK="${REPO_ROOT}/boatlock_ui/build/app/outputs/flutter-apk/app-release.apk"
if [[ ! -f "${APK}" ]]; then
  echo "Android app APK not found: ${APK}" >&2
  exit 1
fi

firmware_sha="$(shasum -a 256 "${FIRMWARE_BIN}" | awk '{print $1}')"
apk_sha="$(shasum -a 256 "${APK}" | awk '{print $1}')"

printf 'deploy_target=nh02 normal_path=phone_ble_ota\n'
printf 'firmware=%s\n' "${FIRMWARE_BIN}"
printf 'firmware_sha256=%s\n' "${firmware_sha}"
printf 'apk=%s\n' "${APK}"
printf 'apk_sha256=%s\n' "${apk_sha}"
printf 'wait_secs=%s\n' "${WAIT_SECS}"

if [[ "${REFRESH_ANDROID_HELPERS}" -eq 1 ]]; then
  "${SCRIPT_DIR}/install.sh" --android-only
  "${SCRIPT_DIR}/android-install.sh"
fi

args=(
  --no-build
  --ota
  --ota-firmware "${FIRMWARE_BIN}"
  --ota-sha256 "${firmware_sha}"
  --wait-secs "${WAIT_SECS}"
)

if [[ -n "${ANDROID_SERIAL}" ]]; then
  args+=(--serial "${ANDROID_SERIAL}")
fi

if [[ -n "${OTA_PORT}" ]]; then
  args+=(--ota-port "${OTA_PORT}")
fi

if [[ "${OTA_LATEST_RELEASE}" -eq 1 ]]; then
  args+=(--ota-latest-release)
fi

"${SCRIPT_DIR}/android-run-app-check.sh" "${args[@]}"
