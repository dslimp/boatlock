# Anchor Simulator

`tools/sim` provides an offline simulation harness for Anchor control hardening.

## What it does

- feeds synthetic GNSS measurements (`lat/lon/time/hdop/sats`) into control logic
- computes control outputs (`thrust_pct`, heading alignment effects)
- simulates boat drift and actuator response
- reports quality metrics:
  - `time_in_deadband_pct`
  - `p95_error_m`
  - `max_error_m`
  - `control_saturation_time_pct`
  - `num_thrust_direction_changes_per_min`
  - `invalid_thrust_count`
  - `invalid_motor_count`
  - `invalid_distance_count`
  - `min_battery_voltage_v`
  - `max_motor_current_a`
  - `max_motor_temp_c`
  - `motor_current_limited_time_pct`
  - `motor_thermal_derated_time_pct`
  - `motor_driver_deadband_time_pct`

## Actuator Model

The simulator keeps the controller command (`thrust_pct`) separate from applied
thrust. Applied thrust now passes through a brushed motor model with:

- driver deadband
- first-order motor response
- battery voltage sag from internal resistance
- current limiting
- thermal accumulation, cooling, and derating

## Scenarios (minimum set)

1. `calm_good_gps`
2. `constant_current`
3. `gusts`
4. `gnss_dropout`
5. `poor_hdop`
6. `position_jumps`
7. `comm_loss`
8. `nan_data`

## Optional RF Water Scenarios

The `russian` scenario set normalizes the first slice of
`tools/sim/research/environment_inputs.*` into runnable profiles:

1. `river_oka_normal_55lb`
2. `volga_spring_flow_80lb`
3. `rybinsk_fetch_55lb`
4. `ladoga_storm_abort`
5. `baltic_gulf_drift`

These scenarios add current, wind/gusts, wave-induced forcing, and a
`p95_rocking_roll_deg` metric. `ladoga_storm_abort` is intentionally modeled as
an environment-abort case, not as a normal anchor-hold success case.

## Run locally

```bash
python3 tools/sim/test_sim_core.py
python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json
python3 tools/sim/run_sim.py --scenario-set russian --check --json-out tools/sim/russian_report.json
python3 tools/sim/test_soak.py
python3 tools/sim/run_soak.py --hours 6 --check --json-out tools/sim/soak_report.json
```

`--check` exits non-zero if scenario thresholds are violated (for CI gate).
