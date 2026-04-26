#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

DEFAULT_PIO_ENV="esp32s3"
if [[ "${BOATLOCK_PIO_ENV+x}" == "x" ]]; then
  PIO_ENV_REQUEST="${BOATLOCK_PIO_ENV}"
  PIO_ENV_REQUEST_SOURCE="BOATLOCK_PIO_ENV"
else
  PIO_ENV_REQUEST="${DEFAULT_PIO_ENV}"
  PIO_ENV_REQUEST_SOURCE="default"
fi
PROFILE_REQUEST=""
PROFILE_REQUEST_SOURCE=""
NO_BUILD=0

usage() {
  cat >&2 <<'USAGE'
usage: tools/hw/nh02/flash.sh [--no-build] [--env PLATFORMIO_ENV] [--profile release|service|acceptance]

Profile aliases:
  release     -> esp32s3_release
  service     -> esp32s3_service
  acceptance  -> esp32s3_acceptance

Legacy/profiled envs accepted through --env or BOATLOCK_PIO_ENV:
  esp32s3                         release-compatible legacy env
  esp32s3_release                 release
  esp32s3_service                 service
  esp32s3_acceptance              acceptance
  esp32s3_bno08x_sh2_uart         service
  esp32s3_debug_wifi_ota          service
  gpio_probe                      debug-probe, no command-scope firmware profile
  uart_rvc_probe_rx12             debug-probe, no command-scope firmware profile
USAGE
}

fail_usage() {
  echo "$1" >&2
  usage
  exit 1
}

resolve_profile() {
  case "$1" in
    release)
      PIO_ENV="esp32s3_release"
      COMMAND_PROFILE="release"
      ;;
    service)
      PIO_ENV="esp32s3_service"
      COMMAND_PROFILE="service"
      ;;
    acceptance)
      PIO_ENV="esp32s3_acceptance"
      COMMAND_PROFILE="acceptance"
      ;;
    "")
      fail_usage "${PROFILE_REQUEST_SOURCE} is empty"
      ;;
    *)
      fail_usage "unknown BoatLock command profile '${1}'"
      ;;
  esac
}

resolve_env() {
  case "$1" in
    release|service|acceptance)
      resolve_profile "$1"
      ;;
    esp32s3|esp32s3_release)
      PIO_ENV="$1"
      COMMAND_PROFILE="release"
      ;;
    esp32s3_service|esp32s3_bno08x_sh2_uart|esp32s3_debug_wifi_ota)
      PIO_ENV="$1"
      COMMAND_PROFILE="service"
      ;;
    esp32s3_acceptance)
      PIO_ENV="$1"
      COMMAND_PROFILE="acceptance"
      ;;
    gpio_probe|uart_rvc_probe_rx12)
      PIO_ENV="$1"
      COMMAND_PROFILE="debug-probe"
      ;;
    "")
      fail_usage "${PIO_ENV_REQUEST_SOURCE} is empty"
      ;;
    native)
      fail_usage "PlatformIO env 'native' is not flashable through nh02"
      ;;
    *)
      fail_usage "unknown or unprofiled PlatformIO env '${1}'"
      ;;
  esac
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -h|--help)
      usage
      exit 0
      ;;
    --no-build)
      NO_BUILD=1
      shift
      ;;
    --env)
      [[ $# -ge 2 ]] || fail_usage "missing PlatformIO environment"
      [[ -n "$2" ]] || fail_usage "PlatformIO environment is empty"
      PIO_ENV_REQUEST="$2"
      PIO_ENV_REQUEST_SOURCE="--env"
      PROFILE_REQUEST=""
      PROFILE_REQUEST_SOURCE=""
      shift 2
      ;;
    --profile)
      [[ $# -ge 2 ]] || fail_usage "missing BoatLock command profile"
      [[ -n "$2" ]] || fail_usage "BoatLock command profile is empty"
      PROFILE_REQUEST="$2"
      PROFILE_REQUEST_SOURCE="--profile"
      shift 2
      ;;
    *)
      fail_usage "unknown argument: $1"
      ;;
  esac
done

if [[ -n "${PROFILE_REQUEST}" ]]; then
  resolve_profile "${PROFILE_REQUEST}"
  REQUEST_DESCRIPTION="${PROFILE_REQUEST_SOURCE}=${PROFILE_REQUEST}"
else
  resolve_env "${PIO_ENV_REQUEST}"
  REQUEST_DESCRIPTION="${PIO_ENV_REQUEST_SOURCE}=${PIO_ENV_REQUEST}"
fi

BUILD_DIR="${FIRMWARE_DIR}/.pio/build/${PIO_ENV}"
BOOT_APP0_BIN="${BOATLOCK_BOOT_APP0_BIN:-${HOME}/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin}"

echo "BoatLock flash profile: ${COMMAND_PROFILE}"
echo "PlatformIO env: ${PIO_ENV} (${REQUEST_DESCRIPTION})"
if [[ "${COMMAND_PROFILE}" == "debug-probe" ]]; then
  echo "warning: ${PIO_ENV} is a debug probe build and has no release/service/acceptance command scope" >&2
fi

if [[ "${NO_BUILD}" -eq 0 ]]; then
  (
    cd "${FIRMWARE_DIR}"
    "${BOATLOCK_PIO_BIN}" run -e "${PIO_ENV}"
  )
fi

for image in bootloader.bin partitions.bin firmware.bin; do
  if [[ ! -f "${BUILD_DIR}/${image}" ]]; then
    echo "missing ${image} for PlatformIO env '${PIO_ENV}': ${BUILD_DIR}/${image}" >&2
    exit 1
  fi
done

if [[ ! -f "${BOOT_APP0_BIN}" ]]; then
  echo "missing boot_app0.bin: ${BOOT_APP0_BIN}" >&2
  exit 1
fi

remote_shell "mkdir -p '${BOATLOCK_NH02_REMOTE_STAGE}'"

"${RSYNC_BASE[@]}" \
  "${BUILD_DIR}/bootloader.bin" \
  "${BUILD_DIR}/partitions.bin" \
  "${BOOT_APP0_BIN}" \
  "${BUILD_DIR}/firmware.bin" \
  "${BOATLOCK_NH02_SSH_TARGET}:${BOATLOCK_NH02_REMOTE_STAGE}/"

remote_shell "'${BOATLOCK_NH02_REMOTE_FLASH_BIN}' '${BOATLOCK_NH02_REMOTE_STAGE}'"
