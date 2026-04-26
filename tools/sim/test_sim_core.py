#!/usr/bin/env python3
from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest

from anchor_sim import (
    AnchorController,
    BrushedMotorModel,
    EnvironmentEvent,
    EnvironmentProfile,
    GnssGate,
    GnssObservation,
    SimConfig,
    SteeringModel,
    YawModel,
    clamp,
    active_environment_events,
    environment_event_reason,
    environment_accel_mps2,
    environment_yaw_accel_dps2,
    russian_water_scenarios,
    scenarios_for_set,
    simulate_scenario,
    wave_rocking_roll_deg,
    wave_steepness_ratio,
)
from run_sim import check_thresholds, provenance_for_scenario_set, thresholds_for_scenario_set
from scenario_data import (
    environment_scenarios_for_set,
    provenance_for_set,
    scenario_set_names,
    thresholds_for_set,
)


class SimCoreTests(unittest.TestCase):
    def _write_scenario_catalog(self, body: dict) -> Path:
        path = Path(self._tmpdir.name) / "environment_profiles.json"
        path.write_text(json.dumps(body, ensure_ascii=True), encoding="utf-8")
        return path

    def setUp(self) -> None:
        self._tmpdir = tempfile.TemporaryDirectory()

    def tearDown(self) -> None:
        self._tmpdir.cleanup()

    def _minimal_catalog(self, scenario: dict | None = None) -> dict:
        return {
            "schema_version": 3,
            "scenario_sets": {
                "test": [
                    scenario
                    or {
                        "id": "test_profile",
                        "water_body": "Test water",
                        "provenance": {
                            "source": "test_fixture",
                            "source_candidate_id": "test_profile_source",
                            "confidence": "test_confidence",
                        },
                        "duration_s": 60,
                        "initial_pos_m": [1.0, 2.0],
                        "boat": {
                            "loaded_boat_mass_kg": 260.0,
                            "water_drag_coefficient": 0.9,
                            "water_drag_area_m2": 0.3,
                            "windage_drag_coefficient": 1.05,
                            "windage_area_m2": 1.1,
                        },
                        "environment": {
                            "current_mps": 0.1,
                            "wind_mps": 1.0,
                            "wave_height_m": 0.2,
                        },
                        "gnss": {"noise_sigma_m": 0.7, "sats": 10, "hdop": 1.0},
                        "thresholds": {"p95_error_m_max": 10.0},
                    }
                ]
            },
        }

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
        self.assertGreater(wave_steepness_ratio(profile), 0.05)

    def test_environment_events_add_transient_wake_and_chop(self) -> None:
        profile = EnvironmentProfile(
            name="test_events",
            water_body="test",
            events=(
                EnvironmentEvent("wake_a", "wake", 10.0, 20.0, 0.6, 1.6, 180.0),
                EnvironmentEvent("chop_a", "chop", 40.0, 20.0, 0.7, 1.2, 250.0),
            ),
        )
        self.assertEqual(environment_event_reason(profile, 12), "WAKE_EVENT")
        self.assertEqual(environment_event_reason(profile, 45), "SHORT_CHOP_EVENT")
        self.assertEqual(len(active_environment_events(profile, 45)), 1)
        self.assertGreater(wave_steepness_ratio(profile, 45), 0.15)
        self.assertGreater(abs(wave_rocking_roll_deg(profile, 45)), 0.1)

    def test_environment_yaw_accel_uses_cross_axis_forcing(self) -> None:
        cfg = SimConfig()
        profile = EnvironmentProfile(
            name="test_yaw",
            water_body="test",
            current_mps=0.5,
            current_direction_deg=90.0,
        )
        beam = environment_yaw_accel_dps2(cfg, profile, 0, 0.0, 0.0, 0.0)
        aligned = environment_yaw_accel_dps2(cfg, profile, 0, 90.0, 0.0, 0.0)
        self.assertGreater(abs(beam), 0.1)
        self.assertLess(abs(aligned), abs(beam))

    def test_yaw_model_adds_heading_inertia(self) -> None:
        cfg = SimConfig(yaw_accel_limit_dps2=30.0, yaw_damping_per_s=0.0)
        yaw = YawModel(cfg)
        first = yaw.update(90.0, 0.0, 1.0)
        second = yaw.update(90.0, 0.0, 1.0)
        self.assertEqual(first.heading_delta_deg, 30.0)
        self.assertEqual(second.heading_delta_deg, 60.0)
        self.assertGreater(first.heading_inertia_lag_deg, second.heading_inertia_lag_deg)

    def test_environment_profile_uses_loaded_mass_for_force_acceleration(self) -> None:
        light = EnvironmentProfile(
            name="light",
            water_body="test",
            loaded_boat_mass_kg=200.0,
            water_drag_area_m2=0.35,
            windage_area_m2=0.000001,
            current_mps=0.6,
        )
        heavy = EnvironmentProfile(
            name="heavy",
            water_body="test",
            loaded_boat_mass_kg=600.0,
            water_drag_area_m2=0.35,
            windage_area_m2=0.000001,
            current_mps=0.6,
        )
        light_ax, _ = environment_accel_mps2(light, 0)
        heavy_ax, _ = environment_accel_mps2(heavy, 0)
        opposed_ax, _ = environment_accel_mps2(light, 0, boat_vx_mps=0.6)
        self.assertGreater(light_ax, heavy_ax)
        self.assertAlmostEqual(opposed_ax, 0.0)

    def test_russian_water_scenarios_are_catalogued(self) -> None:
        scenarios = russian_water_scenarios()
        names = {s.name for s in scenarios}
        self.assertIn("river_oka_normal_55lb", names)
        self.assertIn("ladoga_storm_abort", names)
        self.assertTrue(any(s.profile is not None and s.profile.abort_expected for s in scenarios))

    def test_scenario_data_loads_russian_profiles(self) -> None:
        self.assertIn("russian", scenario_set_names())
        scenarios = environment_scenarios_for_set("russian")
        oka = next(s for s in scenarios if s.name == "river_oka_normal_55lb")
        ladoga = next(s for s in scenarios if s.name == "ladoga_storm_abort")
        self.assertEqual(oka.duration_s, 300)
        self.assertEqual(oka.initial_pos_m, (12.0, -3.0))
        self.assertIsNotNone(oka.profile)
        self.assertEqual(oka.profile.water_body, "Oka lowland river")
        self.assertEqual(oka.profile.loaded_boat_mass_kg, 260.0)
        self.assertEqual(oka.profile.water_drag_area_m2, 0.34)
        self.assertEqual(oka.profile.windage_area_m2, 1.1)
        self.assertEqual(oka.profile.current_mps, 0.35)
        self.assertEqual(oka.profile.wave_height_m, 0.2)
        self.assertEqual(oka.profile.events[0].kind, "wake")
        self.assertEqual(ladoga.duration_s, 180)
        self.assertTrue(ladoga.profile.abort_expected)

    def test_scenario_data_exports_thresholds(self) -> None:
        thresholds = thresholds_for_set("russian")
        self.assertEqual(
            thresholds["ladoga_storm_abort"]["required_failsafe_reason"],
            "ENV_ABORT_EXPECTED",
        )
        self.assertEqual(thresholds["river_oka_normal_55lb"]["p95_error_m_max"], 34.0)
        self.assertEqual(thresholds["river_oka_normal_55lb"]["wake_event_count_min"], 1.0)
        self.assertEqual(thresholds["rybinsk_fetch_55lb"]["chop_event_count_min"], 2.0)

    def test_scenario_data_exports_provenance(self) -> None:
        provenance = provenance_for_set("russian")
        self.assertEqual(
            provenance["river_oka_normal_55lb"]["source"],
            "tools/sim/research/environment_inputs.raw.json",
        )
        self.assertEqual(
            provenance["river_oka_normal_55lb"]["source_candidate_id"],
            "river_oka_normal_55lb",
        )
        self.assertEqual(
            provenance["river_oka_normal_55lb"]["confidence"],
            "low_for_current_high_for_size",
        )

    def test_scenario_data_rejects_bad_schema_version(self) -> None:
        catalog = self._minimal_catalog()
        catalog["schema_version"] = 1
        path = self._write_scenario_catalog(catalog)

        with self.assertRaisesRegex(ValueError, "schema_version"):
            scenario_set_names(path)

    def test_scenario_data_rejects_unknown_set(self) -> None:
        path = self._write_scenario_catalog(self._minimal_catalog())

        with self.assertRaisesRegex(ValueError, "unknown scenario set missing"):
            environment_scenarios_for_set("missing", path)

    def test_scenario_data_rejects_duplicate_ids(self) -> None:
        catalog = self._minimal_catalog()
        catalog["scenario_sets"]["test"].append(catalog["scenario_sets"]["test"][0].copy())
        path = self._write_scenario_catalog(catalog)

        with self.assertRaisesRegex(ValueError, "duplicates test_profile"):
            environment_scenarios_for_set("test", path)

    def test_scenario_data_rejects_nonfinite_and_nonintegral_values(self) -> None:
        path = Path(self._tmpdir.name) / "bad.json"
        path.write_text(
            '{"schema_version":3,"scenario_sets":{"test":[{"id":"bad","water_body":"x",'
            '"provenance":{"source":"x","source_candidate_id":"bad","confidence":"x"},'
            '"duration_s":NaN,"initial_pos_m":[1,2],"environment":{}}]}}',
            encoding="utf-8",
        )
        with self.assertRaisesRegex(ValueError, "non-finite"):
            environment_scenarios_for_set("test", path)

        catalog = self._minimal_catalog()
        catalog["scenario_sets"]["test"][0]["duration_s"] = 60.5
        path = self._write_scenario_catalog(catalog)
        with self.assertRaisesRegex(ValueError, "duration_s"):
            environment_scenarios_for_set("test", path)

    def test_scenario_data_rejects_bad_environment_ranges(self) -> None:
        catalog = self._minimal_catalog()
        catalog["scenario_sets"]["test"][0]["environment"]["gust_mps"] = 3.0
        catalog["scenario_sets"]["test"][0]["environment"]["gust_duration_s"] = 0.0
        path = self._write_scenario_catalog(catalog)

        with self.assertRaisesRegex(ValueError, "gust duration"):
            environment_scenarios_for_set("test", path)

    def test_scenario_data_loads_and_validates_environment_events(self) -> None:
        catalog = self._minimal_catalog()
        catalog["scenario_sets"]["test"][0]["environment"]["events"] = [
            {
                "name": "wake_a",
                "kind": "wake",
                "start_s": 10.0,
                "duration_s": 12.0,
                "height_m": 0.5,
                "period_s": 1.6,
                "direction_deg": 200.0,
            }
        ]
        path = self._write_scenario_catalog(catalog)
        scenario = environment_scenarios_for_set("test", path)[0]
        self.assertEqual(len(scenario.profile.events), 1)
        self.assertEqual(scenario.profile.events[0].kind, "wake")

        catalog["scenario_sets"]["test"][0]["environment"]["events"][0]["kind"] = "swell"
        path = self._write_scenario_catalog(catalog)
        with self.assertRaisesRegex(ValueError, "kind"):
            environment_scenarios_for_set("test", path)

    def test_scenario_data_rejects_bad_boat_physics_ranges(self) -> None:
        catalog = self._minimal_catalog()
        catalog["scenario_sets"]["test"][0]["boat"]["loaded_boat_mass_kg"] = 0.0
        path = self._write_scenario_catalog(catalog)

        with self.assertRaisesRegex(ValueError, "loaded_boat_mass_kg"):
            environment_scenarios_for_set("test", path)

        catalog = self._minimal_catalog()
        catalog["scenario_sets"]["test"][0]["boat"]["extra_drag_knob"] = 1.0
        path = self._write_scenario_catalog(catalog)

        with self.assertRaisesRegex(ValueError, "unknown keys"):
            environment_scenarios_for_set("test", path)

    def test_scenario_data_rejects_unknown_threshold_keys(self) -> None:
        catalog = self._minimal_catalog()
        catalog["scenario_sets"]["test"][0]["thresholds"]["p95_typo"] = 12.0
        path = self._write_scenario_catalog(catalog)

        with self.assertRaisesRegex(ValueError, "not a known threshold key"):
            thresholds_for_set("test", path)

    def test_scenario_data_rejects_bad_provenance(self) -> None:
        catalog = self._minimal_catalog()
        catalog["scenario_sets"]["test"][0]["provenance"]["extra"] = "stale"
        path = self._write_scenario_catalog(catalog)

        with self.assertRaisesRegex(ValueError, "unknown keys"):
            environment_scenarios_for_set("test", path)

        catalog = self._minimal_catalog()
        catalog["scenario_sets"]["test"][0]["provenance"]["confidence"] = ""
        path = self._write_scenario_catalog(catalog)

        with self.assertRaisesRegex(ValueError, "confidence"):
            provenance_for_set("test", path)

    def test_all_scenario_set_names_are_unique(self) -> None:
        names = [scenario.name for scenario in scenarios_for_set("all")]
        self.assertEqual(len(names), len(set(names)))

    def test_run_sim_provenance_for_all_is_catalog_backed(self) -> None:
        provenance = provenance_for_scenario_set("all")
        self.assertIn("river_oka_normal_55lb", provenance)
        self.assertNotIn("calm_good_gps", provenance)

    def test_ladoga_required_failsafe_threshold_is_enforced(self) -> None:
        thresholds = thresholds_for_scenario_set("russian")
        metrics = {
            "time_in_deadband_pct": 0.0,
            "p95_error_m": 0.0,
            "max_error_m": 0.0,
            "p95_rocking_roll_deg": 30.0,
            "p95_rocking_roll_rate_dps": 50.0,
            "max_wave_steepness": 0.2,
            "gnss_frame_degraded_time_pct": 100.0,
            "heading_frame_degraded_time_pct": 100.0,
            "p95_heading_error_deg": 0.0,
            "steering_jammed_time_pct": 0.0,
            "steering_backlash_crossings_per_min": 0.0,
            "events": [],
        }

        violations = check_thresholds("ladoga_storm_abort", metrics, thresholds)

        self.assertIn("missing failsafe_reason=ENV_ABORT_EXPECTED", violations)

    def test_ladoga_storm_marks_environment_abort(self) -> None:
        cfg = SimConfig()
        scenario = next(s for s in russian_water_scenarios() if s.name == "ladoga_storm_abort")
        result = simulate_scenario(cfg, scenario, seed=3)
        events = result["metrics"]["events"]
        self.assertTrue(any(e["failsafe_reason"] == "ENV_ABORT_EXPECTED" for e in events))
        self.assertTrue(any(e.get("sensor_reason") == "SENSOR_FRAME_ROLL_AND_CHOP" for e in events))

    def test_wave_chop_reports_sensor_frame_degradation_metrics(self) -> None:
        cfg = SimConfig()
        scenario = next(s for s in russian_water_scenarios() if s.name == "rybinsk_fetch_55lb")
        result = simulate_scenario(cfg, scenario, seed=6)
        metrics = result["metrics"]
        self.assertGreater(metrics["max_wave_steepness"], 0.09)
        self.assertGreater(metrics["p95_rocking_roll_rate_dps"], 14.0)
        self.assertGreater(metrics["gnss_frame_degraded_time_pct"], 80.0)
        self.assertTrue(any(e.get("sensor_reason") != "NONE" for e in metrics["events"]))

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
        self.assertEqual(metrics["loaded_boat_mass_kg"], 260.0)
        self.assertEqual(metrics["water_drag_area_m2"], 0.34)
        self.assertEqual(metrics["windage_area_m2"], 1.1)
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
        self.assertGreaterEqual(metrics["wake_event_count"], 3.0)
        self.assertGreater(metrics["wake_event_time_pct"], 20.0)
        self.assertGreater(metrics["p95_heading_inertia_lag_deg"], 1.0)
        self.assertGreater(metrics["p95_environment_yaw_accel_dps2"], 0.1)
        self.assertTrue(any(e.get("environment_reason") == "WAKE_EVENT" for e in metrics["events"]))

    def test_wake_steering_backlash_is_offline_scenario(self) -> None:
        names = {s.name for s in scenarios_for_set("core")}
        self.assertIn("wake_steering_backlash", names)

    def test_short_steep_chop_is_offline_scenario(self) -> None:
        scenario = next(s for s in scenarios_for_set("core") if s.name == "short_steep_chop")
        result = simulate_scenario(SimConfig(), scenario, seed=8)
        metrics = result["metrics"]
        self.assertGreaterEqual(metrics["chop_event_count"], 2.0)
        self.assertGreater(metrics["chop_event_time_pct"], 25.0)
        self.assertGreater(metrics["max_wave_steepness"], 0.16)
        self.assertGreater(metrics["gnss_frame_degraded_time_pct"], 20.0)
        self.assertTrue(any(e.get("environment_reason") == "SHORT_CHOP_EVENT" for e in metrics["events"]))


if __name__ == "__main__":
    unittest.main()
