#!/usr/bin/env python3
from __future__ import annotations

from dataclasses import dataclass, field, replace
from typing import Callable, Dict, List, Optional, Tuple
import math
import random


EARTH_RADIUS_M = 6378137.0
ANCHOR_LAT = 59.938600
ANCHOR_LON = 30.314100
REFERENCE_LOADED_BOAT_MASS_KG = 320.0
AIR_DENSITY_KG_M3 = 1.225
WATER_DENSITY_KG_M3 = 1000.0


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


def vector_from_bearing(magnitude: float, bearing_deg_value: float) -> Tuple[float, float]:
    rad = math.radians(bearing_deg_value)
    return math.sin(rad) * magnitude, math.cos(rad) * magnitude


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
    steering_response_dps: float = 180.0
    steering_backlash_deg: float = 0.0
    steering_wrong_zero_deg: float = 0.0
    steering_jam_start_s: float = -1.0
    steering_jam_duration_s: float = 0.0
    ramp_span_m: float = 12.0
    min_pwm_raw: int = 35
    dist_filter_alpha: float = 0.22
    min_on_s: float = 1.2
    min_off_s: float = 0.9
    thrust_accel_mps2: float = 0.85
    motor_response_per_s: float = 1.7
    motor_deadband_pct: float = 8.0
    motor_nominal_voltage_v: float = 12.6
    motor_min_voltage_v: float = 9.5
    motor_internal_resistance_ohm: float = 0.025
    motor_no_load_current_a: float = 1.8
    motor_stall_current_a: float = 55.0
    motor_current_limit_a: float = 45.0
    motor_ambient_temp_c: float = 25.0
    motor_thermal_limit_c: float = 85.0
    motor_thermal_shutdown_c: float = 105.0
    motor_heat_c_per_a2_s: float = 0.00018
    motor_cooling_per_s: float = 0.018
    drag_per_s: float = 0.18
    max_gps_age_ms: int = 2000
    min_sats: int = 6
    max_hdop: float = 3.5
    max_position_jump_m: float = 20.0
    speed_sanity_enabled: bool = True
    max_speed_mps: float = 25.0
    max_accel_mps2: float = 6.0
    required_sentences: int = 0


@dataclass(frozen=True)
class EnvironmentProfile:
    name: str
    water_body: str
    loaded_boat_mass_kg: float = REFERENCE_LOADED_BOAT_MASS_KG
    water_drag_coefficient: float = 0.9
    water_drag_area_m2: float = 0.28
    windage_drag_coefficient: float = 1.05
    windage_area_m2: float = 1.2
    current_mps: float = 0.0
    current_direction_deg: float = 90.0
    current_swing_deg: float = 0.0
    current_swing_period_s: float = 180.0
    wind_mps: float = 0.0
    wind_direction_deg: float = 90.0
    wind_swing_deg: float = 0.0
    wind_swing_period_s: float = 180.0
    gust_mps: float = 0.0
    gust_period_s: float = 90.0
    gust_duration_s: float = 0.0
    wave_height_m: float = 0.0
    wave_period_s: float = 4.0
    wave_direction_deg: float = 90.0
    abort_expected: bool = False


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
    heading_error_deg: float
    thrust_pct: float
    applied_thrust_pct: float
    gnss_ok: bool
    gnss_reason: str
    failsafe_reason: str
    rocking_roll_deg: float = 0.0
    battery_voltage_v: float = 12.6
    motor_current_a: float = 0.0
    motor_temp_c: float = 25.0
    motor_current_limited: bool = False
    motor_thermal_derated: bool = False
    motor_driver_active: bool = False
    steering_jammed: bool = False
    steering_backlash_crossing: bool = False


@dataclass
class Scenario:
    name: str
    duration_s: int
    initial_pos_m: Tuple[float, float]
    env_accel_fn: Callable[[int], Tuple[float, float]]
    obs_fn: Callable[[int, float, float, random.Random], GnssObservation]
    comm_ok_fn: Callable[[int], bool] = lambda _t: True
    inject_nan_fn: Callable[[int], bool] = lambda _t: False
    profile: Optional[EnvironmentProfile] = None
    config_overrides: Dict[str, float] = field(default_factory=dict)


def make_gnss_observer(
    noise_sigma_m: float = 0.7,
    sats: int = 12,
    hdop: float = 0.9,
    sentences: int = 10,
) -> Callable[[int, float, float, random.Random], GnssObservation]:
    def obs(_t: int, x: float, y: float, rnd: random.Random) -> GnssObservation:
        nx = x + rnd.gauss(0.0, noise_sigma_m)
        ny = y + rnd.gauss(0.0, noise_sigma_m)
        lat, lon = local_m_to_latlon(nx, ny, ANCHOR_LAT, ANCHOR_LON)
        return GnssObservation(lat=lat, lon=lon, sats=sats, hdop=hdop, valid=True, sentences=sentences)

    return obs


def _oscillated_direction(base_deg: float, swing_deg: float, period_s: float, t_s: int) -> float:
    if swing_deg == 0.0:
        return base_deg
    period = max(1.0, period_s)
    return base_deg + swing_deg * math.sin((2.0 * math.pi * t_s) / period)


def _gust_speed_mps(profile: EnvironmentProfile, t_s: int) -> float:
    if profile.gust_mps <= profile.wind_mps or profile.gust_duration_s <= 0.0:
        return profile.wind_mps
    period = max(profile.gust_duration_s, profile.gust_period_s)
    return profile.gust_mps if (t_s % period) < profile.gust_duration_s else profile.wind_mps


def wave_rocking_roll_deg(profile: Optional[EnvironmentProfile], t_s: int) -> float:
    if profile is None or profile.wave_height_m <= 0.0:
        return 0.0
    period = max(1.5, profile.wave_period_s)
    phase = (2.0 * math.pi * t_s) / period
    amp = min(24.0, profile.wave_height_m * 7.0)
    return amp * math.sin(phase) + 0.25 * amp * math.sin((phase * 2.0) + 0.7)


def _quadratic_fluid_accel_mps2(
    fluid_vx_mps: float,
    fluid_vy_mps: float,
    boat_vx_mps: float,
    boat_vy_mps: float,
    density_kg_m3: float,
    drag_coefficient: float,
    reference_area_m2: float,
    mass_kg: float,
) -> Tuple[float, float]:
    rel_vx = fluid_vx_mps - boat_vx_mps
    rel_vy = fluid_vy_mps - boat_vy_mps
    rel_speed = math.hypot(rel_vx, rel_vy)
    if rel_speed <= 1e-9:
        return 0.0, 0.0
    mass = max(1.0, mass_kg)
    scale = 0.5 * density_kg_m3 * drag_coefficient * reference_area_m2 * rel_speed / mass
    return scale * rel_vx, scale * rel_vy


def environment_accel_mps2(
    profile: EnvironmentProfile,
    t_s: int,
    boat_vx_mps: float = 0.0,
    boat_vy_mps: float = 0.0,
) -> Tuple[float, float]:
    current_dir = _oscillated_direction(
        profile.current_direction_deg,
        profile.current_swing_deg,
        profile.current_swing_period_s,
        t_s,
    )
    wind_dir = _oscillated_direction(
        profile.wind_direction_deg,
        profile.wind_swing_deg,
        profile.wind_swing_period_s,
        t_s,
    )
    current_vx, current_vy = vector_from_bearing(profile.current_mps, current_dir)
    current_ax, current_ay = _quadratic_fluid_accel_mps2(
        current_vx,
        current_vy,
        boat_vx_mps,
        boat_vy_mps,
        WATER_DENSITY_KG_M3,
        profile.water_drag_coefficient,
        profile.water_drag_area_m2,
        profile.loaded_boat_mass_kg,
    )
    wind_speed = _gust_speed_mps(profile, t_s)
    wind_vx, wind_vy = vector_from_bearing(wind_speed, wind_dir)
    wind_ax, wind_ay = _quadratic_fluid_accel_mps2(
        wind_vx,
        wind_vy,
        boat_vx_mps,
        boat_vy_mps,
        AIR_DENSITY_KG_M3,
        profile.windage_drag_coefficient,
        profile.windage_area_m2,
        profile.loaded_boat_mass_kg,
    )

    wave_ax, wave_ay = 0.0, 0.0
    if profile.wave_height_m > 0.0:
        period = max(1.5, profile.wave_period_s)
        phase = (2.0 * math.pi * t_s) / period
        mass_scale = REFERENCE_LOADED_BOAT_MASS_KG / max(1.0, profile.loaded_boat_mass_kg)
        amp = min(0.12, profile.wave_height_m * 0.018 * (4.0 / period) * mass_scale)
        wave_ax, wave_ay = vector_from_bearing(amp * math.sin(phase), profile.wave_direction_deg)

    return current_ax + wind_ax + wave_ax, current_ay + wind_ay + wave_ay


def profiled_environment_accel_fn(profile: EnvironmentProfile) -> Callable[[int], Tuple[float, float]]:
    return lambda t_s: environment_accel_mps2(profile, t_s)


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


@dataclass
class MotorState:
    applied_thrust_pct: float
    battery_voltage_v: float
    current_a: float
    temp_c: float
    current_limited: bool
    thermal_derated: bool
    driver_active: bool


class BrushedMotorModel:
    def __init__(self, cfg: SimConfig) -> None:
        self.cfg = cfg
        self.drive_pct = 0.0
        self.temp_c = cfg.motor_ambient_temp_c

    def update(self, command_pct: float, dt_s: float) -> MotorState:
        command = clamp(command_pct if math.isfinite(command_pct) else 0.0, 0.0, 100.0)
        driver_active = command >= self.cfg.motor_deadband_pct
        target = command if driver_active else 0.0
        dt = max(0.0, dt_s)
        response = clamp(self.cfg.motor_response_per_s * dt, 0.0, 1.0)
        self.drive_pct += (target - self.drive_pct) * response
        if not driver_active and self.drive_pct < 0.01:
            self.drive_pct = 0.0

        demand_ratio = clamp(self.drive_pct / 100.0, 0.0, 1.0)
        if demand_ratio <= 0.0:
            current_a = 0.0
        else:
            current_a = self.cfg.motor_no_load_current_a + (
                self.cfg.motor_stall_current_a * math.pow(demand_ratio, 1.15)
            )

        current_limited = current_a > self.cfg.motor_current_limit_a
        current_scale = 1.0
        if current_limited and current_a > self.cfg.motor_no_load_current_a:
            usable_limit = max(0.0, self.cfg.motor_current_limit_a - self.cfg.motor_no_load_current_a)
            usable_demand = max(0.01, current_a - self.cfg.motor_no_load_current_a)
            current_scale = clamp(usable_limit / usable_demand, 0.0, 1.0)
            current_a = self.cfg.motor_current_limit_a

        thermal_derated = self.temp_c >= self.cfg.motor_thermal_limit_c
        if thermal_derated:
            thermal_span = max(0.1, self.cfg.motor_thermal_shutdown_c - self.cfg.motor_thermal_limit_c)
            thermal_scale = clamp(
                (self.cfg.motor_thermal_shutdown_c - self.temp_c) / thermal_span,
                0.0,
                1.0,
            )
        else:
            thermal_scale = 1.0

        current_a = self.cfg.motor_no_load_current_a + (
            max(0.0, current_a - self.cfg.motor_no_load_current_a) * thermal_scale
        ) if demand_ratio > 0.0 else 0.0
        battery_voltage = self.cfg.motor_nominal_voltage_v - (
            current_a * self.cfg.motor_internal_resistance_ohm
        )
        battery_voltage = clamp(battery_voltage, self.cfg.motor_min_voltage_v, self.cfg.motor_nominal_voltage_v)
        voltage_span = max(0.1, self.cfg.motor_nominal_voltage_v - self.cfg.motor_min_voltage_v)
        voltage_scale = clamp(
            (battery_voltage - self.cfg.motor_min_voltage_v) / voltage_span,
            0.0,
            1.0,
        )

        heat = current_a * current_a * self.cfg.motor_heat_c_per_a2_s
        cooling = max(0.0, self.temp_c - self.cfg.motor_ambient_temp_c) * self.cfg.motor_cooling_per_s
        self.temp_c += (heat - cooling) * dt
        self.temp_c = max(self.cfg.motor_ambient_temp_c, self.temp_c)
        thermal_derated = thermal_derated or self.temp_c >= self.cfg.motor_thermal_limit_c

        applied = self.drive_pct * current_scale * voltage_scale * thermal_scale
        return MotorState(
            applied_thrust_pct=clamp(applied, 0.0, 100.0),
            battery_voltage_v=battery_voltage,
            current_a=clamp(current_a, 0.0, self.cfg.motor_current_limit_a),
            temp_c=self.temp_c,
            current_limited=current_limited,
            thermal_derated=thermal_derated,
            driver_active=driver_active,
        )


@dataclass
class SteeringState:
    heading_delta_deg: float
    jammed: bool
    backlash_crossing: bool


class SteeringModel:
    def __init__(self, cfg: SimConfig) -> None:
        self.cfg = cfg
        self.last_command_sign = 0
        self.backlash_remaining_deg = 0.0

    def update(self, t_s: float, requested_delta_deg: float, dt_s: float) -> SteeringState:
        dt = max(0.0, dt_s)
        jam_start = self.cfg.steering_jam_start_s
        jam_end = jam_start + max(0.0, self.cfg.steering_jam_duration_s)
        jammed = jam_start >= 0.0 and jam_start <= t_s < jam_end
        if jammed:
            return SteeringState(heading_delta_deg=0.0, jammed=True, backlash_crossing=False)

        command = requested_delta_deg if math.isfinite(requested_delta_deg) else 0.0
        command = clamp(command, -self.cfg.max_turn_rate_dps * dt, self.cfg.max_turn_rate_dps * dt)
        if abs(command) < 1e-9:
            return SteeringState(heading_delta_deg=0.0, jammed=False, backlash_crossing=False)

        sign = 1 if command > 0.0 else -1
        backlash_crossing = (
            self.cfg.steering_backlash_deg > 0.0
            and self.last_command_sign != 0
            and sign != self.last_command_sign
        )
        if backlash_crossing:
            self.backlash_remaining_deg = max(0.0, self.cfg.steering_backlash_deg)
        self.last_command_sign = sign

        command_mag = abs(command)
        if self.backlash_remaining_deg > 0.0:
            lost = min(command_mag, self.backlash_remaining_deg)
            self.backlash_remaining_deg -= lost
            command_mag -= lost

        if command_mag <= 0.0:
            return SteeringState(heading_delta_deg=0.0, jammed=False, backlash_crossing=backlash_crossing)

        biased_command = (sign * command_mag) - self.cfg.steering_wrong_zero_deg
        max_response = max(0.0, self.cfg.steering_response_dps) * dt
        achieved = clamp(biased_command, -max_response, max_response)
        return SteeringState(
            heading_delta_deg=achieved,
            jammed=False,
            backlash_crossing=backlash_crossing,
        )


def simulate_scenario(cfg: SimConfig, scenario: Scenario, seed: int = 1) -> Dict[str, object]:
    if scenario.config_overrides:
        cfg = replace(cfg, **scenario.config_overrides)
    rnd = random.Random(seed)
    gate = GnssGate(cfg)
    ctrl = AnchorController(cfg)
    motor = BrushedMotorModel(cfg)
    steering = SteeringModel(cfg)

    anchor_lat = ANCHOR_LAT
    anchor_lon = ANCHOR_LON
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
    invalid_motor_count = 0

    for t in range(0, scenario.duration_s):
        obs = scenario.obs_fn(t, x_m, y_m, rnd)
        if obs.valid and obs.lat is not None and obs.lon is not None:
            last_obs = obs
            last_obs_t = t
        age_ms = 999999 if last_obs is None else int((t - last_obs_t) * 1000)
        gnss_ok, gnss_reason, meas_x, meas_y = gate.evaluate(t, last_obs, anchor_lat, anchor_lon, age_ms)

        failsafe_reason = "NONE"
        if scenario.profile is not None and scenario.profile.abort_expected:
            anchor_enabled = False
            failsafe_reason = "ENV_ABORT_EXPECTED"
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
        requested_heading_delta = clamp(
            heading_err,
            -cfg.max_turn_rate_dps * cfg.dt_s,
            cfg.max_turn_rate_dps * cfg.dt_s,
        )
        steering_state = steering.update(float(t), requested_heading_delta, cfg.dt_s)
        heading += steering_state.heading_delta_deg
        heading = wrap_deg(heading)
        heading_err = angle_diff_deg(brg_to_anchor, heading)

        allow_thrust = anchor_enabled and gnss_ok and abs(angle_diff_deg(brg_to_anchor, heading)) <= cfg.heading_align_deg
        thrust_pct = ctrl.update(float(t), dist_meas, allow_thrust)
        if (not math.isfinite(thrust_pct)) or thrust_pct < 0.0 or thrust_pct > 100.0:
            invalid_thrust_count += 1
        if thrust_pct > 0.5:
            thrust_dirs.append(heading)

        motor_state = motor.update(thrust_pct, cfg.dt_s)
        if (
            not math.isfinite(motor_state.applied_thrust_pct)
            or not math.isfinite(motor_state.battery_voltage_v)
            or not math.isfinite(motor_state.current_a)
            or not math.isfinite(motor_state.temp_c)
        ):
            invalid_motor_count += 1

        thrust_acc = (motor_state.applied_thrust_pct / 100.0) * cfg.thrust_accel_mps2
        if scenario.profile is not None:
            mass_scale = REFERENCE_LOADED_BOAT_MASS_KG / max(1.0, scenario.profile.loaded_boat_mass_kg)
            thrust_acc *= mass_scale
        hx = math.sin(math.radians(heading))
        hy = math.cos(math.radians(heading))
        if scenario.profile is not None:
            ax_env, ay_env = environment_accel_mps2(scenario.profile, t, vx, vy)
        else:
            ax_env, ay_env = scenario.env_accel_fn(t)
        rocking_roll = wave_rocking_roll_deg(scenario.profile, t)
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
                heading_error_deg=heading_err,
                thrust_pct=thrust_pct,
                applied_thrust_pct=motor_state.applied_thrust_pct,
                gnss_ok=gnss_ok,
                gnss_reason=gnss_reason,
                failsafe_reason=failsafe_reason,
                rocking_roll_deg=rocking_roll,
                battery_voltage_v=motor_state.battery_voltage_v,
                motor_current_a=motor_state.current_a,
                motor_temp_c=motor_state.temp_c,
                motor_current_limited=motor_state.current_limited,
                motor_thermal_derated=motor_state.thermal_derated,
                motor_driver_active=motor_state.driver_active,
                steering_jammed=steering_state.jammed,
                steering_backlash_crossing=steering_state.backlash_crossing,
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
    heading_errors = sorted(abs(s.heading_error_deg) for s in samples)
    idx95_heading = int(0.95 * (len(heading_errors) - 1)) if heading_errors else 0
    rocking_abs = sorted(abs(s.rocking_roll_deg) for s in samples)
    idx95_rock = int(0.95 * (len(rocking_abs) - 1)) if rocking_abs else 0
    applied = [s.applied_thrust_pct for s in samples]
    voltages = [s.battery_voltage_v for s in samples]
    currents = [s.motor_current_a for s in samples]
    temps = [s.motor_temp_c for s in samples]
    current_limited_hits = sum(1 for s in samples if s.motor_current_limited)
    thermal_derated_hits = sum(1 for s in samples if s.motor_thermal_derated)
    driver_inactive_hits = sum(1 for s in samples if s.thrust_pct > 0.0 and not s.motor_driver_active)
    steering_jammed_hits = sum(1 for s in samples if s.steering_jammed)
    backlash_crossings = sum(1 for s in samples if s.steering_backlash_crossing)
    thrust_misaligned_hits = sum(
        1
        for s in samples
        if s.applied_thrust_pct > 0.5 and abs(s.heading_error_deg) > cfg.heading_align_deg
    )

    metrics = {
        "loaded_boat_mass_kg": (
            scenario.profile.loaded_boat_mass_kg
            if scenario.profile is not None
            else REFERENCE_LOADED_BOAT_MASS_KG
        ),
        "water_drag_coefficient": (
            scenario.profile.water_drag_coefficient if scenario.profile is not None else 0.0
        ),
        "water_drag_area_m2": (
            scenario.profile.water_drag_area_m2 if scenario.profile is not None else 0.0
        ),
        "windage_drag_coefficient": (
            scenario.profile.windage_drag_coefficient if scenario.profile is not None else 0.0
        ),
        "windage_area_m2": (
            scenario.profile.windage_area_m2 if scenario.profile is not None else 0.0
        ),
        "time_in_deadband_pct": (deadband_hits * 100.0) / max(1, len(samples)),
        "p95_error_m": p95,
        "max_error_m": max(dists) if dists else 0.0,
        "p95_heading_error_deg": heading_errors[idx95_heading] if heading_errors else 0.0,
        "max_heading_error_deg": max(heading_errors) if heading_errors else 0.0,
        "steering_jammed_time_pct": (steering_jammed_hits * 100.0) / max(1, len(samples)),
        "steering_backlash_crossings_per_min": backlash_crossings * 60.0 / max(1, scenario.duration_s),
        "thrust_while_misaligned_time_pct": (thrust_misaligned_hits * 100.0) / max(1, len(samples)),
        "control_saturation_time_pct": (sat_hits * 100.0) / max(1, len(samples)),
        "num_thrust_direction_changes_per_min": dir_changes * 60.0 / max(1, scenario.duration_s),
        "invalid_thrust_count": float(invalid_thrust_count),
        "invalid_distance_count": float(invalid_distance_count),
        "invalid_motor_count": float(invalid_motor_count),
        "p95_rocking_roll_deg": rocking_abs[idx95_rock] if rocking_abs else 0.0,
        "max_rocking_roll_deg": max(rocking_abs) if rocking_abs else 0.0,
        "avg_applied_thrust_pct": sum(applied) / max(1, len(applied)),
        "max_applied_thrust_pct": max(applied) if applied else 0.0,
        "min_battery_voltage_v": min(voltages) if voltages else cfg.motor_nominal_voltage_v,
        "max_motor_current_a": max(currents) if currents else 0.0,
        "max_motor_temp_c": max(temps) if temps else cfg.motor_ambient_temp_c,
        "motor_current_limited_time_pct": (current_limited_hits * 100.0) / max(1, len(samples)),
        "motor_thermal_derated_time_pct": (thermal_derated_hits * 100.0) / max(1, len(samples)),
        "motor_driver_deadband_time_pct": (driver_inactive_hits * 100.0) / max(1, len(samples)),
        "environment_abort_expected": 1.0 if scenario.profile is not None and scenario.profile.abort_expected else 0.0,
        "events": events,
    }
    return {
        "scenario": scenario.name,
        "duration_s": scenario.duration_s,
        "metrics": metrics,
    }


def default_scenarios() -> List[Scenario]:
    obs_good = make_gnss_observer()

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

    def accel_wake(t: int) -> Tuple[float, float]:
        base_x, base_y = 0.022, 0.0
        wake = 0.0
        if 35 <= t <= 55 or 115 <= t <= 135 or 195 <= t <= 215:
            wake = 0.11 * math.sin((t % 20) * math.pi / 10.0)
        return base_x, base_y + wake

    return [
        Scenario("calm_good_gps", 240, (8.0, 0.0), accel_zero, obs_good),
        Scenario("constant_current", 240, (10.0, -4.0), accel_current, obs_good),
        Scenario("gusts", 240, (10.0, 6.0), accel_gusts, obs_good),
        Scenario(
            "wake_steering_backlash",
            260,
            (11.0, -5.0),
            accel_wake,
            obs_good,
            config_overrides={
                "steering_response_dps": 55.0,
                "steering_backlash_deg": 7.0,
                "steering_wrong_zero_deg": 1.5,
                "steering_jam_start_s": 150.0,
                "steering_jam_duration_s": 8.0,
            },
        ),
        Scenario("gnss_dropout", 240, (9.0, -3.0), accel_current, obs_dropout),
        Scenario("poor_hdop", 240, (8.0, 4.0), accel_current, obs_hdop_bad),
        Scenario("position_jumps", 240, (11.0, 1.0), accel_current, obs_jump),
        Scenario("comm_loss", 240, (9.0, 0.0), accel_current, obs_good, comm_ok_fn=lambda t: not (110 <= t <= 140)),
        Scenario("nan_data", 240, (9.0, 2.0), accel_current, obs_good, inject_nan_fn=lambda t: t == 120),
    ]


def russian_water_scenarios() -> List[Scenario]:
    from scenario_data import environment_scenarios_for_set

    return environment_scenarios_for_set("russian")


def scenarios_for_set(name: str) -> List[Scenario]:
    if name == "core":
        return default_scenarios()
    from scenario_data import environment_scenarios_for_set, scenario_set_names

    data_sets = scenario_set_names()
    if name in data_sets:
        return environment_scenarios_for_set(name)
    if name == "all":
        scenarios = default_scenarios()
        seen = {scenario.name for scenario in scenarios}
        for data_set in data_sets:
            for scenario in environment_scenarios_for_set(data_set):
                if scenario.name in seen:
                    raise ValueError(f"scenario {scenario.name} is duplicated in all")
                seen.add(scenario.name)
                scenarios.append(scenario)
        return scenarios
    valid = ", ".join(["core", *data_sets, "all"])
    raise ValueError(f"unknown scenario set: {name}; valid sets: {valid}")
