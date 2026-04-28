import 'package:boatlock_ui/ble/ble_boatlock.dart';
import 'package:boatlock_ui/models/boat_data.dart';
import 'package:boatlock_ui/pages/map_page.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  testWidgets('null telemetry returns map to disconnected state', (
    tester,
  ) async {
    final ble = await _pumpMapPage(tester);

    expect(find.text('Поиск устройства BoatLock…'), findsOneWidget);

    ble.emit(_boatData());
    await tester.pump();

    expect(find.text('Поиск устройства BoatLock…'), findsNothing);

    ble.emit(null);
    await tester.pump();

    expect(find.text('Поиск устройства BoatLock…'), findsOneWidget);

    await tester.tap(find.byTooltip('Сохранить якорь'));
    await tester.pump();

    expect(ble.setAnchorCalls, 0);
  });

  testWidgets('anchor preflight blocks enable when readiness is missing', (
    tester,
  ) async {
    final ble = await _pumpMapPage(tester);
    ble.emit(
      _boatData(
        lat: 0,
        lon: 0,
        anchorLat: 0,
        anchorLon: 0,
        statusReasons: 'NO_GPS,NO_HEADING',
        compassQ: 0,
        gnssQ: 0,
        stepSpr: 0,
        stepGear: 0,
        stepMaxSpd: 0,
        stepAccel: 0,
      ),
    );
    await tester.pump();

    await tester.tap(find.byTooltip('Включить якорь'));
    await tester.pumpAndSettle();

    expect(find.text('Проверка перед якорем'), findsOneWidget);
    final enableButton = tester.widget<FilledButton>(
      find.widgetWithText(FilledButton, 'Включить'),
    );
    expect(enableButton.onPressed, isNull);

    await tester.tap(find.text('Отмена'));
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 300));

    expect(ble.anchorOnCalls, 0);
    expect(find.textContaining('ANCHOR_ON заблокирован:'), findsOneWidget);

    await tester.tap(find.byTooltip('История якоря'));
    await tester.pumpAndSettle();

    expect(find.text('ANCHOR_ON'), findsOneWidget);
    expect(find.textContaining('app: blocked:'), findsOneWidget);
  });

  testWidgets('anchor command failure shows send failure feedback', (
    tester,
  ) async {
    final ble = await _pumpMapPage(tester);
    ble.setAnchorResult = false;
    ble.emit(_boatData());
    await tester.pump();

    await tester.tap(find.byTooltip('Сохранить якорь'));
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 300));

    expect(ble.setAnchorCalls, 1);
    expect(find.text('SET_ANCHOR не отправлена'), findsOneWidget);
  });

  testWidgets('anchor history records commands and telemetry transitions', (
    tester,
  ) async {
    final ble = await _pumpMapPage(tester);
    ble.emit(_boatData());
    await tester.pump();

    await tester.tap(find.byTooltip('Сохранить якорь'));
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 300));

    ble.emit(
      _boatData(mode: 'ANCHOR', status: 'WARN', statusReasons: 'NO_GPS'),
    );
    await tester.pump();
    ble.emitLog('[EVENT] FAILSAFE_TRIGGERED reason=STOP_CMD action=STOP');
    await tester.pump();

    await tester.tap(find.byTooltip('История якоря'));
    await tester.pumpAndSettle();

    expect(find.text('История якоря'), findsOneWidget);
    expect(find.text('SET_ANCHOR'), findsOneWidget);
    expect(find.text('app: request sent'), findsOneWidget);
    expect(find.text('MODE'), findsOneWidget);
    expect(find.text('telemetry: IDLE -> ANCHOR'), findsOneWidget);
    expect(find.text('REASON'), findsOneWidget);
    expect(find.text('telemetry: NO_GPS'), findsOneWidget);
    expect(find.text('FAILSAFE_TRIGGERED'), findsOneWidget);
    expect(
      find.text(
        'device: [EVENT] FAILSAFE_TRIGGERED reason=STOP_CMD action=STOP',
      ),
      findsOneWidget,
    );
    expect(find.text('LOG'), findsNothing);
  });

  testWidgets('anchor history ignores unrelated event logs', (tester) async {
    final ble = await _pumpMapPage(tester);
    ble.emit(_boatData());
    await tester.pump();

    ble.emitLog('[EVENT] OTA_BEGIN size=1');
    await tester.pump();

    await tester.tap(find.byTooltip('История якоря'));
    await tester.pumpAndSettle();

    expect(find.text('OTA_BEGIN'), findsNothing);
    expect(find.textContaining('OTA_BEGIN'), findsNothing);
  });

  testWidgets('anchor nudge sends firmware cardinal command', (tester) async {
    final ble = await _pumpMapPage(tester);
    ble.emit(_boatData(mode: 'ANCHOR'));
    await tester.pump();

    await tester.tap(find.byTooltip('Сдвинуть якорь влево'));
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 300));

    expect(ble.nudgeDirs, ['LEFT']);
    expect(find.text('NUDGE_DIR:LEFT отправлена'), findsOneWidget);
  });

  testWidgets('anchor nudge is hidden until active anchor context exists', (
    tester,
  ) async {
    final ble = await _pumpMapPage(tester);
    ble.emit(_boatData(mode: 'ANCHOR', anchorLat: 0, anchorLon: 0));
    await tester.pump();

    expect(find.byTooltip('Сдвинуть якорь вперед'), findsNothing);

    ble.emit(_boatData());
    await tester.pump();

    expect(find.byTooltip('Сдвинуть якорь вперед'), findsNothing);
  });

  testWidgets('anchor nudge failure shows send failure feedback', (
    tester,
  ) async {
    final ble = await _pumpMapPage(tester);
    ble.nudgeDirResult = false;
    ble.emit(_boatData(mode: 'ANCHOR'));
    await tester.pump();

    await tester.tap(find.byTooltip('Сдвинуть якорь вперед'));
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 300));

    expect(ble.nudgeDirs, ['FWD']);
    expect(find.text('NUDGE_DIR:FWD не отправлена'), findsOneWidget);
  });

  testWidgets('emergency stop sends STOP immediately from left button', (
    tester,
  ) async {
    final ble = await _pumpMapPage(tester);
    ble.emit(_boatData());
    await tester.pump();

    final stopRight = tester.getTopRight(find.byTooltip('Аварийный STOP')).dx;
    final refreshLeft = tester.getTopLeft(find.byTooltip('Обновить данные')).dx;
    expect(stopRight, lessThan(refreshLeft));

    await tester.tap(find.byTooltip('Аварийный STOP'));
    await tester.pump();
    expect(find.text('Остановить якорь и моторы прямо сейчас?'), findsNothing);
    await tester.pump(const Duration(milliseconds: 300));

    expect(ble.stopAllCalls, 1);
    expect(find.text('STOP отправлен'), findsOneWidget);
  });

  testWidgets('manual sheet exposes stop without emergency STOP', (
    tester,
  ) async {
    final ble = await _pumpMapPage(tester);
    ble.emit(_boatData(mode: 'MANUAL'));
    await tester.pump();

    await tester.tap(find.byTooltip('Ручное управление'));
    await tester.pumpAndSettle();

    expect(find.text('Стоп'), findsOneWidget);
    expect(find.text('STOP'), findsNothing);

    await tester.tap(find.text('Стоп'));
    await tester.pump();
    await tester.pump(const Duration(milliseconds: 300));

    expect(ble.manualOffCalls, 1);
    expect(find.text('Остановлено'), findsOneWidget);
  });
}

Future<FakeBleBoatLock> _pumpMapPage(WidgetTester tester) async {
  tester.view.physicalSize = const Size(1200, 1000);
  tester.view.devicePixelRatio = 1.0;
  addTearDown(tester.view.resetPhysicalSize);
  addTearDown(tester.view.resetDevicePixelRatio);

  late FakeBleBoatLock ble;
  await tester.pumpWidget(
    MaterialApp(
      home: MapPage(
        bleFactory: ({required onData, onLog}) {
          ble = FakeBleBoatLock(onData: onData, onLog: onLog);
          return ble;
        },
        enableLocation: false,
        loadMapTiles: false,
      ),
    ),
  );
  await tester.pump();
  return ble;
}

BoatData _boatData({
  double lat = 55.751244,
  double lon = 37.618423,
  double anchorLat = 55.7513,
  double anchorLon = 37.6185,
  String status = 'OK',
  String statusReasons = '',
  String mode = 'IDLE',
  int compassQ = 3,
  int gnssQ = 2,
  int stepSpr = 200,
  double stepGear = 36,
  double stepMaxSpd = 1000,
  double stepAccel = 500,
}) {
  return BoatData(
    lat: lat,
    lon: lon,
    anchorLat: anchorLat,
    anchorLon: anchorLon,
    anchorHeading: 12,
    distance: 3.5,
    anchorBearing: 120,
    heading: 42,
    battery: 87,
    status: status,
    statusReasons: statusReasons,
    mode: mode,
    rssi: -58,
    holdHeading: false,
    stepSpr: stepSpr,
    stepGear: stepGear,
    stepMaxSpd: stepMaxSpd,
    stepAccel: stepAccel,
    headingRaw: 42,
    compassOffset: 0,
    compassQ: compassQ,
    magQ: 3,
    gyroQ: 3,
    rvAcc: 2,
    magNorm: 50,
    gyroNorm: 0.02,
    pitch: 1.2,
    roll: -0.4,
    gnssQ: gnssQ,
  );
}

class FakeBleBoatLock extends BleBoatLock {
  FakeBleBoatLock({required super.onData, super.onLog});

  int setAnchorCalls = 0;
  int anchorOnCalls = 0;
  int stopAllCalls = 0;
  int manualOffCalls = 0;
  final List<String> nudgeDirs = [];
  bool setAnchorResult = true;
  bool anchorOnResult = true;
  bool stopAllResult = true;
  bool manualOffResult = true;
  bool nudgeDirResult = true;

  void emit(BoatData? data) => onData(data);
  void emitLog(String line) => onLog?.call(line);

  @override
  Future<void> connectAndListen() async {}

  @override
  Future<void> requestSnapshot() async {}

  @override
  Future<bool> setAnchor() async {
    setAnchorCalls += 1;
    return setAnchorResult;
  }

  @override
  Future<bool> anchorOn() async {
    anchorOnCalls += 1;
    return anchorOnResult;
  }

  @override
  Future<bool> stopAll() async {
    stopAllCalls += 1;
    return stopAllResult;
  }

  @override
  Future<bool> manualOff() async {
    manualOffCalls += 1;
    return manualOffResult;
  }

  @override
  Future<bool> nudgeDir(String direction) async {
    nudgeDirs.add(direction);
    return nudgeDirResult;
  }

  @override
  void dispose() {}
}
