import 'package:boatlock_ui/e2e/app_e2e_probe.dart';
import 'package:boatlock_ui/ble/ble_command_rejection.dart';
import 'package:boatlock_ui/smoke/ble_smoke_mode.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('app e2e define is explicit and disabled by default', () {
    expect(kBoatLockAppE2eModeDefine, 'BOATLOCK_APP_E2E_MODE');
    expect(
      kBoatLockAppE2eOtaLatestReleaseDefine,
      'BOATLOCK_APP_E2E_OTA_LATEST_RELEASE',
    );
    expect(kBoatLockAppE2eModeName, isEmpty);
    expect(kBoatLockAppE2eOtaLatestRelease, isFalse);
  });

  test('maps firmware profile rejections to app e2e stages and reasons', () {
    const simRejection = BleCommandRejection(
      reason: 'profile',
      profile: 'release',
      scope: 'dev_hil',
      command: 'SIM_RUN:S0_hold_still_good,1',
    );
    const compassRejection = BleCommandRejection(
      reason: 'profile',
      profile: 'release',
      scope: 'service',
      command: 'COMPASS_CAL_START',
    );
    const otaBeginRejection = BleCommandRejection(
      reason: 'profile',
      profile: 'release',
      scope: 'service',
      command: 'OTA_BEGIN:711776,abcd',
    );
    const otaFinishRejection = BleCommandRejection(
      reason: 'profile',
      profile: 'release',
      scope: 'service',
      command: 'OTA_FINISH',
    );

    expect(
      appE2eProfileRejectionStage(simRejection),
      'command_rejected_SIM_RUN_release',
    );
    expect(
      appE2eProfileRejectionReason(BleSmokeMode.sim, simRejection),
      'app_sim_rejected_by_profile_release',
    );
    expect(
      appE2eProfileRejectionReason(BleSmokeMode.sim_suite, simRejection),
      'app_sim_suite_rejected_by_profile_release',
    );
    expect(
      appE2eProfileRejectionReason(BleSmokeMode.compass, compassRejection),
      'app_compass_rejected_by_profile_release',
    );
    expect(
      appE2eProfileRejectionReason(BleSmokeMode.ota, otaBeginRejection),
      'app_ota_begin_rejected_by_profile_release',
    );
    expect(
      appE2eProfileRejectionReason(BleSmokeMode.ota, otaFinishRejection),
      'app_ota_finish_rejected_by_profile_release',
    );
    expect(
      appE2eProfileRejectionReason(BleSmokeMode.anchor, simRejection),
      isNull,
    );
  });

  test('parses on-device SIM suite logs', () {
    expect(parseAppE2eSimListLog('[SIM] LIST S0_hold_still_good,S1_current'), [
      'S0_hold_still_good',
      'S1_current',
    ]);
    expect(parseAppE2eSimListLog('[SIM] STATUS {}'), isEmpty);

    expect(
      parseAppE2eSimJsonLog(
        '[SIM] STATUS {"id":"S0","state":"DONE"}',
        '[SIM] STATUS ',
      ),
      {'id': 'S0', 'state': 'DONE'},
    );
    expect(
      parseAppE2eSimJsonLog('[SIM] STATUS not-json', '[SIM] STATUS '),
      isNull,
    );
    expect(appE2eSimReportChunk('[SIM] REPORT {"id":"S0"'), '{"id":"S0"');
    expect(appE2eSimReportChunk('[SIM] REPORT unavailable'), isNull);
  });
}
