#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

RUN_PUB_GET=0
E2E_MODE=""
OTA_URL=""
OTA_SHA256=""
OTA_LATEST_RELEASE=0
FIRMWARE_MANIFEST_URL=""
FIRMWARE_GITHUB_REPO=""
LATEST_RELEASE_GITHUB_REPO="${BOATLOCK_LATEST_RELEASE_GITHUB_REPO:-dslimp/boatlock}"

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
    --e2e-ota-latest-release)
      OTA_LATEST_RELEASE=1
      if [[ -z "${FIRMWARE_MANIFEST_URL}" && -z "${FIRMWARE_GITHUB_REPO}" ]]; then
        FIRMWARE_GITHUB_REPO="${LATEST_RELEASE_GITHUB_REPO}"
      fi
      shift
      ;;
    --firmware-manifest-url)
      FIRMWARE_MANIFEST_URL="${2:?missing firmware manifest URL}"
      shift 2
      ;;
    --firmware-github-repo)
      FIRMWARE_GITHUB_REPO="${2:?missing GitHub repository}"
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

if [[ "${E2E_MODE}" == "ota" &&
  "${OTA_LATEST_RELEASE}" -eq 0 &&
  ( -z "${OTA_URL}" || -z "${OTA_SHA256}" ) ]]; then
  echo "--ota-url and --ota-sha256 are required for e2e mode ota" >&2
  exit 1
fi

if [[ -z "${FIRMWARE_MANIFEST_URL}" && -z "${FIRMWARE_GITHUB_REPO}" ]]; then
  FIRMWARE_GITHUB_REPO="${LATEST_RELEASE_GITHUB_REPO}"
fi

(
  cd "${FLUTTER_DIR}"
  if [[ "${RUN_PUB_GET}" -eq 1 ]]; then
    "${FLUTTER_ENV[@]}" "${BOATLOCK_ANDROID_FLUTTER_BIN}" pub get --offline
  fi
  build_args=(
    --release \
    --no-pub \
  )
  if [[ -n "${FIRMWARE_MANIFEST_URL}" ]]; then
    build_args+=(
      --dart-define="BOATLOCK_FIRMWARE_UPDATE_MANIFEST_URL=${FIRMWARE_MANIFEST_URL}"
    )
  fi
  if [[ -n "${FIRMWARE_GITHUB_REPO}" ]]; then
    build_args+=(
      --dart-define="BOATLOCK_FIRMWARE_UPDATE_GITHUB_REPO=${FIRMWARE_GITHUB_REPO}"
    )
  fi
  if [[ -n "${E2E_MODE}" ]]; then
    build_args+=(
      --dart-define="BOATLOCK_APP_E2E_MODE=${E2E_MODE}" \
    )
    if [[ "${E2E_MODE}" == "ota" ]]; then
      if [[ "${OTA_LATEST_RELEASE}" -eq 1 ]]; then
        build_args+=(--dart-define="BOATLOCK_APP_E2E_OTA_LATEST_RELEASE=true")
      else
        build_args+=(
          --dart-define="BOATLOCK_APP_E2E_OTA_URL=${OTA_URL}"
          --dart-define="BOATLOCK_APP_E2E_OTA_SHA256=${OTA_SHA256}"
        )
      fi
    fi
    build_args+=(--target lib/main.dart)
    "${FLUTTER_ENV[@]}" "${BOATLOCK_ANDROID_FLUTTER_BIN}" build apk "${build_args[@]}"
  else
    build_args+=(--target lib/main.dart)
    "${FLUTTER_ENV[@]}" "${BOATLOCK_ANDROID_FLUTTER_BIN}" build apk "${build_args[@]}"
  fi
)

if [[ ! -f "${BOATLOCK_ANDROID_APK}" ]]; then
  echo "Android app APK was not produced at ${BOATLOCK_ANDROID_APK}" >&2
  exit 1
fi

printf '%s\n' "${BOATLOCK_ANDROID_APK}"
