import 'package:boatlock_ui/models/anchor_preflight.dart';
import 'package:boatlock_ui/models/boat_data.dart';
import 'package:flutter_test/flutter_test.dart';

BoatData _data({
  double lat = 59.9386,
  double lon = 30.3141,
  double anchorLat = 59.9387,
  double anchorLon = 30.3142,
  String status = 'OK',
  String statusReasons = '',
  int compassQ = 3,
  int gnssQ = 2,
  bool secPaired = false,
  bool secAuth = false,
  int stepSpr = 7200,
  double stepMaxSpd = 1000,
  double stepAccel = 500,
}) {
  return BoatData(
    lat: lat,
    lon: lon,
    anchorLat: anchorLat,
    anchorLon: anchorLon,
    anchorHeading: 0,
    distance: 0,
    anchorBearing: 0,
    heading: 0,
    battery: 50,
    status: status,
    statusReasons: statusReasons,
    mode: 'IDLE',
    rssi: -60,
    holdHeading: false,
    stepSpr: stepSpr,
    stepMaxSpd: stepMaxSpd,
    stepAccel: stepAccel,
    headingRaw: 0,
    compassOffset: 0,
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
    compassQ: compassQ,
  );
}

void main() {
  test('passes when anchor prerequisites are visible as ready', () {
    final preflight = buildAnchorPreflight(_data());

    expect(preflight.canEnable, isTrue);
    expect(preflight.blockedSummary, isEmpty);
  });

  test('blocks anchor enable without saved anchor or hardware fix', () {
    final preflight = buildAnchorPreflight(
      _data(anchorLat: 0, anchorLon: 0, gnssQ: 1),
    );

    expect(preflight.canEnable, isFalse);
    expect(preflight.blockedSummary, contains('Anchor'));
    expect(preflight.blockedSummary, contains('GNSS'));
  });

  test('blocks paired devices until owner session is authenticated', () {
    final preflight = buildAnchorPreflight(
      _data(secPaired: true, secAuth: false),
    );

    expect(preflight.canEnable, isFalse);
    expect(preflight.blockedSummary, contains('Auth'));
  });

  test('blocks heading and safety failures from reason flags', () {
    final preflight = buildAnchorPreflight(
      _data(status: 'ALERT', statusReasons: 'NO_HEADING,STOP_CMD'),
    );

    expect(preflight.canEnable, isFalse);
    expect(preflight.blockedSummary, contains('Heading'));
    expect(preflight.blockedSummary, contains('Safety'));
  });
}
