#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

PORT=5555
ANDROID_SERIAL=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --port)
      PORT="${2:?missing tcpip port}"
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

serial_arg=""
if [[ -n "${ANDROID_SERIAL}" ]]; then
  serial_arg="--serial '${ANDROID_SERIAL}'"
fi

remote_shell "'${BOATLOCK_NH02_REMOTE_ANDROID_WIFI_DEBUG_BIN}' --port '${PORT}' ${serial_arg}"
