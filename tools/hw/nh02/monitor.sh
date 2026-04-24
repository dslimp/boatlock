#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

if [[ "$#" -eq 0 && -t 0 && -t 1 ]]; then
  exec "${BOATLOCK_MINITERM_BIN}" "${BOATLOCK_NH02_RFC2217_URL}" 115200
fi

exec "${BOATLOCK_PIO_PYTHON}" "${SCRIPT_DIR}/serial_monitor.py" \
  --url "${BOATLOCK_NH02_RFC2217_URL}" \
  "$@"
