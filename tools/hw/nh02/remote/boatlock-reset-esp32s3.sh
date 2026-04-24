#!/usr/bin/env bash

set -euo pipefail

RFC2217_SERVICE="${BOATLOCK_RFC2217_SERVICE:-boatlock-esp32s3-rfc2217.service}"
USB_DEV="${BOATLOCK_USB_DEV:-/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_98:88:E0:03:BA:5C-if00}"
ESPTOOL_BIN="${BOATLOCK_ESPTOOL_BIN:-/opt/boatlock-hw/.venv/bin/esptool}"

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
  --before default-reset \
  --after hard-reset \
  chip-id >/dev/null
