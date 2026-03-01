#!/usr/bin/env python3
from __future__ import annotations

import unittest

from anchor_sim import SimConfig, GnssObservation, GnssGate, AnchorController, clamp


class SimCoreTests(unittest.TestCase):
    def test_clamp(self) -> None:
        self.assertEqual(clamp(5.0, 0.0, 1.0), 1.0)
        self.assertEqual(clamp(-1.0, 0.0, 1.0), 0.0)
        self.assertEqual(clamp(0.5, 0.0, 1.0), 0.5)

    def test_gnss_gate_rejects_stale(self) -> None:
        cfg = SimConfig(max_gps_age_ms=2000)
        gate = GnssGate(cfg)
        obs = GnssObservation(lat=59.0, lon=30.0, sats=10, hdop=1.0, valid=True)
        ok, reason, _x, _y = gate.evaluate(10, obs, 59.0, 30.0, age_ms=3001)
        self.assertFalse(ok)
        self.assertEqual(reason, "GPS_DATA_STALE")

    def test_gnss_gate_rejects_jump(self) -> None:
        cfg = SimConfig(max_position_jump_m=10.0)
        gate = GnssGate(cfg)
        obs1 = GnssObservation(lat=59.0, lon=30.0, sats=12, hdop=1.0, valid=True)
        obs2 = GnssObservation(lat=59.001, lon=30.001, sats=12, hdop=1.0, valid=True)
        ok1, _, _, _ = gate.evaluate(1, obs1, 59.0, 30.0, age_ms=0)
        self.assertTrue(ok1)
        ok2, reason, _, _ = gate.evaluate(2, obs2, 59.0, 30.0, age_ms=0)
        self.assertFalse(ok2)
        self.assertEqual(reason, "GPS_POSITION_JUMP")

    def test_controller_respects_ramp(self) -> None:
        cfg = SimConfig()
        ctrl = AnchorController(cfg)
        p1 = ctrl.update(0.0, 20.0, True)
        p2 = ctrl.update(0.1, 20.0, True)
        self.assertGreaterEqual(p2, p1)
        self.assertLessEqual((p2 - p1), 4.0)


if __name__ == "__main__":
    unittest.main()
