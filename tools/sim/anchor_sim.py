#!/usr/bin/env python3
from __future__ import annotations

from dataclasses import dataclass, field
from typing import Callable, Dict, List, Optional, Tuple
import math
import random


EARTH_RADIUS_M = 6378137.0


def clamp(v: float, lo: float, hi: float) -> float:
    return lo if v < lo else hi if v > hi else v


def wrap_deg(deg: float) -> float:
    x = deg % 360.0
    if x < 0:
        x += 360.0
    return x


def angle_diff_deg(a: float, b: float) -> float:
    d = (a - b + 180.0) % 360.0 - 180.0
    return d


def bearing_deg(dx_m: float, dy_m: float) -> float:
    # dx east, dy north
    return wrap_deg(math.degrees(math.atan2(dx_m, dy_m)))


def latlon_to_local_m(lat: float, lon: float, lat0: float, lon0: float) -> Tuple[float, float]:
    dlat = math.radians(lat - lat0)
    dlon = math.radians(lon - lon0)
    x = dlon * EARTH_RADIUS_M * math.cos(math.radians(lat0))
    y = dlat * EARTH_RADIUS_M
    return x, y


def local_m_to_latlon(x_m: float, y_m: float, lat0: float, lon0: float) -> Tuple[float, float]:
    lat = lat0 + math.degrees(y_m / EARTH_RADIUS_M)
    lon = lon0 + math.degrees(x_m / (EARTH_RADIUS_M * math.cos(math.radians(lat0))))
    return lat, lon


@dataclass
class SimConfig:
    dt_s: float = 1.0
    hold_radius_m: float = 2.0
    deadband_m: float = 1.5
    max_thrust_pct_anchor: float = 75.0
    thrust_ramp_pct_per_s: float = 35.0
    max_turn_rate_dps: float = 180.0
    heading_align_deg: float = 5.0
    ramp_span_m: float = 12.0
    min_pwm_raw: int = 35
    dist_filter_alpha: float = 0.22
    min_on_s: float = 1.2
    min_off_s: float = 0.9
    thrust_accel_mps2: float = 0.85
    drag_per_s: float = 0.18
    max_gps_age_ms: int = 2000
    min_sats: int = 6
    max_hdop: float = 3.5
    max_position_jump_m: float = 20.0
    speed_sanity_enabled: bool = True
    max_speed_mps: float = 25.0
    max_accel_mps2: float = 6.0
    required_sentences: int = 0


@dataclass
class GnssObservation:
    lat: Optional[float]
    lon: Optional[float]
    sats: int
    hdop: float
    valid: bool
    sentences: int = 10


@dataclass
class StepSample:
    t_s: int
    dist_true_m: float
    dist_meas_m: float
    bearing_to_anchor_deg: float
    heading_deg: float
    thrust_pct: float
    gnss_ok: bool
    gnss_reason: str
    failsafe_reason: str


@dataclass
class Scenario:
    name: str
    duration_s: int
    initial_pos_m: Tuple[float, float]
    env_accel_fn: Callable[[int], Tuple[float, float]]
    obs_fn: Callable[[int, float, float, random.Random], GnssObservation]
    comm_ok_fn: Callable[[int], bool] = lambda _t: True
    inject_nan_fn: Callable[[int], bool] = lambda _t: False


class GnssGate:
    def __init__(self, cfg: SimConfig) -> None:
        self.cfg = cfg
        self.last_obs_xy: Optional[Tuple[float, float]] = None
        self.last_obs_t_s: Optional[int] = None
        self.last_speed_mps: Optional[float] = None

    def evaluate(
        self,
        t_s: int,
        obs: Optional[GnssObservation],
        anchor_lat: float,
        anchor_lon: float,
        age_ms: int,
    ) -> Tuple[bool, str, float, float]:
        if obs is None or not obs.valid or obs.lat is None or obs.lon is None:
            return False, "GPS_NO_FIX", float("nan"), float("nan")
        if age_ms > self.cfg.max_gps_age_ms:
            return False, "GPS_DATA_STALE", float("nan"), float("nan")
        if obs.sats < self.cfg.min_sats:
            return False, "GPS_SATS_TOO_LOW", float("nan"), float("nan")
        if math.isfinite(obs.hdop) and obs.hdop > self.cfg.max_hdop:
            return False, "GPS_HDOP_TOO_HIGH", float("nan"), float("nan")

        x_m, y_m = latlon_to_local_m(obs.lat, obs.lon, anchor_lat, anchor_lon)
        if self.last_obs_xy is not None and self.last_obs_t_s is not None:
            dt = max(1, t_s - self.last_obs_t_s)
            jump = math.hypot(x_m - self.last_obs_xy[0], y_m - self.last_obs_xy[1])
            if jump > self.cfg.max_position_jump_m:
                return False, "GPS_POSITION_JUMP", x_m, y_m
            if self.cfg.speed_sanity_enabled:
                speed = jump / dt
                if speed > self.cfg.max_speed_mps:
                    return False, "GPS_SPEED_INVALID", x_m, y_m
                if self.last_speed_mps is not None:
                    accel = abs(speed - self.last_speed_mps) / dt
                    if accel > self.cfg.max_accel_mps2:
                        return False, "GPS_ACCEL_INVALID", x_m, y_m
                self.last_speed_mps = speed

        if self.cfg.required_sentences > 0 and obs.sentences < self.cfg.required_sentences:
            return False, "GPS_SENTENCES_MISSING", x_m, y_m

        self.last_obs_xy = (x_m, y_m)
        self.last_obs_t_s = t_s
        return True, "NONE", x_m, y_m


class AnchorController:
    def __init__(self, cfg: SimConfig) -> None:
        self.cfg = cfg
        self.filtered_dist: Optional[float] = None
        self.auto_active = False
        self.auto_state_changed_s = 0.0
        self.pwm_raw = 0.0
        self.last_ramp_t_s: Optional[float] = None

    def update(self, t_s: float, dist_m: float, allow: bool) -> float:
        if not allow or not math.isfinite(dist_m) or dist_m <= 0.0:
            self.filtered_dist = None
            self.auto_active = False
            self.pwm_raw = 0.0
            self.last_ramp_t_s = t_s
            return 0.0

        if self.filtered_dist is None:
            self.filtered_dist = dist_m
        else:
            self.filtered_dist += (dist_m - self.filtered_dist) * self.cfg.dist_filter_alpha

        idle_radius = self.cfg.hold_radius_m + self.cfg.deadband_m
        should_active = self.filtered_dist >= idle_radius
        min_state_s = self.cfg.min_on_s if self.auto_active else self.cfg.min_off_s
        if should_active != self.auto_active and (t_s - self.auto_state_changed_s) >= min_state_s:
            self.auto_active = should_active
            self.auto_state_changed_s = t_s
            if not self.auto_active:
                self.pwm_raw = 0.0
                return 0.0

        if not self.auto_active:
            self.pwm_raw = 0.0
            return 0.0

        over = max(0.0, self.filtered_dist - idle_radius)
        ratio = clamp(over / self.cfg.ramp_span_m, 0.0, 1.0)
        max_pwm = clamp((self.cfg.max_thrust_pct_anchor / 100.0) * 255.0, self.cfg.min_pwm_raw, 255.0)
        target_pwm = self.cfg.min_pwm_raw + ratio * (max_pwm - self.cfg.min_pwm_raw)

        if self.last_ramp_t_s is None:
            self.last_ramp_t_s = t_s
        dt = max(0.0, min(1.0, t_s - self.last_ramp_t_s))
        self.last_ramp_t_s = t_s
        max_delta = max(1.0, (self.cfg.thrust_ramp_pct_per_s / 100.0) * 255.0 * dt)
        if self.pwm_raw < target_pwm:
            self.pwm_raw = min(target_pwm, self.pwm_raw + max_delta)
        else:
            self.pwm_raw = max(target_pwm, self.pwm_raw - max_delta)
        return clamp(self.pwm_raw * 100.0 / 255.0, 0.0, 100.0)


def simulate_scenario(cfg: SimConfig, scenario: Scenario, seed: int = 1) -> Dict[str, object]:
    rnd = random.Random(seed)
    gate = GnssGate(cfg)
    ctrl = AnchorController(cfg)

    anchor_lat = 59.938600
    anchor_lon = 30.314100
    x_m, y_m = scenario.initial_pos_m
    vx, vy = 0.0, 0.0
    heading = 0.0
    last_obs: Optional[GnssObservation] = None
    last_obs_t = 0
    anchor_enabled = True

    samples: List[StepSample] = []
    events: List[Dict[str, object]] = []
    last_event_key = ""
    thrust_dirs: List[float] = []
    invalid_thrust_count = 0
    invalid_distance_count = 0

    for t in range(0, scenario.duration_s):
        obs = scenario.obs_fn(t, x_m, y_m, rnd)
        if obs.valid and obs.lat is not None and obs.lon is not None:
            last_obs = obs
            last_obs_t = t
        age_ms = 999999 if last_obs is None else int((t - last_obs_t) * 1000)
        gnss_ok, gnss_reason, meas_x, meas_y = gate.evaluate(t, last_obs, anchor_lat, anchor_lon, age_ms)

        failsafe_reason = "NONE"
        if not scenario.comm_ok_fn(t):
            anchor_enabled = False
            failsafe_reason = "COMM_TIMEOUT"
        if scenario.inject_nan_fn(t):
            anchor_enabled = False
            failsafe_reason = "INTERNAL_ERROR_NAN"

        if not gnss_ok and anchor_enabled and gnss_reason in ("GPS_DATA_STALE", "GPS_NO_FIX"):
            # degrade to stop mode on hard GNSS failure
            anchor_enabled = False
            failsafe_reason = "GPS_WEAK"

        dist_true = math.hypot(x_m, y_m)
        if math.isfinite(meas_x) and math.isfinite(meas_y):
            dist_meas = math.hypot(meas_x, meas_y)
            brg_to_anchor = bearing_deg(-meas_x, -meas_y)
        else:
            dist_meas = dist_true
            brg_to_anchor = bearing_deg(-x_m, -y_m)

        heading_err = angle_diff_deg(brg_to_anchor, heading)
        max_turn = cfg.max_turn_rate_dps * cfg.dt_s
        heading += clamp(heading_err, -max_turn, max_turn)
        heading = wrap_deg(heading)

        allow_thrust = anchor_enabled and gnss_ok and abs(angle_diff_deg(brg_to_anchor, heading)) <= cfg.heading_align_deg
        thrust_pct = ctrl.update(float(t), dist_meas, allow_thrust)
        if (not math.isfinite(thrust_pct)) or thrust_pct < 0.0 or thrust_pct > 100.0:
            invalid_thrust_count += 1
        if thrust_pct > 0.5:
            thrust_dirs.append(heading)

        thrust_acc = (thrust_pct / 100.0) * cfg.thrust_accel_mps2
        hx = math.sin(math.radians(heading))
        hy = math.cos(math.radians(heading))
        ax_env, ay_env = scenario.env_accel_fn(t)
        ax = ax_env + thrust_acc * hx - cfg.drag_per_s * vx
        ay = ay_env + thrust_acc * hy - cfg.drag_per_s * vy
        vx += ax * cfg.dt_s
        vy += ay * cfg.dt_s
        x_m += vx * cfg.dt_s
        y_m += vy * cfg.dt_s
        if (not math.isfinite(x_m)) or (not math.isfinite(y_m)) or (not math.isfinite(dist_true)):
            invalid_distance_count += 1

        key = f"{gnss_reason}|{failsafe_reason}"
        if key != last_event_key and (gnss_reason != "NONE" or failsafe_reason != "NONE"):
            events.append({"t_s": t, "gnss_reason": gnss_reason, "failsafe_reason": failsafe_reason})
            last_event_key = key

        samples.append(
            StepSample(
                t_s=t,
                dist_true_m=dist_true,
                dist_meas_m=dist_meas,
                bearing_to_anchor_deg=brg_to_anchor,
                heading_deg=heading,
                thrust_pct=thrust_pct,
                gnss_ok=gnss_ok,
                gnss_reason=gnss_reason,
                failsafe_reason=failsafe_reason,
            )
        )

    dists = [s.dist_true_m for s in samples]
    deadband_limit = cfg.hold_radius_m + cfg.deadband_m
    deadband_hits = sum(1 for d in dists if d <= deadband_limit)
    sat_hits = sum(1 for s in samples if s.thrust_pct >= cfg.max_thrust_pct_anchor - 0.1)
    dir_changes = 0
    last_dir: Optional[float] = None
    for d in thrust_dirs:
        if last_dir is not None and abs(angle_diff_deg(d, last_dir)) > 120.0:
            dir_changes += 1
        last_dir = d

    sorted_dists = sorted(dists)
    idx95 = int(0.95 * (len(sorted_dists) - 1))
    p95 = sorted_dists[idx95]

    metrics = {
        "time_in_deadband_pct": (deadband_hits * 100.0) / max(1, len(samples)),
        "p95_error_m": p95,
        "max_error_m": max(dists) if dists else 0.0,
        "control_saturation_time_pct": (sat_hits * 100.0) / max(1, len(samples)),
        "num_thrust_direction_changes_per_min": dir_changes * 60.0 / max(1, scenario.duration_s),
        "invalid_thrust_count": float(invalid_thrust_count),
        "invalid_distance_count": float(invalid_distance_count),
        "events": events,
    }
    return {
        "scenario": scenario.name,
        "duration_s": scenario.duration_s,
        "metrics": metrics,
    }


def default_scenarios() -> List[Scenario]:
    def obs_good(t: int, x: float, y: float, rnd: random.Random) -> GnssObservation:
        lat0, lon0 = 59.938600, 30.314100
        nx = x + rnd.gauss(0.0, 0.7)
        ny = y + rnd.gauss(0.0, 0.7)
        lat, lon = local_m_to_latlon(nx, ny, lat0, lon0)
        return GnssObservation(lat=lat, lon=lon, sats=12, hdop=0.9, valid=True, sentences=10)

    def obs_dropout(t: int, x: float, y: float, rnd: random.Random) -> GnssObservation:
        if (80 <= t <= 84) or (160 <= t <= 164):
            return GnssObservation(lat=None, lon=None, sats=0, hdop=99.0, valid=False, sentences=0)
        return obs_good(t, x, y, rnd)

    def obs_hdop_bad(t: int, x: float, y: float, rnd: random.Random) -> GnssObservation:
        o = obs_good(t, x, y, rnd)
        if (70 <= t <= 90) or (150 <= t <= 170):
            o.hdop = 6.5
        return o

    def obs_jump(t: int, x: float, y: float, rnd: random.Random) -> GnssObservation:
        o = obs_good(t, x, y, rnd)
        if t in (60, 61, 140):
            lat0, lon0 = 59.938600, 30.314100
            jlat, jlon = local_m_to_latlon(x + 35.0, y - 27.0, lat0, lon0)
            o.lat, o.lon = jlat, jlon
        return o

    def accel_zero(_t: int) -> Tuple[float, float]:
        return 0.0, 0.0

    def accel_current(_t: int) -> Tuple[float, float]:
        return 0.028, 0.0

    def accel_gusts(t: int) -> Tuple[float, float]:
        if 50 <= t <= 65 or 130 <= t <= 145:
            return 0.09, -0.02
        return 0.025, 0.0

    return [
        Scenario("calm_good_gps", 240, (8.0, 0.0), accel_zero, obs_good),
        Scenario("constant_current", 240, (10.0, -4.0), accel_current, obs_good),
        Scenario("gusts", 240, (10.0, 6.0), accel_gusts, obs_good),
        Scenario("gnss_dropout", 240, (9.0, -3.0), accel_current, obs_dropout),
        Scenario("poor_hdop", 240, (8.0, 4.0), accel_current, obs_hdop_bad),
        Scenario("position_jumps", 240, (11.0, 1.0), accel_current, obs_jump),
        Scenario("comm_loss", 240, (9.0, 0.0), accel_current, obs_good, comm_ok_fn=lambda t: not (110 <= t <= 140)),
        Scenario("nan_data", 240, (9.0, 2.0), accel_current, obs_good, inject_nan_fn=lambda t: t == 120),
    ]
