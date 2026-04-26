#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from dataclasses import asdict
from pathlib import Path
from typing import Dict, List, Tuple

from anchor_sim import SimConfig, Scenario, default_scenarios, simulate_scenario


def clone_scenario(base: Scenario, duration_s: int, name_suffix: str) -> Scenario:
    return Scenario(
        name=f"{base.name}{name_suffix}",
        duration_s=duration_s,
        initial_pos_m=base.initial_pos_m,
        env_accel_fn=base.env_accel_fn,
        obs_fn=base.obs_fn,
        comm_ok_fn=base.comm_ok_fn,
        inject_nan_fn=base.inject_nan_fn,
        profile=base.profile,
    )


def aggregate(results: List[Dict[str, object]]) -> Dict[str, float]:
    if not results:
        return {
            "duration_s": 0.0,
            "avg_time_in_deadband_pct": 0.0,
            "max_error_m": 0.0,
            "max_p95_error_m": 0.0,
            "max_control_saturation_time_pct": 0.0,
            "avg_direction_changes_per_min": 0.0,
            "invalid_thrust_count": 0.0,
            "invalid_motor_count": 0.0,
            "invalid_distance_count": 0.0,
            "event_count": 0.0,
        }

    total_duration = 0.0
    sum_deadband = 0.0
    sum_dir_changes = 0.0
    max_error = 0.0
    max_p95 = 0.0
    max_sat = 0.0
    invalid_thrust = 0.0
    invalid_motor = 0.0
    invalid_distance = 0.0
    event_count = 0.0

    for r in results:
        duration = float(r["duration_s"])
        m = r["metrics"]
        total_duration += duration
        sum_deadband += m["time_in_deadband_pct"] * duration
        sum_dir_changes += m["num_thrust_direction_changes_per_min"] * duration
        max_error = max(max_error, float(m["max_error_m"]))
        max_p95 = max(max_p95, float(m["p95_error_m"]))
        max_sat = max(max_sat, float(m["control_saturation_time_pct"]))
        invalid_thrust += float(m.get("invalid_thrust_count", 0.0))
        invalid_motor += float(m.get("invalid_motor_count", 0.0))
        invalid_distance += float(m.get("invalid_distance_count", 0.0))
        event_count += float(len(m.get("events", [])))

    w = max(1.0, total_duration)
    return {
        "duration_s": total_duration,
        "avg_time_in_deadband_pct": sum_deadband / w,
        "max_error_m": max_error,
        "max_p95_error_m": max_p95,
        "max_control_saturation_time_pct": max_sat,
        "avg_direction_changes_per_min": sum_dir_changes / w,
        "invalid_thrust_count": invalid_thrust,
        "invalid_motor_count": invalid_motor,
        "invalid_distance_count": invalid_distance,
        "event_count": event_count,
    }


def evaluate_failures(agg: Dict[str, float], results: List[Dict[str, object]]) -> List[str]:
    violations: List[str] = []
    if agg["invalid_thrust_count"] > 0.0:
        violations.append(f"invalid_thrust_count={agg['invalid_thrust_count']:.0f} > 0")
    if agg["invalid_motor_count"] > 0.0:
        violations.append(f"invalid_motor_count={agg['invalid_motor_count']:.0f} > 0")
    if agg["invalid_distance_count"] > 0.0:
        violations.append(f"invalid_distance_count={agg['invalid_distance_count']:.0f} > 0")
    if agg["max_error_m"] > 120.0:
        violations.append(f"max_error_m={agg['max_error_m']:.2f} > 120.00")
    if agg["max_p95_error_m"] > 80.0:
        violations.append(f"max_p95_error_m={agg['max_p95_error_m']:.2f} > 80.00")
    if agg["max_control_saturation_time_pct"] > 95.0:
        violations.append(
            f"max_control_saturation_time_pct={agg['max_control_saturation_time_pct']:.2f} > 95.00"
        )

    saw_comm_timeout = False
    saw_nan_failsafe = False
    for r in results:
        events = r["metrics"].get("events", [])
        for e in events:
            if e.get("failsafe_reason") == "COMM_TIMEOUT":
                saw_comm_timeout = True
            if e.get("failsafe_reason") == "INTERNAL_ERROR_NAN":
                saw_nan_failsafe = True
    if not saw_comm_timeout:
        violations.append("missing COMM_TIMEOUT event during soak")
    if not saw_nan_failsafe:
        violations.append("missing INTERNAL_ERROR_NAN event during soak")

    return violations


def run_soak(cfg: SimConfig, total_duration_s: int, segment_s: int, seed: int) -> Tuple[List[Dict[str, object]], Dict[str, float], List[str]]:
    templates = default_scenarios()
    chosen = [t for t in templates if t.name in ("calm_good_gps", "constant_current", "gusts", "gnss_dropout", "position_jumps", "comm_loss", "nan_data")]
    if not chosen:
        chosen = templates

    runs: List[Dict[str, object]] = []
    elapsed = 0
    idx = 0
    while elapsed < total_duration_s:
        base = chosen[idx % len(chosen)]
        duration = min(segment_s, total_duration_s - elapsed)
        scenario = clone_scenario(base, duration, f"_seg{idx}")
        run = simulate_scenario(cfg, scenario, seed + idx)
        runs.append(run)
        elapsed += duration
        idx += 1

    agg = aggregate(runs)
    violations = evaluate_failures(agg, runs)
    return runs, agg, violations


def main() -> int:
    ap = argparse.ArgumentParser(description="BoatLock soak + fault-injection simulator")
    ap.add_argument("--hours", type=float, default=6.0, help="Soak duration in hours")
    ap.add_argument("--segment-s", type=int, default=600, help="Segment duration in seconds")
    ap.add_argument("--seed", type=int, default=101, help="Random seed")
    ap.add_argument("--check", action="store_true", help="Exit non-zero when violations found")
    ap.add_argument("--json-out", type=str, default="tools/sim/soak_report.json", help="Path to JSON report")
    args = ap.parse_args()

    total_duration_s = max(1, int(args.hours * 3600.0))
    cfg = SimConfig()
    runs, agg, violations = run_soak(cfg, total_duration_s, max(30, args.segment_s), args.seed)

    print("Soak summary:")
    print(f"duration_s={agg['duration_s']:.0f}")
    print(f"avg_time_in_deadband_pct={agg['avg_time_in_deadband_pct']:.2f}")
    print(f"max_error_m={agg['max_error_m']:.2f}")
    print(f"max_p95_error_m={agg['max_p95_error_m']:.2f}")
    print(f"max_control_saturation_time_pct={agg['max_control_saturation_time_pct']:.2f}")
    print(f"avg_direction_changes_per_min={agg['avg_direction_changes_per_min']:.2f}")
    print(f"event_count={agg['event_count']:.0f}")
    if violations:
        print("violations:")
        for v in violations:
            print(f"  - {v}")
    else:
        print("violations: none")

    out_path = Path(args.json_out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(
        json.dumps(
            {
                "config": asdict(cfg),
                "hours": args.hours,
                "segment_s": args.segment_s,
                "aggregate": agg,
                "violations": violations,
                "runs": runs,
            },
            ensure_ascii=True,
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    print(f"Wrote report: {out_path}")

    if args.check and violations:
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
