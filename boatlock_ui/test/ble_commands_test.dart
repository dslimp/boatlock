import 'package:boatlock_ui/ble/ble_commands.dart';
import 'package:boatlock_ui/models/boat_data.dart';
import 'package:flutter_test/flutter_test.dart';

BoatData _data({required double lat, required double lon}) {
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
    status: '',
    statusReasons: '',
    mode: '',
    rssi: 0,
    holdHeading: false,
    stepSpr: 200,
    stepMaxSpd: 1000,
    stepAccel: 500,
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
    secPaired: false,
    secAuth: false,
    secPairWindowOpen: false,
    secReject: 'NONE',
    gnssQ: 0,
  );
}

void main() {
  test('buildSetAnchorCommand returns null for null data', () {
    expect(buildSetAnchorCommand(null), isNull);
  });

  test('buildSetAnchorCommand returns null for zero coordinates', () {
    expect(buildSetAnchorCommand(_data(lat: 0, lon: 30.2)), isNull);
    expect(buildSetAnchorCommand(_data(lat: 59.1, lon: 0)), isNull);
  });

  test('buildSetAnchorCommand builds protocol command with 6 digits', () {
    expect(
      buildSetAnchorCommand(_data(lat: 59.1234567, lon: 30.7654321)),
      'SET_ANCHOR:59.123457,30.765432',
    );
  });

  test('buildSetAnchorCommandFromCoords validates ranges', () {
    expect(
      buildSetAnchorCommandFromCoords(59.1, 30.2),
      'SET_ANCHOR:59.100000,30.200000',
    );
    expect(buildSetAnchorCommandFromCoords(double.nan, 30.2), isNull);
    expect(buildSetAnchorCommandFromCoords(59.1, double.infinity), isNull);
    expect(buildSetAnchorCommandFromCoords(-95.0, 30.0), isNull);
    expect(buildSetAnchorCommandFromCoords(59.0, 181.0), isNull);
  });

  test('buildSetPhoneGpsCommand validates and formats payload', () {
    expect(
      buildSetPhoneGpsCommand(59.9386312, 30.3141119, speedKmh: 12.34),
      'SET_PHONE_GPS:59.938631,30.314112,12.3',
    );
    expect(
      buildSetPhoneGpsCommand(
        59.9386312,
        30.3141119,
        speedKmh: 12.34,
        satellites: 9,
      ),
      'SET_PHONE_GPS:59.938631,30.314112,12.3,9',
    );
    expect(buildSetPhoneGpsCommand(95, 30.0), isNull);
    expect(buildSetPhoneGpsCommand(59.0, 200.0), isNull);
  });

  test('buildNudgeDirCommand validates direction', () {
    expect(buildNudgeDirCommand('left'), 'NUDGE_DIR:LEFT');
    expect(buildNudgeDirCommand('foo'), isNull);
  });

  test('buildNudgeBearingCommand normalizes bearing', () {
    expect(buildNudgeBearingCommand(450.0), 'NUDGE_BRG:90.0');
    expect(buildNudgeBearingCommand(-10.0), 'NUDGE_BRG:350.0');
    expect(buildNudgeBearingCommand(double.nan), isNull);
  });

  test('buildSetAnchorProfileCommand validates profile values', () {
    expect(buildSetAnchorProfileCommand('quiet'), 'SET_ANCHOR_PROFILE:quiet');
    expect(
      buildSetAnchorProfileCommand(' CURRENT '),
      'SET_ANCHOR_PROFILE:current',
    );
    expect(buildSetAnchorProfileCommand('storm'), isNull);
    expect(buildSetAnchorProfileCommand(''), isNull);
  });

  test('buildManualSetCommand validates atomic deadman payload', () {
    expect(
      buildManualSetCommand(steer: -1, throttlePct: 45, ttlMs: 1000),
      'MANUAL_SET:-1,45,1000',
    );
    expect(
      buildManualSetCommand(steer: 2, throttlePct: 45, ttlMs: 1000),
      isNull,
    );
    expect(
      buildManualSetCommand(steer: 0, throttlePct: 101, ttlMs: 1000),
      isNull,
    );
    expect(buildManualSetCommand(steer: 0, throttlePct: 10, ttlMs: 50), isNull);
  });
}
