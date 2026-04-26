/*
  BoatLock ground trike simulator, V0.

  Goal:
  - Child-car / tabletop scale ground simulator for BoatLock.
  - Two passive rear wheels.
  - One front wheel that both steers and drives.
  - NEMA17 steering motor drives an 8 mm kingpin through a coupler.
  - Wheel load is carried by 608 bearings, not by the NEMA17 shaft.

  OpenSCAD usage examples:
    openscad -o stl/deck_plate.stl -D 'part="deck_plate"' boatlock_ground_trike.scad
    openscad -o stl/steering_bearing_carrier.stl -D 'part="steering_bearing_carrier"' boatlock_ground_trike.scad
    openscad -o stl/front_fork_25ga.stl -D 'part="front_fork_25ga"' boatlock_ground_trike.scad
    openscad -o stl/rear_axle_block.stl -D 'part="rear_axle_block"' boatlock_ground_trike.scad
    openscad -o stl/motor_cap_25ga.stl -D 'part="motor_cap_25ga"' boatlock_ground_trike.scad
*/

$fn = 72;

part = "assembly"; // assembly, deck_plate, steering_bearing_carrier, front_fork_25ga, rear_axle_block, motor_cap_25ga

eps = 0.02;

// Main layout.
deck_len = 260;
deck_w = 150;
deck_t = 4;
deck_r = 12;
front_pivot_x = 78;
rear_axle_x = -82;
deck_z = front_wheel_od + 30;

// Hardware clearances.
m3_clear = 3.4;
m4_clear = 4.4;
m6_axle_clear = 6.4;
kingpin_clear = 8.6;
nema17_face = 42.3;
nema17_hole_spacing = 31.0;
nema17_center_bore = 23.0;
bearing_608_od = 22.3;
bearing_608_id_clear = 8.6;
bearing_608_depth = 7.2;

// Front wheel / motor reference.
front_wheel_od = 70;
front_wheel_w = 24;
front_axle_z = front_wheel_od / 2;
fork_leg_y = front_wheel_w / 2 + 7;
fork_leg_t = 6;
fork_crown_z = front_axle_z + 15;
fork_hub_h = 30;
motor_25ga_d = 25.6;
motor_25ga_len = 45;
motor_axis_x = -24;
motor_axis_y = fork_leg_y + 12;
motor_axis_z = front_axle_z + 25;

// Rear wheel reference.
rear_wheel_od = 65;
rear_wheel_w = 26;
rear_track = 178;
rear_drop = deck_z - rear_wheel_od / 2;

module rounded_rect_2d(l, w, r) {
  offset(r = r) square([l - 2 * r, w - 2 * r], center = true);
}

module rounded_box(l, w, h, r) {
  linear_extrude(height = h) rounded_rect_2d(l, w, r);
}

module rounded_box_center(l, w, h, r) {
  translate([0, 0, -h / 2]) rounded_box(l, w, h, r);
}

module slot_x(len, d, h) {
  hull() {
    translate([-len / 2, 0, 0]) cylinder(d = d, h = h);
    translate([ len / 2, 0, 0]) cylinder(d = d, h = h);
  }
}

module slot_y(len, d, h) {
  hull() {
    translate([0, -len / 2, 0]) cylinder(d = d, h = h);
    translate([0,  len / 2, 0]) cylinder(d = d, h = h);
  }
}

module deck_plate() {
  difference() {
    rounded_box(deck_len, deck_w, deck_t, deck_r);

    // NEMA17 and steering carrier center access.
    translate([front_pivot_x, 0, -eps])
      cylinder(d = nema17_center_bore, h = deck_t + 2 * eps);

    // NEMA17 face screws on top of deck.
    for (sx = [-1, 1], sy = [-1, 1]) {
      translate([
        front_pivot_x + sx * nema17_hole_spacing / 2,
        sy * nema17_hole_spacing / 2,
        -eps
      ])
        cylinder(d = m3_clear, h = deck_t + 2 * eps);
    }

    // Under-deck bearing carrier screws.
    for (sx = [-1, 1], sy = [-1, 1]) {
      translate([front_pivot_x + sx * 28, sy * 28, -eps])
        cylinder(d = m3_clear, h = deck_t + 2 * eps);
    }

    // Rear passive wheel block mount holes.
    for (side = [-1, 1], dx = [-16, 16]) {
      translate([rear_axle_x + dx, side * (deck_w / 2 - 17), -eps])
        cylinder(d = m3_clear, h = deck_t + 2 * eps);
    }

    // Electronics grid and cable slots.
    for (x = [-40, 0, 40], y = [-34, 34]) {
      translate([x, y, -eps]) cylinder(d = m3_clear, h = deck_t + 2 * eps);
    }
    for (x = [-46, 8, 56]) {
      translate([x, 0, -eps]) slot_y(72, 7, deck_t + 2 * eps);
    }
  }
}

module steering_bearing_carrier() {
  carrier_l = 70;
  carrier_h = 16;

  difference() {
    rounded_box(carrier_l, carrier_l, carrier_h, 8);

    // Kingpin clearance through both bearings.
    translate([0, 0, -eps])
      cylinder(d = bearing_608_id_clear, h = carrier_h + 2 * eps);

    // 608 bearing pockets. Print and ream/sand for real bearings.
    translate([0, 0, -eps])
      cylinder(d = bearing_608_od, h = bearing_608_depth + eps);
    translate([0, 0, carrier_h - bearing_608_depth])
      cylinder(d = bearing_608_od, h = bearing_608_depth + eps);

    // Deck mount screws.
    for (sx = [-1, 1], sy = [-1, 1]) {
      translate([sx * 28, sy * 28, -eps])
        cylinder(d = m3_clear, h = carrier_h + 2 * eps);
    }
  }
}

module front_fork_25ga() {
  /*
    The wheel axle is a 6 mm steel shaft between fork legs.
    The CAD leaves a 25GA/25D gearmotor saddle on one side of the rotating fork.
    Default drive assumption: motor drives the wheel axle with a small belt or
    printed spur stage. This keeps wheel radial load off the motor output shaft.
  */
  difference() {
    union() {
      // Kingpin clamp boss. Uses 8 mm shaft/bolt through 608 bearings.
      translate([0, 0, fork_crown_z])
        cylinder(d = 30, h = fork_hub_h);

      // Crown ties the two legs and the steering boss together.
      translate([0, 0, fork_crown_z - 3])
        rounded_box_center(58, 52, 14, 5);

      // Fork legs.
      for (y = [-fork_leg_y, fork_leg_y]) {
        translate([0, y, (front_axle_z + fork_crown_z) / 2 - 4])
          rounded_box_center(22, fork_leg_t, fork_crown_z - front_axle_z + 16, 3);

        translate([0, y, front_axle_z])
          rotate([90, 0, 0])
            cylinder(d = 18, h = fork_leg_t, center = true);
      }

      // Side motor mount pad for 25GA/25D class gearmotor.
      translate([motor_axis_x, motor_axis_y, motor_axis_z])
        rounded_box_center(54, 8, 42, 3);

      // Extra rib back to the crown for belt tension loads.
      translate([-13, motor_axis_y - 8, (motor_axis_z + fork_crown_z) / 2])
        rounded_box_center(14, 12, abs(fork_crown_z - motor_axis_z) + 16, 3);
    }

    // 8 mm kingpin.
    translate([0, 0, fork_crown_z - 10])
      cylinder(d = kingpin_clear, h = fork_hub_h + 22);

    // Split relief for the kingpin clamp. Add M4 screw/nut in real print.
    translate([15, -1.0, fork_crown_z - 2])
      cube([20, 2, fork_hub_h + 8]);
    translate([13, 0, fork_crown_z + fork_hub_h / 2])
      rotate([0, 90, 0])
        cylinder(d = m4_clear, h = 28, center = true);

    // Front wheel axle.
    translate([0, 0, front_axle_z])
      rotate([90, 0, 0])
        cylinder(d = m6_axle_clear, h = 2 * fork_leg_y + 22, center = true);

    // 25GA motor saddle and two face-screw slots.
    translate([motor_axis_x, motor_axis_y, motor_axis_z])
      rotate([90, 0, 0])
        cylinder(d = motor_25ga_d, h = 18, center = true);

    for (sx = [-1, 1], sz = [-1, 1]) {
      translate([motor_axis_x + sx * 18, motor_axis_y, motor_axis_z + sz * 11])
        rotate([90, 0, 0])
          cylinder(d = m3_clear, h = 18, center = true);
    }
  }
}

module motor_cap_25ga() {
  difference() {
    rounded_box_center(54, 10, 34, 3);

    // Clamp saddle. Sand to fit the exact motor can.
    rotate([90, 0, 0])
      cylinder(d = motor_25ga_d + 0.3, h = 14, center = true);

    for (sx = [-1, 1], sz = [-1, 1]) {
      translate([sx * 18, 0, sz * 11])
        rotate([90, 0, 0])
          cylinder(d = m3_clear, h = 14, center = true);
    }
  }
}

module rear_axle_block() {
  difference() {
    union() {
      // Axle-to-deck drop bracket for the high steering stack.
      translate([0, 0, rear_drop / 2])
        rounded_box_center(48, 18, rear_drop + 20, 4);
      translate([0, 0, rear_drop + 5])
        rounded_box_center(54, 24, 10, 4);
      translate([0, 0, 0])
        rotate([90, 0, 0])
          cylinder(d = 19, h = 18, center = true);
      for (x = [-11, 11]) {
        translate([x, 0, rear_drop / 2])
          rounded_box_center(38, 8, rear_drop + 4, 3);
      }
    }

    // M6 shoulder bolt / 6 mm shaft for passive rear wheel.
    translate([0, 0, 0])
      rotate([90, 0, 0])
        cylinder(d = m6_axle_clear, h = 26, center = true);

    // Two M3 mount screws into the deck.
    for (x = [-16, 16]) {
      translate([x, 0, rear_drop + 5])
        cylinder(d = m3_clear, h = 18, center = true);
    }
  }
}

module nema17_reference() {
  color([0.2, 0.2, 0.22, 0.35]) {
    translate([0, 0, 19]) cube([nema17_face, nema17_face, 38], center = true);
    translate([0, 0, -10]) cylinder(d = 5, h = 24);
    cylinder(d = 22, h = 2);
  }
}

module wheel_reference(d, w, axle_d) {
  color([0.05, 0.05, 0.05, 0.35])
    rotate([90, 0, 0]) cylinder(d = d, h = w, center = true);
  color([0.8, 0.8, 0.8, 0.35])
    rotate([90, 0, 0]) cylinder(d = axle_d, h = w + 10, center = true);
}

module motor_25ga_reference() {
  color([0.6, 0.55, 0.48, 0.35])
    translate([motor_axis_x, motor_axis_y, motor_axis_z])
      rotate([90, 0, 0])
        cylinder(d = 25, h = motor_25ga_len, center = true);
}

module assembly() {
  // Ground reference.
  color([0.85, 0.85, 0.85, 0.18])
    translate([0, 0, -0.5]) cube([310, 230, 1], center = true);

  // Chassis deck.
  color([0.25, 0.55, 0.9, 0.65])
    translate([0, 0, front_wheel_od + 30]) deck_plate();

  // Steering stack.
  translate([front_pivot_x, 0, front_wheel_od + 30 - 16])
    color([0.95, 0.65, 0.25, 0.75]) steering_bearing_carrier();
  translate([front_pivot_x, 0, front_wheel_od + 30 + deck_t])
    nema17_reference();
  translate([front_pivot_x, 0, 0])
    color([0.95, 0.55, 0.2, 0.85]) front_fork_25ga();

  // Front driven wheel and motor references.
  translate([front_pivot_x, 0, front_axle_z])
    wheel_reference(front_wheel_od, front_wheel_w, 6);
  translate([front_pivot_x, 0, 0])
    motor_25ga_reference();

  // Rear passive wheel blocks and wheels.
  for (side = [-1, 1]) {
    translate([rear_axle_x, side * (deck_w / 2 - 17), rear_wheel_od / 2])
      color([0.95, 0.55, 0.2, 0.85]) rear_axle_block();
    translate([rear_axle_x, side * rear_track / 2, rear_wheel_od / 2])
      wheel_reference(rear_wheel_od, rear_wheel_w, 6);
  }
}

if (part == "deck_plate") {
  deck_plate();
} else if (part == "steering_bearing_carrier") {
  steering_bearing_carrier();
} else if (part == "front_fork_25ga") {
  front_fork_25ga();
} else if (part == "rear_axle_block") {
  rear_axle_block();
} else if (part == "motor_cap_25ga") {
  motor_cap_25ga();
} else {
  assembly();
}
