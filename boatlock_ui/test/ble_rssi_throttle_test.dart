import 'package:boatlock_ui/ble/ble_rssi_throttle.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('BleRssiThrottle allows the first read immediately', () {
    final throttle = BleRssiThrottle();

    expect(throttle.shouldRead(DateTime(2026)), isTrue);
  });

  test('BleRssiThrottle suppresses reads inside the interval', () {
    final throttle = BleRssiThrottle(interval: const Duration(seconds: 5));
    final start = DateTime(2026);

    expect(throttle.shouldRead(start), isTrue);
    expect(throttle.shouldRead(start.add(const Duration(seconds: 4))), isFalse);
    expect(throttle.shouldRead(start.add(const Duration(seconds: 5))), isTrue);
  });

  test('BleRssiThrottle reset allows a fresh immediate read', () {
    final throttle = BleRssiThrottle(interval: const Duration(seconds: 5));
    final start = DateTime(2026);

    expect(throttle.shouldRead(start), isTrue);
    expect(throttle.shouldRead(start.add(const Duration(seconds: 1))), isFalse);
    throttle.reset();
    expect(throttle.shouldRead(start.add(const Duration(seconds: 1))), isTrue);
  });
}
