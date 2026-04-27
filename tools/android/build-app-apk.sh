#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

RUN_PUB_GET=0

while [[ $# -gt 0 ]]; do
  case "$1" in
    --pub-get)
      RUN_PUB_GET=1
      shift
      ;;
    *)
      echo "unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

(
  cd "${FLUTTER_DIR}"
  if [[ "${RUN_PUB_GET}" -eq 1 ]]; then
    "${FLUTTER_ENV[@]}" "${BOATLOCK_ANDROID_FLUTTER_BIN}" pub get --offline
  fi
  build_args=(
    --release \
    --no-pub \
  )
  build_args+=(--target lib/main.dart)
  "${FLUTTER_ENV[@]}" "${BOATLOCK_ANDROID_FLUTTER_BIN}" build apk "${build_args[@]}"
)

if [[ ! -f "${BOATLOCK_ANDROID_APK}" ]]; then
  echo "Android app APK was not produced at ${BOATLOCK_ANDROID_APK}" >&2
  exit 1
fi

printf '%s\n' "${BOATLOCK_ANDROID_APK}"
