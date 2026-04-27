enum BoatLockCommandScope { release, service, devHil, unknown }

const bool kBoatLockServiceUiEnabled = bool.fromEnvironment(
  'BOATLOCK_SERVICE_UI',
);

const bool kBoatLockDevHilCommandsEnabled = bool.fromEnvironment(
  'BOATLOCK_DEV_HIL_COMMANDS',
);

const Set<String> _releaseExactCommands = {
  'STREAM_START',
  'STREAM_STOP',
  'SNAPSHOT',
  'ANCHOR_ON',
  'ANCHOR_OFF',
  'STOP',
  'HEARTBEAT',
  'MANUAL_OFF',
  'PAIR_CLEAR',
  'AUTH_HELLO',
  'SIM_LIST',
  'SIM_STATUS',
  'SIM_REPORT',
  'SIM_ABORT',
};

const List<String> _releaseCommandPrefixes = [
  'SET_ANCHOR:',
  'MANUAL_TARGET:',
  'NUDGE_DIR:',
  'NUDGE_BRG:',
  'SET_HOLD_HEADING:',
  'PAIR_SET:',
  'AUTH_PROVE:',
  'SIM_RUN:',
];

const Set<String> _serviceExactCommands = {
  'RESET_COMPASS_OFFSET',
  'SET_STEPPER_BOW',
  'COMPASS_CAL_START',
  'COMPASS_DCD_SAVE',
  'COMPASS_DCD_AUTOSAVE_ON',
  'COMPASS_DCD_AUTOSAVE_OFF',
  'COMPASS_TARE_Z',
  'COMPASS_TARE_SAVE',
  'COMPASS_TARE_CLEAR',
  'OTA_FINISH',
  'OTA_ABORT',
};

const List<String> _serviceCommandPrefixes = [
  'SET_ANCHOR_PROFILE:',
  'SET_COMPASS_OFFSET:',
  'SET_STEP_MAXSPD:',
  'SET_STEP_ACCEL:',
  'OTA_BEGIN:',
];

const Set<String> _devHilExactCommands = {};

const List<String> _devHilCommandPrefixes = ['SET_PHONE_GPS:'];

BoatLockCommandScope classifyBoatLockCommand(String command) {
  final cmd = command;
  if (cmd.isEmpty) return BoatLockCommandScope.unknown;
  if (_releaseExactCommands.contains(cmd) ||
      _matchesPrefix(cmd, _releaseCommandPrefixes)) {
    return BoatLockCommandScope.release;
  }
  if (_serviceExactCommands.contains(cmd) ||
      _matchesPrefix(cmd, _serviceCommandPrefixes)) {
    return BoatLockCommandScope.service;
  }
  if (_devHilExactCommands.contains(cmd) ||
      _matchesPrefix(cmd, _devHilCommandPrefixes)) {
    return BoatLockCommandScope.devHil;
  }
  return BoatLockCommandScope.unknown;
}

bool _matchesPrefix(String command, List<String> prefixes) {
  for (final prefix in prefixes) {
    if (command.startsWith(prefix)) return true;
  }
  return false;
}
