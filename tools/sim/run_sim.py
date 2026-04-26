#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Dict, List

from anchor_sim import SimConfig, scenarios_for_set, simulate_scenario


DEFAULT_THRESHOLDS: Dict[str, Dict[str, object]] = {
    "calm_good_gps": {
        "time_in_deadband_pct_min": 35.0,
        "p95_error_m_max": 18.0,
        "max_error_m_max": 35.0,
    },
    "constant_current": {
        "time_in_deadband_pct_min": 30.0,
        "p95_error_m_max": 20.0,
        "max_error_m_max": 40.0,
    },
    "gusts": {
        "time_in_deadband_pct_min": 22.0,
        "p95_error_m_max": 24.0,
        "max_error_m_max": 45.0,
    },
    "gnss_dropout": {
        "p95_error_m_max": 28.0,
        "max_error_m_max": 50.0,
    },
    "poor_hdop": {
        "p95_error_m_max": 30.0,
        "max_error_m_max": 55.0,
    },
    "position_jumps": {
        "p95_error_m_max": 26.0,
        "max_error_m_max": 50.0,
    },
    "comm_loss": {
        "p95_error_m_max": 40.0,
        "max_error_m_max": 80.0,
    },
    "nan_data": {
        "p95_error_m_max": 40.0,
        "max_error_m_max": 80.0,
    },
    "river_oka_normal_55lb": {
        "time_in_deadband_pct_min": 12.0,
        "p95_error_m_max": 34.0,
        "max_error_m_max": 70.0,
        "p95_rocking_roll_deg_min": 0.8,
    },
    "volga_spring_flow_80lb": {
        "p95_error_m_max": 90.0,
        "max_error_m_max": 150.0,
        "p95_rocking_roll_deg_min": 2.0,
    },
    "rybinsk_fetch_55lb": {
        "p95_error_m_max": 110.0,
        "max_error_m_max": 190.0,
        "p95_rocking_roll_deg_min": 7.0,
    },
    "ladoga_storm_abort": {
        "max_error_m_max": 500.0,
        "p95_rocking_roll_deg_min": 14.0,
        "required_failsafe_reason": "ENV_ABORT_EXPECTED",
    },
    "baltic_gulf_drift": {
        "p95_error_m_max": 95.0,
        "max_error_m_max": 170.0,
        "p95_rocking_roll_deg_min": 5.0,
    },
}


def check_thresholds(scenario_name: str, metrics: Dict[str, object]) -> List[str]:
    limits = DEFAULT_THRESHOLDS.get(scenario_name, {})
    errs: List[str] = []
    min_dead = limits.get("time_in_deadband_pct_min")
    max_p95 = limits.get("p95_error_m_max")
    max_max = limits.get("max_error_m_max")
    min_rocking = limits.get("p95_rocking_roll_deg_min")
    required_failsafe = limits.get("required_failsafe_reason")
    if isinstance(min_dead, float) and metrics["time_in_deadband_pct"] < min_dead:
        errs.append(
            f"time_in_deadband_pct={metrics['time_in_deadband_pct']:.2f} < {min_dead:.2f}"
        )
    if isinstance(max_p95, float) and metrics["p95_error_m"] > max_p95:
        errs.append(f"p95_error_m={metrics['p95_error_m']:.2f} > {max_p95:.2f}")
    if isinstance(max_max, float) and metrics["max_error_m"] > max_max:
        errs.append(f"max_error_m={metrics['max_error_m']:.2f} > {max_max:.2f}")
    if isinstance(min_rocking, float) and metrics["p95_rocking_roll_deg"] < min_rocking:
        errs.append(
            f"p95_rocking_roll_deg={metrics['p95_rocking_roll_deg']:.2f} < {min_rocking:.2f}"
        )
    if isinstance(required_failsafe, str):
        events = metrics.get("events", [])
        saw_required = any(
            isinstance(e, dict) and e.get("failsafe_reason") == required_failsafe
            for e in events
        )
        if not saw_required:
            errs.append(f"missing failsafe_reason={required_failsafe}")
    return errs


def main() -> int:
    ap = argparse.ArgumentParser(description="BoatLock Anchor Simulator")
    ap.add_argument("--seed", type=int, default=7, help="Random seed")
    ap.add_argument(
        "--scenario-set",
        choices=("core", "russian", "all"),
        default="core",
        help="Scenario set to run",
    )
    ap.add_argument("--check", action="store_true", help="Fail on threshold violations")
    ap.add_argument(
        "--json-out",
        type=str,
        default="tools/sim/report.json",
        help="Path to JSON report",
    )
    args = ap.parse_args()

    cfg = SimConfig()
    results = []
    failed = False

    print("Scenario summary:")
    print("name | deadband% | p95(m) | max(m) | sat% | dirChanges/min | rock95(deg) | status")
    print("-" * 110)
    for i, sc in enumerate(scenarios_for_set(args.scenario_set)):
        r = simulate_scenario(cfg, sc, seed=args.seed + i)
        m = r["metrics"]
        errs = check_thresholds(sc.name, m)
        status = "PASS" if not errs else "FAIL"
        if errs:
            failed = True
        results.append(
            {
                "scenario": sc.name,
                "duration_s": r["duration_s"],
                "metrics": m,
                "status": status,
                "violations": errs,
            }
        )
        print(
            f"{sc.name} | "
            f"{m['time_in_deadband_pct']:.2f} | "
            f"{m['p95_error_m']:.2f} | "
            f"{m['max_error_m']:.2f} | "
            f"{m['control_saturation_time_pct']:.2f} | "
            f"{m['num_thrust_direction_changes_per_min']:.2f} | "
            f"{m['p95_rocking_roll_deg']:.2f} | "
            f"{status}"
        )
        for e in errs:
            print(f"  - {e}")

    out_path = Path(args.json_out)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(
        json.dumps(
            {
                "config": cfg.__dict__,
                "scenario_set": args.scenario_set,
                "thresholds": DEFAULT_THRESHOLDS,
                "results": results,
            },
            ensure_ascii=True,
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )
    print(f"\nWrote report: {out_path}")

    if args.check and failed:
        print("Simulation threshold checks failed.")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
