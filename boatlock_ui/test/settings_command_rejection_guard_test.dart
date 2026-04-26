import 'package:boatlock_ui/ble/ble_command_rejection.dart';
import 'package:boatlock_ui/ble/ble_debug_snapshot.dart';
import 'package:boatlock_ui/pages/settings_command_rejection_guard.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('finds matching command rejection after baseline event count', () {
    const oldRejection = BleCommandRejection(
      reason: 'profile',
      profile: 'release',
      scope: 'service',
      command: 'SET_COMPASS_OFFSET:1.0',
    );
    const newRejection = BleCommandRejection(
      reason: 'profile',
      profile: 'release',
      scope: 'service',
      command: 'SET_STEP_MAXSPD:1200',
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
        commandPrefix: 'SET_STEP_MAXSPD',
      ),
      newRejection,
    );
    expect(
      findMatchingCommandRejectionAfter(
        snapshot,
        baselineEvents: 1,
        commandPrefix: 'SET_COMPASS_OFFSET',
      ),
      isNull,
    );
  });

  test('falls back to last rejection when history is not populated', () {
    const rejection = BleCommandRejection(
      reason: 'profile',
      profile: 'release',
      scope: 'service',
      command: 'RESET_COMPASS_OFFSET',
    );

    final snapshot = BleDebugSnapshot.initial().copyWith(
      commandRejectEvents: 1,
      lastCommandRejection: rejection,
    );

    expect(
      findMatchingCommandRejectionAfter(
        snapshot,
        baselineEvents: 0,
        commandPrefix: 'RESET_COMPASS_OFFSET',
      ),
      rejection,
    );
  });

  test('finds latest rejection matching any requested command prefix', () {
    const beginRejection = BleCommandRejection(
      reason: 'profile',
      profile: 'release',
      scope: 'service',
      command: 'OTA_BEGIN:4096,abcd',
    );
    const finishRejection = BleCommandRejection(
      reason: 'profile',
      profile: 'release',
      scope: 'service',
      command: 'OTA_FINISH',
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
        commandPrefixes: const ['OTA_BEGIN', 'OTA_FINISH'],
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
