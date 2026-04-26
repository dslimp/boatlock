import 'package:boatlock_ui/ble/ble_command_rejection.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('parses firmware profile rejection log', () {
    final rejection = parseBleCommandRejectionLog(
      '[BLE] command rejected reason=profile profile=release '
      'scope=service command=OTA_BEGIN:4096,abcd',
    );

    expect(rejection, isNotNull);
    expect(rejection!.reason, 'profile');
    expect(rejection.profile, 'release');
    expect(rejection.scope, 'service');
    expect(rejection.command, 'OTA_BEGIN:4096,abcd');
    expect(rejection.commandName, 'OTA_BEGIN');
    expect(rejection.requiredProfile, 'service');
    expect(rejection.matchesCommandPrefix('OTA_BEGIN'), isTrue);
    expect(rejection.matchesCommandPrefix('OTA_FINISH'), isFalse);
  });

  test('maps dev HIL scope to acceptance profile', () {
    final rejection = parseBleCommandRejectionLog(
      '[BLE] command rejected reason=profile profile=service '
      'scope=dev_hil command=SIM_RUN:S0,1',
    );

    expect(rejection, isNotNull);
    expect(rejection!.commandName, 'SIM_RUN');
    expect(rejection.requiredProfile, 'acceptance');
    expect(rejection.matchesCommandPrefix('SIM_RUN'), isTrue);
  });

  test('rejects unrelated log lines and unknown tokens', () {
    expect(parseBleCommandRejectionLog('[OTA] begin rejected'), isNull);
    expect(
      parseBleCommandRejectionLog(
        '[BLE] command rejected reason=auth profile=release '
        'scope=service command=OTA_BEGIN:bad',
      ),
      isNull,
    );
    expect(
      parseBleCommandRejectionLog(
        '[BLE] command rejected reason=profile profile=debug '
        'scope=service command=OTA_BEGIN:bad',
      ),
      isNull,
    );
  });
}
