import 'package:boatlock_ui/ble/ble_ids.dart';
import 'package:boatlock_ui/ble/ble_scan_config.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('scan service filter targets the BoatLock GATT service', () {
    final filters = boatLockScanServiceFilters();

    expect(filters.length, 1);
    expect(filters.single.toString().toLowerCase(), boatLockServiceUuid);
  });

  test('scan completion wait covers plugin scan timeout', () {
    expect(kBoatLockScanCompletionWait, greaterThan(kBoatLockScanTimeout));
  });
}
