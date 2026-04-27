import 'package:boatlock_ui/ble/ble_command_rejection.dart';
import 'package:boatlock_ui/ble/ble_debug_snapshot.dart';
import 'package:boatlock_ui/pages/settings_command_rejection_guard.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('finds matching command rejection after baseline event count', () {
    const oldRejection = BleCommandRejection(
      reason: 'profile',
      profile: 'release',
      scope: 'unknown',
      command: 'SET_ROUTE:old',
    );
    const newRejection = BleCommandRejection(
      reason: 'profile',
      profile: 'release',
      scope: 'dev_hil',
      command: 'SET_PHONE_GPS:59,30',
    );

    final snapshot = BleDebugSnapshot.initial().copyWith(
      commandRejectEvents: 2,
      lastCommandRejection: newRejection,
      commandRejects: const [oldRejection, newRejection],
    );

    expect(
      findMatchingCommandRejectionAfter(
        snapshot,
        baselineEvents: 1,
        commandPrefix: 'SET_PHONE_GPS',
      ),
      newRejection,
    );
    expect(
      findMatchingCommandRejectionAfter(
        snapshot,
        baselineEvents: 1,
        commandPrefix: 'SET_ROUTE',
      ),
      isNull,
    );
  });

  test('falls back to last rejection when history is not populated', () {
    const rejection = BleCommandRejection(
      reason: 'profile',
      profile: 'release',
      scope: 'unknown',
      command: 'SET_ROUTE:old',
    );

    final snapshot = BleDebugSnapshot.initial().copyWith(
      commandRejectEvents: 1,
      lastCommandRejection: rejection,
    );

    expect(
      findMatchingCommandRejectionAfter(
        snapshot,
        baselineEvents: 0,
        commandPrefix: 'SET_ROUTE',
      ),
      rejection,
    );
  });

  test('finds latest rejection matching any requested command prefix', () {
    const beginRejection = BleCommandRejection(
      reason: 'profile',
      profile: 'release',
      scope: 'unknown',
      command: 'SET_ROUTE:old',
    );
    const finishRejection = BleCommandRejection(
      reason: 'profile',
      profile: 'release',
      scope: 'dev_hil',
      command: 'SET_PHONE_GPS:59,30',
    );

    final snapshot = BleDebugSnapshot.initial().copyWith(
      commandRejectEvents: 2,
      lastCommandRejection: finishRejection,
      commandRejects: const [beginRejection, finishRejection],
    );

    expect(
      findAnyMatchingCommandRejectionAfter(
        snapshot,
        baselineEvents: 0,
        commandPrefixes: const ['SET_ROUTE', 'SET_PHONE_GPS'],
      ),
      finishRejection,
    );
    expect(
      findAnyMatchingCommandRejectionAfter(
        snapshot,
        baselineEvents: 0,
        commandPrefixes: const ['SET_STEP_ACCEL'],
      ),
      isNull,
    );
  });
}
