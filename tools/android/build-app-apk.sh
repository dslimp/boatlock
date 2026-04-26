#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

RUN_PUB_GET=0
E2E_MODE=""
OTA_URL=""
OTA_SHA256=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --pub-get)
      RUN_PUB_GET=1
      shift
      ;;
    --e2e-mode)
      E2E_MODE="${2:?missing e2e mode}"
      shift 2
      ;;
    --ota-url)
      OTA_URL="${2:?missing OTA URL}"
      shift 2
      ;;
    --ota-sha256)
      OTA_SHA256="${2:?missing OTA SHA-256}"
      shift 2
      ;;
    *)
      echo "unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

if [[ -n "${E2E_MODE}" ]]; then
  boatlock_validate_app_e2e_mode "${E2E_MODE}"
fi

if [[ "${E2E_MODE}" == "ota" && ( -z "${OTA_URL}" || -z "${OTA_SHA256}" ) ]]; then
  echo "--ota-url and --ota-sha256 are required for e2e mode ota" >&2
  exit 1
fi

(
  cd "${FLUTTER_DIR}"
  if [[ "${RUN_PUB_GET}" -eq 1 ]]; then
    "${FLUTTER_ENV[@]}" "${BOATLOCK_ANDROID_FLUTTER_BIN}" pub get --offline
  fi
  if [[ -n "${E2E_MODE}" ]]; then
    build_args=(
      --debug \
      --no-pub \
      --dart-define="BOATLOCK_APP_E2E_MODE=${E2E_MODE}" \
    )
    if [[ "${E2E_MODE}" == "ota" ]]; then
      build_args+=(
        --dart-define="BOATLOCK_APP_E2E_OTA_URL=${OTA_URL}"
        --dart-define="BOATLOCK_APP_E2E_OTA_SHA256=${OTA_SHA256}"
      )
    fi
    build_args+=(--target lib/main.dart)
    "${FLUTTER_ENV[@]}" "${BOATLOCK_ANDROID_FLUTTER_BIN}" build apk "${build_args[@]}"
  else
    "${FLUTTER_ENV[@]}" "${BOATLOCK_ANDROID_FLUTTER_BIN}" build apk \
      --debug \
      --no-pub \
      --target lib/main.dart
  fi
)

printf '%s\n' "${BOATLOCK_ANDROID_APK}"
