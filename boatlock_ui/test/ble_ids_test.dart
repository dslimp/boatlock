import 'package:boatlock_ui/ble/ble_ids.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('boatLockFullUuid expands 16-bit Bluetooth UUIDs', () {
    expect(
      boatLockFullUuid(boatLockServiceUuid),
      '000012ab-0000-1000-8000-00805f9b34fb',
    );
    expect(boatLockFullUuid('ABCD'), '0000abcd-0000-1000-8000-00805f9b34fb');
  });

  test('isBoatLockUuid accepts exact short and Bluetooth base UUID forms', () {
    expect(isBoatLockUuid('12ab', boatLockServiceUuid), isTrue);
    expect(
      isBoatLockUuid(
        '000012ab-0000-1000-8000-00805f9b34fb',
        boatLockServiceUuid,
      ),
      isTrue,
    );
    expect(isBoatLockUuid('112ab0', boatLockServiceUuid), isFalse);
    expect(
      isBoatLockUuid(
        'f00012ab-0451-4000-b000-000000000000',
        boatLockServiceUuid,
      ),
      isFalse,
    );
  });
}
