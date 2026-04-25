import 'package:boatlock_ui/ble/ble_command_text.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('accepts printable ascii command within firmware queue limit', () {
    final command = 'A' * boatLockCommandMaxBytes;

    expect(validateBoatLockCommandText(command), isNull);
    expect(encodeBoatLockCommandText('STOP'), 'STOP'.codeUnits);
    expect(
      encodeBoatLockCommandText(command),
      hasLength(boatLockCommandMaxBytes),
    );
  });

  test('rejects empty and overlong commands', () {
    expect(validateBoatLockCommandText(''), 'empty');
    expect(
      validateBoatLockCommandText('A' * (boatLockCommandMaxBytes + 1)),
      'too_long',
    );
    expect(
      encodeBoatLockCommandText('A' * (boatLockCommandMaxBytes + 1)),
      isNull,
    );
  });

  test('rejects control and non-ascii command bytes', () {
    expect(validateBoatLockCommandText('STOP\n'), 'bad_bytes');
    expect(validateBoatLockCommandText('STOP\u0000JUNK'), 'bad_bytes');
    expect(validateBoatLockCommandText('STOPé'), 'bad_bytes');
    expect(encodeBoatLockCommandText('STOP\n'), isNull);
  });
}
