import 'package:boatlock_ui/ble/ble_debug_snapshot.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:boatlock_ui/models/boat_data.dart';
import 'package:boatlock_ui/widgets/status_panel.dart';

BoatData _data({
  double lat = 59.9386,
  double lon = 30.3141,
  double anchorLat = 59.1234,
  double anchorLon = 30.5678,
  double anchorHeading = 87.0,
  double distance = 12.3,
  double anchorBearing = 271.4,
  String status = 'OK',
  String statusReasons = '',
  String mode = 'IDLE',
  int rssi = -63,
  int stepSpr = 7200,
  double stepMaxSpd = 1000,
  double stepAccel = 500,
  int compassQ = 3,
  int magQ = 2,
  int gyroQ = 2,
  int gnssQ = 2,
  bool secPaired = false,
  bool secAuth = false,
}) {
  return BoatData(
    lat: lat,
    lon: lon,
    anchorLat: anchorLat,
    anchorLon: anchorLon,
    anchorHeading: anchorHeading,
    distance: distance,
    anchorBearing: anchorBearing,
    heading: 45,
    battery: 50,
    status: status,
    statusReasons: statusReasons,
    mode: mode,
    rssi: rssi,
    holdHeading: true,
    stepSpr: stepSpr,
    stepMaxSpd: stepMaxSpd,
    stepAccel: stepAccel,
    headingRaw: 44,
    compassOffset: 0,
    compassQ: compassQ,
    magQ: magQ,
    gyroQ: gyroQ,
    rvAcc: 1.5,
    magNorm: 35,
    gyroNorm: 0.2,
    pitch: 1,
    roll: 2,
    secPaired: secPaired,
    secAuth: secAuth,
    secPairWindowOpen: false,
    secReject: 'NONE',
    gnssQ: gnssQ,
  );
}

BleDebugSnapshot _diagnostics({
  bool connected = true,
  bool scanning = false,
  bool hasDataChar = true,
  bool hasCommandChar = true,
  bool hasLogChar = true,
  int dataEvents = 4,
}) {
  return BleDebugSnapshot.initial().copyWith(
    connectionState: connected ? 'connected' : 'disconnected',
    deviceId: connected ? 'boatlock-test' : '',
    deviceName: connected ? 'BoatLock' : '',
    isScanning: scanning,
    hasDataChar: hasDataChar,
    hasCommandChar: hasCommandChar,
    hasLogChar: hasLogChar,
    dataEvents: dataEvents,
  );
}

void main() {
  testWidgets('shows compact readiness when telemetry is ready', (
    WidgetTester tester,
  ) async {
    await tester.pumpWidget(
      MaterialApp(
        home: StatusPanel(data: _data(), diagnostics: _diagnostics()),
      ),
    );

    expect(find.text('Готовность: OK'), findsOneWidget);
    expect(find.text('BLE: link'), findsOneWidget);
    expect(find.text('DATA: live'), findsOneWidget);
    expect(find.text('AUTH: без пары'), findsOneWidget);
    expect(find.text('GNSS: Q2'), findsOneWidget);
    expect(find.text('HDG: Q3'), findsOneWidget);
    expect(find.text('ANCH: есть'), findsOneWidget);
    expect(find.text('SAFE: OK'), findsOneWidget);
    expect(find.text('MODE: IDLE'), findsOneWidget);
    expect(find.text('DRV: stepper OK'), findsOneWidget);
    expect(find.text('DST/BRG: 12.3 м / 271°'), findsOneWidget);
  });

  testWidgets('surfaces blocked reasons from current telemetry', (
    WidgetTester tester,
  ) async {
    await tester.pumpWidget(
      MaterialApp(
        home: StatusPanel(
          data: _data(
            lat: 0,
            lon: 0,
            anchorLat: 0,
            anchorLon: 0,
            status: 'ALERT',
            statusReasons: 'NO_GPS,NO_COMPASS,STOP_CMD',
            mode: 'MANUAL',
            stepSpr: 0,
            gnssQ: 0,
            compassQ: 0,
            secPaired: true,
            secAuth: false,
          ),
          diagnostics: _diagnostics(),
        ),
      ),
    );

    expect(find.text('Готовность: BLOCKED 6'), findsOneWidget);
    expect(find.text('AUTH: нужна'), findsOneWidget);
    expect(find.text('GNSS: Q0'), findsOneWidget);
    expect(find.text('HDG: нет'), findsOneWidget);
    expect(find.text('ANCH: нет'), findsOneWidget);
    expect(find.text('SAFE: ALERT'), findsOneWidget);
    expect(find.text('MODE: MANUAL'), findsOneWidget);
    expect(find.text('DRV: cfg'), findsOneWidget);
  });

  testWidgets('does not show anchor range when current fix is absent', (
    WidgetTester tester,
  ) async {
    await tester.pumpWidget(
      MaterialApp(
        home: StatusPanel(
          data: _data(lat: 0, lon: 0, gnssQ: 0),
          diagnostics: _diagnostics(),
        ),
      ),
    );

    expect(find.text('ANCH: есть'), findsOneWidget);
    expect(find.text('DST/BRG: нет / -'), findsOneWidget);
  });

  testWidgets('renders BLE and data readiness without telemetry', (
    WidgetTester tester,
  ) async {
    await tester.pumpWidget(
      MaterialApp(home: StatusPanel(diagnostics: _diagnostics(scanning: true))),
    );

    expect(find.text('Готовность: WAIT'), findsOneWidget);
    expect(find.text('BLE: link'), findsOneWidget);
    expect(find.text('DATA: нет'), findsOneWidget);
  });

  testWidgets('shows missing GATT state from diagnostics', (
    WidgetTester tester,
  ) async {
    await tester.pumpWidget(
      MaterialApp(
        home: StatusPanel(
          data: _data(),
          diagnostics: _diagnostics(hasCommandChar: false),
        ),
      ),
    );

    expect(find.text('Готовность: WARN 1'), findsOneWidget);
    expect(find.text('BLE: GATT'), findsOneWidget);
    expect(find.text('DATA: live'), findsOneWidget);
  });
}
