# Config Schema

Schema version: `0x15` (`Settings::VERSION`).

Settings are stored in EEPROM as:

1. `version` (`uint8_t`)
2. `values[count]` (`float[count]`)
3. `crc32` (`uint32_t`, over raw `values` bytes)

On boot:

- if version mismatches, defaults are loaded and persisted (migration path)
- if CRC mismatches, defaults are loaded and persisted
- any loaded value outside allowed range is normalized and logged as `CONFIG_REJECTED`

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
- `Kp`, `Ki`, `Kd`
- `PidILim` (`1..500`)
- `ReacqStrat` (`0..1`, `0=soft`, `1=aggressive`)
- `AnchorProf` (`0..2`, `0=quiet`, `1=normal`, `2=current`)

### Safety supervisor

- `CommToutMs` (`500..60000`)
- `CtrlLoopMs` (`100..10000`)
- `SensorTout` (`300..30000`)
- `FailAct` (`0..1`, `0=STOP`, `1=Manual`)
- `MaxThrustS` (`10..3600`)
- `NanAct` (`0..1`, `0=STOP`, `1=Manual`)

### UX / Events

- `DriftAlert` (`2..200`)
- `DriftFail` (`5..500`)
- `GpsWeakHys` (`0.5..60`)
- `EventRateMs` (`100..10000`)
