#!/usr/bin/env python3
from __future__ import annotations

import unittest

from anchor_sim import SimConfig
from run_soak import aggregate, run_soak


class SoakTests(unittest.TestCase):
    def test_aggregate_empty(self) -> None:
        a = aggregate([])
        self.assertEqual(a["duration_s"], 0.0)
        self.assertEqual(a["event_count"], 0.0)

    def test_short_soak_runs_and_has_fault_events(self) -> None:
        cfg = SimConfig()
        runs, agg, violations = run_soak(cfg, total_duration_s=2400, segment_s=300, seed=9)
        self.assertGreater(len(runs), 0)
        self.assertGreater(agg["duration_s"], 0.0)
        self.assertGreaterEqual(agg["event_count"], 1.0)
        self.assertEqual(agg["invalid_thrust_count"], 0.0)
        self.assertEqual(agg["invalid_motor_count"], 0.0)
        # Short run still should include comm + nan segments from scenario cycle.
        self.assertFalse(any("missing COMM_TIMEOUT" in v for v in violations))
        self.assertFalse(any("missing INTERNAL_ERROR_NAN" in v for v in violations))


if __name__ == "__main__":
    unittest.main()
