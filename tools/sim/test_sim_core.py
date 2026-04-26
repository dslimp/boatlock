#!/usr/bin/env python3
from __future__ import annotations

import unittest

from anchor_sim import (
    AnchorController,
    BrushedMotorModel,
    EnvironmentProfile,
    GnssGate,
    GnssObservation,
    SimConfig,
    SteeringModel,
    clamp,
    environment_accel_mps2,
    russian_water_scenarios,
    scenarios_for_set,
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

    def test_motor_deadband_blocks_low_command(self) -> None:
        cfg = SimConfig(motor_deadband_pct=10.0)
        motor = BrushedMotorModel(cfg)
        state = motor.update(5.0, 1.0)
        self.assertFalse(state.driver_active)
        self.assertEqual(state.applied_thrust_pct, 0.0)
        self.assertEqual(state.current_a, 0.0)

    def test_motor_current_limit_caps_output(self) -> None:
        cfg = SimConfig(motor_current_limit_a=20.0)
        motor = BrushedMotorModel(cfg)
        state = motor.update(100.0, 1.0)
        self.assertTrue(state.current_limited)
        self.assertLessEqual(state.current_a, 20.0)
        self.assertLess(state.applied_thrust_pct, 100.0)
        self.assertLess(state.battery_voltage_v, cfg.motor_nominal_voltage_v)

    def test_motor_thermal_derate_reduces_output(self) -> None:
        cfg = SimConfig(
            motor_current_limit_a=80.0,
            motor_thermal_limit_c=28.0,
            motor_thermal_shutdown_c=34.0,
            motor_heat_c_per_a2_s=0.0004,
            motor_cooling_per_s=0.0,
        )
        motor = BrushedMotorModel(cfg)
        states = [motor.update(100.0, 1.0) for _ in range(8)]
        self.assertTrue(any(s.thermal_derated for s in states))
        self.assertLess(states[-1].applied_thrust_pct, states[1].applied_thrust_pct)

    def test_steering_response_limits_heading_delta(self) -> None:
        cfg = SimConfig(steering_response_dps=12.0, max_turn_rate_dps=90.0)
        steering = SteeringModel(cfg)
        state = steering.update(0.0, 90.0, 1.0)
        self.assertEqual(state.heading_delta_deg, 12.0)
        self.assertFalse(state.jammed)

    def test_steering_backlash_consumes_reversal(self) -> None:
        cfg = SimConfig(steering_response_dps=90.0, steering_backlash_deg=8.0)
        steering = SteeringModel(cfg)
        first = steering.update(0.0, 20.0, 1.0)
        second = steering.update(1.0, -6.0, 1.0)
        third = steering.update(2.0, -10.0, 1.0)
        self.assertEqual(first.heading_delta_deg, 20.0)
        self.assertTrue(second.backlash_crossing)
        self.assertEqual(second.heading_delta_deg, 0.0)
        self.assertEqual(third.heading_delta_deg, -8.0)

    def test_steering_jam_blocks_heading_delta(self) -> None:
        cfg = SimConfig(steering_jam_start_s=5.0, steering_jam_duration_s=2.0)
        steering = SteeringModel(cfg)
        jammed = steering.update(5.0, 30.0, 1.0)
        recovered = steering.update(7.0, 30.0, 1.0)
        self.assertTrue(jammed.jammed)
        self.assertEqual(jammed.heading_delta_deg, 0.0)
        self.assertFalse(recovered.jammed)
        self.assertGreater(recovered.heading_delta_deg, 0.0)

    def test_steering_wrong_zero_biases_achieved_delta(self) -> None:
        cfg = SimConfig(steering_response_dps=90.0, steering_wrong_zero_deg=2.0)
        steering = SteeringModel(cfg)
        state = steering.update(0.0, 10.0, 1.0)
        self.assertEqual(state.heading_delta_deg, 8.0)

    def test_simulation_reports_motor_metrics(self) -> None:
        cfg = SimConfig()
        scenario = next(s for s in russian_water_scenarios() if s.name == "river_oka_normal_55lb")
        result = simulate_scenario(cfg, scenario, seed=4)
        metrics = result["metrics"]
        self.assertIn("max_motor_current_a", metrics)
        self.assertGreater(metrics["max_motor_current_a"], 0.0)
        self.assertLess(metrics["min_battery_voltage_v"], cfg.motor_nominal_voltage_v)
        self.assertEqual(metrics["invalid_motor_count"], 0.0)

    def test_simulation_reports_steering_metrics(self) -> None:
        cfg = SimConfig()
        scenario = next(s for s in scenarios_for_set("core") if s.name == "wake_steering_backlash")
        result = simulate_scenario(cfg, scenario, seed=5)
        metrics = result["metrics"]
        self.assertIn("p95_heading_error_deg", metrics)
        self.assertGreater(metrics["max_heading_error_deg"], 0.0)
        self.assertGreater(metrics["steering_jammed_time_pct"], 0.0)
        self.assertGreater(metrics["steering_backlash_crossings_per_min"], 0.0)
        self.assertIn("thrust_while_misaligned_time_pct", metrics)

    def test_wake_steering_backlash_is_offline_scenario(self) -> None:
        names = {s.name for s in scenarios_for_set("core")}
        self.assertIn("wake_steering_backlash", names)


if __name__ == "__main__":
    unittest.main()
