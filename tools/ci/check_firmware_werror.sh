#!/usr/bin/env bash
set -euo pipefail

tmp_cpp="$(mktemp /tmp/boatlock-werror-XXXXXX.cpp)"
trap 'rm -f "$tmp_cpp"' EXIT

cat >"$tmp_cpp" <<'EOF'
#include "GnssQualityGate.h"
#include "AnchorSupervisor.h"
#include "AnchorProfiles.h"
#include "AnchorReasons.h"
#include "Settings.h"
#include "MotorControl.h"
#include "StepperControl.h"

int main() { return 0; }
EOF

g++ -std=gnu++17 -Wall -Wextra -Werror -Wpedantic \
  -Iboatlock -Iboatlock/test/mocks \
  -fsyntax-only "$tmp_cpp"
