#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"
source "${REPO_ROOT}/tools/android/common.sh"

BUILD_FIRST=1
INSTALL_APP=1
WAIT_SECS=45
ANDROID_SERIAL=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --no-build)
      BUILD_FIRST=0
      shift
      ;;
    --no-install)
      INSTALL_APP=0
      shift
      ;;
    --wait-secs)
      WAIT_SECS="${2:?missing wait seconds}"
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

if [[ "${BUILD_FIRST}" -eq 1 ]]; then
  "${REPO_ROOT}/tools/android/build-smoke-apk.sh" >/dev/null
fi

if [[ "${INSTALL_APP}" -eq 1 && ! -f "${BOATLOCK_ANDROID_APK}" ]]; then
  echo "smoke APK not found at ${BOATLOCK_ANDROID_APK}" >&2
  exit 1
fi

remote_apk_path=""
if [[ "${INSTALL_APP}" -eq 1 ]]; then
  remote_shell "mkdir -p '${BOATLOCK_NH02_REMOTE_ANDROID_STAGE}'"
  remote_apk_path="${BOATLOCK_NH02_REMOTE_ANDROID_STAGE}/$(basename "${BOATLOCK_ANDROID_APK}")"
  "${RSYNC_BASE[@]}" "${BOATLOCK_ANDROID_APK}" "${BOATLOCK_NH02_SSH_TARGET}:${remote_apk_path}"
fi

serial_arg=""
if [[ -n "${ANDROID_SERIAL}" ]]; then
  serial_arg="--serial '${ANDROID_SERIAL}'"
fi

perm_args=""
for perm in "${ADB_PERMISSIONS[@]}"; do
  perm_args+=" --perm '${perm}'"
done

install_arg=""
if [[ "${INSTALL_APP}" -eq 0 ]]; then
  install_arg="--no-install"
fi

remote_shell "'${BOATLOCK_NH02_REMOTE_ANDROID_SMOKE_BIN}' \
  --package '${BOATLOCK_ANDROID_PACKAGE}' \
  --activity '${BOATLOCK_ANDROID_ACTIVITY}' \
  --wait-secs '${WAIT_SECS}' \
  ${install_arg} \
  ${serial_arg} \
  ${perm_args} \
  ${remote_apk_path:+--apk '${remote_apk_path}'}"
