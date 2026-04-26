class BleCommandRejection {
  const BleCommandRejection({
    required this.reason,
    required this.profile,
    required this.scope,
    required this.command,
  });

  final String reason;
  final String profile;
  final String scope;
  final String command;

  String get commandName {
    final separator = command.indexOf(':');
    return separator < 0 ? command : command.substring(0, separator);
  }

  String get requiredProfile {
    if (scope == 'dev_hil') return 'acceptance';
    if (scope == 'service') return 'service';
    return scope;
  }

  bool matchesCommandPrefix(String prefix) {
    return command == prefix || command.startsWith('$prefix:');
  }
}

final RegExp _profileRejectLine = RegExp(
  r'^\[BLE\] command rejected reason=(profile) '
  r'profile=(release|service|acceptance) '
  r'scope=(service|dev_hil|unknown) command=(.+)$',
);

BleCommandRejection? parseBleCommandRejectionLog(String line) {
  final match = _profileRejectLine.firstMatch(line.trim());
  if (match == null) return null;
  return BleCommandRejection(
    reason: match.group(1)!,
    profile: match.group(2)!,
    scope: match.group(3)!,
    command: match.group(4)!,
  );
}
