#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
FLUTTER_DIR="${REPO_ROOT}/boatlock_ui"

BOATLOCK_ANDROID_FLUTTER_BIN="${BOATLOCK_ANDROID_FLUTTER_BIN:-${REPO_ROOT}/flutter/bin/flutter}"
BOATLOCK_ANDROID_ADB_BIN="${BOATLOCK_ANDROID_ADB_BIN:-/usr/local/bin/adb}"
BOATLOCK_ANDROID_PACKAGE="${BOATLOCK_ANDROID_PACKAGE:-com.example.boatlock_ui}"
BOATLOCK_ANDROID_ACTIVITY="${BOATLOCK_ANDROID_ACTIVITY:-${BOATLOCK_ANDROID_PACKAGE}/.MainActivity}"
BOATLOCK_ANDROID_APK="${BOATLOCK_ANDROID_APK:-${FLUTTER_DIR}/build/app/outputs/flutter-apk/app-debug.apk}"
BOATLOCK_ANDROID_HOME="${BOATLOCK_ANDROID_HOME:-${HOME:-/tmp}}"
BOATLOCK_ANDROID_XDG_CACHE_HOME="${BOATLOCK_ANDROID_XDG_CACHE_HOME:-${XDG_CACHE_HOME:-${BOATLOCK_ANDROID_HOME}/.cache}}"
BOATLOCK_ANDROID_GRADLE_USER_HOME="${BOATLOCK_ANDROID_GRADLE_USER_HOME:-${GRADLE_USER_HOME:-${BOATLOCK_ANDROID_HOME}/.gradle}}"
BOATLOCK_SMOKE_MODES=(basic reconnect manual status sim anchor compass gps)
BOATLOCK_APP_E2E_MODES=(basic reconnect manual status sim anchor compass gps ota)

FLUTTER_ENV=(
  env
  HOME="${BOATLOCK_ANDROID_HOME}"
  XDG_CACHE_HOME="${BOATLOCK_ANDROID_XDG_CACHE_HOME}"
  FLUTTER_SUPPRESS_ANALYTICS=true
  GRADLE_USER_HOME="${BOATLOCK_ANDROID_GRADLE_USER_HOME}"
)

ADB_PERMISSIONS=(
  android.permission.ACCESS_FINE_LOCATION
  android.permission.ACCESS_COARSE_LOCATION
  android.permission.BLUETOOTH_SCAN
  android.permission.BLUETOOTH_CONNECT
)

boatlock_is_smoke_mode() {
  local value="${1:-}"
  local mode
  for mode in "${BOATLOCK_SMOKE_MODES[@]}"; do
    if [[ "${value}" == "${mode}" ]]; then
      return 0
    fi
  done
  return 1
}

boatlock_validate_smoke_mode() {
  local value="${1:-}"
  if boatlock_is_smoke_mode "${value}"; then
    return 0
  fi
  echo "unsupported smoke mode: ${value}" >&2
  echo "supported smoke modes: ${BOATLOCK_SMOKE_MODES[*]}" >&2
  return 1
}

boatlock_is_app_e2e_mode() {
  local value="${1:-}"
  local mode
  for mode in "${BOATLOCK_APP_E2E_MODES[@]}"; do
    if [[ "${value}" == "${mode}" ]]; then
      return 0
    fi
  done
  return 1
}

boatlock_validate_app_e2e_mode() {
  local value="${1:-}"
  if boatlock_is_app_e2e_mode "${value}"; then
    return 0
  fi
  echo "unsupported app e2e mode: ${value}" >&2
  echo "supported app e2e modes: ${BOATLOCK_APP_E2E_MODES[*]}" >&2
  return 1
}
