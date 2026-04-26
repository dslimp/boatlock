bool boatLockAllowsPlainCommand({
  required String command,
  required bool paired,
  required bool pairWindowOpen,
}) {
  if (!paired) return true;
  if (command == 'STREAM_START' ||
      command == 'STREAM_STOP' ||
      command == 'SNAPSHOT') {
    return true;
  }
  if (command == 'AUTH_HELLO' ||
      command.startsWith('AUTH_PROVE:') ||
      command.startsWith('PAIR_SET:') ||
      command.startsWith('SEC_CMD:')) {
    return true;
  }
  return command == 'PAIR_CLEAR' && pairWindowOpen;
}
