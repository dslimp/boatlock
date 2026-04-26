import 'package:boatlock_ui/ble/ble_discovery_check.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('boatLockDiscoveryComplete requires all current characteristics', () {
    expect(
      boatLockDiscoveryComplete(
        dataFound: true,
        commandFound: true,
        logFound: true,
        otaFound: true,
      ),
      isTrue,
    );
    expect(
      boatLockDiscoveryComplete(
        dataFound: true,
        commandFound: true,
        logFound: false,
        otaFound: true,
      ),
      isFalse,
    );
    expect(
      boatLockDiscoveryComplete(
        dataFound: true,
        commandFound: true,
        logFound: true,
        otaFound: false,
      ),
      isFalse,
    );
  });

  test('describeMissingBoatLockCharacteristics names missing endpoints', () {
    expect(
      describeMissingBoatLockCharacteristics(
        dataFound: true,
        commandFound: true,
        logFound: true,
        otaFound: true,
      ),
      'none',
    );
    expect(
      describeMissingBoatLockCharacteristics(
        dataFound: false,
        commandFound: true,
        logFound: false,
        otaFound: false,
      ),
      'data:34cd,log:78ab,ota:9abc',
    );
  });
}
