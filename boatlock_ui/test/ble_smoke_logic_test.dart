import 'dart:convert';

import 'package:boatlock_ui/ble/ble_command_rejection.dart';
import 'package:boatlock_ui/models/boat_data.dart';
import 'package:boatlock_ui/smoke/ble_smoke_logic.dart';
import 'package:flutter_test/flutter_test.dart';

BoatData _data({
  double lat = 0,
  double lon = 0,
  String mode = 'IDLE',
  String status = 'OK',
  String statusReasons = '',
  bool secPaired = false,
  bool secAuth = false,
  int rssi = -52,
  int gnssQ = 0,
}) {
  return BoatData(
    lat: lat,
    lon: lon,
    anchorLat: 0,
    anchorLon: 0,
    anchorHeading: 0,
    distance: 0,
    anchorBearing: 0,
    heading: 0,
    battery: 0,
    status: status,
    statusReasons: statusReasons,
    mode: mode,
    rssi: rssi,
    holdHeading: false,
    stepSpr: 7200,
    stepMaxSpd: 700,
    stepAccel: 250,
    headingRaw: 0,
    compassOffset: 0,
    compassQ: 0,
    magQ: 0,
    gyroQ: 0,
    rvAcc: 0,
    magNorm: 0,
    gyroNorm: 0,
    pitch: 0,
    roll: 0,
    secPaired: secPaired,
    secAuth: secAuth,
    secPairWindowOpen: false,
    secReject: 'NONE',
    gnssQ: gnssQ,
  );
}

void main() {
  test('smokeTelemetryLooksHealthy requires known mode and status', () {
    expect(smokeTelemetryLooksHealthy(_data()), isTrue);
    expect(smokeTelemetryLooksHealthy(_data(mode: '', status: 'OK')), isFalse);
    expect(
      smokeTelemetryLooksHealthy(_data(mode: 'IDLE', status: '')),
      isFalse,
    );
    expect(
      smokeTelemetryLooksHealthy(_data(mode: 'UNKNOWN', status: 'OK')),
      isFalse,
    );
    expect(
      smokeTelemetryLooksHealthy(_data(mode: 'IDLE', status: 'UNKNOWN')),
      isFalse,
    );
    expect(smokeTelemetryLooksHealthy(null), isFalse);
  });

  test('status smoke identifies STOP alert and recovery', () {
    expect(
      smokeStatusStopAlertSeen(
        _data(status: 'ALERT', statusReasons: 'NO_GPS,STOP_CMD'),
      ),
      isTrue,
    );
    expect(
      smokeStatusStopAlertSeen(
        _data(status: 'WARN', statusReasons: 'STOP_CMD'),
      ),
      isFalse,
    );
    expect(
      smokeStatusRecoveredAfterStop(
        _data(mode: 'IDLE', status: 'WARN', statusReasons: 'NO_GPS'),
      ),
      isTrue,
    );
    expect(
      smokeStatusRecoveredAfterStop(
        _data(mode: 'MANUAL', status: 'WARN', statusReasons: 'NO_GPS'),
      ),
      isFalse,
    );
  });

  test('anchor smoke identifies denied log and safe mode', () {
    expect(
      smokeAnchorDeniedLogSeen('[EVENT] ANCHOR_DENIED reason=NO_GPS'),
      isTrue,
    );
    expect(smokeAnchorDeniedLogSeen('[BLE] ANCHOR_ON accepted'), isFalse);
    expect(smokeAnchorRejectedSafely(_data(mode: 'IDLE')), isTrue);
    expect(smokeAnchorRejectedSafely(_data(mode: 'ANCHOR')), isFalse);
  });

  test('compass smoke identifies calibration command logs', () {
    expect(
      smokeCompassCalStartLogSeen('[EVENT] COMPASS_CAL_START ok=0'),
      isTrue,
    );
    expect(
      smokeCompassDcdAutosaveLogSeen(
        '[EVENT] COMPASS_DCD_AUTOSAVE enabled=0 ok=0',
      ),
      isTrue,
    );
    expect(smokeCompassDcdSaveLogSeen('[EVENT] COMPASS_DCD_SAVE ok=0'), isTrue);
    expect(smokeCompassDcdSaveLogSeen('[BLE] COMPASS_DCD_SAVE'), isFalse);
  });

  test('smoke identifies firmware profile command rejections', () {
    const hilLine =
        '[BLE] command rejected reason=profile profile=release '
        'scope=dev_hil command=SET_PHONE_GPS:59,30';

    expect(
      smokeCommandRejectedByProfile(
        hilLine,
        commandPrefix: 'SET_PHONE_GPS',
        profile: 'release',
        scope: 'dev_hil',
      ),
      isTrue,
    );
    expect(
      smokeCommandRejectedByProfile(
        hilLine,
        commandPrefix: 'SIM_RUN',
        profile: 'release',
      ),
      isFalse,
    );
  });

  test('gps smoke requires real coordinates and live GNSS quality', () {
    expect(
      smokeGpsFixLooksHealthy(_data(lat: 59.938631, lon: 30.314112, gnssQ: 1)),
      isTrue,
    );
    expect(
      smokeGpsFixLooksHealthy(
        _data(
          lat: 59.938631,
          lon: 30.314112,
          gnssQ: 2,
          statusReasons: 'GPS_WEAK',
        ),
      ),
      isTrue,
    );
    expect(smokeGpsFixLooksHealthy(_data(lat: 0, lon: 0, gnssQ: 2)), isFalse);
    expect(
      smokeGpsFixLooksHealthy(_data(lat: 59.938631, lon: 30.314112, gnssQ: 0)),
      isFalse,
    );
    expect(
      smokeGpsFixLooksHealthy(
        _data(
          lat: 59.938631,
          lon: 30.314112,
          gnssQ: 2,
          statusReasons: 'NO_GPS',
        ),
      ),
      isFalse,
    );
  });

  test('encodeSmokeResultLine serializes expected payload', () {
    final line = encodeSmokeResultLine(
      buildSmokeResultPayload(
        pass: true,
        reason: 'telemetry_received',
        dataEvents: 2,
        deviceLogEvents: 1,
        data: _data(statusReasons: 'NO_GPS', secPaired: true),
        lastDeviceLog: '[BLE] advertising started',
      ),
    );

    expect(line.startsWith(kBoatLockSmokeResultPrefix), isTrue);
    final payload = jsonDecode(
      line.substring(kBoatLockSmokeResultPrefix.length),
    );
    expect(payload['pass'], isTrue);
    expect(payload['reason'], 'telemetry_received');
    expect(payload['mode'], 'IDLE');
    expect(payload['statusReasons'], 'NO_GPS');
    expect(payload['secPaired'], isTrue);
    expect(payload['lat'], 0.0);
    expect(payload['lon'], 0.0);
    expect(payload['gnssQ'], 0);
    expect(payload['commandRejectCommandName'], '');
  });

  test('result payload carries structured command rejection fields', () {
    const rejection = BleCommandRejection(
      reason: 'profile',
      profile: 'release',
      scope: 'dev_hil',
      command: 'SET_PHONE_GPS:59,30',
    );
    final payload = buildSmokeResultPayload(
      pass: false,
      reason: 'ota_rejected',
      dataEvents: 1,
      deviceLogEvents: 2,
      commandRejection: rejection,
      lastDeviceLog:
          '[BLE] command rejected reason=profile profile=release scope=dev_hil command=SET_PHONE_GPS:59,30',
    );

    expect(payload['commandRejectReason'], 'profile');
    expect(payload['commandRejectProfile'], 'release');
    expect(payload['commandRejectScope'], 'dev_hil');
    expect(payload['commandRejectCommand'], 'SET_PHONE_GPS:59,30');
    expect(payload['commandRejectCommandName'], 'SET_PHONE_GPS');
    expect(payload['commandRejectRequiredProfile'], 'acceptance');
  });

  test('encodeSmokeStageLine serializes stage marker', () {
    final line = encodeSmokeStageLine('first_telemetry');

    expect(line.startsWith(kBoatLockSmokeStagePrefix), isTrue);
    final payload = jsonDecode(
      line.substring(kBoatLockSmokeStagePrefix.length),
    );
    expect(payload['stage'], 'first_telemetry');
  });
}
