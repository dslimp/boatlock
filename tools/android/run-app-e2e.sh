#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

BUILD_FIRST=1
MODE="basic"
PASS_ARGS=()
OTA_URL=""
OTA_SHA256=""
WAIT_SET=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --reconnect)
      MODE="reconnect"
      PASS_ARGS+=("$1")
      shift
      ;;
    --manual)
      MODE="manual"
      PASS_ARGS+=("$1")
      shift
      ;;
    --status)
      MODE="status"
      PASS_ARGS+=("$1")
      shift
      ;;
    --sim)
      MODE="sim"
      PASS_ARGS+=("$1")
      shift
      ;;
    --anchor)
      MODE="anchor"
      PASS_ARGS+=("$1")
      shift
      ;;
    --compass)
      MODE="compass"
      PASS_ARGS+=("$1")
      shift
      ;;
    --gps)
      MODE="gps"
      PASS_ARGS+=("$1")
      shift
      ;;
    --ota)
      MODE="ota"
      shift
      ;;
    --ota-url)
      OTA_URL="${2:?missing OTA URL}"
      shift 2
      ;;
    --ota-sha256)
      OTA_SHA256="${2:?missing OTA SHA-256}"
      shift 2
      ;;
    --no-build)
      BUILD_FIRST=0
      PASS_ARGS+=("$1")
      shift
      ;;
    --no-install)
      PASS_ARGS+=("$1")
      shift
      ;;
    --wait-secs)
      PASS_ARGS+=("$1" "${2:?missing wait seconds}")
      WAIT_SET=1
      shift 2
      ;;
    *)
      echo "unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

if [[ "${MODE}" == "ota" ]]; then
  if [[ "${BUILD_FIRST}" -eq 1 && ( -z "${OTA_URL}" || -z "${OTA_SHA256}" ) ]]; then
    echo "--ota-url and --ota-sha256 are required for --ota" >&2
    exit 1
  fi
  if [[ "${WAIT_SET}" -eq 0 ]]; then
    PASS_ARGS+=(--wait-secs 1800)
  fi
fi

if [[ "${BUILD_FIRST}" -eq 1 ]]; then
  build_args=(--e2e-mode "${MODE}")
  if [[ "${MODE}" == "ota" ]]; then
    build_args+=(--ota-url "${OTA_URL}" --ota-sha256 "${OTA_SHA256}")
  fi
  "${SCRIPT_DIR}/build-app-apk.sh" "${build_args[@]}" >/dev/null
fi

if [[ "${#PASS_ARGS[@]}" -eq 0 ]]; then
  exec "${SCRIPT_DIR}/run-smoke.sh" --no-build
fi
exec "${SCRIPT_DIR}/run-smoke.sh" --no-build "${PASS_ARGS[@]}"
