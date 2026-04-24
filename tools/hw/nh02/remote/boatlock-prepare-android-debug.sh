#!/usr/bin/env bash

set -euo pipefail

adb start-server >/dev/null 2>&1 || true

state="$(adb get-state 2>/dev/null || true)"
if [[ "${state}" != "device" ]]; then
  echo "adb target is not ready: state='${state:-missing}'" >&2
  adb devices -l >&2 || true
  exit 1
fi

echo "before:"
for key in adb_enabled verifier_verify_adb_installs package_verifier_enable package_verifier_user_consent; do
  printf '%s=%s\n' "${key}" "$(adb shell settings get global "${key}" 2>/dev/null | tr -d '\r' || true)"
done
printf 'install_non_market_apps=%s\n' "$(adb shell settings get secure install_non_market_apps 2>/dev/null | tr -d '\r' || true)"

adb shell settings put global verifier_verify_adb_installs 0
adb shell settings put global package_verifier_enable 0
adb shell settings put global package_verifier_user_consent 0
adb shell settings put secure install_non_market_apps 1 >/dev/null 2>&1 || true

echo "after:"
for key in adb_enabled verifier_verify_adb_installs package_verifier_enable package_verifier_user_consent; do
  printf '%s=%s\n' "${key}" "$(adb shell settings get global "${key}" 2>/dev/null | tr -d '\r' || true)"
done
printf 'install_non_market_apps=%s\n' "$(adb shell settings get secure install_non_market_apps 2>/dev/null | tr -d '\r' || true)"
