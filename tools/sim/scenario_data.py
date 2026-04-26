#!/usr/bin/env python3
from __future__ import annotations

import json
import math
from pathlib import Path
from typing import Dict, List, Mapping, Tuple

from anchor_sim import (
    EnvironmentEvent,
    EnvironmentProfile,
    Scenario,
    make_gnss_observer,
    profiled_environment_accel_fn,
)


DEFAULT_SCENARIO_DATA = Path(__file__).with_name("scenarios") / "environment_profiles.json"
SCENARIO_SCHEMA_VERSION = 3

ALLOWED_ROOT_KEYS = {"schema_version", "scenario_sets"}
ALLOWED_SCENARIO_KEYS = {
    "id",
    "water_body",
    "provenance",
    "motor_class",
    "expected",
    "duration_s",
    "initial_pos_m",
    "boat",
    "environment",
    "gnss",
    "thresholds",
}
ALLOWED_PROVENANCE_KEYS = {"source", "source_candidate_id", "confidence"}
ALLOWED_BOAT_KEYS = {
    "loaded_boat_mass_kg",
    "water_drag_coefficient",
    "water_drag_area_m2",
    "windage_drag_coefficient",
    "windage_area_m2",
}
ALLOWED_ENVIRONMENT_KEYS = {
    "current_mps",
    "current_direction_deg",
    "current_swing_deg",
    "current_swing_period_s",
    "wind_mps",
    "wind_direction_deg",
    "wind_swing_deg",
    "wind_swing_period_s",
    "gust_mps",
    "gust_period_s",
    "gust_duration_s",
    "wave_height_m",
    "wave_period_s",
    "wave_direction_deg",
    "abort_expected",
    "events",
}
ALLOWED_ENVIRONMENT_EVENT_KEYS = {
    "name",
    "kind",
    "start_s",
    "duration_s",
    "height_m",
    "period_s",
    "direction_deg",
}
ALLOWED_GNSS_KEYS = {"noise_sigma_m", "sats", "hdop", "sentences"}
ALLOWED_THRESHOLD_KEYS = {
    "time_in_deadband_pct_min",
    "p95_error_m_max",
    "max_error_m_max",
    "p95_rocking_roll_deg_min",
    "p95_rocking_roll_rate_dps_min",
    "max_wave_steepness_min",
    "gnss_frame_degraded_time_pct_min",
    "heading_frame_degraded_time_pct_min",
    "wake_event_time_pct_min",
    "chop_event_time_pct_min",
    "wake_event_count_min",
    "chop_event_count_min",
    "p95_heading_error_deg_max",
    "p95_yaw_rate_dps_max",
    "p95_heading_inertia_lag_deg_min",
    "p95_environment_yaw_accel_dps2_min",
    "steering_jammed_time_pct_min",
    "steering_backlash_crossings_per_min_min",
    "required_failsafe_reason",
}


def _as_mapping(value: object, path: str) -> Mapping[str, object]:
    if not isinstance(value, dict):
        raise ValueError(f"{path} must be an object")
    return value


def _reject_unknown_keys(data: Mapping[str, object], allowed: set[str], path: str) -> None:
    unknown = sorted(set(data) - allowed)
    if unknown:
        raise ValueError(f"{path} has unknown keys: {', '.join(unknown)}")


def _as_list(value: object, path: str) -> List[object]:
    if not isinstance(value, list):
        raise ValueError(f"{path} must be a list")
    return value


def _finite_number_value(value: object, path: str) -> float:
    if not isinstance(value, (int, float)) or isinstance(value, bool):
        raise ValueError(f"{path} must be a number")
    number = float(value)
    if not math.isfinite(number):
        raise ValueError(f"{path} must be finite")
    return number


def _required_str(data: Mapping[str, object], key: str, path: str) -> str:
    value = data.get(key)
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{path}.{key} must be a non-empty string")
    return value


def _optional_str(data: Mapping[str, object], key: str, path: str) -> None:
    if key in data:
        _required_str(data, key, path)


def _number(data: Mapping[str, object], key: str, path: str, default: float | None = None) -> float:
    value = data.get(key, default)
    return _finite_number_value(value, f"{path}.{key}")


def _integer(data: Mapping[str, object], key: str, path: str, default: int | None = None) -> int:
    value = data.get(key, default)
    number = _finite_number_value(value, f"{path}.{key}")
    if not number.is_integer():
        raise ValueError(f"{path}.{key} must be an integer")
    return int(number)


def _positive_integer(data: Mapping[str, object], key: str, path: str) -> int:
    value = _integer(data, key, path)
    if value <= 0:
        raise ValueError(f"{path}.{key} must be positive")
    return value


def _direction(data: Mapping[str, object], key: str, path: str, default: float) -> float:
    value = _number(data, key, path, default)
    if value < 0.0 or value >= 360.0:
        raise ValueError(f"{path}.{key} must be in [0, 360)")
    return value


def _non_negative(data: Mapping[str, object], key: str, path: str, default: float) -> float:
    value = _number(data, key, path, default)
    if value < 0.0:
        raise ValueError(f"{path}.{key} must be non-negative")
    return value


def _positive(data: Mapping[str, object], key: str, path: str, default: float) -> float:
    value = _number(data, key, path, default)
    if value <= 0.0:
        raise ValueError(f"{path}.{key} must be positive")
    return value


def _bool(data: Mapping[str, object], key: str, path: str, default: bool = False) -> bool:
    value = data.get(key, default)
    if not isinstance(value, bool):
        raise ValueError(f"{path}.{key} must be a boolean")
    return value


def _position(data: Mapping[str, object], path: str) -> Tuple[float, float]:
    raw = _as_list(data.get("initial_pos_m"), f"{path}.initial_pos_m")
    if len(raw) != 2:
        raise ValueError(f"{path}.initial_pos_m must contain exactly two numbers")
    return (
        _finite_number_value(raw[0], f"{path}.initial_pos_m[0]"),
        _finite_number_value(raw[1], f"{path}.initial_pos_m[1]"),
    )


def _environment_events(env: Mapping[str, object], path: str) -> Tuple[EnvironmentEvent, ...]:
    if "events" not in env:
        return ()
    rows = _as_list(env.get("events"), f"{path}.events")
    events: List[EnvironmentEvent] = []
    seen: set[str] = set()
    for index, row in enumerate(rows):
        event_path = f"{path}.events[{index}]"
        event = _as_mapping(row, event_path)
        _reject_unknown_keys(event, ALLOWED_ENVIRONMENT_EVENT_KEYS, event_path)
        name = _required_str(event, "name", event_path)
        if name in seen:
            raise ValueError(f"{event_path}.name duplicates {name}")
        seen.add(name)
        kind = _required_str(event, "kind", event_path)
        if kind not in ("wake", "chop"):
            raise ValueError(f"{event_path}.kind must be wake or chop")
        events.append(
            EnvironmentEvent(
                name=name,
                kind=kind,
                start_s=_non_negative(event, "start_s", event_path, 0.0),
                duration_s=_positive(event, "duration_s", event_path, 1.0),
                height_m=_positive(event, "height_m", event_path, 0.1),
                period_s=_positive(event, "period_s", event_path, 1.0),
                direction_deg=_direction(event, "direction_deg", event_path, 90.0),
            )
        )
    return tuple(events)


def _environment_profile(data: Mapping[str, object], scenario_id: str, water_body: str, path: str) -> EnvironmentProfile:
    boat = _as_mapping(data.get("boat", {}), f"{path}.boat")
    _reject_unknown_keys(boat, ALLOWED_BOAT_KEYS, f"{path}.boat")
    env = _as_mapping(data.get("environment"), f"{path}.environment")
    _reject_unknown_keys(env, ALLOWED_ENVIRONMENT_KEYS, f"{path}.environment")
    boat_path = f"{path}.boat"
    env_path = f"{path}.environment"
    profile = EnvironmentProfile(
        name=scenario_id,
        water_body=water_body,
        loaded_boat_mass_kg=_positive(boat, "loaded_boat_mass_kg", boat_path, 320.0),
        water_drag_coefficient=_positive(boat, "water_drag_coefficient", boat_path, 0.9),
        water_drag_area_m2=_positive(boat, "water_drag_area_m2", boat_path, 0.28),
        windage_drag_coefficient=_positive(boat, "windage_drag_coefficient", boat_path, 1.05),
        windage_area_m2=_positive(boat, "windage_area_m2", boat_path, 1.2),
        current_mps=_non_negative(env, "current_mps", env_path, 0.0),
        current_direction_deg=_direction(env, "current_direction_deg", env_path, 90.0),
        current_swing_deg=_non_negative(env, "current_swing_deg", env_path, 0.0),
        current_swing_period_s=_number(env, "current_swing_period_s", env_path, 180.0),
        wind_mps=_non_negative(env, "wind_mps", env_path, 0.0),
        wind_direction_deg=_direction(env, "wind_direction_deg", env_path, 90.0),
        wind_swing_deg=_non_negative(env, "wind_swing_deg", env_path, 0.0),
        wind_swing_period_s=_number(env, "wind_swing_period_s", env_path, 180.0),
        gust_mps=_non_negative(env, "gust_mps", env_path, 0.0),
        gust_period_s=_number(env, "gust_period_s", env_path, 90.0),
        gust_duration_s=_number(env, "gust_duration_s", env_path, 0.0),
        wave_height_m=_non_negative(env, "wave_height_m", env_path, 0.0),
        wave_period_s=_number(env, "wave_period_s", env_path, 4.0),
        wave_direction_deg=_direction(env, "wave_direction_deg", env_path, 90.0),
        abort_expected=_bool(env, "abort_expected", env_path, False),
        events=_environment_events(env, env_path),
    )
    if profile.wave_period_s <= 0.0:
        raise ValueError(f"{path}.environment.wave_period_s must be positive")
    if profile.current_swing_period_s <= 0.0 or profile.wind_swing_period_s <= 0.0:
        raise ValueError(f"{path}.environment swing periods must be positive")
    if profile.gust_period_s <= 0.0 or profile.gust_duration_s < 0.0:
        raise ValueError(f"{path}.environment gust timing is invalid")
    if profile.gust_duration_s > profile.gust_period_s:
        raise ValueError(f"{path}.environment.gust_duration_s must not exceed gust_period_s")
    if profile.gust_mps > profile.wind_mps and profile.gust_duration_s <= 0.0:
        raise ValueError(f"{path}.environment gust duration must be positive when gust_mps exceeds wind_mps")
    return profile


def _provenance(data: Mapping[str, object], path: str) -> Dict[str, str]:
    raw = _as_mapping(data.get("provenance"), f"{path}.provenance")
    _reject_unknown_keys(raw, ALLOWED_PROVENANCE_KEYS, f"{path}.provenance")
    return {
        "source": _required_str(raw, "source", f"{path}.provenance"),
        "source_candidate_id": _required_str(raw, "source_candidate_id", f"{path}.provenance"),
        "confidence": _required_str(raw, "confidence", f"{path}.provenance"),
    }


def _thresholds(data: Mapping[str, object], path: str) -> Dict[str, object]:
    raw = _as_mapping(data.get("thresholds", {}), f"{path}.thresholds")
    thresholds: Dict[str, object] = {}
    for key, value in raw.items():
        if not isinstance(key, str) or not key:
            raise ValueError(f"{path}.thresholds keys must be strings")
        if key not in ALLOWED_THRESHOLD_KEYS:
            raise ValueError(f"{path}.thresholds.{key} is not a known threshold key")
        if key == "required_failsafe_reason":
            if not isinstance(value, str) or not value:
                raise ValueError(f"{path}.thresholds.{key} must be a non-empty string")
            thresholds[key] = value
        else:
            thresholds[key] = _finite_number_value(value, f"{path}.thresholds.{key}")
    return thresholds


def _catalog(path: Path = DEFAULT_SCENARIO_DATA) -> Mapping[str, object]:
    data = json.loads(
        path.read_text(encoding="utf-8"),
        parse_constant=lambda value: (_ for _ in ()).throw(
            ValueError(f"{path} contains non-finite JSON constant {value}")
        ),
    )
    root = _as_mapping(data, str(path))
    _reject_unknown_keys(root, ALLOWED_ROOT_KEYS, str(path))
    if root.get("schema_version") != SCENARIO_SCHEMA_VERSION:
        raise ValueError(f"{path}.schema_version must be {SCENARIO_SCHEMA_VERSION}")
    return _as_mapping(root.get("scenario_sets"), f"{path}.scenario_sets")


def _scenario_items(name: str, path: Path) -> List[Tuple[str, Mapping[str, object], str]]:
    sets = _catalog(path)
    if name not in sets:
        valid = ", ".join(sorted(sets))
        raise ValueError(f"unknown scenario set {name}; valid sets: {valid}")
    rows = _as_list(sets.get(name), f"{path}.scenario_sets.{name}")
    if not rows:
        raise ValueError(f"{path}.scenario_sets.{name} must not be empty")
    items: List[Tuple[str, Mapping[str, object], str]] = []
    seen: set[str] = set()
    for index, row in enumerate(rows):
        item_path = f"{path}.scenario_sets.{name}[{index}]"
        item = _as_mapping(row, item_path)
        _reject_unknown_keys(item, ALLOWED_SCENARIO_KEYS, item_path)
        scenario_id = _required_str(item, "id", item_path)
        if scenario_id in seen:
            raise ValueError(f"{item_path}.id duplicates {scenario_id}")
        seen.add(scenario_id)
        items.append((scenario_id, item, item_path))
    return items


def scenario_set_names(path: Path = DEFAULT_SCENARIO_DATA) -> List[str]:
    sets = _catalog(path)
    for name, rows in sets.items():
        if not isinstance(name, str) or not name:
            raise ValueError(f"{path}.scenario_sets keys must be non-empty strings")
        if not isinstance(rows, list) or not rows:
            raise ValueError(f"{path}.scenario_sets.{name} must be a non-empty list")
    return sorted(sets.keys())


def environment_scenarios_for_set(name: str, path: Path = DEFAULT_SCENARIO_DATA) -> List[Scenario]:
    scenarios: List[Scenario] = []
    for scenario_id, item, item_path in _scenario_items(name, path):
        water_body = _required_str(item, "water_body", item_path)
        _provenance(item, item_path)
        _optional_str(item, "motor_class", item_path)
        _optional_str(item, "expected", item_path)
        duration_s = _positive_integer(item, "duration_s", item_path)
        profile = _environment_profile(item, scenario_id, water_body, item_path)
        for event in profile.events:
            if event.start_s >= duration_s:
                raise ValueError(f"{item_path}.environment.events.{event.name} starts after duration_s")
        gnss = _as_mapping(item.get("gnss", {}), f"{item_path}.gnss")
        _reject_unknown_keys(gnss, ALLOWED_GNSS_KEYS, f"{item_path}.gnss")
        sats = _integer(gnss, "sats", item_path, 12)
        sentences = _integer(gnss, "sentences", item_path, 10)
        noise_sigma_m = _number(gnss, "noise_sigma_m", item_path, 0.7)
        hdop = _number(gnss, "hdop", item_path, 0.9)
        if sats < 0 or sentences < 0 or noise_sigma_m < 0.0 or hdop < 0.0:
            raise ValueError(f"{item_path}.gnss values must be non-negative")
        scenarios.append(
            Scenario(
                scenario_id,
                duration_s,
                _position(item, item_path),
                profiled_environment_accel_fn(profile),
                make_gnss_observer(
                    noise_sigma_m=noise_sigma_m,
                    sats=sats,
                    hdop=hdop,
                    sentences=sentences,
                ),
                profile=profile,
            )
        )
    return scenarios


def provenance_for_set(name: str, path: Path = DEFAULT_SCENARIO_DATA) -> Dict[str, Dict[str, str]]:
    provenance: Dict[str, Dict[str, str]] = {}
    for scenario_id, item, item_path in _scenario_items(name, path):
        provenance[scenario_id] = _provenance(item, item_path)
    return provenance


def thresholds_for_set(name: str, path: Path = DEFAULT_SCENARIO_DATA) -> Dict[str, Dict[str, object]]:
    thresholds: Dict[str, Dict[str, object]] = {}
    for scenario_id, item, item_path in _scenario_items(name, path):
        _provenance(item, item_path)
        thresholds[scenario_id] = _thresholds(item, item_path)
    return thresholds
