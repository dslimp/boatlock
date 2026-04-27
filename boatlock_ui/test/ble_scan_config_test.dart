import 'package:boatlock_ui/ble/ble_scan_config.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('scan completion wait covers plugin scan timeout', () {
    expect(kBoatLockScanCompletionWait, greaterThan(kBoatLockScanTimeout));
  });
}
