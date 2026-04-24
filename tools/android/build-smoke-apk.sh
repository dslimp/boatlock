#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

RUN_PUB_GET=0
SMOKE_MODE="basic"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --pub-get)
      RUN_PUB_GET=1
      shift
      ;;
    --mode)
      SMOKE_MODE="${2:?missing smoke mode}"
      shift 2
      ;;
    *)
      echo "unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

case "${SMOKE_MODE}" in
  basic|reconnect|manual) ;;
  *)
    echo "unsupported smoke mode: ${SMOKE_MODE}" >&2
    exit 1
    ;;
esac

(
  cd "${FLUTTER_DIR}"
  if [[ "${RUN_PUB_GET}" -eq 1 ]]; then
    "${FLUTTER_ENV[@]}" "${BOATLOCK_ANDROID_FLUTTER_BIN}" pub get --offline
  fi
  "${FLUTTER_ENV[@]}" "${BOATLOCK_ANDROID_FLUTTER_BIN}" build apk \
    --debug \
    --no-pub \
    --dart-define="BOATLOCK_SMOKE_MODE=${SMOKE_MODE}" \
    --target lib/main_smoke.dart
)

printf '%s\n' "${BOATLOCK_ANDROID_APK}"
