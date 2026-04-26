# Config Schema

Schema version: `0x1A` (`Settings::VERSION`).

Settings are stored in ESP32 NVS namespace `boatlock_cfg` as:

1. `schema` (`uint8_t`)
2. one raw `float` blob per setting key

On boot:

- missing keys keep current defaults and are persisted on the next boot writeback
- removed keys are ignored
- older schema versions keep existing values by current key, apply explicit migration rules, and write the current schema marker
- future schema versions are loaded read-only: current known keys may be used in RAM, but boot does not downgrade the `schema` marker or write missing/default keys
- invalid NVS values use defaults and are persisted on the next boot writeback
- any loaded value outside allowed range is normalized and logged as `CONFIG_REJECTED`

Write policy:

- runtime `save()` writes NVS only when settings are dirty
- dirty setting keys are staged and committed with one `nvs_commit()`
- setting a value equal to the current normalized value does not create a flash commit
- non-finite runtime values are rejected instead of being persisted
- failed NVS set/commit calls are logged as `CONFIG_SAVE_FAILED` and keep settings dirty for retry
- older-version migration, missing-key recovery, and boot-time normalization still force a commit
- future-version rollback does not force a boot commit; operator-initiated setting changes can still be saved by the older firmware

Migration rules:

- versioned migrations live in `Settings::applyMigrations()`
- key renames must be encoded as explicit legacy-key rules instead of relying on defaults
- current legacy rule: schemas before `0x18` may migrate legacy `MaxThrust` into `MaxThrustA` when `MaxThrustA` is missing
- schemas before `0x1A` migrate the exact untouched anchor default bundle
  `HoldRadius=2.5`, `DeadbandM=1.5`, `MaxThrustA=60`, `ThrRampA=35`
  to quiet defaults `3.0/2.2/45/20`; any custom/profile value preserves the stored bundle

## Groups

### GNSS quality gate

- `MinFixType` (`2..3`)
- `MinSats` (`3..20`)
- `MaxHdop` (`0.5..10`)
- `MaxGpsAgeMs` (`300..20000`)
- `MaxPosJumpM` (`1..200`)
- `MinStatSpd` (`0.05..2.0`)
- `SpdSanity` (`0..1`)
- `MaxSpdMps` (`1..60`)
- `MaxAccMps2` (`0.5..20`)
- `ReqSent` (`0..20`)

`MinFixType` is reserved in current runtime build.

- active runtime source: `fix_type_source = NONE`
- runtime value: `fix_type = UNKNOWN`
- one-shot boot event:
  `CONFIG_FIELD_IGNORED field=min_fix_type reason=fix_type_unavailable`

### Anchor control

- `HoldRadius` (`0.5..20`)
- `DeadbandM` (`0.2..10`)
- `MaxThrustA` (`10..100`)
- `ThrRampA` (`1..100`)
- `MaxTurnRt` (`30..720`)

Legacy runtime PID knobs are removed. Anchor thrust uses bounded deadband, max-thrust, approach damping, anti-hunt timing, and slew/ramp limiting instead of hidden self-adaptive PID persistence.

### Safety supervisor

- `CommToutMs` (`3000..60000`)
- `CtrlLoopMs` (`100..10000`)
- `SensorTout` (`300..30000`)

Runtime failsafes always stop motion and latch `HOLD`. They do not automatically enter `MANUAL`; operator control must be re-enabled explicitly after a failsafe.

### UX / Events

- `DriftAlert` (`2..200`)
- `DriftFail` (`5..500`)
- `GpsWeakHys` (`0.5..60`)
- `EventRateMs` (`100..10000`)
