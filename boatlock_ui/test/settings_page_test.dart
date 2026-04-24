import 'package:boatlock_ui/ble/ble_boatlock.dart';
import 'package:boatlock_ui/pages/settings_page.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

class FakeBleBoatLock extends BleBoatLock {
  FakeBleBoatLock() : super(onData: (_) {});

  bool? holdHeadingValue;
  String? ownerSecretValue;
  int pairCalls = 0;
  int authCalls = 0;
  int clearCalls = 0;

  @override
  Future<bool> setHoldHeading(bool enabled) async {
    holdHeadingValue = enabled;
    return true;
  }

  @override
  void setOwnerSecret(String? secret) {
    ownerSecretValue = BleBoatLock.normalizeOwnerSecret(secret);
  }

  @override
  String generateOwnerSecret() {
    ownerSecretValue = '00112233445566778899AABBCCDDEEFF';
    return ownerSecretValue!;
  }

  @override
  Future<bool> pairWithOwnerSecret([String? secret]) async {
    pairCalls += 1;
    ownerSecretValue = BleBoatLock.normalizeOwnerSecret(secret);
    return true;
  }

  @override
  Future<bool> authenticateOwner([String? secret]) async {
    authCalls += 1;
    ownerSecretValue = BleBoatLock.normalizeOwnerSecret(secret);
    return true;
  }

  @override
  Future<bool> clearPairing() async {
    clearCalls += 1;
    return true;
  }
}

Widget _wrap(Widget child) => MaterialApp(home: child);

SettingsPage _page(FakeBleBoatLock ble) {
  return SettingsPage(
    ble: ble,
    holdHeading: false,
    stepSpr: 4096,
    stepMaxSpd: 1000,
    stepAccel: 500,
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
    secPaired: false,
    secAuth: false,
    secPairWindowOpen: true,
    secReject: 'NONE',
  );
}

void main() {
  testWidgets('security actions are visible and pairing/auth are wired', (
    WidgetTester tester,
  ) async {
    await tester.binding.setSurfaceSize(const Size(900, 1800));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final ble = FakeBleBoatLock();
    await tester.pumpWidget(_wrap(_page(ble)));

    expect(find.text('Owner secret'), findsOneWidget);
    await tester.enterText(
      find.byType(TextField).last,
      '00112233445566778899aabbccddeeff',
    );
    await tester.tap(find.text('Привязать'));
    await tester.pumpAndSettle();
    expect(ble.pairCalls, 1);
    expect(ble.ownerSecretValue, '00112233445566778899AABBCCDDEEFF');

    await tester.tap(find.text('Авторизоваться'));
    await tester.pumpAndSettle();
    expect(ble.authCalls, 1);
  });

  testWidgets('hold heading switch uses secure BLE method', (
    WidgetTester tester,
  ) async {
    final ble = FakeBleBoatLock();
    await tester.pumpWidget(_wrap(_page(ble)));

    await tester.tap(find.byType(SwitchListTile));
    await tester.pumpAndSettle();
    expect(ble.holdHeadingValue, true);
  });
}
