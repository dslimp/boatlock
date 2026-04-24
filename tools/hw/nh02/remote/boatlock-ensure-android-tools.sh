#!/usr/bin/env bash

set -euo pipefail

export DEBIAN_FRONTEND=noninteractive

if command -v adb >/dev/null 2>&1; then
  echo "adb already installed: $(command -v adb)"
  adb version | sed -n '1,2p'
  exit 0
fi

apt-get update
apt-get install -y adb

echo "adb installed: $(command -v adb)"
adb version | sed -n '1,2p'
