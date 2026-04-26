# BoatLock Ground Trike Simulator V0

This is a first-pass printable CAD concept for a small ground simulator:

- two passive rear wheels
- one front wheel that steers and drives
- NEMA17 steering motor on the fixed deck
- 8 mm steering kingpin carried by two 608 bearings
- front drive motor mounted on the rotating fork

The model is intentionally parametric and conservative. It is not tied to a
specific purchased wheel or gearmotor yet. The FreeCAD script is the primary
source for generated `.FCStd` and STL artifacts; the OpenSCAD file is kept as a
simple text fallback.

## Drive Choice

Default CAD assumption: a 25GA / 25D class 12 V metal gearmotor mounted on the
front fork, driving the front wheel axle through a small belt or spur stage.

That is preferable to hanging the wheel directly on the motor shaft: the wheel
load goes into a 6 mm axle and fork bearings/holes, while the motor only
provides torque. A cheap TT motor can move a light bench model, but it is loose
and weak for a useful BoatLock simulator.

Suggested first motor class:

- 25GA370 or 25D metal gearmotor
- 12 V
- 100-200 rpm with a 65-80 mm rubber wheel
- encoder optional, not required for the current BoatLock manual drive path

The front motor wires need a slack service loop. Do not use unlimited steering;
limit the front fork mechanically to about +/-100 degrees unless a slip ring is
added.

## Steering Stack

The NEMA17 is fixed to the deck and should only transmit steering torque.
The front wheel load is not meant to go through the NEMA17 shaft.

Stack from top to bottom:

1. NEMA17 on deck, 31 mm M3 pattern
2. 5 mm to 8 mm shaft coupler
3. 8 mm steel kingpin or shoulder bolt
4. two 608 bearings in the printed carrier
5. printed front fork clamped to the kingpin

## FreeCAD Export

The FreeCAD path is primary for this model. It can be driven directly by
`freecadcmd` for repeatable exports, or through the repo-enabled FreeCAD MCP
server for live model inspection and iteration.

```bash
./export_freecad.sh
```

Outputs:

- `out/boatlock_ground_trike.FCStd`
- `out/stl/deck_plate.stl`
- `out/stl/steering_bearing_carrier.stl`
- `out/stl/front_fork_25ga.stl`
- `out/stl/rear_axle_block.stl`
- `out/stl/motor_cap_25ga.stl`

The `.FCStd` contains the assembly with translucent reference solids for the
front wheel, rear wheels, NEMA17, and front 25GA motor. The STL files are
exported as individual printable parts.

## OpenSCAD Fallback

Open `boatlock_ground_trike.scad` and set `part` to one of:

- `assembly`
- `deck_plate`
- `steering_bearing_carrier`
- `front_fork_25ga`
- `rear_axle_block`
- `motor_cap_25ga`

Export examples:

```bash
mkdir -p stl
openscad -o stl/deck_plate.stl -D 'part="deck_plate"' boatlock_ground_trike.scad
openscad -o stl/steering_bearing_carrier.stl -D 'part="steering_bearing_carrier"' boatlock_ground_trike.scad
openscad -o stl/front_fork_25ga.stl -D 'part="front_fork_25ga"' boatlock_ground_trike.scad
openscad -o stl/rear_axle_block.stl -D 'part="rear_axle_block"' boatlock_ground_trike.scad
openscad -o stl/motor_cap_25ga.stl -D 'part="motor_cap_25ga"' boatlock_ground_trike.scad
```

## Print Notes

- Print in PETG, ASA, PA, or PA-CF. Avoid PLA for the steering fork if it will
  see vibration, heat, or repeated crashes.
- Drill/ream functional holes after printing.
- Use M3 heat-set inserts or through screws with nuts for serviceable joints.
- Sand the 608 bearing pockets to fit the exact bearings.
- Print the front fork with enough perimeters that axle and kingpin holes are
  surrounded by solid material.

## BoatLock Firmware Caveat

This CAD switches the steering concept from the current 28BYJ-48 / ULN2003
bench path to NEMA17 mechanics. A normal NEMA17 driver path is not a drop-in
replacement for the current firmware wiring. Before connecting a powered NEMA17
driver, add and validate the real steering driver path in firmware and bench
acceptance.
