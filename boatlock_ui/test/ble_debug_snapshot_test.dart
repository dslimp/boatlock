import 'package:boatlock_ui/ble/ble_command_rejection.dart';
import 'package:boatlock_ui/ble/ble_debug_snapshot.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('tracks ready flags and preserves immutable log lists', () {
    final snapshot = BleDebugSnapshot.initial().copyWith(
      connectionState: 'connected',
      hasDataChar: true,
      hasCommandChar: true,
      hasLogChar: true,
      hasOtaChar: true,
      appLogLines: const ['connected'],
      deviceLogLines: const ['[BLE] advertising started'],
    );

    expect(snapshot.connected, isTrue);
    expect(snapshot.coreReady, isTrue);
    expect(snapshot.otaReady, isTrue);
    expect(snapshot.appLogLines, ['connected']);
    expect(snapshot.deviceLogLines, ['[BLE] advertising started']);
  });

  test('can explicitly clear timestamps', () {
    final now = DateTime(2026, 4, 26, 12);
    final snapshot = BleDebugSnapshot.initial().copyWith(
      connectedAt: now,
      lastDataAt: now,
    );

    final cleared = snapshot.copyWith(
      clearConnectedAt: true,
      clearLastDataAt: true,
    );

    expect(cleared.connectedAt, isNull);
    expect(cleared.lastDataAt, isNull);
  });

  test('tracks command rejection diagnostics', () {
    const rejection = BleCommandRejection(
      reason: 'profile',
      profile: 'release',
      scope: 'dev_hil',
      command: 'SET_PHONE_GPS:59,30',
    );

    final snapshot = BleDebugSnapshot.initial().copyWith(
      commandRejectEvents: 1,
      lastCommandRejection: rejection,
      commandRejects: const [rejection],
    );

    expect(snapshot.commandRejectEvents, 1);
    expect(snapshot.lastCommandRejection?.commandName, 'SET_PHONE_GPS');
    expect(snapshot.commandRejects.single.requiredProfile, 'acceptance');

    final cleared = snapshot.copyWith(clearLastCommandRejection: true);
    expect(cleared.lastCommandRejection, isNull);
    expect(cleared.commandRejects, const [rejection]);
  });
}
