# BoatLock Environment Inputs Research

Date: 2026-04-25

Purpose: collect raw, source-backed input ranges for future BoatLock hardware/simulator scenarios. This is not a normalized simulator schema yet. Values below are intended as candidate scenario inputs and test boundaries.

## Units And Caveats

- Current velocity is in `m/s`. `1 m/s = 3.6 km/h`.
- Wind speed is in `m/s`; marine sources often use knots. `1 kt = 0.514 m/s`.
- Trolling motor thrust is static pound-force. Use `1 lbf = 4.44822 N` when converting to force.
- Wave height is normally significant wave height when taken from marine forecast/observation systems. Individual waves can be higher.
- River and reservoir current is not a stable water-body constant. It changes by reach, dam operation, channel section, season, wind setup, and flood state. Use ranges and named scenarios, not one "Volga speed" constant.

## Motor Classes

| Class | Typical examples | Source-backed numbers | Scenario use |
| --- | --- | --- | --- |
| 30-40 lb, 12 V | Minn Kota Endura 30/40; Newport NV 36 | Minn Kota Endura class includes 30 and 40 lb 12 V models. Newport 36 lb is 12 V, max draw 29 A. | very light inflatable, kayak, protected small river/lake; little wind/current margin |
| 45-55 lb, 12 V | Minn Kota Endura Max 45/55; Newport NV 46/55 | Minn Kota Endura Max 55: 12 V, max draw 50 A. Newport 46/55: 12 V, max draw 40/52 A. | likely BoatLock early bench equivalent; common small-boat transom motor |
| 62 lb, 12 V | Newport NV/L 62 | Newport NV 62: 12 V, max draw 58 A. | high end of single-battery brushed transom motors |
| 70-90 lb, 24 V | Minn Kota 70/80/90 class; Newport NV 86; Garmin Force at 24 V | Minn Kota selection guide maps 70-90 lb to 24 V classes. Newport 86: 24 V, max draw 48 A. Garmin Force table: 80 lb at 24 V, 57 A at full throttle. | medium boats, stronger current/wind, larger reservoirs |
| 100-115 lb, 36 V | Minn Kota Terrova/Riptide 112/115; Garmin Force at 36 V | Minn Kota Riptide Terrova 112: 36 V, max draw 52 A. Garmin Force table: 100 lb at 36 V, 54 A at full throttle. | large windage boats, big lakes/reservoirs, rougher water |
| 97/120 lb, 24/36 V | Lowrance Ghost/Ghost X | Lowrance official page confirms 24/36 V and anchor/heading modes; exact 97/120 lb values are visible in dealer listings, not in the captured official spec text. | keep as low-confidence comparative high-power GPS-anchor class |

Minn Kota sizing baseline:

- Rule of thumb: at least 2 lb thrust per 100 lb fully loaded boat; add margin for wind/current.
- Selection guide bands: <=1599 lb boat -> 30-40 lb/12 V; 1600-2599 lb -> 40-55 lb/12 V; 2600-3599 lb -> 70-80 lb/24 V; 3600-4599 lb -> 80-90 lb/24 V; 4600+ lb -> 112-115 lb/36 V.

## Wind And Wave Baseline

Use Beaufort/sea-state rows as generic wind-to-wave scenarios. Met Office notes these wave heights are for well-developed open sea and lag wind changes; reservoirs/lakes with shorter fetch need separate caps.

| Scenario | Wind m/s | Beaufort | Open-water wave baseline | BoatLock use |
| --- | ---: | ---: | --- | --- |
| calm | 0-2 | 0-1 | 0-0.1 m | sensor noise floor, no environmental load |
| light_breeze | 2-5 | 2-3 | 0.2-1.0 m | ordinary lake/river day |
| moderate_breeze | 6-8 | 4 | about 1.0 m, max about 1.5 m | common small-craft challenge |
| fresh_breeze | 9-11 | 5 | about 2.0 m, max about 2.5 m | likely out-of-scope for small open boat on large water |
| strong_breeze | 11-14 | 6 | about 3.0 m, max about 4.0 m | emergency/failsafe only |
| near_gale | 14-17 | 7 | about 4.0 m, max about 5.5 m | do not expect anchor hold; test hard exit/quiet state |

## Water Body Classes For Simulation

| Class | Current range | Wave range | Wind range | Notes |
| --- | --- | --- | --- | --- |
| protected_small_lake_or_cove | 0-0.05 m/s | 0-0.3 m | 0-6 m/s, gusts 8-10 | Good for baseline tuning and GPS noise tests. |
| lowland_river_normal | 0.2-0.7 m/s | 0.1-0.8 m local chop | 0-10 m/s | Volga/Oka-like lowland river sections. Use vector aligned with channel. |
| lowland_river_flood_or_dam_release | 0.8-1.7 m/s | 0.3-1.2 m plus wakes | 5-15 m/s | Should stress pre-enable and containment. Natural upper Volga data showed 1.5-1.7 m/s during spring flood. |
| large_reservoir_open_fetch | 0.03-0.30 m/s background; 0.3-0.8 m/s wind/nearshore storm velocities possible | 0.5-2.5 m, extremes 3-5 m by site | 6-20 m/s | Rybinsk/Kuibyshev-like. Short, steep waves are more relevant than ocean swell. |
| large_lake_ladoga_onega | 0.05-0.20 m/s typical circulation/drift; storm drift can be much higher, Onega secondary source says up to 1.3 m/s locally | 1.0-3.5 m common storm class; extremes up to 4.5-6 m by lake/area | 6-20+ m/s | Treat as "freshwater sea"; use for fail-closed scenarios, not normal hold acceptance. |
| baltic_gulf_or_brackish_sea | 0.05-0.15 m/s common Gulf of Finland surface drift; 0.5 m/s hard storms; >1 m/s straits | 0.5-2.5 m Gulf/coastal; Baltic storm records higher | 5-20+ m/s | Include salinity/corrosion only for hardware durability, not control loop initially. |

## Priority Water Bodies

### Rivers

| Water body | Type | Size source data | Current/wind/wave data for scenarios | Confidence |
| --- | --- | --- | --- | --- |
| Volga | regulated lowland river + reservoir cascade | Britannica: length 3530 km; basin 1,380,000 km2; average discharge increases from about 180 m3/s at Tver to about 8050 m3/s near mouth. | ScienceDirect summary for pre-regulated upper Volga: summer low-flow velocities 0.26-0.32 m/s in deep areas, 0.50-0.70 m/s over shallow bars; spring flood 1.50-1.70 m/s. Present cascade has complex reservoir circulation and reduced current velocities. | medium: useful ranges, not reach-specific |
| Oka | lowland river, Volga tributary | Britannica: length 1500 km; basin 245,000 km2; narrow/winding upper valley then broad lowland. | Good public source for current speed not found in this pass. Use lowland river normal/flood classes until hydropost/loctia data is added. | low for current, high for size |
| Kama | large Volga tributary/reservoir chain | Need dedicated source pass if selected for tests. | Use Volga cascade/reservoir class for now. | research_gap |
| Neva | short high-discharge outlet from Ladoga to Gulf of Finland | Need dedicated source pass if selected for tests. | Likely important for strong current + urban channel wake scenarios, but not sourced here. | research_gap |

### Lakes And Reservoirs

| Water body | Type | Size source data | Wave/current data for scenarios | Confidence |
| --- | --- | --- | --- | --- |
| Ladoga | large lake / freshwater sea | Britannica: area about 17,600 km2 without islands, length 219 km, mean depth 51 m, max depth 230 m; seiches are observed. | Ladoga sailing/hydrology source: central/northern deep-water waves usually <=3.5 m, Valaam up to 4.5 m, south-east can reach 6 m under strong northerly winds; wave period often 3.5 s, max 7 s. Volgo-Balt storm forecasts in 2025 show 6-11 m/s winds with 1-2.3 m waves, and a July case with 12-14 m/s, gusts 17-20 m/s, 2.2-2.8 m waves. | medium-high |
| Onega | large lake / freshwater sea | Britannica: area 9720 km2, length 248 km, width 80 km, max depth about 116 m. | Britannica: autumn gale waves sometimes 14-15 ft/about 4.5 m. Secondary Russian summaries mention frequent waves and storm waves around 3.5 m. Use 0.5-1.5 m ordinary, 2.5-4.5 m storm. | medium |
| Rybinsk Reservoir | large Volga reservoir | Voda.gov.ru: NPU area 4550 km2, volume 25.42 km3. | Public sources: waves can reach about 2 m; sailing school cites 2-2.5 m at 20 m/s wind on shipping routes; hydrology paper summary gives 1.5-1.7 m at 15 m/s and 2-2.5 m at 20 m/s for 60 km fetch. | medium |
| Kuibyshev Reservoir | very large Volga reservoir | Voda.gov.ru: NPU area 6150 km2, volume 58 km3. | Local/guide sources: storm periods mainly spring/autumn; high waves up to about 2.5 m are expected, some reports cite >3 m and rare higher localized waves. Use 0.5-1.5 m ordinary, 2-3 m storm, 5 m extreme only as fail-safe test. | medium-low for waves |
| Gorky Reservoir | Volga reservoir | Voda.gov.ru: NPU area 1591 km2, volume 8.815 km3. | No wave/current source captured; use large_reservoir_open_fetch with lower fetch cap than Rybinsk/Kuibyshev. | research_gap |
| Saratov Reservoir | Volga reservoir | Voda.gov.ru: NPU area 1831 km2, volume 12.87 km3. | No wave/current source captured; use large_reservoir_open_fetch. | research_gap |
| Volgograd Reservoir | Volga reservoir | Voda.gov.ru: NPU area 3117 km2, volume 31.45 km3. | No wave/current source captured; use large_reservoir_open_fetch plus lower-Volga wind exposure. | research_gap |

### Sea / Brackish Test Classes

| Water body | Type | Candidate data | Confidence |
| --- | --- | --- | --- |
| Gulf of Finland / Baltic coastal water | brackish sea, coastal/gulf | Finnish Meteorological Institute: Baltic surface currents are typically 0.05-0.10 m/s, hard storms up to 0.50 m/s, narrow straits >1 m/s; sea current velocity often about 1% of wind speed. Gulf of Finland drifter study: 0.05-0.15 m/s most common, 1.5% of wind speed is a reasonable estimate for many cases. FMI wave observations show significant wave height and highest wave separately; HELCOM 2024 report notes Baltic storm records including 5.6 m significant wave in the Bothnian Sea. | high for Baltic class |
| Open sea generic | marine fail-safe envelope | Use Beaufort/Met Office open-sea wind/wave table; treat Beaufort 5+ as operationally unsafe for current small BoatLock target unless explicitly testing hard exits. | high generic, low site-specific |

## Inputs Worth Carrying Into Future HIL Scenarios

- `motor_thrust_lbf`, `motor_voltage_v`, `motor_max_current_a`, `motor_response_tau_s`, `motor_deadband_pct`.
- `boat_mass_kg`, `windage_area_m2`, `drag_coefficient`, `underwater_drag_coeff`, `heading_inertia`.
- `wind_mean_mps`, `wind_gust_mps`, `gust_period_s`, `gust_duration_s`, `wind_direction_deg`.
- `current_mean_mps`, `current_shear_mps`, `current_direction_deg`, `current_reversal_period_s`.
- `wave_significant_height_m`, `wave_peak_height_m`, `wave_period_s`, `wave_direction_deg`.
- `wake_event_height_m`, `wake_event_period_s`, `wake_count_per_hour` for rivers/reservoir ship traffic.
- `gnss_noise_sigma_m`, `gnss_dropout_rate`, `gnss_jump_m`, `hdop_profile`, `satellite_count_profile`.
- `compass_noise_deg`, `compass_dropout_s`, `magnetic_disturbance_deg`.
- `battery_voltage_nominal_v`, `battery_voltage_sag_v`, `current_limit_a`, `thermal_derate_pct`.
- `scenario_abort_expected`: true for high-wave/high-current cases where safe behavior is "quiet hold", not successful anchor hold.

## Candidate Scenario Seeds

| Scenario | Motor | Environment | Target behavior |
| --- | --- | --- | --- |
| river_oka_normal_55lb | 55 lb / 12 V / 50-52 A | current 0.35 m/s, wind 4 m/s, wave 0.2 m | hold if configured boat mass is light; otherwise bounded drift/fail threshold |
| volga_spring_flow_80lb | 80 lb / 24 V / 57 A | current 1.2 m/s, wind 6 m/s, wave 0.5 m | should expose insufficient thrust or containment breach cleanly |
| rybinsk_fetch_55lb | 55 lb / 12 V / 52 A | current 0.1 m/s, wind 10 m/s gust 14, wave 1.5 m period 3 s | no twitch/thrash; likely fail or conservative bounded correction |
| ladoga_storm_abort | 100+ lb class | wind 17-20 m/s, wave 2.5-4 m, current 0.3-0.5 m/s | do not attempt normal hold; verify fail-closed and motor quiet state |
| baltic_gulf_drift | 80-100 lb class | current = 0.015 * wind, wind 8-12 m/s, wave 0.8-1.5 m | test wind-driven current vector changes and GNSS/heading noise |

## Sources

- Minn Kota, Trolling Motor Selection & Boat Size Guide: https://minnkota.johnsonoutdoors.com/us/support/trolling-motor-installation-guides/motor-selection-boat-size-guide
- Minn Kota, Thrust and Speed: https://minnkota-help.johnsonoutdoors.com/hc/en-us/articles/4413536408343-Thrust-and-Speed
- Minn Kota Endura Max 55 specs: https://minnkota.johnsonoutdoors.com/us/shop/freshwater-trolling-motors/endura-max/1352156m
- Minn Kota Riptide Terrova 112 specs: https://minnkota.johnsonoutdoors.com/us/shop/saltwater-trolling-motors/riptide-terrova/1363792
- Newport NV Series specs: https://newportvessels.com/products/nv-series-saltwater-trolling-motor
- Garmin Force owner manual, thrust/current table: https://www8.garmin.com/manuals/webhelp/forcetrollingmotor/EN-GB/GUID-9BEB843E-2C2B-450E-B9B2-F4C9F29D0C2E.html
- Lowrance Ghost product/spec page: https://www.lowrance.com/lowrance/type/trolling-motor/ghost-47/
- Met Office Beaufort wind force scale: https://weather.metoffice.gov.uk/guides/coast-and-sea/beaufort-scale
- NOAA/NWS Beaufort examples: https://www.weather.gov/pqr/beaufort and https://www.wpc.ncep.noaa.gov/html/beaufort.shtml
- USGS, how streamflow is measured: https://www.usgs.gov/water-science-school/science/how-streamflow-measured
- USGS, index velocity: https://www.usgs.gov/hydroacoustics/index-velocity
- Britannica, Volga River: https://www.britannica.com/place/Volga-River
- Britannica, Oka River: https://www.britannica.com/place/Oka-River
- ScienceDirect hydrological-regime summary with Volga velocity ranges: https://www.sciencedirect.com/topics/earth-and-planetary-sciences/hydrological-regime
- Voda.gov.ru, Rybinsk Reservoir: https://voda.gov.ru/reservoirs/7273/
- Voda.gov.ru, Gorky Reservoir: https://voda.gov.ru/reservoirs/7272/
- Voda.gov.ru, Kuibyshev Reservoir: https://voda.gov.ru/reservoirs/7337/
- Voda.gov.ru, Saratov Reservoir: https://voda.gov.ru/reservoirs/7339/
- Voda.gov.ru, Volgograd Reservoir: https://voda.gov.ru/reservoirs/7335/
- Britannica, Lake Ladoga: https://www.britannica.com/place/Lake-Ladoga
- Ladoga sailing/hydrology notes: https://ladoga-lake.ru/pages/artcl-ladoga-sailing-03.php
- Volgo-Balt Ladoga storm forecast example, 2025-07-04/05: https://www.volgo-balt.ru/opendata/putevaya-informatsiya/pogoda/shtorm-prognoz-pogody-po-rayonam-ladozhskogo-ozera-ot-18-00-4-iyulya-2025-g-do-06-00-5-iyulya-2025-g/
- Britannica, Lake Onega: https://www.britannica.com/place/Lake-Onega
- Finnish Meteorological Institute, Baltic Sea currents: https://en.ilmatieteenlaitos.fi/baltic-sea-currents
- Finnish Meteorological Institute, wave observations/forecasts: https://en.ilmatieteenlaitos.fi/wave-height
- ScienceDirect, Gulf of Finland surface drift study: https://www.sciencedirect.com/science/article/pii/S0272771420308027
- HELCOM, Wave climate in the Baltic Sea 2024: https://helcom.fi/publications/wave-climate-in-the-baltic-sea-2024/
