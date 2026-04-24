#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "${SCRIPT_DIR}/common.sh"

remote_shell "'${BOATLOCK_NH02_REMOTE_ANDROID_PREP_BIN}'"
