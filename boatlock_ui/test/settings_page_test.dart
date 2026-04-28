import 'package:boatlock_ui/ble/ble_boatlock.dart';
import 'package:boatlock_ui/ble/ble_command_rejection.dart';
import 'package:boatlock_ui/pages/settings_page.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

class FakeBleBoatLock extends BleBoatLock {
  FakeBleBoatLock() : super(onData: (_) {});

  bool? holdHeadingValue;

  @override
  Future<bool> setHoldHeading(bool enabled) async {
    holdHeadingValue = enabled;
    return true;
  }
}

Widget _wrap(Widget child) => MaterialApp(home: child);

SettingsPage _page(FakeBleBoatLock ble) {
  return SettingsPage(
    ble: ble,
    holdHeading: false,
    stepMaxSpd: 1000,
    stepAccel: 500,
    stepSpr: 200,
    stepGear: 36,
    compassOffset: 0,
    compassQ: 0,
    magQ: 0,
    gyroQ: 0,
    rvAcc: 0,
    magNorm: 0,
    gyroNorm: 0,
    pitch: 0,
    roll: 0,
    isConnected: true,
  );
}

void main() {
  testWidgets('legacy pairing controls are removed', (
    WidgetTester tester,
  ) async {
    await tester.binding.setSurfaceSize(const Size(900, 1800));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final ble = FakeBleBoatLock();
    await tester.pumpWidget(_wrap(_page(ble)));

    expect(find.text('Привязать'), findsNothing);
    expect(find.text('Авторизоваться'), findsNothing);
  });

  testWidgets('hold heading switch writes through direct BLE method', (
    WidgetTester tester,
  ) async {
    final ble = FakeBleBoatLock();
    await tester.pumpWidget(_wrap(_page(ble)));

    await tester.tap(
      find.widgetWithText(SwitchListTile, 'Поддерживать курс носа'),
    );
    await tester.pumpAndSettle();
    expect(ble.holdHeadingValue, true);
  });

  testWidgets('setup command controls are hidden until enabled', (
    WidgetTester tester,
  ) async {
    await tester.binding.setSurfaceSize(const Size(900, 1800));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final ble = FakeBleBoatLock();
    await tester.pumpWidget(_wrap(_page(ble)));

    expect(find.text('Макс. скорость'), findsNothing);
    expect(find.text('Офсет компаса'), findsNothing);
    expect(find.text('Прошивка ESP32'), findsNothing);
    expect(find.text('BNO08x quality'), findsNothing);
    expect(find.text('Настройка оборудования'), findsOneWidget);
  });

  testWidgets('shows firmware profile command rejection from diagnostics', (
    WidgetTester tester,
  ) async {
    final ble = FakeBleBoatLock();
    await tester.pumpWidget(_wrap(_page(ble)));

    ble.diagnostics.value = ble.diagnostics.value.copyWith(
      commandRejectEvents: 1,
      lastCommandRejection: const BleCommandRejection(
        reason: 'profile',
        profile: 'release',
        scope: 'dev_hil',
        command: 'SET_PHONE_GPS:59,30',
      ),
    );
    await tester.pump();

    expect(
      find.text(
        'Команда SET_PHONE_GPS отклонена профилем release: нужен acceptance',
      ),
      findsOneWidget,
    );
  });
}
