#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

ANDROID_ONLY=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --android-only)
      ANDROID_ONLY=1
      shift
      ;;
    *)
      echo "unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

remote_shell "mkdir -p '${BOATLOCK_NH02_REMOTE_ROOT}/bin' '${BOATLOCK_NH02_REMOTE_STAGE}' /tmp/boatlock-hw-install"

"${RSYNC_BASE[@]}" \
  "${SCRIPT_DIR}/boatlock-esp32s3-rfc2217.service" \
  "${SCRIPT_DIR}/remote/boatlock-flash-esp32s3.sh" \
  "${SCRIPT_DIR}/remote/boatlock-reset-esp32s3.sh" \
  "${SCRIPT_DIR}/remote/boatlock-ensure-android-tools.sh" \
  "${SCRIPT_DIR}/remote/boatlock-run-android-smoke.sh" \
  "${SCRIPT_DIR}/remote/boatlock-prepare-android-debug.sh" \
  "${SCRIPT_DIR}/remote/boatlock-enable-android-wifi-debug.sh" \
  "${BOATLOCK_NH02_SSH_TARGET}:/tmp/boatlock-hw-install/"

remote_shell "set -e
install -m 0755 /tmp/boatlock-hw-install/boatlock-ensure-android-tools.sh '${BOATLOCK_NH02_REMOTE_ROOT}/bin/boatlock-ensure-android-tools.sh'
install -m 0755 /tmp/boatlock-hw-install/boatlock-run-android-smoke.sh '${BOATLOCK_NH02_REMOTE_ROOT}/bin/boatlock-run-android-smoke.sh'
install -m 0755 /tmp/boatlock-hw-install/boatlock-prepare-android-debug.sh '${BOATLOCK_NH02_REMOTE_ROOT}/bin/boatlock-prepare-android-debug.sh'
install -m 0755 /tmp/boatlock-hw-install/boatlock-enable-android-wifi-debug.sh '${BOATLOCK_NH02_REMOTE_ROOT}/bin/boatlock-enable-android-wifi-debug.sh'"

if [[ "${ANDROID_ONLY}" -eq 1 ]]; then
  echo "installed nh02 Android helpers only"
  exit 0
fi

remote_shell "set -e
if [[ ! -x '${BOATLOCK_NH02_REMOTE_ROOT}/.venv/bin/esptool' ]]; then
  mkdir -p '${BOATLOCK_NH02_REMOTE_ROOT}'
  python3 -m venv '${BOATLOCK_NH02_REMOTE_ROOT}/.venv'
  '${BOATLOCK_NH02_REMOTE_ROOT}/.venv/bin/pip' install --upgrade pip
  '${BOATLOCK_NH02_REMOTE_ROOT}/.venv/bin/pip' install esptool
fi
install -m 0755 /tmp/boatlock-hw-install/boatlock-flash-esp32s3.sh '${BOATLOCK_NH02_REMOTE_ROOT}/bin/boatlock-flash-esp32s3.sh'
install -m 0755 /tmp/boatlock-hw-install/boatlock-reset-esp32s3.sh '${BOATLOCK_NH02_REMOTE_ROOT}/bin/boatlock-reset-esp32s3.sh'
install -m 0644 /tmp/boatlock-hw-install/boatlock-esp32s3-rfc2217.service /etc/systemd/system/${BOATLOCK_NH02_REMOTE_RFC2217_SERVICE}
systemctl daemon-reload
systemctl enable '${BOATLOCK_NH02_REMOTE_RFC2217_SERVICE}'
systemctl restart '${BOATLOCK_NH02_REMOTE_RFC2217_SERVICE}'
systemctl --no-pager --full status '${BOATLOCK_NH02_REMOTE_RFC2217_SERVICE}' | sed -n '1,20p'"
