#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

remote_shell "set -e
printf 'adb:\n'
if command -v adb >/dev/null 2>&1; then
  command -v adb
  adb version | sed -n '1,2p'
else
  echo 'missing'
fi
printf '\nusb:\n'
lsusb | grep -E '${BOATLOCK_NH02_ANDROID_PHONE_USB_GREP}' || echo 'no-android-usb-match'
printf '\nadb devices:\n'
if command -v adb >/dev/null 2>&1; then
  adb start-server >/dev/null 2>&1 || true
  adb devices -l
else
  echo 'adb unavailable'
fi"
