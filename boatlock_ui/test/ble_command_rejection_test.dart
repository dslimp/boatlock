import 'package:boatlock_ui/ble/ble_command_rejection.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('parses firmware profile rejection log', () {
    final rejection = parseBleCommandRejectionLog(
      '[BLE] command rejected reason=profile profile=release '
      'scope=dev_hil command=SET_PHONE_GPS:59,30',
    );

    expect(rejection, isNotNull);
    expect(rejection!.reason, 'profile');
    expect(rejection.profile, 'release');
    expect(rejection.scope, 'dev_hil');
    expect(rejection.command, 'SET_PHONE_GPS:59,30');
    expect(rejection.commandName, 'SET_PHONE_GPS');
    expect(rejection.requiredProfile, 'acceptance');
    expect(rejection.matchesCommandPrefix('SET_PHONE_GPS'), isTrue);
    expect(rejection.matchesCommandPrefix('OTA_FINISH'), isFalse);
  });

  test('maps dev HIL scope to acceptance profile', () {
    final rejection = parseBleCommandRejectionLog(
      '[BLE] command rejected reason=profile profile=release '
      'scope=dev_hil command=SET_PHONE_GPS:59,30',
    );

    expect(rejection, isNotNull);
    expect(rejection!.commandName, 'SET_PHONE_GPS');
    expect(rejection.requiredProfile, 'acceptance');
    expect(rejection.matchesCommandPrefix('SET_PHONE_GPS'), isTrue);
  });

  test('parses unknown-scope profile rejection as fail-closed feedback', () {
    final rejection = parseBleCommandRejectionLog(
      '[BLE] command rejected reason=profile profile=acceptance '
      'scope=unknown command=SET_ROUTE:old',
    );

    expect(rejection, isNotNull);
    expect(rejection!.commandName, 'SET_ROUTE');
    expect(rejection.requiredProfile, 'unknown');
    expect(rejection.matchesCommandPrefix('SET_ROUTE'), isTrue);
  });

  test('rejects unrelated log lines and unknown tokens', () {
    expect(parseBleCommandRejectionLog('[OTA] begin rejected'), isNull);
    expect(
      parseBleCommandRejectionLog(
        '[BLE] command rejected reason=bad_crc profile=release '
        'scope=dev_hil command=SET_PHONE_GPS:bad',
      ),
      isNull,
    );
    expect(
      parseBleCommandRejectionLog(
        '[BLE] command rejected reason=profile profile=debug '
        'scope=dev_hil command=SET_PHONE_GPS:bad',
      ),
      isNull,
    );
  });
}
