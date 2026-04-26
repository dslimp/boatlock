import 'package:boatlock_ui/ble/ble_boatlock.dart';
import 'package:boatlock_ui/ble/ble_command_rejection.dart';
import 'package:boatlock_ui/ble/ble_command_scope.dart';
import 'package:boatlock_ui/ble/ble_security_codec.dart';
import 'package:boatlock_ui/pages/settings_page.dart';
import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

class ServiceFakeBleBoatLock extends BleBoatLock {
  ServiceFakeBleBoatLock() : super(onData: (_) {});

  bool rejectNextStepMaxSpeed = false;
  String? ownerSecretValue;
  double? stepMaxSpeedValue;

  @override
  void setOwnerSecret(String? secret) {
    ownerSecretValue = normalizeOwnerSecret(secret);
  }

  @override
  Future<bool> setStepMaxSpeed(double value) async {
    stepMaxSpeedValue = value;
    if (rejectNextStepMaxSpeed) {
      Future<void>.delayed(const Duration(milliseconds: 10), () {
        _emitProfileRejection('SET_STEP_MAXSPD:${value.round()}');
      });
    }
    return true;
  }

  void _emitProfileRejection(String command) {
    const reason = 'profile';
    final rejection = BleCommandRejection(
      reason: reason,
      profile: 'release',
      scope: 'service',
      command: command,
    );
    diagnostics.value = diagnostics.value.copyWith(
      commandRejectEvents: diagnostics.value.commandRejectEvents + 1,
      lastCommandRejection: rejection,
      commandRejects: List<BleCommandRejection>.unmodifiable([
        ...diagnostics.value.commandRejects,
        rejection,
      ]),
    );
  }
}

Widget _wrap(Widget child) => MaterialApp(home: child);

SettingsPage _page(ServiceFakeBleBoatLock ble) {
  return SettingsPage(
    ble: ble,
    holdHeading: false,
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
  testWidgets('rolls back service setting when profile rejection arrives', (
    WidgetTester tester,
  ) async {
    if (!kBoatLockServiceUiEnabled) {
      expect(kBoatLockServiceUiEnabled, isFalse);
      return;
    }

    await tester.binding.setSurfaceSize(const Size(900, 1800));
    addTearDown(() => tester.binding.setSurfaceSize(null));
    final ble = ServiceFakeBleBoatLock()..rejectNextStepMaxSpeed = true;
    await tester.pumpWidget(_wrap(_page(ble)));

    await tester.tap(find.text('Макс. скорость'));
    await tester.pumpAndSettle();
    await tester.enterText(
      find.descendant(
        of: find.byType(AlertDialog),
        matching: find.byType(TextField),
      ),
      '1200',
    );
    await tester.tap(find.text('OK'));
    await tester.pump(const Duration(milliseconds: 50));
    await tester.pump(const Duration(milliseconds: 700));

    expect(ble.stepMaxSpeedValue, 1200);
    expect(find.text('1000'), findsOneWidget);
    expect(find.text('1200'), findsNothing);
    expect(
      find.text(
        'Команда SET_STEP_MAXSPD отклонена профилем release: нужен service',
      ),
      findsOneWidget,
    );
  });
}
