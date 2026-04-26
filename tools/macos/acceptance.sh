#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
FLUTTER_DIR="${REPO_ROOT}/boatlock_ui"
BUILD_SCRIPT="${SCRIPT_DIR}/build-app.sh"

LATEST_MAIN_MANIFEST_URL="${BOATLOCK_LATEST_MAIN_MANIFEST_URL:-https://dslimp.github.io/boatlock/firmware/main/manifest.json}"
LOG_ROOT="${BOATLOCK_MACOS_ACCEPTANCE_LOG_DIR:-${TMPDIR:-/tmp}/boatlock-macos-acceptance}"
LOG_DIR="${LOG_ROOT}/$(date +%Y%m%d-%H%M%S)"

RUN_BUILD=1
RUN_PUB_GET=0
BUILD_MODE="release"
APP_PATH=""
ARTIFACT_ZIP=""
STATIC_ONLY=0
MANUAL=0
LAUNCH_SECONDS=8
USE_LATEST_MAIN=1
FIRMWARE_MANIFEST_URL=""
FIRMWARE_GITHUB_REPO=""
EXECUTABLE_NAME=""

usage() {
  cat <<'USAGE'
usage: tools/macos/acceptance.sh [options]

Build or inspect the macOS service app, then run a local no-BLE acceptance
slice. The automated runtime smoke only proves that the app bundle starts and
stays alive locally; a real BLE update still requires hardware acceptance.

Options:
  --pub-get                         Run flutter pub get before building.
  --debug                           Build/use Debug output.
  --release                         Build/use Release output (default).
  --latest-main-service             Build service UI with latest-main manifest (default).
  --firmware-manifest-url URL       Build service UI with a custom manifest URL.
  --firmware-github-repo OWNER/REPO Build service UI with latest release source.
  --no-build                        Use the existing Flutter build output.
  --app-path PATH                   Validate this .app bundle instead of building.
  --artifact-zip PATH               Unpack a CI artifact zip such as
                                    boatlock-macos-service-main.zip and validate it.
  --static-only                     Validate bundle/signature/entitlements only.
  --runtime-seconds N               Launch-smoke duration before terminating the app.
  --manual                          Open the app and print the operator checklist.
  -h, --help                        Show this help.
USAGE
}

fail() {
  echo "macOS acceptance failed: $*" >&2
  exit 1
}

require_command() {
  if ! command -v "$1" >/dev/null 2>&1; then
    fail "missing required command: $1"
  fi
}

is_positive_int() {
  case "$1" in
    ''|*[!0-9]*)
      return 1
      ;;
    0)
      return 1
      ;;
    *)
      return 0
      ;;
  esac
}

plist_raw() {
  if [[ -x /usr/libexec/PlistBuddy ]]; then
    /usr/libexec/PlistBuddy -c "Print :$1" "$2" 2>/dev/null
  else
    plutil -extract "$1" raw "$2" 2>/dev/null
  fi
}

expect_plist_true() {
  local key="$1"
  local plist="$2"
  local value

  value="$(plist_raw "${key}" "${plist}")" || fail "missing plist key ${key} in ${plist}"
  if [[ "${value}" != "true" ]]; then
    fail "plist key ${key} in ${plist} is ${value}, expected true"
  fi
}

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
    --latest-main-service)
      USE_LATEST_MAIN=1
      shift
      ;;
    --firmware-manifest-url)
      USE_LATEST_MAIN=0
      FIRMWARE_MANIFEST_URL="${2:?missing firmware manifest URL}"
      shift 2
      ;;
    --firmware-github-repo)
      USE_LATEST_MAIN=0
      FIRMWARE_GITHUB_REPO="${2:?missing GitHub repository}"
      shift 2
      ;;
    --no-build)
      RUN_BUILD=0
      shift
      ;;
    --app-path)
      RUN_BUILD=0
      APP_PATH="${2:?missing app path}"
      shift 2
      ;;
    --artifact-zip)
      RUN_BUILD=0
      ARTIFACT_ZIP="${2:?missing artifact zip path}"
      shift 2
      ;;
    --static-only)
      STATIC_ONLY=1
      shift
      ;;
    --runtime-seconds)
      LAUNCH_SECONDS="${2:?missing runtime seconds}"
      if ! is_positive_int "${LAUNCH_SECONDS}"; then
        fail "--runtime-seconds must be a positive integer"
      fi
      shift 2
      ;;
    --manual)
      MANUAL=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      fail "unknown argument: $1"
      ;;
  esac
done

if [[ -n "${APP_PATH}" && -n "${ARTIFACT_ZIP}" ]]; then
  fail "--app-path and --artifact-zip are mutually exclusive"
fi

mkdir -p "${LOG_DIR}"

build_service_app() {
  local build_log="${LOG_DIR}/build.log"
  local build_args=("--${BUILD_MODE}")
  local produced_path

  if [[ "${RUN_PUB_GET}" -eq 1 ]]; then
    build_args+=(--pub-get)
  fi
  if [[ "${USE_LATEST_MAIN}" -eq 1 ]]; then
    build_args+=(--latest-main-service)
  fi
  if [[ -n "${FIRMWARE_MANIFEST_URL}" ]]; then
    build_args+=(--firmware-manifest-url "${FIRMWARE_MANIFEST_URL}")
  fi
  if [[ -n "${FIRMWARE_GITHUB_REPO}" ]]; then
    build_args+=(--firmware-github-repo "${FIRMWARE_GITHUB_REPO}")
  fi

  "${BUILD_SCRIPT}" "${build_args[@]}" 2>&1 | tee "${build_log}"
  produced_path="$(tail -n 1 "${build_log}")"
  if [[ ! -d "${produced_path}" ]]; then
    fail "build wrapper did not end with an app path; see ${build_log}"
  fi
  APP_PATH="${produced_path}"
}

extract_artifact_zip() {
  local extract_dir="${LOG_DIR}/artifact"
  local found_app=""
  local app_count=0
  local candidate

  [[ -f "${ARTIFACT_ZIP}" ]] || fail "artifact zip not found: ${ARTIFACT_ZIP}"
  mkdir -p "${extract_dir}"

  if command -v ditto >/dev/null 2>&1; then
    ditto -x -k "${ARTIFACT_ZIP}" "${extract_dir}"
  else
    require_command unzip
    unzip -q "${ARTIFACT_ZIP}" -d "${extract_dir}"
  fi

  while IFS= read -r candidate; do
    found_app="${candidate}"
    app_count=$((app_count + 1))
  done < <(find "${extract_dir}" -maxdepth 4 -type d -name "*.app" -print)

  if [[ "${app_count}" -ne 1 ]]; then
    fail "expected exactly one .app in ${ARTIFACT_ZIP}, found ${app_count}"
  fi
  APP_PATH="${found_app}"
}

default_app_path() {
  local build_config

  case "${BUILD_MODE}" in
    debug)
      build_config="Debug"
      ;;
    release)
      build_config="Release"
      ;;
    *)
      fail "unsupported build mode: ${BUILD_MODE}"
      ;;
  esac
  APP_PATH="${FLUTTER_DIR}/build/macos/Build/Products/${build_config}/boatlock_ui.app"
}

validate_bundle() {
  local info_plist="${APP_PATH}/Contents/Info.plist"
  local executable_path
  local bundle_id
  local usage_value
  local entitlements_plist="${LOG_DIR}/entitlements.plist"
  local otool_log="${LOG_DIR}/otool.txt"

  require_command plutil
  require_command codesign

  [[ -d "${APP_PATH}" ]] || fail "app bundle not found: ${APP_PATH}"
  [[ -f "${info_plist}" ]] || fail "Info.plist not found in ${APP_PATH}"
  plutil -lint -s "${info_plist}"

  bundle_id="$(plist_raw CFBundleIdentifier "${info_plist}")" || fail "missing CFBundleIdentifier"
  EXECUTABLE_NAME="$(plist_raw CFBundleExecutable "${info_plist}")" || fail "missing CFBundleExecutable"
  executable_path="${APP_PATH}/Contents/MacOS/${EXECUTABLE_NAME}"
  [[ -x "${executable_path}" ]] || fail "app executable is missing or not executable: ${executable_path}"

  for key in NSBluetoothAlwaysUsageDescription NSLocationUsageDescription; do
    usage_value="$(plist_raw "${key}" "${info_plist}")" || fail "missing ${key}"
    [[ -n "${usage_value}" ]] || fail "${key} is empty"
  done

  codesign --verify --verbose "${APP_PATH}"
  codesign -d --entitlements :- "${APP_PATH}" > "${entitlements_plist}" 2>"${LOG_DIR}/codesign-entitlements.stderr"
  plutil -lint -s "${entitlements_plist}"

  for key in \
    com.apple.security.app-sandbox \
    com.apple.security.device.bluetooth \
    com.apple.security.network.client \
    com.apple.security.personal-information.location
  do
    expect_plist_true "${key}" "${entitlements_plist}"
  done

  if command -v otool >/dev/null 2>&1; then
    otool -L "${executable_path}" > "${otool_log}"
    grep -q "CoreBluetooth.framework" "${otool_log}" || fail "CoreBluetooth.framework is not linked"
  fi

  printf 'bundle ok: %s (%s)\n' "${APP_PATH}" "${bundle_id}"
}

pid_is_existing() {
  local pid="$1"
  local existing_pid

  for existing_pid in ${EXISTING_APP_PIDS:-}; do
    if [[ "${pid}" == "${existing_pid}" ]]; then
      return 0
    fi
  done
  return 1
}

find_new_app_pid() {
  local pid

  for pid in $(pgrep -x "${EXECUTABLE_NAME}" 2>/dev/null || true); do
    if ! pid_is_existing "${pid}"; then
      printf '%s\n' "${pid}"
      return 0
    fi
  done
  return 1
}

run_launch_smoke() {
  local stdout_log="${LOG_DIR}/runtime.stdout.log"
  local stderr_log="${LOG_DIR}/runtime.stderr.log"
  local open_pid
  local app_pid=""
  local poll_count=0

  if [[ "$(uname -s)" != "Darwin" ]]; then
    fail "runtime smoke requires macOS; use --static-only on other hosts"
  fi
  require_command open
  require_command pgrep

  EXISTING_APP_PIDS="$(pgrep -x "${EXECUTABLE_NAME}" 2>/dev/null || true)"
  open -n -F -W --stdout "${stdout_log}" --stderr "${stderr_log}" "${APP_PATH}" &
  open_pid="$!"

  while [[ "${poll_count}" -lt 100 ]]; do
    app_pid="$(find_new_app_pid || true)"
    if [[ -n "${app_pid}" ]]; then
      break
    fi
    if ! kill -0 "${open_pid}" 2>/dev/null; then
      if wait "${open_pid}"; then
        fail "open exited before ${EXECUTABLE_NAME} appeared"
      else
        fail "open failed before ${EXECUTABLE_NAME} appeared; see ${stderr_log}"
      fi
    fi
    poll_count=$((poll_count + 1))
    sleep 0.1
  done

  [[ -n "${app_pid}" ]] || fail "${EXECUTABLE_NAME} did not appear in process list"
  sleep "${LAUNCH_SECONDS}"

  if ! kill -0 "${app_pid}" 2>/dev/null; then
    fail "${EXECUTABLE_NAME} exited before ${LAUNCH_SECONDS}s smoke window; see ${stderr_log}"
  fi

  kill -TERM "${app_pid}" 2>/dev/null || true
  poll_count=0
  while [[ "${poll_count}" -lt 50 ]]; do
    if ! kill -0 "${open_pid}" 2>/dev/null; then
      wait "${open_pid}" || true
      break
    fi
    poll_count=$((poll_count + 1))
    sleep 0.1
  done
  if kill -0 "${open_pid}" 2>/dev/null; then
    kill -TERM "${open_pid}" 2>/dev/null || true
    wait "${open_pid}" || true
  fi

  printf 'runtime smoke ok: %s stayed up for %ss\n' "${EXECUTABLE_NAME}" "${LAUNCH_SECONDS}"
}

print_manual_checklist() {
  cat <<USAGE
Manual macOS service update acceptance:
- App bundle: ${APP_PATH}
- Build source: ${FIRMWARE_GITHUB_REPO:-${FIRMWARE_MANIFEST_URL:-${LATEST_MAIN_MANIFEST_URL}}}
- Confirm the app opens without a crash and macOS permission prompts are understandable.
- Open Settings and confirm service firmware update controls are visible.
- Without BLE hardware, record this only as bundle/runtime smoke accepted.
- With service-capable BoatLock hardware, connect, authenticate if paired, start the update, and require download, SHA verification, BLE OTA progress, reboot, reconnect, and telemetry recovery.

Logs: ${LOG_DIR}
USAGE
}

if [[ "${RUN_BUILD}" -eq 1 ]]; then
  build_service_app
elif [[ -n "${ARTIFACT_ZIP}" ]]; then
  extract_artifact_zip
elif [[ -z "${APP_PATH}" ]]; then
  default_app_path
fi

validate_bundle

if [[ "${MANUAL}" -eq 1 ]]; then
  print_manual_checklist
  if [[ "${STATIC_ONLY}" -eq 0 ]]; then
    require_command open
    open -n -F "${APP_PATH}"
  fi
  exit 0
fi

if [[ "${STATIC_ONLY}" -eq 1 ]]; then
  printf 'static acceptance ok: %s\n' "${APP_PATH}"
  printf 'logs: %s\n' "${LOG_DIR}"
  exit 0
fi

run_launch_smoke
printf 'logs: %s\n' "${LOG_DIR}"
