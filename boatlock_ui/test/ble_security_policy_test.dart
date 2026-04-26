import 'package:boatlock_ui/ble/ble_security_policy.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('unpaired commands can be sent as plain commands', () {
    expect(
      boatLockAllowsPlainCommand(
        command: 'ANCHOR_ON',
        paired: false,
        pairWindowOpen: false,
      ),
      isTrue,
    );
    expect(
      boatLockAllowsPlainCommand(
        command: 'HEARTBEAT',
        paired: false,
        pairWindowOpen: false,
      ),
      isTrue,
    );
  });

  test('paired mode keeps only transport auth and pairing setup plain', () {
    for (final command in const [
      'STREAM_START',
      'STREAM_STOP',
      'SNAPSHOT',
      'AUTH_HELLO',
      'AUTH_PROVE:abcd',
      'PAIR_SET:00112233445566778899AABBCCDDEEFF',
      'SEC_CMD:00000001:0000000000000000:HEARTBEAT',
    ]) {
      expect(
        boatLockAllowsPlainCommand(
          command: command,
          paired: true,
          pairWindowOpen: false,
        ),
        isTrue,
        reason: command,
      );
    }
  });

  test('paired mode blocks raw safety and control commands', () {
    for (final command in const [
      'AUTH_DEBUG',
      'AUTH_PROVE',
      'HEARTBEAT',
      'STOP',
      'ANCHOR_OFF',
      'ANCHOR_ON',
      'MANUAL_SET:0,0,500',
      'OTA_BEGIN:4096,abcd',
    ]) {
      expect(
        boatLockAllowsPlainCommand(
          command: command,
          paired: true,
          pairWindowOpen: false,
        ),
        isFalse,
        reason: command,
      );
    }
  });

  test(
    'raw pair clear is allowed only while physical pairing window is open',
    () {
      expect(
        boatLockAllowsPlainCommand(
          command: 'PAIR_CLEAR',
          paired: true,
          pairWindowOpen: false,
        ),
        isFalse,
      );
      expect(
        boatLockAllowsPlainCommand(
          command: 'PAIR_CLEAR',
          paired: true,
          pairWindowOpen: true,
        ),
        isTrue,
      );
    },
  );
}
