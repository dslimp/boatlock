#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

BUILD_FIRST=1
MODE="basic"
PASS_ARGS=()
OTA_URL=""
OTA_SHA256=""
OTA_LATEST_RELEASE=0
FIRMWARE_MANIFEST_URL=""
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
    --sim-suite)
      MODE="sim_suite"
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
    --ota-latest-release)
      MODE="ota"
      OTA_LATEST_RELEASE=1
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
    --firmware-manifest-url)
      FIRMWARE_MANIFEST_URL="${2:?missing firmware manifest URL}"
      OTA_LATEST_RELEASE=1
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
  if [[ "${OTA_LATEST_RELEASE}" -eq 0 &&
    ( -z "${OTA_URL}" || -z "${OTA_SHA256}" ) ]]; then
    echo "--ota-url and --ota-sha256 are required for --ota" >&2
    exit 1
  fi
  if [[ "${WAIT_SET}" -eq 0 ]]; then
    PASS_ARGS+=(--wait-secs 1800)
  fi
fi

if [[ "${MODE}" == "sim_suite" && "${WAIT_SET}" -eq 0 ]]; then
  PASS_ARGS+=(--wait-secs 1800)
fi

if [[ "${BUILD_FIRST}" -eq 1 ]]; then
  "${SCRIPT_DIR}/build-app-apk.sh" >/dev/null
fi

run_args=(--no-build --mode "${MODE}")
if [[ "${OTA_LATEST_RELEASE}" -eq 1 ]]; then
  run_args+=(--ota-latest-release)
fi
if [[ -n "${FIRMWARE_MANIFEST_URL}" ]]; then
  run_args+=(--firmware-manifest-url "${FIRMWARE_MANIFEST_URL}")
fi
if [[ -n "${OTA_URL}" ]]; then
  run_args+=(--ota-url "${OTA_URL}")
fi
if [[ -n "${OTA_SHA256}" ]]; then
  run_args+=(--ota-sha256 "${OTA_SHA256}")
fi
exec "${SCRIPT_DIR}/run-smoke.sh" "${run_args[@]}" "${PASS_ARGS[@]}"
