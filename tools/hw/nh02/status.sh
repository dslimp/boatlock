#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

remote_shell "printf 'service=%s\n' '${BOATLOCK_NH02_REMOTE_RFC2217_SERVICE}'
systemctl is-enabled '${BOATLOCK_NH02_REMOTE_RFC2217_SERVICE}' 2>/dev/null || true
systemctl is-active '${BOATLOCK_NH02_REMOTE_RFC2217_SERVICE}' 2>/dev/null || true
printf '\nusb:\n'
ls -l '${BOATLOCK_NH02_REMOTE_USB_DEV}'
printf '\nlisten:\n'
ss -ltn '( sport = :${BOATLOCK_NH02_RFC2217_PORT} )' || true
printf '\nrecent logs:\n'
journalctl -u '${BOATLOCK_NH02_REMOTE_RFC2217_SERVICE}' -n 20 --no-pager"
