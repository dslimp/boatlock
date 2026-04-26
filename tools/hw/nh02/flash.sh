#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

PIO_ENV="${BOATLOCK_PIO_ENV:-esp32s3}"
NO_BUILD=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-build)
      NO_BUILD=1
      shift
      ;;
    --env)
      PIO_ENV="${2:?missing PlatformIO environment}"
      shift 2
      ;;
    *)
      echo "unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

BUILD_DIR="${FIRMWARE_DIR}/.pio/build/${PIO_ENV}"
BOOT_APP0_BIN="${BOATLOCK_BOOT_APP0_BIN:-${HOME}/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin}"

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
