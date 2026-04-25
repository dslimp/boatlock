import 'package:boatlock_ui/ble/ble_log_line.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('decodeBoatLockLogLine strips trailing nul padding', () {
    expect(
      decodeBoatLockLogLine(<int>[...'[EVENT] STOP\n'.codeUnits, 0, 0, 120]),
      '[EVENT] STOP\n',
    );
    expect(decodeBoatLockLogLine(<int>[0, 0]), '');
  });

  test('decodeBoatLockLogLine keeps malformed log bytes printable', () {
    expect(decodeBoatLockLogLine(<int>[0xE2, 0x28, 0xA1]), contains('�'));
  });
}
