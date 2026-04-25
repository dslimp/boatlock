#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

RUN_PUB_GET=0
E2E_MODE=""

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
    *)
      echo "unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

if [[ -n "${E2E_MODE}" ]]; then
  boatlock_validate_smoke_mode "${E2E_MODE}"
fi

(
  cd "${FLUTTER_DIR}"
  if [[ "${RUN_PUB_GET}" -eq 1 ]]; then
    "${FLUTTER_ENV[@]}" "${BOATLOCK_ANDROID_FLUTTER_BIN}" pub get --offline
  fi
  if [[ -n "${E2E_MODE}" ]]; then
    "${FLUTTER_ENV[@]}" "${BOATLOCK_ANDROID_FLUTTER_BIN}" build apk \
      --debug \
      --no-pub \
      --dart-define="BOATLOCK_APP_E2E_MODE=${E2E_MODE}" \
      --target lib/main.dart
  else
    "${FLUTTER_ENV[@]}" "${BOATLOCK_ANDROID_FLUTTER_BIN}" build apk \
      --debug \
      --no-pub \
      --target lib/main.dart
  fi
)

printf '%s\n' "${BOATLOCK_ANDROID_APK}"
