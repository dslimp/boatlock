import 'dart:convert';

import 'package:boatlock_ui/models/boat_data.dart';
import 'package:boatlock_ui/smoke/ble_smoke_logic.dart';
import 'package:flutter_test/flutter_test.dart';

BoatData _data({
  String mode = 'IDLE',
  String status = 'OK',
  String statusReasons = '',
  bool secPaired = false,
  bool secAuth = false,
  int rssi = -52,
}) {
  return BoatData(
    lat: 0,
    lon: 0,
    anchorLat: 0,
    anchorLon: 0,
    anchorHeading: 0,
    distance: 0,
    heading: 0,
    battery: 0,
    status: status,
    statusReasons: statusReasons,
    mode: mode,
    rssi: rssi,
    holdHeading: false,
    stepSpr: 4096,
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
  );
}

void main() {
  test('smokeTelemetryLooksHealthy requires mode and status', () {
    expect(smokeTelemetryLooksHealthy(_data()), isTrue);
    expect(smokeTelemetryLooksHealthy(_data(mode: '', status: 'OK')), isFalse);
    expect(smokeTelemetryLooksHealthy(_data(mode: 'IDLE', status: '')), isFalse);
    expect(smokeTelemetryLooksHealthy(null), isFalse);
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
    final payload = jsonDecode(line.substring(kBoatLockSmokeResultPrefix.length));
    expect(payload['pass'], isTrue);
    expect(payload['reason'], 'telemetry_received');
    expect(payload['mode'], 'IDLE');
    expect(payload['statusReasons'], 'NO_GPS');
    expect(payload['secPaired'], isTrue);
  });
}
