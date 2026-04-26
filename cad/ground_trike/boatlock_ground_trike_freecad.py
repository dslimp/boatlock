#!/usr/bin/env python3
"""Build the BoatLock ground trike concept in FreeCAD and export printable STLs."""

from pathlib import Path

import FreeCAD as App
import Mesh
import Part


ROOT = Path(__file__).resolve().parent
OUT_DIR = ROOT / "out"
STL_DIR = OUT_DIR / "stl"

DOC = App.newDocument("BoatLockGroundTrike")


deck_len = 260.0
deck_w = 150.0
deck_t = 4.0
deck_r = 12.0
front_pivot_x = 78.0
rear_axle_x = -82.0
deck_z = 100.0

m3_clear = 3.4
m4_clear = 4.4
m6_axle_clear = 6.4
kingpin_clear = 8.6
nema17_face = 42.3
nema17_hole_spacing = 31.0
nema17_center_bore = 23.0
bearing_608_od = 22.3
bearing_608_id_clear = 8.6
bearing_608_depth = 7.2

front_wheel_od = 70.0
front_wheel_w = 24.0
front_axle_z = front_wheel_od / 2.0
fork_leg_y = front_wheel_w / 2.0 + 7.0
fork_leg_t = 6.0
fork_crown_z = front_axle_z + 15.0
fork_hub_h = 30.0
motor_25ga_d = 25.6
motor_25ga_len = 45.0
motor_axis_x = -24.0
motor_axis_y = fork_leg_y + 12.0
motor_axis_z = front_axle_z + 25.0

rear_wheel_od = 65.0
rear_wheel_w = 26.0
rear_track = 178.0
rear_drop = deck_z - rear_wheel_od / 2.0


def v(x, y, z):
    return App.Vector(float(x), float(y), float(z))


def box_center(l, w, h, center):
    cx, cy, cz = center
    return Part.makeBox(l, w, h, v(cx - l / 2.0, cy - w / 2.0, cz - h / 2.0))


def box_min(l, w, h, corner):
    return Part.makeBox(l, w, h, v(*corner))


def cyl_z(d, h, center):
    cx, cy, cz = center
    return Part.makeCylinder(d / 2.0, h, v(cx, cy, cz - h / 2.0))


def cyl_axis(d, length, center, axis):
    shape = Part.makeCylinder(d / 2.0, length, v(0, 0, -length / 2.0))
    if axis == "x":
        rot = App.Rotation(v(0, 1, 0), 90)
    elif axis == "y":
        rot = App.Rotation(v(1, 0, 0), 90)
    else:
        rot = App.Rotation(v(0, 0, 1), 0)
    shape.Placement = App.Placement(v(*center), rot)
    return shape


def fuse_many(shapes):
    shape = shapes[0]
    for other in shapes[1:]:
        shape = shape.fuse(other)
    return shape.removeSplitter()


def cut_many(shape, cutters):
    for cutter in cutters:
        shape = shape.cut(cutter)
    return shape.removeSplitter()


def rounded_box(l, w, h, r, center):
    cx, cy, cz = center
    rr = min(r, max(0.1, min(l, w) / 2.0 - 0.1))
    parts = [
        box_center(l - 2.0 * rr, w, h, center),
        box_center(l, w - 2.0 * rr, h, center),
    ]
    for sx in (-1, 1):
        for sy in (-1, 1):
            parts.append(cyl_z(2.0 * rr, h, (cx + sx * (l / 2.0 - rr), cy + sy * (w / 2.0 - rr), cz)))
    return fuse_many(parts)


def slot_y(length, d, h, center):
    cx, cy, cz = center
    return fuse_many(
        [
            box_center(d, length, h, center),
            cyl_z(d, h, (cx, cy - length / 2.0, cz)),
            cyl_z(d, h, (cx, cy + length / 2.0, cz)),
        ]
    )


def deck_plate():
    shape = rounded_box(deck_len, deck_w, deck_t, deck_r, (0, 0, deck_t / 2.0))
    cuts = [cyl_z(nema17_center_bore, deck_t + 2.0, (front_pivot_x, 0, deck_t / 2.0))]
    for sx in (-1, 1):
        for sy in (-1, 1):
            cuts.append(
                cyl_z(
                    m3_clear,
                    deck_t + 2.0,
                    (front_pivot_x + sx * nema17_hole_spacing / 2.0, sy * nema17_hole_spacing / 2.0, deck_t / 2.0),
                )
            )
            cuts.append(cyl_z(m3_clear, deck_t + 2.0, (front_pivot_x + sx * 28.0, sy * 28.0, deck_t / 2.0)))
    for side in (-1, 1):
        for dx in (-16.0, 16.0):
            cuts.append(cyl_z(m3_clear, deck_t + 2.0, (rear_axle_x + dx, side * (deck_w / 2.0 - 17.0), deck_t / 2.0)))
    for x in (-40.0, 0.0, 40.0):
        for y in (-34.0, 34.0):
            cuts.append(cyl_z(m3_clear, deck_t + 2.0, (x, y, deck_t / 2.0)))
    for x in (-46.0, 8.0, 56.0):
        cuts.append(slot_y(72.0, 7.0, deck_t + 2.0, (x, 0, deck_t / 2.0)))
    return cut_many(shape, cuts)


def steering_bearing_carrier():
    carrier_l = 70.0
    carrier_h = 16.0
    shape = rounded_box(carrier_l, carrier_l, carrier_h, 8.0, (0, 0, carrier_h / 2.0))
    cuts = [
        cyl_z(bearing_608_id_clear, carrier_h + 2.0, (0, 0, carrier_h / 2.0)),
        cyl_z(bearing_608_od, bearing_608_depth + 0.2, (0, 0, bearing_608_depth / 2.0)),
        cyl_z(bearing_608_od, bearing_608_depth + 0.2, (0, 0, carrier_h - bearing_608_depth / 2.0)),
    ]
    for sx in (-1, 1):
        for sy in (-1, 1):
            cuts.append(cyl_z(m3_clear, carrier_h + 2.0, (sx * 28.0, sy * 28.0, carrier_h / 2.0)))
    return cut_many(shape, cuts)


def front_fork_25ga():
    shapes = [
        cyl_z(30.0, fork_hub_h, (0, 0, fork_crown_z + fork_hub_h / 2.0)),
        rounded_box(58.0, 52.0, 14.0, 5.0, (0, 0, fork_crown_z - 3.0)),
        rounded_box(54.0, 8.0, 42.0, 3.0, (motor_axis_x, motor_axis_y, motor_axis_z)),
        rounded_box(14.0, 12.0, abs(fork_crown_z - motor_axis_z) + 16.0, 3.0, (-13.0, motor_axis_y - 8.0, (motor_axis_z + fork_crown_z) / 2.0)),
    ]
    for y in (-fork_leg_y, fork_leg_y):
        shapes.append(rounded_box(22.0, fork_leg_t, fork_crown_z - front_axle_z + 16.0, 3.0, (0, y, (front_axle_z + fork_crown_z) / 2.0 - 4.0)))
        shapes.append(cyl_axis(18.0, fork_leg_t, (0, y, front_axle_z), "y"))
    shape = fuse_many(shapes)
    cuts = [
        cyl_z(kingpin_clear, fork_hub_h + 22.0, (0, 0, fork_crown_z + fork_hub_h / 2.0 + 1.0)),
        box_min(20.0, 2.0, fork_hub_h + 8.0, (15.0, -1.0, fork_crown_z - 2.0)),
        cyl_axis(m4_clear, 28.0, (13.0, 0, fork_crown_z + fork_hub_h / 2.0), "x"),
        cyl_axis(m6_axle_clear, 2.0 * fork_leg_y + 22.0, (0, 0, front_axle_z), "y"),
        cyl_axis(motor_25ga_d, 18.0, (motor_axis_x, motor_axis_y, motor_axis_z), "y"),
    ]
    for sx in (-1, 1):
        for sz in (-1, 1):
            cuts.append(cyl_axis(m3_clear, 18.0, (motor_axis_x + sx * 18.0, motor_axis_y, motor_axis_z + sz * 11.0), "y"))
    return cut_many(shape, cuts)


def rear_axle_block():
    shapes = [
        rounded_box(48.0, 18.0, rear_drop + 20.0, 4.0, (0, 0, rear_drop / 2.0)),
        rounded_box(54.0, 24.0, 10.0, 4.0, (0, 0, rear_drop + 5.0)),
        cyl_axis(19.0, 18.0, (0, 0, 0), "y"),
        box_center(38.0, 8.0, rear_drop + 4.0, (-11.0, 0, rear_drop / 2.0)),
        box_center(38.0, 8.0, rear_drop + 4.0, (11.0, 0, rear_drop / 2.0)),
    ]
    shape = fuse_many(shapes)
    cuts = [cyl_axis(m6_axle_clear, 26.0, (0, 0, 0), "y")]
    for x in (-16.0, 16.0):
        cuts.append(cyl_z(m3_clear, 18.0, (x, 0, rear_drop + 5.0)))
    return cut_many(shape, cuts)


def motor_cap_25ga():
    shape = rounded_box(54.0, 10.0, 34.0, 3.0, (0, 0, 0))
    cuts = [cyl_axis(motor_25ga_d + 0.3, 14.0, (0, 0, 0), "y")]
    for sx in (-1, 1):
        for sz in (-1, 1):
            cuts.append(cyl_axis(m3_clear, 14.0, (sx * 18.0, 0, sz * 11.0), "y"))
    return cut_many(shape, cuts)


def wheel_ref(d, w, center):
    tire = cyl_axis(d, w, center, "y")
    hub = cyl_axis(8.0, w + 10.0, center, "y")
    return fuse_many([tire, hub])


def nema17_ref():
    return fuse_many(
        [
            box_center(nema17_face, nema17_face, 38.0, (0, 0, 19.0)),
            cyl_z(22.0, 2.0, (0, 0, 1.0)),
            cyl_z(5.0, 24.0, (0, 0, -10.0)),
        ]
    )


def motor_ref():
    return cyl_axis(25.0, motor_25ga_len, (motor_axis_x, motor_axis_y, motor_axis_z), "y")


def add_obj(name, shape, color=None, placement=None):
    obj = DOC.addObject("Part::Feature", name)
    obj.Shape = shape
    if placement is not None:
        obj.Placement = placement
    if color is not None:
        try:
            obj.ViewObject.ShapeColor = color
            obj.ViewObject.Transparency = int(color[3] * 100) if len(color) == 4 else 0
        except Exception:
            pass
    return obj


def export_stl(name, shape):
    obj = DOC.addObject("Part::Feature", f"export_{name}")
    obj.Shape = shape
    path = STL_DIR / f"{name}.stl"
    Mesh.export([obj], str(path))
    DOC.removeObject(obj.Name)
    return path


def assert_single_solid(name, shape):
    count = len(shape.Solids)
    if count != 1:
        raise RuntimeError(f"{name}: expected 1 solid body, got {count}")
    return count


def main():
    OUT_DIR.mkdir(exist_ok=True)
    STL_DIR.mkdir(exist_ok=True)

    parts = {
        "deck_plate": deck_plate(),
        "steering_bearing_carrier": steering_bearing_carrier(),
        "front_fork_25ga": front_fork_25ga(),
        "rear_axle_block": rear_axle_block(),
        "motor_cap_25ga": motor_cap_25ga(),
    }

    for name, shape in parts.items():
        assert_single_solid(name, shape)
        export_stl(name, shape)

    add_obj("deck_plate", parts["deck_plate"], (0.25, 0.55, 0.9, 0.0), App.Placement(v(-front_pivot_x, 0, deck_z), App.Rotation()))
    add_obj("steering_bearing_carrier", parts["steering_bearing_carrier"], (0.95, 0.65, 0.25, 0.0), App.Placement(v(0, 0, deck_z - 16.0), App.Rotation()))
    add_obj("front_fork_25ga", parts["front_fork_25ga"], (0.95, 0.55, 0.2, 0.0))
    add_obj("motor_cap_25ga", parts["motor_cap_25ga"], (0.95, 0.55, 0.2, 0.0), App.Placement(v(motor_axis_x, motor_axis_y + 12.0, motor_axis_z), App.Rotation()))
    for side in (-1, 1):
        add_obj(
            f"rear_axle_block_{'left' if side < 0 else 'right'}",
            parts["rear_axle_block"],
            (0.95, 0.55, 0.2, 0.0),
            App.Placement(v(rear_axle_x - front_pivot_x, side * (deck_w / 2.0 - 17.0), rear_wheel_od / 2.0), App.Rotation()),
        )

    add_obj("front_wheel_reference", wheel_ref(front_wheel_od, front_wheel_w, (0, 0, front_axle_z)), (0.05, 0.05, 0.05, 0.65))
    for side in (-1, 1):
        add_obj(
            f"rear_wheel_reference_{'left' if side < 0 else 'right'}",
            wheel_ref(rear_wheel_od, rear_wheel_w, (rear_axle_x - front_pivot_x, side * rear_track / 2.0, rear_wheel_od / 2.0)),
            (0.05, 0.05, 0.05, 0.65),
        )
    add_obj("nema17_reference", nema17_ref(), (0.2, 0.2, 0.22, 0.65), App.Placement(v(0, 0, deck_z + deck_t), App.Rotation()))
    add_obj("front_25ga_motor_reference", motor_ref(), (0.6, 0.55, 0.48, 0.65))

    DOC.recompute()
    fcstd_path = OUT_DIR / "boatlock_ground_trike.FCStd"
    DOC.saveAs(str(fcstd_path))

    print(f"FreeCAD: {App.Version()[0]}.{App.Version()[1]}.{App.Version()[2]}")
    print(f"FCStd: {fcstd_path}")
    print(f"STL dir: {STL_DIR}")
    print(f"deck: {deck_len:.0f} x {deck_w:.0f} x {deck_t:.0f} mm")
    print(f"wheelbase: {abs(rear_axle_x - front_pivot_x):.0f} mm")
    print(f"rear track: {rear_track:.0f} mm")
    print(f"rear drop bracket axle-to-deck-bottom: {rear_drop:.1f} mm")
    print("solids: " + ", ".join(f"{name}=1" for name in parts))


if __name__ == "__main__":
    main()
