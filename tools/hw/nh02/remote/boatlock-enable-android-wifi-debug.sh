#!/usr/bin/env bash

set -euo pipefail

PORT=5555
SERIAL=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --port)
      PORT="${2:?missing tcpip port}"
      shift 2
      ;;
    --serial)
      SERIAL="${2:?missing android serial}"
      shift 2
      ;;
    *)
      echo "unknown argument: $1" >&2
      exit 1
      ;;
  esac
done

ADB_USB=(adb)
if [[ -n "${SERIAL}" ]]; then
  ADB_USB+=(-s "${SERIAL}")
fi

adb start-server >/dev/null 2>&1 || true

if [[ -z "${SERIAL}" ]]; then
  SERIAL="$(adb devices -l | awk '$2 == "device" && $0 ~ / usb:/ { print $1; exit }')"
  if [[ -z "${SERIAL}" ]]; then
    SERIAL="$(adb devices -l | awk '$2 == "device" { print $1; exit }')"
  fi
  if [[ -n "${SERIAL}" ]]; then
    ADB_USB=(adb -s "${SERIAL}")
  fi
fi

state="$("${ADB_USB[@]}" get-state 2>/dev/null || true)"
if [[ "${state}" != "device" ]]; then
  echo "adb USB target is not ready: state='${state:-missing}'" >&2
  adb devices -l >&2 || true
  exit 1
fi

model="$("${ADB_USB[@]}" shell getprop ro.product.model 2>/dev/null | tr -d '\r' || true)"
device="$("${ADB_USB[@]}" shell getprop ro.product.device 2>/dev/null | tr -d '\r' || true)"
ip_addr="$("${ADB_USB[@]}" shell ip -o -4 addr show wlan0 2>/dev/null \
  | tr -d '\r' \
  | awk '{ split($4, a, "/"); print a[1]; exit }')"

if [[ -z "${ip_addr}" ]]; then
  ip_addr="$("${ADB_USB[@]}" shell ip route get 1.1.1.1 2>/dev/null \
    | tr -d '\r' \
    | awk '{ for (i = 1; i <= NF; i++) if ($i == "src") { print $(i + 1); exit } }')"
fi

if [[ -z "${ip_addr}" ]]; then
  echo "could not determine Android Wi-Fi IP; connect the phone to the test AP first" >&2
  exit 1
fi

printf 'android_usb_serial=%s\n' "${SERIAL:-auto}"
printf 'android_model=%s\n' "${model:-unknown}"
printf 'android_device=%s\n' "${device:-unknown}"
printf 'android_wifi_ip=%s\n' "${ip_addr}"

"${ADB_USB[@]}" tcpip "${PORT}" >/dev/null
sleep 3

connect_output="$(adb connect "${ip_addr}:${PORT}" 2>&1 || true)"
printf '%s\n' "${connect_output}"

wifi_state="$(adb -s "${ip_addr}:${PORT}" get-state 2>/dev/null || true)"
if [[ "${wifi_state}" != "device" ]]; then
  echo "adb Wi-Fi target is not ready: state='${wifi_state:-missing}'" >&2
  adb devices -l >&2 || true
  exit 1
fi

printf 'android_wifi_serial=%s:%s\n' "${ip_addr}" "${PORT}"
adb devices -l
