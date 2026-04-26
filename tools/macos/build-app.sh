#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
FLUTTER_DIR="${REPO_ROOT}/boatlock_ui"

BOATLOCK_MACOS_FLUTTER_BIN="${BOATLOCK_MACOS_FLUTTER_BIN:-${REPO_ROOT}/flutter/bin/flutter}"
BUILD_MODE="release"
RUN_PUB_GET=0
SERVICE_UI=0
FIRMWARE_MANIFEST_URL=""
FIRMWARE_GITHUB_REPO=""
LATEST_MAIN_MANIFEST_URL="${BOATLOCK_LATEST_MAIN_MANIFEST_URL:-https://dslimp.github.io/boatlock/firmware/main/manifest.json}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --pub-get)
      RUN_PUB_GET=1
      shift
      ;;
    --debug)
      BUILD_MODE="debug"
      shift
      ;;
    --release)
      BUILD_MODE="release"
      shift
      ;;
    --service-ui)
      SERVICE_UI=1
      shift
      ;;
    --latest-main-service)
      SERVICE_UI=1
      FIRMWARE_MANIFEST_URL="${LATEST_MAIN_MANIFEST_URL}"
      shift
      ;;
    --firmware-manifest-url)
      SERVICE_UI=1
      FIRMWARE_MANIFEST_URL="${2:?missing firmware manifest URL}"
      shift 2
      ;;
    --firmware-github-repo)
      SERVICE_UI=1
      FIRMWARE_GITHUB_REPO="${2:?missing GitHub repository}"
      shift 2
      ;;
    *)
      echo "unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

build_args=("--${BUILD_MODE}")
if [[ "${SERVICE_UI}" -eq 1 ]]; then
  build_args+=(--dart-define="BOATLOCK_SERVICE_UI=true")
fi
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

(
  cd "${FLUTTER_DIR}"
  if [[ "${RUN_PUB_GET}" -eq 1 ]]; then
    "${BOATLOCK_MACOS_FLUTTER_BIN}" pub get
  fi
  "${BOATLOCK_MACOS_FLUTTER_BIN}" build macos "${build_args[@]}"
)

case "${BUILD_MODE}" in
  debug)
    BUILD_CONFIG="Debug"
    ;;
  release)
    BUILD_CONFIG="Release"
    ;;
  *)
    echo "unsupported macOS build mode: ${BUILD_MODE}" >&2
    exit 1
    ;;
esac

APP_PATH="${FLUTTER_DIR}/build/macos/Build/Products/${BUILD_CONFIG}/boatlock_ui.app"
if [[ ! -d "${APP_PATH}" ]]; then
  echo "macOS app was not produced at ${APP_PATH}" >&2
  exit 1
fi

printf '%s\n' "${APP_PATH}"
