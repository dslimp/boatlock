const int boatLockCommandMaxBytes = 191;

String? validateBoatLockCommandText(String command) {
  if (command.isEmpty) {
    return 'empty';
  }
  if (command.length > boatLockCommandMaxBytes) {
    return 'too_long';
  }
  for (final unit in command.codeUnits) {
    if (unit < 0x20 || unit > 0x7E) {
      return 'bad_bytes';
    }
  }
  return null;
}

List<int>? encodeBoatLockCommandText(String command) {
  if (validateBoatLockCommandText(command) != null) {
    return null;
  }
  return command.codeUnits;
}
