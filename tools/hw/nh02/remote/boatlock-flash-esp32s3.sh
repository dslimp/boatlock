#!/usr/bin/env bash

set -euo pipefail

ARTIFACT_DIR="${1:-/opt/boatlock-hw/stage/esp32s3}"
RFC2217_SERVICE="${BOATLOCK_RFC2217_SERVICE:-boatlock-esp32s3-rfc2217.service}"
USB_DEV="${BOATLOCK_USB_DEV:-/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_98:88:E0:03:BA:5C-if00}"
ESPTOOL_BIN="${BOATLOCK_ESPTOOL_BIN:-/opt/boatlock-hw/.venv/bin/esptool}"
BAUD_RATE="${BOATLOCK_FLASH_BAUD:-460800}"

for file in bootloader.bin partitions.bin boot_app0.bin firmware.bin; do
  if [[ ! -f "${ARTIFACT_DIR}/${file}" ]]; then
    echo "missing artifact: ${ARTIFACT_DIR}/${file}" >&2
    exit 1
  fi
done

service_was_active=0
if systemctl is-active --quiet "${RFC2217_SERVICE}"; then
  service_was_active=1
  systemctl stop "${RFC2217_SERVICE}"
fi

cleanup() {
  if [[ "${service_was_active}" -eq 1 ]]; then
    systemctl start "${RFC2217_SERVICE}"
  fi
}
trap cleanup EXIT

"${ESPTOOL_BIN}" \
  --chip esp32s3 \
  --port "${USB_DEV}" \
  --baud "${BAUD_RATE}" \
  --before default-reset \
  --after hard-reset \
  write-flash \
  0x0 "${ARTIFACT_DIR}/bootloader.bin" \
  0x8000 "${ARTIFACT_DIR}/partitions.bin" \
  0xe000 "${ARTIFACT_DIR}/boot_app0.bin" \
  0x10000 "${ARTIFACT_DIR}/firmware.bin"
