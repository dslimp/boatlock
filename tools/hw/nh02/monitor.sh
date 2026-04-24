#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

exec "${BOATLOCK_MINITERM_BIN}" "${BOATLOCK_NH02_RFC2217_URL}" 115200
