import 'package:boatlock_ui/app_check/app_check_probe.dart';
import 'package:boatlock_ui/ble/ble_command_rejection.dart';
import 'package:boatlock_ui/smoke/ble_smoke_mode.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('app check config is runtime data', () {
    const config = BoatLockAppCheckConfig(
      mode: BleSmokeMode.ota,
      otaUrl: 'http://127.0.0.1:18080/firmware.bin',
      otaSha256:
          '0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef',
    );
    expect(config.mode, BleSmokeMode.ota);
    expect(config.otaLatestRelease, isFalse);
    expect(config.firmwareUpdateClient().configured, isTrue);
  });

  test('maps firmware profile rejections to app check stages and reasons', () {
    const compassRejection = BleCommandRejection(
      reason: 'profile',
      profile: 'release',
      scope: 'unknown',
      command: 'COMPASS_CAL_START',
    );
    const otaBeginRejection = BleCommandRejection(
      reason: 'profile',
      profile: 'release',
      scope: 'unknown',
      command: 'OTA_BEGIN:711776,abcd',
    );
    const otaFinishRejection = BleCommandRejection(
      reason: 'profile',
      profile: 'release',
      scope: 'unknown',
      command: 'OTA_FINISH',
    );

    expect(
      appCheckProfileRejectionReason(BleSmokeMode.compass, compassRejection),
      'app_compass_rejected_by_profile_release',
    );
    expect(
      appCheckProfileRejectionReason(BleSmokeMode.ota, otaBeginRejection),
      'app_ota_begin_rejected_by_profile_release',
    );
    expect(
      appCheckProfileRejectionReason(BleSmokeMode.ota, otaFinishRejection),
      'app_ota_finish_rejected_by_profile_release',
    );
    expect(
      appCheckProfileRejectionStage(compassRejection),
      'command_rejected_COMPASS_CAL_START_release',
    );
    expect(
      appCheckProfileRejectionReason(BleSmokeMode.sim, compassRejection),
      isNull,
    );
  });

  test('parses on-device SIM suite logs', () {
    expect(parseAppCheckSimListLog('[SIM] LIST S0,S1,RF0'), [
      'S0',
      'S1',
      'RF0',
    ]);
    expect(parseAppCheckSimListLog('[SIM] STATUS {}'), isEmpty);

    expect(
      parseAppCheckSimJsonLog(
        '[SIM] STATUS {"id":"S0","state":"DONE"}',
        '[SIM] STATUS ',
      ),
      {'id': 'S0', 'state': 'DONE'},
    );
    expect(
      parseAppCheckSimJsonLog('[SIM] STATUS not-json', '[SIM] STATUS '),
      isNull,
    );
    expect(appCheckSimReportChunk('[SIM] REPORT {"id":"S0"'), '{"id":"S0"');
    expect(appCheckSimReportChunk('[SIM] REPORT unavailable'), isNull);
  });
}
