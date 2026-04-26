#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

BUILD_FIRST=1
MODE="basic"
PASS_ARGS=()
OTA_FIRMWARE=""
OTA_LATEST_MAIN=0
OTA_MANIFEST=""
OTA_PORT=18080
OTA_PORT_SET=0
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
    --ota-latest-main)
      MODE="ota"
      OTA_LATEST_MAIN=1
      shift
      ;;
    --ota-firmware)
      OTA_FIRMWARE="${2:?missing OTA firmware path}"
      shift 2
      ;;
    --ota-port)
      OTA_PORT="${2:?missing OTA port}"
      OTA_PORT_SET=1
      shift 2
      ;;
    --ota-sha256)
      OTA_SHA256="${2:?missing OTA SHA-256}"
      shift 2
      ;;
    --esp-reset)
      MODE="reconnect"
      PASS_ARGS+=("$1")
      shift
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
    --serial)
      PASS_ARGS+=("$1" "${2:?missing android serial}")
      shift 2
      ;;
    *)
      echo "unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

if [[ "${MODE}" == "ota" ]]; then
  if [[ -z "${OTA_FIRMWARE}" ]]; then
    echo "--ota-firmware is required for --ota" >&2
    exit 1
  fi
  if [[ ! -f "${OTA_FIRMWARE}" ]]; then
    echo "OTA firmware not found: ${OTA_FIRMWARE}" >&2
    exit 1
  fi
  if [[ -z "${OTA_SHA256}" ]]; then
    OTA_SHA256="$(shasum -a 256 "${OTA_FIRMWARE}" | awk '{print $1}')"
  fi
  if [[ "${OTA_PORT_SET}" -eq 0 ]]; then
    OTA_PORT="$(remote_shell "python3 - <<'PY'
import socket
for port in range(18080, 18100):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        sock.bind(('127.0.0.1', port))
    except OSError:
        sock.close()
        continue
    sock.close()
    print(port)
    break
else:
    raise SystemExit('no free OTA HTTP port in 18080..18099')
PY")"
  fi
  printf 'ota_port=%s\n' "${OTA_PORT}"
  if [[ "${OTA_LATEST_MAIN}" -eq 1 ]]; then
    OTA_MANIFEST="$(mktemp -t boatlock-ota-manifest.XXXXXX.json)"
    (
      cd "${REPO_ROOT}"
      python3 tools/ci/generate_firmware_update_manifest.py \
        --firmware "${OTA_FIRMWARE}" \
        --out "${OTA_MANIFEST}" \
        --base-url "http://127.0.0.1:${OTA_PORT}" \
        --git-sha "$(git rev-parse HEAD)" \
        --firmware-version "$(python3 tools/ci/check_firmware_version.py --print-only)"
    )
    PASS_ARGS+=(
      --ota-firmware "${OTA_FIRMWARE}"
      --ota-manifest "${OTA_MANIFEST}"
      --ota-port "${OTA_PORT}"
    )
  else
    PASS_ARGS+=(--ota-firmware "${OTA_FIRMWARE}" --ota-port "${OTA_PORT}")
  fi
  if [[ "${WAIT_SET}" -eq 0 ]]; then
    PASS_ARGS+=(--wait-secs 1800)
  fi
fi

if [[ "${BUILD_FIRST}" -eq 1 ]]; then
  build_args=(--e2e-mode "${MODE}")
  if [[ "${MODE}" == "ota" ]]; then
    if [[ "${OTA_LATEST_MAIN}" -eq 1 ]]; then
      build_args+=(
        --e2e-ota-latest-main
        --firmware-manifest-url "http://127.0.0.1:${OTA_PORT}/manifest.json"
      )
    else
      build_args+=(
        --ota-url "http://127.0.0.1:${OTA_PORT}/firmware.bin"
        --ota-sha256 "${OTA_SHA256}"
      )
    fi
  fi
  "${REPO_ROOT}/tools/android/build-app-apk.sh" "${build_args[@]}" >/dev/null
fi

if [[ "${#PASS_ARGS[@]}" -eq 0 ]]; then
  exec "${SCRIPT_DIR}/android-run-smoke.sh" --no-build
fi
exec "${SCRIPT_DIR}/android-run-smoke.sh" --no-build "${PASS_ARGS[@]}"
