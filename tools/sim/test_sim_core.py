#!/usr/bin/env python3
from __future__ import annotations

import unittest

from anchor_sim import (
    AnchorController,
    EnvironmentProfile,
    GnssGate,
    GnssObservation,
    SimConfig,
    clamp,
    environment_accel_mps2,
    russian_water_scenarios,
    simulate_scenario,
    wave_rocking_roll_deg,
)


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

    def test_environment_profile_adds_wave_rocking_and_forcing(self) -> None:
        profile = EnvironmentProfile(
            name="test_fetch",
            water_body="test",
            current_mps=0.3,
            wind_mps=8.0,
            wave_height_m=1.2,
            wave_period_s=3.0,
        )
        ax0, ay0 = environment_accel_mps2(profile, 0)
        ax1, ay1 = environment_accel_mps2(profile, 1)
        self.assertNotEqual((ax0, ay0), (ax1, ay1))
        self.assertGreater(abs(wave_rocking_roll_deg(profile, 1)), 1.0)

    def test_russian_water_scenarios_are_catalogued(self) -> None:
        scenarios = russian_water_scenarios()
        names = {s.name for s in scenarios}
        self.assertIn("river_oka_normal_55lb", names)
        self.assertIn("ladoga_storm_abort", names)
        self.assertTrue(any(s.profile is not None and s.profile.abort_expected for s in scenarios))

    def test_ladoga_storm_marks_environment_abort(self) -> None:
        cfg = SimConfig()
        scenario = next(s for s in russian_water_scenarios() if s.name == "ladoga_storm_abort")
        result = simulate_scenario(cfg, scenario, seed=3)
        events = result["metrics"]["events"]
        self.assertTrue(any(e["failsafe_reason"] == "ENV_ABORT_EXPECTED" for e in events))


if __name__ == "__main__":
    unittest.main()
