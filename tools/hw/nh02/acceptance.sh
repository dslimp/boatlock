#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

RESET_FIRST=1
ARGS=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-reset)
      RESET_FIRST=0
      shift
      ;;
    *)
      ARGS+=("$1")
      shift
      ;;
  esac
done

if [[ "${RESET_FIRST}" -eq 1 ]]; then
  if ! remote_shell "test -x '${BOATLOCK_NH02_REMOTE_RESET_BIN}'"; then
    echo "missing remote reset helper on nh02; run tools/hw/nh02/install.sh first" >&2
    exit 1
  fi
  remote_shell "'${BOATLOCK_NH02_REMOTE_RESET_BIN}'"
fi

exec "${BOATLOCK_PIO_PYTHON}" "${SCRIPT_DIR}/acceptance.py" \
  --url "${BOATLOCK_NH02_RFC2217_URL}" \
  "${ARGS[@]}"
