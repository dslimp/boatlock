import 'package:boatlock_ui/ble/ble_boatlock.dart';
import 'package:boatlock_ui/ble/ble_debug_snapshot.dart';
import 'package:boatlock_ui/models/boat_data.dart';
import 'package:boatlock_ui/pages/diagnostics_page.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

class FakeBleBoatLock extends BleBoatLock {
  FakeBleBoatLock() : super(onData: (_) {});

  int snapshotRequests = 0;

  @override
  Future<void> requestSnapshot() async {
    snapshotRequests += 1;
  }
}

BoatData _data() => BoatData(
  lat: 59.123456,
  lon: 30.654321,
  anchorLat: 0,
  anchorLon: 0,
  anchorHeading: 0,
  distance: 0,
  heading: 12,
  battery: 0,
  status: 'WARN',
  statusReasons: 'NO_GPS',
  mode: 'IDLE',
  rssi: -47,
  holdHeading: false,
  stepSpr: 4096,
  stepMaxSpd: 700,
  stepAccel: 250,
  headingRaw: 12,
  compassOffset: 0,
  compassQ: 1,
  magQ: 1,
  gyroQ: 1,
  rvAcc: 2,
  magNorm: 40,
  gyroNorm: 0.1,
  pitch: 1,
  roll: 2,
  secPaired: false,
  secAuth: false,
  secPairWindowOpen: false,
  secReject: 'NONE',
  gnssQ: 0,
);

void main() {
  testWidgets('shows BLE diagnostics and logs', (tester) async {
    await tester.binding.setSurfaceSize(const Size(900, 1800));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final ble = FakeBleBoatLock();
    ble.diagnostics.value = BleDebugSnapshot.initial().copyWith(
      adapterState: 'on',
      connectionState: 'connected',
      deviceId: '98:88:E0:03:BA:5D',
      deviceName: 'BoatLock',
      mtu: 247,
      rssi: -47,
      hasDataChar: true,
      hasCommandChar: true,
      hasLogChar: true,
      hasOtaChar: true,
      dataEvents: 12,
      deviceLogEvents: 2,
      appLogEvents: 3,
      deviceLogLines: const ['[OTA] finish ok size=711776'],
      appLogLines: const ['Connected to device'],
    );

    await tester.pumpWidget(
      MaterialApp(
        home: DiagnosticsPage(ble: ble, data: _data()),
      ),
    );

    expect(find.text('Диагностика'), findsOneWidget);
    expect(find.text('BoatLock / 98:88:E0:03:BA:5D'), findsOneWidget);
    expect(find.text('247'), findsOneWidget);
    expect(find.text('IDLE'), findsOneWidget);
    expect(find.text('NO_GPS'), findsOneWidget);
    expect(find.textContaining('[OTA] finish ok'), findsOneWidget);

    await tester.tap(find.byIcon(Icons.refresh));
    await tester.pump();
    expect(ble.snapshotRequests, 1);
  });
}
