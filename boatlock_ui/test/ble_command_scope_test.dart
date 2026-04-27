import 'package:boatlock_ui/ble/ble_command_scope.dart';
import 'package:flutter_test/flutter_test.dart';

void main() {
  test('classifies normal water commands as release scope', () {
    expect(
      classifyBoatLockCommand('STREAM_START'),
      BoatLockCommandScope.release,
    );
    expect(
      classifyBoatLockCommand('SET_ANCHOR:59.100000,30.200000'),
      BoatLockCommandScope.release,
    );
    expect(classifyBoatLockCommand('ANCHOR_ON'), BoatLockCommandScope.release);
    expect(
      classifyBoatLockCommand('MANUAL_TARGET:0,35,500'),
      BoatLockCommandScope.release,
    );
    expect(classifyBoatLockCommand('STOP'), BoatLockCommandScope.release);
    expect(
      classifyBoatLockCommand('SET_HOLD_HEADING:1'),
      BoatLockCommandScope.release,
    );
    expect(
      classifyBoatLockCommand('SEC_CMD:00000001:deadbeef:ANCHOR_ON'),
      BoatLockCommandScope.unknown,
    );
  });

  test('classifies installer and update commands as service scope', () {
    expect(
      classifyBoatLockCommand('SET_ANCHOR_PROFILE:quiet'),
      BoatLockCommandScope.service,
    );
    expect(
      classifyBoatLockCommand('SET_COMPASS_OFFSET:12.5'),
      BoatLockCommandScope.service,
    );
    expect(
      classifyBoatLockCommand('RESET_COMPASS_OFFSET'),
      BoatLockCommandScope.service,
    );
    expect(
      classifyBoatLockCommand('SET_STEP_MAXSPD:1000'),
      BoatLockCommandScope.service,
    );
    expect(
      classifyBoatLockCommand(
        'OTA_BEGIN:4096,00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff',
      ),
      BoatLockCommandScope.service,
    );
    expect(classifyBoatLockCommand('OTA_FINISH'), BoatLockCommandScope.service);
    expect(classifyBoatLockCommand('OTA_ABORT'), BoatLockCommandScope.service);
    expect(
      classifyBoatLockCommand('COMPASS_DCD_SAVE'),
      BoatLockCommandScope.service,
    );
  });

  test('classifies simulation commands as release scope', () {
    expect(classifyBoatLockCommand('SIM_LIST'), BoatLockCommandScope.release);
    expect(
      classifyBoatLockCommand('SIM_RUN:S0,1'),
      BoatLockCommandScope.release,
    );
    expect(classifyBoatLockCommand('SIM_STATUS'), BoatLockCommandScope.release);
    expect(classifyBoatLockCommand('SIM_REPORT'), BoatLockCommandScope.release);
    expect(classifyBoatLockCommand('SIM_ABORT'), BoatLockCommandScope.release);
  });

  test('classifies injected GPS commands as dev HIL scope', () {
    expect(
      classifyBoatLockCommand('SET_PHONE_GPS:59.100000,30.200000,0.0'),
      BoatLockCommandScope.devHil,
    );
  });

  test('rejects unknown and prefix-like commands', () {
    expect(classifyBoatLockCommand(''), BoatLockCommandScope.unknown);
    expect(classifyBoatLockCommand(' STOP'), BoatLockCommandScope.unknown);
    expect(classifyBoatLockCommand('STOP '), BoatLockCommandScope.unknown);
    expect(
      classifyBoatLockCommand('SIM_RUNNER:S0'),
      BoatLockCommandScope.unknown,
    );
    expect(
      classifyBoatLockCommand('OTA_BEGIN_NOW'),
      BoatLockCommandScope.unknown,
    );
    expect(classifyBoatLockCommand('MANUAL_TARGET'), BoatLockCommandScope.unknown);
  });
}
