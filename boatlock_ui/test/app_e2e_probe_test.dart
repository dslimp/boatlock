import 'package:boatlock_ui/e2e/app_e2e_probe.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('app e2e define is explicit and disabled by default', () {
    expect(kBoatLockAppE2eModeDefine, 'BOATLOCK_APP_E2E_MODE');
    expect(kBoatLockAppE2eModeName, isEmpty);
  });
}
