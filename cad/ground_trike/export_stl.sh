#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

if ! command -v openscad >/dev/null 2>&1; then
  echo "openscad is required to export STL files" >&2
  exit 1
fi

mkdir -p stl

openscad -o stl/deck_plate.stl -D 'part="deck_plate"' boatlock_ground_trike.scad
openscad -o stl/steering_bearing_carrier.stl -D 'part="steering_bearing_carrier"' boatlock_ground_trike.scad
openscad -o stl/front_fork_25ga.stl -D 'part="front_fork_25ga"' boatlock_ground_trike.scad
openscad -o stl/rear_axle_block.stl -D 'part="rear_axle_block"' boatlock_ground_trike.scad
openscad -o stl/motor_cap_25ga.stl -D 'part="motor_cap_25ga"' boatlock_ground_trike.scad
