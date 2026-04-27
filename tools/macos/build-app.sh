#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
FLUTTER_DIR="${REPO_ROOT}/boatlock_ui"

BOATLOCK_MACOS_FLUTTER_BIN="${BOATLOCK_MACOS_FLUTTER_BIN:-${REPO_ROOT}/flutter/bin/flutter}"
RUN_PUB_GET=0
FIRMWARE_MANIFEST_URL=""
FIRMWARE_GITHUB_REPO=""
LATEST_RELEASE_GITHUB_REPO="${BOATLOCK_LATEST_RELEASE_GITHUB_REPO:-dslimp/boatlock}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --pub-get)
      RUN_PUB_GET=1
      shift
      ;;
    --release)
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

if [[ -z "${FIRMWARE_MANIFEST_URL}" && -z "${FIRMWARE_GITHUB_REPO}" ]]; then
  FIRMWARE_GITHUB_REPO="${LATEST_RELEASE_GITHUB_REPO}"
fi

build_args=(--release)
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

APP_PATH="${FLUTTER_DIR}/build/macos/Build/Products/Release/boatlock_ui.app"
if [[ ! -d "${APP_PATH}" ]]; then
  echo "macOS app was not produced at ${APP_PATH}" >&2
  exit 1
fi

printf '%s\n' "${APP_PATH}"
