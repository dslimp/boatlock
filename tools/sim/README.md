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
  - `invalid_distance_count`

## Scenarios (minimum set)

1. `calm_good_gps`
2. `constant_current`
3. `gusts`
4. `gnss_dropout`
5. `poor_hdop`
6. `position_jumps`
7. `comm_loss`
8. `nan_data`

## Run locally

```bash
python3 tools/sim/test_sim_core.py
python3 tools/sim/run_sim.py --check --json-out tools/sim/report.json
python3 tools/sim/test_soak.py
python3 tools/sim/run_soak.py --hours 6 --check --json-out tools/sim/soak_report.json
```

`--check` exits non-zero if scenario thresholds are violated (for CI gate).
