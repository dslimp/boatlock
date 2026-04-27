#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"
source "${REPO_ROOT}/tools/android/common.sh"

BUILD_FIRST=1
ANDROID_SERIAL=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-build)
      BUILD_FIRST=0
      shift
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

if [[ "${BUILD_FIRST}" -eq 1 ]]; then
  "${REPO_ROOT}/tools/android/build-app-apk.sh" >/dev/null
fi

if [[ -z "${ANDROID_SERIAL}" && "${BOATLOCK_NH02_ANDROID_WIFI_ADB:-1}" -eq 1 ]]; then
  ANDROID_SERIAL="$(boatlock_nh02_android_wifi_serial)"
  if [[ -z "${ANDROID_SERIAL}" ]]; then
    echo "could not enable Android ADB Wi-Fi" >&2
    exit 1
  fi
fi

if [[ ! -f "${BOATLOCK_ANDROID_APK}" ]]; then
  echo "Android app APK not found at ${BOATLOCK_ANDROID_APK}" >&2
  exit 1
fi

remote_shell "'${BOATLOCK_NH02_REMOTE_ANDROID_TOOLS_BIN}'" >/dev/null
remote_shell "mkdir -p '${BOATLOCK_NH02_REMOTE_ANDROID_STAGE}'"

remote_apk_path="${BOATLOCK_NH02_REMOTE_ANDROID_STAGE}/$(basename "${BOATLOCK_ANDROID_APK}")"
"${RSYNC_BASE[@]}" "${BOATLOCK_ANDROID_APK}" \
  "${BOATLOCK_NH02_SSH_TARGET}:${remote_apk_path}"

serial_arg=()
if [[ -n "${ANDROID_SERIAL}" ]]; then
  serial_arg=(-s "${ANDROID_SERIAL}")
fi

remote_install_script=$(cat <<SH
set -e
adb ${serial_arg[*]} install -r '${remote_apk_path}'
adb ${serial_arg[*]} shell pm grant '${BOATLOCK_ANDROID_PACKAGE}' android.permission.ACCESS_FINE_LOCATION >/dev/null 2>&1 || true
adb ${serial_arg[*]} shell pm grant '${BOATLOCK_ANDROID_PACKAGE}' android.permission.ACCESS_COARSE_LOCATION >/dev/null 2>&1 || true
adb ${serial_arg[*]} shell pm grant '${BOATLOCK_ANDROID_PACKAGE}' android.permission.BLUETOOTH_SCAN >/dev/null 2>&1 || true
adb ${serial_arg[*]} shell pm grant '${BOATLOCK_ANDROID_PACKAGE}' android.permission.BLUETOOTH_CONNECT >/dev/null 2>&1 || true
SH
)

remote_shell "${remote_install_script}"
printf '%s\n' "${remote_apk_path}"
