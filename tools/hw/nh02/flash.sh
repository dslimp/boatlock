#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

if [[ "${1:-}" != "--no-build" ]]; then
  (
    cd "${FIRMWARE_DIR}"
    "${BOATLOCK_PIO_BIN}" run -e esp32s3
  )
fi

remote_shell "mkdir -p '${BOATLOCK_NH02_REMOTE_STAGE}'"

"${RSYNC_BASE[@]}" \
  "${FIRMWARE_DIR}/.pio/build/esp32s3/bootloader.bin" \
  "${FIRMWARE_DIR}/.pio/build/esp32s3/partitions.bin" \
  "${FIRMWARE_DIR}/.pio/build/esp32s3/firmware.bin" \
  "${BOATLOCK_NH02_SSH_TARGET}:${BOATLOCK_NH02_REMOTE_STAGE}/"

remote_shell "'${BOATLOCK_NH02_REMOTE_FLASH_BIN}' '${BOATLOCK_NH02_REMOTE_STAGE}'"
